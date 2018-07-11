/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include "ns3/core-module.h"

#include "bbr-common.h"
#include "sent-packet-manager.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("SentPacketManager");
namespace bbr
{

SentPacketManager::SentPacketManager(ConnectionStats *stats,
                                     CongestionControlType congestion_control_type,
                                     LossDetectionType loss_type)
    : unacked_packets_(),
      stats_(stats),
      initial_congestion_window_(kInitialCongestionWindow),
      general_loss_algorithm_(loss_type),
      loss_algorithm_(&general_loss_algorithm_),
      least_packet_awaited_by_peer_(1),
      first_rto_transmission_(0),
      consecutive_rto_count_(0),
      consecutive_tlp_count_(0),
      pending_timer_transmission_count_(0),
      max_tail_loss_probes_(kDefaultMaxTailLossProbes),
      enable_half_rtt_tail_loss_probe_(false),
      using_pacing_(true),
      largest_newly_acked_(0),
      largest_packet_peer_knows_is_acked_(0)
{
    SetSendAlgorithm(congestion_control_type);
    rtt_stats_.set_initial_rtt_ms(std::max(kMinInitialRoundTripTimeMs, std::min(kMaxInitialRoundTripTimeMs, 100u)));       
    max_tail_loss_probes_ = kDefaultMaxTailLossProbes;
    enable_half_rtt_tail_loss_probe_ = false;
    use_new_rto_ = true;
    undo_pending_retransmits_ = false;
}

SentPacketManager::~SentPacketManager()
{
}

void SentPacketManager::SetMaxPacingRate(Bandwidth max_pacing_rate)
{
    pacing_sender_.set_max_pacing_rate(max_pacing_rate);
}

void SentPacketManager::OnIncomingAck(const AckFrame &ack_frame, uint64_t ack_receive_time)
{
    NS_ASSERT(SEQ_LE(ack_frame.largest_observed, unacked_packets_.largest_sent_packet()));
    ByteCount prior_in_flight = unacked_packets_.bytes_in_flight();
    UpdatePacketInformationReceivedByPeer(ack_frame);
    bool rtt_updated = MaybeUpdateRTT(ack_frame, ack_receive_time);
    NS_ASSERT(SEQ_GE(ack_frame.largest_observed, unacked_packets_.largest_observed()));
    unacked_packets_.IncreaseLargestObserved(ack_frame.largest_observed);

    HandleAckForSentPackets(ack_frame);
    InvokeLossDetection(ack_receive_time);
    // Ignore losses in RTO mode.
    if (consecutive_rto_count_ > 0 && !use_new_rto_)
    {
        packets_lost_.clear();
    }
    MaybeInvokeCongestionEvent(rtt_updated, prior_in_flight, ack_receive_time);
    unacked_packets_.RemoveObsoletePackets();

    // Anytime we are making forward progress and have a new RTT estimate, reset
    // the backoff counters.
    if (rtt_updated)
    {
        if (consecutive_rto_count_ > 0)
        {
            // If the ack acknowledges data sent prior to the RTO,
            // the RTO was spurious.
            if (ack_frame.largest_observed < first_rto_transmission_)
            {
                // Replace SRTT with latest_rtt and increase the variance to prevent
                // a spurious RTO from happening again.
                rtt_stats_.ExpireSmoothedMetrics();
            }
            else
            {
                if (!use_new_rto_)
                {
                    send_algorithm_->OnRetransmissionTimeout(true);
                }
            }
        }
        // Reset all retransmit counters any time a new packet is acked.
        consecutive_rto_count_ = 0;
        consecutive_tlp_count_ = 0;
    }
    // TODO(ianswett): Consider replacing the pending_retransmissions_ with a
    // fast way to retrieve the next pending retransmission, if there are any.
    // A single packet number indicating all packets below that are lost should
    // be all the state that is necessary.
    while (undo_pending_retransmits_ && !pending_retransmissions_.empty() &&
           pending_retransmissions_.front().first > largest_newly_acked_ &&
           pending_retransmissions_.front().second == LOSS_RETRANSMISSION)
    {
        // Cancel any pending retransmissions larger than largest_newly_acked_.
        unacked_packets_.RestoreToInFlight(pending_retransmissions_.front().first);
        pending_retransmissions_.pop_front();
    }
}

void SentPacketManager::UpdatePacketInformationReceivedByPeer(const AckFrame &ack_frame)
{
    if (ack_frame.packets.Empty())
    {
        least_packet_awaited_by_peer_ = ack_frame.largest_observed + 1;
    }
    else
    {
        least_packet_awaited_by_peer_ = ack_frame.packets.Min();
    }
}

void SentPacketManager::MaybeInvokeCongestionEvent(
    bool rtt_updated,
    ByteCount prior_in_flight,
    uint64_t event_time)
{
    if (!rtt_updated && packets_acked_.empty() && packets_lost_.empty())
    {
        return;
    }
    if (using_pacing_)
    {
        pacing_sender_.OnCongestionEvent(rtt_updated, prior_in_flight, event_time, packets_acked_, packets_lost_);
    }
    else
    {
        send_algorithm_->OnCongestionEvent(rtt_updated, prior_in_flight, event_time, packets_acked_, packets_lost_);
    }
    packets_acked_.clear();
    packets_lost_.clear();
}

void SentPacketManager::HandleAckForSentPackets(const AckFrame &ack_frame)
{
    const bool skip_unackable_packets_early = false;
    // Go through the packets we have not received an ack for and see if this
    // incoming_ack shows they've been seen by the peer.
    uint64_t ack_delay_time = ack_frame.ack_delay_time;
    PacketNumber packet_number = unacked_packets_.GetLeastUnacked();
    for (UnackedPacketMap::iterator it = unacked_packets_.begin(); it != unacked_packets_.end(); ++it, ++packet_number)
    {
        if (packet_number > ack_frame.largest_observed)
        {
            // These packets are still in flight.
            break;
        }
        if (skip_unackable_packets_early && it->is_unackable)
        {
            continue;
        }
        if (!ack_frame.packets.Contains(packet_number))
        {
            // Packet is still missing.
            continue;
        }
        // Packet was acked, so remove it from our unacked packet list.
        NS_LOG_DEBUG("Got an ack for packet " << packet_number);

        // If data is associated with the most recent transmission of this
        // packet, then inform the caller.
        if (it->in_flight)
        {
            packets_acked_.push_back(std::make_pair(packet_number, it->bytes_sent));
        }
        else if (skip_unackable_packets_early || !it->is_unackable)
        {
            // Packets are marked unackable after they've been acked once.
            largest_newly_acked_ = packet_number;
        }
        MarkPacketHandled(packet_number, &(*it), ack_delay_time);
    }
}

void SentPacketManager::MarkForRetransmission(
    PacketNumber packet_number,
    TransmissionType transmission_type)
{
    const TransmissionInfo &transmission_info = unacked_packets_.GetTransmissionInfo(packet_number);
    NS_ASSERT_MSG(transmission_info.data_packet, "data_packet null: " << packet_number);
    // Both TLP and the new RTO leave the packets in flight and let the loss
    // detection decide if packets are lost.
    if (transmission_type != TLP_RETRANSMISSION && transmission_type != RTO_RETRANSMISSION)
    {
        unacked_packets_.RemoveFromInFlight(packet_number);
    }
    // TODO(ianswett): Currently the RTO can fire while there are pending NACK
    // retransmissions for the same data, which is not ideal.
    if (ContainsKey(pending_retransmissions_, packet_number))
    {
        return;
    }

    pending_retransmissions_[packet_number] = transmission_type;
}

void SentPacketManager::RecordOneSpuriousRetransmission(const TransmissionInfo &info)
{
    stats_->bytes_spuriously_retransmitted += info.bytes_sent;
    ++stats_->packets_spuriously_retransmitted;
}

void SentPacketManager::RecordSpuriousRetransmissions(const TransmissionInfo& info,PacketNumber acked_packet_number) {
  PacketNumber retransmission = info.retransmission;
  while (retransmission != 0) {
    const TransmissionInfo& retransmit_info = unacked_packets_.GetTransmissionInfo(retransmission);      
    retransmission = retransmit_info.retransmission;
    RecordOneSpuriousRetransmission(retransmit_info);
  }
  // Only inform the loss detection of spurious retransmits it caused.
  if (unacked_packets_.GetTransmissionInfo(info.retransmission).transmission_type == LOSS_RETRANSMISSION) {         
    loss_algorithm_->SpuriousRetransmitDetected(unacked_packets_, Simulator::Now().GetMilliSeconds(), rtt_stats_, info.retransmission);        
  }
}

bool SentPacketManager::HasPendingRetransmissions() const {
  return !pending_retransmissions_.empty();
}

PacketHeader SentPacketManager::NextPendingRetransmission() {
    NS_ASSERT_MSG(!pending_retransmissions_.empty(),
                  "Unexpected call to NextPendingRetransmission() with empty pending "
                      << "retransmission list. Corrupted memory usage imminent.");
    PacketNumber packet_number = pending_retransmissions_.begin()->first;
    TransmissionType transmission_type = pending_retransmissions_.begin()->second;

  NS_ASSERT_MSG(unacked_packets_.IsUnacked(packet_number),  "NextPending:"<< packet_number);
  const TransmissionInfo& transmission_info = unacked_packets_.GetTransmissionInfo(packet_number);
  NS_ASSERT(transmission_info.data_packet);

  PacketHeader header;
  header.m_packet_seq = 0;
  header.m_old_packet_seq = packet_number;
  header.m_transmission_type = transmission_type;
  header.m_sent_time = 0;
  header.m_data_length = transmission_info.data_packet->data_length;
  header.m_data_packet = transmission_info.data_packet;
  header.m_data_seq = transmission_info.data_packet->data_seq;

    // by dd
    header.PicType = transmission_info.data_packet->PicType;
    header.PicIndex = transmission_info.data_packet->PicIndex;
    header.PicDataLen = transmission_info.data_packet->PicDataLen;
    header.PicPktNum = transmission_info.data_packet->PicPktNum;
    header.PicCurPktSeq = transmission_info.data_packet->PicCurPktSeq;
    header.PicGenTime = transmission_info.data_packet->PicGenTime;

  return header;
}

PacketNumber SentPacketManager::GetNewestRetransmission(
    PacketNumber packet_number,
    const TransmissionInfo& transmission_info) const {
  PacketNumber retransmission = transmission_info.retransmission;
  while (retransmission != 0) {
    packet_number = retransmission;
    retransmission = unacked_packets_.GetTransmissionInfo(retransmission).retransmission;       
  }
  return packet_number;
}

void SentPacketManager::MarkPacketHandled(PacketNumber packet_number, TransmissionInfo* info, uint64_t ack_delay_time) {                                            
  PacketNumber newest_transmission = GetNewestRetransmission(packet_number, *info);
  // Remove the most recent packet, if it is pending retransmission.
  pending_retransmissions_.erase(newest_transmission);
  // The AckListener needs to be notified about the most recent
  // transmission, since that's the one only one it tracks.
  if (newest_transmission != packet_number)
  {
    RecordSpuriousRetransmissions(*info, packet_number);
  }

  unacked_packets_.RemoveFromInFlight(info);
  unacked_packets_.RemoveRetransmittability(info);
  info->is_unackable = true;
}

bool SentPacketManager::HasUnackedPackets() const {
  return unacked_packets_.HasUnackedPackets();
}

PacketNumber SentPacketManager::GetLeastUnacked() const {
  return unacked_packets_.GetLeastUnacked();
}

bool SentPacketManager::OnPacketSent(PacketHeader &header,
                                     PacketNumber original_packet_number,
                                     uint64_t sent_time,
                                     TransmissionType transmission_type,
                                     HasRetransmittableData has_retransmittable_data)
{
    PacketNumber packet_number = header.m_packet_seq;
    NS_ASSERT(packet_number > 0);
    NS_ASSERT(!unacked_packets_.IsUnacked(packet_number));

    if (original_packet_number != 0)
    {
        pending_retransmissions_.erase(original_packet_number);
    }

    if (pending_timer_transmission_count_ > 0)
    {
        --pending_timer_transmission_count_;
    }

    bool in_flight;
    if (using_pacing_)
    {
        in_flight = pacing_sender_.OnPacketSent(
            sent_time, unacked_packets_.bytes_in_flight(), packet_number,
            header.m_data_length, has_retransmittable_data);
    }
    else
    {
        in_flight = send_algorithm_->OnPacketSent(
            sent_time, unacked_packets_.bytes_in_flight(), packet_number,
            header.m_data_length, has_retransmittable_data);
    }

    unacked_packets_.AddSentPacket(header, original_packet_number, transmission_type, sent_time, in_flight);                                  
    // Reset the retransmission timer anytime a pending packet is sent.
    return in_flight;
}

void SentPacketManager::OnRetransmissionTimeout() {
  NS_ASSERT(unacked_packets_.HasInFlightPackets());
  NS_ASSERT(0u == pending_timer_transmission_count_);
  // Handshake retransmission, timer based loss detection, TLP, and RTO are
  // implemented with a single alarm. The handshake alarm is set when the
  // handshake has not completed, the loss alarm is set when the loss detection
  // algorithm says to, and the TLP and  RTO alarms are set after that.
  // The TLP alarm is always set to run for under an RTO.
  switch (GetRetransmissionMode()) {
    case LOSS_MODE: {
      ++stats_->loss_timeout_count;
      ByteCount prior_in_flight = unacked_packets_.bytes_in_flight();
      const uint64_t now = Simulator::Now().GetMilliSeconds();
      InvokeLossDetection(now);
      MaybeInvokeCongestionEvent(false, prior_in_flight, now);
      return;
    }
    case TLP_MODE:
      // If no tail loss probe can be sent, because there are no retransmittable
      // packets, execute a conventional RTO to abandon old packets.
      ++stats_->tlp_count;
      ++consecutive_tlp_count_;
      pending_timer_transmission_count_ = 1;
      // TLPs prefer sending new data instead of retransmitting data, so
      // give the connection a chance to write before completing the TLP.
      return;
    case RTO_MODE:
      ++stats_->rto_count;
      RetransmitRtoPackets();
      return;
  }
}

bool SentPacketManager::MaybeRetransmitTailLossProbe() {
  if (pending_timer_transmission_count_ == 0) {
    return false;
  }
  PacketNumber packet_number = unacked_packets_.GetLeastUnacked();
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++packet_number) {
    // Only retransmit frames which are in flight, and therefore have been sent.
    if (!it->in_flight || !it->data_packet) {
      continue;
    }
    MarkForRetransmission(packet_number, TLP_RETRANSMISSION);
    return true;
  }
  NS_LOG_INFO("No retransmittable packets, so RetransmitOldestPacket failed.");
  return false;
}

void SentPacketManager::RetransmitRtoPackets() {
    NS_ASSERT_MSG(pending_timer_transmission_count_ <= 0, "Retransmissions already queued:" << pending_timer_transmission_count_);                 
    // Mark two packets for retransmission.
    PacketNumber packet_number = unacked_packets_.GetLeastUnacked();
    for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
         it != unacked_packets_.end(); ++it, ++packet_number)
    {
        if (it->data_packet &&
            pending_timer_transmission_count_ < kMaxRetransmissionsOnTimeout)
        {
            MarkForRetransmission(packet_number, RTO_RETRANSMISSION);
            ++pending_timer_transmission_count_;
        }
        // Abandon non-retransmittable data that's in flight to ensure it doesn't
        // fill up the congestion window.
        const bool has_retransmissions = it->retransmission != 0;
        if (!it->data_packet && it->in_flight &&
            !has_retransmissions)
        {
            // Log only for non-retransmittable data.
            // Retransmittable data is marked as lost during loss detection, and will
            // be logged later.
            unacked_packets_.RemoveFromInFlight(packet_number);
        }
  }
  if (pending_timer_transmission_count_ > 0) {
    if (consecutive_rto_count_ == 0) {
      first_rto_transmission_ = unacked_packets_.largest_sent_packet() + 1;
    }
    ++consecutive_rto_count_;
  }
}

SentPacketManager::RetransmissionTimeoutMode
SentPacketManager::GetRetransmissionMode() const {
  NS_ASSERT(unacked_packets_.HasInFlightPackets());
  if (loss_algorithm_->GetLossTimeout() != 0) {
    return LOSS_MODE;
  }
  if (consecutive_tlp_count_ < max_tail_loss_probes_) {
    if (unacked_packets_.HasUnackedRetransmittableFrames()) {
      return TLP_MODE;
    }
  }
  return RTO_MODE;
}

void SentPacketManager::InvokeLossDetection(uint64_t time) {
  if (!packets_acked_.empty()) {
    NS_ASSERT(packets_acked_.front().first <= packets_acked_.back().first);
    largest_newly_acked_ = packets_acked_.back().first;
  }
  loss_algorithm_->DetectLosses(unacked_packets_, time, rtt_stats_, largest_newly_acked_, &packets_lost_);
  for (const auto& pair : packets_lost_) {
    ++stats_->packets_lost;

    // TODO(ianswett): This could be optimized.
    if (unacked_packets_.HasRetransmittableFrames(pair.first)) {
      MarkForRetransmission(pair.first, LOSS_RETRANSMISSION);
    } else {
      // Since we will not retransmit this, we need to remove it from
      // unacked_packets_.   This is either the current transmission of
      // a packet whose previous transmission has been acked or a packet that
      // has been TLP retransmitted.
      unacked_packets_.RemoveFromInFlight(pair.first);
    }
  }
}

bool SentPacketManager::MaybeUpdateRTT(const AckFrame& ack_frame, uint64_t ack_receive_time) {
  // We rely on ack_delay_time to compute an RTT estimate, so we
  // only update rtt when the largest observed gets acked.
  // NOTE: If ack is a truncated ack, then the largest observed is in fact
  // unacked, and may cause an RTT sample to be taken.
  if (!unacked_packets_.IsUnacked(ack_frame.largest_observed)) {
    return false;
  }
  // We calculate the RTT based on the highest ACKed packet number, the lower
  // packet numbers will include the ACK aggregation delay.
  const TransmissionInfo& transmission_info = unacked_packets_.GetTransmissionInfo(ack_frame.largest_observed);
  // Ensure the packet has a valid sent time.
  if (transmission_info.sent_time == 0) {
    NS_LOG_WARN("Acked packet has zero sent time, largest_observed:" << ack_frame.largest_observed);
    return false;
  }

  uint64_t send_delta = ack_receive_time - transmission_info.sent_time;
  rtt_stats_.UpdateRtt(send_delta, ack_frame.ack_delay_time, ack_receive_time);

  return true;
}

uint64_t SentPacketManager::TimeUntilSend(uint64_t now) {
  uint64_t delay = INFINITETIME;
  // The TLP logic is entirely contained within SentPacketManager, so the
  // send algorithm does not need to be consulted.
  if (pending_timer_transmission_count_ > 0) {
    delay = 0;
  } else if (using_pacing_) {
    delay = pacing_sender_.TimeUntilSend(now, unacked_packets_.bytes_in_flight());
  } else {
    delay = send_algorithm_->TimeUntilSend(now, unacked_packets_.bytes_in_flight());       
  }
  return delay;
}

const uint64_t SentPacketManager::GetRetransmissionTime() const {
  // Don't set the timer if there is nothing to retransmit or we've already
  // queued a tlp transmission and it hasn't been sent yet.
  if (!unacked_packets_.HasInFlightPackets() || pending_timer_transmission_count_ > 0) {     
    return 0;
  }
  if (!unacked_packets_.HasUnackedRetransmittableFrames()) {
    return 0;
  }
  switch (GetRetransmissionMode()) {
    case LOSS_MODE:
      return loss_algorithm_->GetLossTimeout();
    case TLP_MODE: {
      // TODO(ianswett): When CWND is available, it would be preferable to
      // set the timer based on the earliest retransmittable packet.
      // Base the updated timer on the send time of the last packet.
      const int64_t sent_time = unacked_packets_.GetLastPacketSentTime();
      const int64_t tlp_time = sent_time + GetTailLossProbeDelay();
      // Ensure the TLP timer never gets set to a time in the past.
      return std::max(Simulator::Now().GetMilliSeconds(), tlp_time);
    }
    case RTO_MODE: {
      // The RTO is based on the first outstanding packet.
      const uint64_t sent_time = unacked_packets_.GetLastPacketSentTime();
      uint64_t rto_time = sent_time + GetRetransmissionDelay();
      // Wait for TLP packets to be acked before an RTO fires.
      uint64_t tlp_time =
          unacked_packets_.GetLastPacketSentTime() + GetTailLossProbeDelay();
      return std::max(tlp_time, rto_time);
    }
    default:
        NS_LOG_WARN("GetRetransmissionTime invalid mode");
  }
  return 0;
}

const uint64_t SentPacketManager::GetTailLossProbeDelay() const
{
    int64_t srtt = rtt_stats_.smoothed_rtt();
    if (srtt == 0)
    {
        srtt = rtt_stats_.initial_rtt_ms();
    }
    if (enable_half_rtt_tail_loss_probe_ && consecutive_tlp_count_ == 0u)
    {
        return std::max(kMinTailLossProbeTimeoutMs, static_cast<int64_t>(0.5 * srtt));
    }
    if (!unacked_packets_.HasMultipleInFlightPackets())
    {
        return std::max(2 * srtt, static_cast<int64_t>(1.5 * srtt + kMinRetransmissionTimeMs / 2));
    }
    return std::max(kMinTailLossProbeTimeoutMs, static_cast<int64_t>(2 * srtt));
}

const uint64_t SentPacketManager::GetRetransmissionDelay() const
{
    uint64_t retransmission_delay = 0;
    if (rtt_stats_.smoothed_rtt() == 0)
    {
        // We are in the initial state, use default timeout values.
        retransmission_delay = kDefaultRetransmissionTimeMs;
    }
    else
    {
        retransmission_delay = rtt_stats_.smoothed_rtt() + 4 * rtt_stats_.mean_deviation();           
        if (retransmission_delay < kMinRetransmissionTimeMs)
        {
            retransmission_delay = kMinRetransmissionTimeMs;
        }
    }

    // Calculate exponential back off.
    retransmission_delay = retransmission_delay * (1 << std::min<size_t>(consecutive_rto_count_, kMaxRetransmissions));
    if (retransmission_delay > kMaxRetransmissionTimeMs)
    {
        return kMaxRetransmissionTimeMs;
    }
    return retransmission_delay;
}

const RttStats* SentPacketManager::GetRttStats() const {
    return &rtt_stats_;
}

Bandwidth SentPacketManager::BandwidthEstimate() const {
  // TODO(ianswett): Remove BandwidthEstimate from SendAlgorithmInterface
  // and implement the logic here.
  return send_algorithm_->BandwidthEstimate();
}

ByteCount SentPacketManager::GetBytesInFlight() const {
  return unacked_packets_.bytes_in_flight();
}

void SentPacketManager::SetSendAlgorithm(CongestionControlType congestion_control_type) {
  SetSendAlgorithm(SendAlgorithmInterface::Create(
      &rtt_stats_, &unacked_packets_, congestion_control_type,
      stats_, initial_congestion_window_));
}

void SentPacketManager::SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {   
  send_algorithm_.reset(send_algorithm);
  pacing_sender_.set_sender(send_algorithm);
}

PacketNumber SentPacketManager::GetLargestObserved() const {
  return unacked_packets_.largest_observed();
}

PacketNumber SentPacketManager::GetLargestSentPacket() const {
  return unacked_packets_.largest_sent_packet();
}

// Remove this method when deprecating QUIC_VERSION_33.
PacketNumber SentPacketManager::GetLeastPacketAwaitedByPeer() const {
  return least_packet_awaited_by_peer_;
}

bool SentPacketManager::InSlowStart() const {
  return send_algorithm_->InSlowStart();
}

size_t SentPacketManager::GetConsecutiveRtoCount() const {
  return consecutive_rto_count_;
}

size_t SentPacketManager::GetConsecutiveTlpCount() const {
  return consecutive_tlp_count_;
}

void SentPacketManager::OnApplicationLimited() {
  send_algorithm_->OnApplicationLimited(unacked_packets_.bytes_in_flight());
}

const SendAlgorithmInterface* SentPacketManager::GetSendAlgorithm() const {
  return send_algorithm_.get();
}

}
}