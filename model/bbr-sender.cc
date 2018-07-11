/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include <algorithm>
#include "ns3/core-module.h"

#include "bbr-sender.h"
#include "packet-header.h"

bool debug = true;

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("BbrSender");
namespace bbr
{
// Constants based on TCP defaults.
const ByteCount kMaxSegmentSize = kDefaultTCPMSS;
// The minimum CWND to ensure delayed acks don't reduce bandwidth measurements.
// Does not inflate the pacing rate.
const ByteCount kMinimumCongestionWindow = 4 * kMaxSegmentSize;

// The gain used for the slow start, equal to 2/ln(2).
const float kHighGain = 2.885f;

//-------------------------------------add by dd start-----------------------------------//
const ByteCount kDefaultMinimumCongestionWindow = 4 * kMaxSegmentSize;
const float kDefaultHighGain = 2.885f;
// The newly derived gain for STARTUP, equal to 4 * ln(2)
const float kDerivedHighGain = 2.773f;
// The newly derived CWND gain for STARTUP, 2.
const float kDerivedHighCWNDGain = 2.773f;

// The gain used in STARTUP after loss has been detected.
// 1.5 is enough to allow for 25% exogenous loss and still observe a 25% growth
// in measured bandwidth.
const float kStartupAfterLossGain = 1.5f;
//-------------------------------------add by dd stop-----------------------------------//

// The gain used to drain the queue after the slow start.
const float kDrainGain = 1.f / kHighGain;
// The cycle of gains used during the PROBE_BW stage.
const float kPacingGain[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};

// The length of the gain cycle.
const size_t kGainCycleLength = sizeof(kPacingGain) / sizeof(kPacingGain[0]);
// The size of the bandwidth filter window, in round-trips.
const RoundTripCount kBandwidthWindowSize = kGainCycleLength + 2;

// The time after which the current min_rtt value expires.
const uint64_t kMinRttExpiry = SECOND(10);
// The minimum time the connection can spend in PROBE_RTT mode.
const uint64_t kProbeRttTime = MILLISECOND(200);
// Support bandwidth resumption in BBR.
const bool kBbrBandwidthResumption = false;
// Add the equivalent number of bytes as 3 TCP TSO segments to BBR CWND.
const bool kBbrAddTsoCwnd = false;

// If the bandwidth does not increase by the factor of |kStartupGrowthTarget|
// within |kRoundTripsWithoutGrowthBeforeExitingStartup| rounds, the connection
// will exit the STARTUP mode.
const float kStartupGrowthTarget = 1.25;
const RoundTripCount kRoundTripsWithoutGrowthBeforeExitingStartup = 3;
const float kBbrCwndGain = 2.0f;
const float kBbrRttVariationWeight = 0.0f;

BbrSender::DebugState::DebugState(const BbrSender &sender)
    : mode(sender.mode_),
      max_bandwidth(sender.max_bandwidth_.GetBest()),
      round_trip_count(sender.round_trip_count_),
      gain_cycle_index(sender.cycle_current_offset_),
      congestion_window(sender.congestion_window_),
      is_at_full_bandwidth(sender.is_at_full_bandwidth_),
      bandwidth_at_last_round(sender.bandwidth_at_last_round_),
      rounds_without_bandwidth_gain(sender.rounds_without_bandwidth_gain_),
      min_rtt(sender.min_rtt_),
      min_rtt_timestamp(sender.min_rtt_timestamp_),
      recovery_state(sender.recovery_state_),
      recovery_window(sender.recovery_window_),
      last_sample_is_app_limited(sender.last_sample_is_app_limited_),
      end_of_app_limited_phase(sender.sampler_->end_of_app_limited_phase()) {}

BbrSender::DebugState::DebugState(const DebugState &state) = default;

BbrSender::BbrSender(const RttStats *rtt_stats,
                     const UnackedPacketMap *unacked_packets,
                     PacketCount initial_tcp_congestion_window,
                     PacketCount max_tcp_congestion_window)
    : rtt_stats_(rtt_stats),
      unacked_packets_(unacked_packets),
      random_(),
      mode_(STARTUP),
      sampler_(new BandwidthSampler()),
      round_trip_count_(0),
      last_sent_packet_(0),
      current_round_trip_end_(0),
      max_bandwidth_(kBandwidthWindowSize, Bandwidth::Zero(), 0),
      max_ack_height_(kBandwidthWindowSize, 0, 0),
      aggregation_epoch_start_time_(0),
      aggregation_epoch_bytes_(0),
      bytes_acked_since_queue_drained_(0),
      max_aggregation_bytes_multiplier_(0),
      min_rtt_(0),
      min_rtt_timestamp_(0),
      congestion_window_(initial_tcp_congestion_window * kDefaultTCPMSS),
      initial_congestion_window_(initial_tcp_congestion_window * kDefaultTCPMSS),
      max_congestion_window_(max_tcp_congestion_window * kDefaultTCPMSS),
    //-------------------------------------add by dd start-----------------------------------//
      min_congestion_window_(kDefaultMinimumCongestionWindow),
      high_gain_(kDefaultHighGain),
      high_cwnd_gain_(kDefaultHighGain),
      drain_gain_(1.f / kDefaultHighGain),
      has_non_app_limited_sample_(false),
      is_app_limited_recovery_(false),
      enable_ack_aggregation_during_startup_(false),
      drain_to_target_(false),
    //-------------------------------------add by dd stop-----------------------------------//
      pacing_rate_(Bandwidth::Zero()),
      pacing_gain_(1),
      congestion_window_gain_(1),
      congestion_window_gain_constant_(kBbrCwndGain),
      rtt_variance_weight_(kBbrRttVariationWeight),
      num_startup_rtts_(kRoundTripsWithoutGrowthBeforeExitingStartup),
      exit_startup_on_loss_(false),
      cycle_current_offset_(0),
      last_cycle_start_(0),
      is_at_full_bandwidth_(false),
      rounds_without_bandwidth_gain_(0),
      bandwidth_at_last_round_(Bandwidth::Zero()),
      exiting_quiescence_(false),
      exit_probe_rtt_at_(0),
      probe_rtt_round_passed_(false),
      last_sample_is_app_limited_(false),
      recovery_state_(NOT_IN_RECOVERY),
      end_recovery_at_(0),
      recovery_window_(max_congestion_window_),
      rate_based_recovery_(false)
{
    random_ = CreateObject<UniformRandomVariable> ();
    EnterStartupMode();
}

BbrSender::~BbrSender() {}

//-------------------------------------add by dd start-----------------------------------//
void BbrSender::SetInitialCongestionWindowInPackets(PacketCount congestion_window) {
    if (mode_ == STARTUP) {
        initial_congestion_window_ = congestion_window * kDefaultTCPMSS;
        congestion_window_ = congestion_window * kDefaultTCPMSS;
    }
}
//-------------------------------------add by dd stop-----------------------------------//

bool BbrSender::InSlowStart() const
{
    return mode_ == STARTUP;
}

bool BbrSender::OnPacketSent(uint64_t sent_time, ByteCount bytes_in_flight, PacketNumber packet_number, ByteCount bytes, HasRetransmittableData is_retransmittable)
{
    last_sent_packet_ = packet_number;

    if (bytes_in_flight == 0 && sampler_->is_app_limited())
    {
        exiting_quiescence_ = true;
    }

    if (aggregation_epoch_start_time_ == 0)
    {
        aggregation_epoch_start_time_ = sent_time;
    }

    sampler_->OnPacketSent(sent_time, packet_number, bytes, bytes_in_flight, is_retransmittable);
    return true;
}

uint64_t BbrSender::TimeUntilSend(uint64_t /* now */, ByteCount bytes_in_flight)
{
    if (bytes_in_flight < GetCongestionWindow())
    {
        return 0;
    }
    return INFINITETIME;
}

Bandwidth BbrSender::PacingRate(ByteCount bytes_in_flight) const
{
    if (pacing_rate_.IsZero())
    {
        //return kHighGain * Bandwidth::FromBytesAndTimeDelta(initial_congestion_window_, GetMinRtt());    // by dd
        return high_gain_ * Bandwidth::FromBytesAndTimeDelta(initial_congestion_window_, GetMinRtt()); // by dd
    }
    return pacing_rate_;
}

Bandwidth BbrSender::BandwidthEstimate() const
{
    return max_bandwidth_.GetBest();
}

ByteCount BbrSender::GetCongestionWindow() const
{
    if (mode_ == PROBE_RTT)
    {
        return kMinimumCongestionWindow;
    }

    //if (InRecovery() && !rate_based_recovery_)                    // by dd
    if (InRecovery() && !(rate_based_startup_ && mode_ == STARTUP)) // by dd
    {
        return std::min(congestion_window_, recovery_window_);
    }

    return congestion_window_;
}

ByteCount BbrSender::GetSlowStartThreshold() const
{
    return 0;
}

bool BbrSender::InRecovery() const
{
    return recovery_state_ != NOT_IN_RECOVERY;
}

bool BbrSender::IsProbingForMoreBandwidth() const 
{
    return (mode_ == PROBE_BW && pacing_gain_ > 1) || mode_ == STARTUP;
}

// void BbrSender::SetFromConfig(const QuicConfig& config,
//                               Perspective perspective) {
//   if (config.HasClientRequestedIndependentOption(kLRTT, perspective)) {
//     exit_startup_on_loss_ = true;
//   }
//   if (config.HasClientRequestedIndependentOption(k1RTT, perspective)) {
//     num_startup_rtts_ = 1;
//   }
//   if (config.HasClientRequestedIndependentOption(k2RTT, perspective)) {
//     num_startup_rtts_ = 2;
//   }
//   if (config.HasClientRequestedIndependentOption(kBBRS, perspective)) {
//     slower_startup_ = true;
//   }
//   if (config.HasClientRequestedIndependentOption(kBBR3, perspective)) {
//     drain_to_target_ = true;
//   }
//   if (config.HasClientRequestedIndependentOption(kBBS1, perspective)) {
//     rate_based_startup_ = true;
//   }
//   if (config.HasClientRequestedIndependentOption(kBBS2, perspective)) {
//     initial_conservation_in_startup_ = MEDIUM_GROWTH;
//   }
//   if (config.HasClientRequestedIndependentOption(kBBS3, perspective)) {
//     initial_conservation_in_startup_ = GROWTH;
//   }
//   if (config.HasClientRequestedIndependentOption(kBBR4, perspective)) {
//     max_ack_height_.SetWindowLength(2 * kBandwidthWindowSize);
//   }
//   if (config.HasClientRequestedIndependentOption(kBBR5, perspective)) {
//     max_ack_height_.SetWindowLength(4 * kBandwidthWindowSize);
//   }
//   if (GetQuicReloadableFlag(quic_bbr_less_probe_rtt) &&
//       config.HasClientRequestedIndependentOption(kBBR6, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_less_probe_rtt, 1, 3);
//     probe_rtt_based_on_bdp_ = true;
//   }
//   if (GetQuicReloadableFlag(quic_bbr_less_probe_rtt) &&
//       config.HasClientRequestedIndependentOption(kBBR7, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_less_probe_rtt, 2, 3);
//     probe_rtt_skipped_if_similar_rtt_ = true;
//   }
//   if (GetQuicReloadableFlag(quic_bbr_less_probe_rtt) &&
//       config.HasClientRequestedIndependentOption(kBBR8, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_less_probe_rtt, 3, 3);
//     probe_rtt_disabled_if_app_limited_ = true;
//   }
//   if (GetQuicReloadableFlag(quic_bbr_slower_startup3) &&
//       config.HasClientRequestedIndependentOption(kBBQ1, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_slower_startup3, 1, 4);
//     set_high_gain(kDerivedHighGain);
//     set_high_cwnd_gain(kDerivedHighGain);
//     set_drain_gain(1.f / kDerivedHighGain);
//   }
//   if (GetQuicReloadableFlag(quic_bbr_slower_startup3) &&
//       config.HasClientRequestedIndependentOption(kBBQ2, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_slower_startup3, 2, 4);
//     set_high_cwnd_gain(kDerivedHighCWNDGain);
//   }
//   if (GetQuicReloadableFlag(quic_bbr_slower_startup3) &&
//       config.HasClientRequestedIndependentOption(kBBQ3, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_slower_startup3, 3, 4);
//     enable_ack_aggregation_during_startup_ = true;
//   }
//   if (GetQuicReloadableFlag(quic_bbr_slower_startup3) &&
//       config.HasClientRequestedIndependentOption(kBBQ4, perspective)) {
//     QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_slower_startup3, 4, 4);
//     set_drain_gain(kModerateProbeRttMultiplier);
//   }
//   if (config.HasClientRequestedIndependentOption(kMIN1, perspective)) {
//     min_congestion_window_ = kMaxSegmentSize;
//   }
// }

void BbrSender::AdjustNetworkParameters(Bandwidth bandwidth, uint64_t rtt)
{
    if (!kBbrBandwidthResumption)
    {
        return;
    }

    NS_LOG_DEBUG("kBbrBandwidthResumption:" << kBbrBandwidthResumption);

    if (!bandwidth.IsZero())
    {
        max_bandwidth_.Update(bandwidth, round_trip_count_);
    }
    if (rtt != 0 && (min_rtt_ > rtt || min_rtt_ == 0))
    {
        min_rtt_ = rtt;
    }
}

void BbrSender::OnCongestionEvent(bool /*rtt_updated*/, ByteCount prior_in_flight, uint64_t event_time,const CongestionVector &acked_packets, const CongestionVector &lost_packets)                                 
{
    const ByteCount total_bytes_acked_before = sampler_->total_bytes_acked();

    bool is_round_start = false;
    bool min_rtt_expired = false;

    DiscardLostPackets(lost_packets);

    // Input the new data into the BBR model of the connection.
    ByteCount excess_acked = 0;     // by dd ,add new
    if (!acked_packets.empty())
    {
        PacketNumber last_acked_packet = acked_packets.rbegin()->first;
        is_round_start = UpdateRoundTripCounter(last_acked_packet);
        min_rtt_expired = UpdateBandwidthAndMinRtt(event_time, acked_packets);
        UpdateRecoveryState(last_acked_packet, !lost_packets.empty(), is_round_start);

        const ByteCount bytes_acked = sampler_->total_bytes_acked() - total_bytes_acked_before;

        //UpdateAckAggregationBytes(event_time, bytes_acked); // comment by dd
        excess_acked = UpdateAckAggregationBytes(event_time, bytes_acked); // add by dd
        /*--------------------------Comment by dd start--------------------------------------------------*/
        // if (max_aggregation_bytes_multiplier_ > 0)
        // {
        //     if (unacked_packets_->bytes_in_flight() <= 1.25 * GetTargetCongestionWindow(pacing_gain_))
        //     {
        //         bytes_acked_since_queue_drained_ = 0;
        //     }
        //     else
        //     {
        //         bytes_acked_since_queue_drained_ += bytes_acked;
        //     }
        // }
        /*----------------------------Comment by dd stop--------------------------------------------------*/
    }

    // Handle logic specific to PROBE_BW mode.
    if (mode_ == PROBE_BW)
    {
        UpdateGainCyclePhase(event_time, prior_in_flight, !lost_packets.empty());
    }

    // Handle logic specific to STARTUP and DRAIN modes.
    if (is_round_start && !is_at_full_bandwidth_)
    {
        CheckIfFullBandwidthReached();
    }
    MaybeExitStartupOrDrain(event_time);

    // Handle logic specific to PROBE_RTT.
    MaybeEnterOrExitProbeRtt(event_time, is_round_start, min_rtt_expired);

    // Calculate number of packets acked and lost.
    ByteCount bytes_acked = sampler_->total_bytes_acked() - total_bytes_acked_before;
    ByteCount bytes_lost = 0;
    for (const auto &packet : lost_packets)
    {
        bytes_lost += packet.second;
    }

    // After the model is updated, recalculate the pacing rate and congestion window.
    CalculatePacingRate();
    //CalculateCongestionWindow(bytes_acked); // by dd 
    CalculateCongestionWindow(bytes_acked, excess_acked); // by dd, add new paramter
    CalculateRecoveryWindow(bytes_acked, bytes_lost);

    // Cleanup internal state.
    sampler_->RemoveObsoletePackets(unacked_packets_->GetLeastUnacked());
}

CongestionControlType BbrSender::GetCongestionControlType() const
{
    return kBBR;
}

uint64_t BbrSender::GetMinRtt() const
{
    return min_rtt_ != 0 ? min_rtt_ : rtt_stats_->initial_rtt_ms();
}

ByteCount BbrSender::GetTargetCongestionWindow(float gain) const
{
    ByteCount bdp = GetMinRtt() * BandwidthEstimate();
    ByteCount congestion_window = gain * bdp;

    // BDP estimate will be zero if no bandwidth samples are available yet.
    if (congestion_window == 0)
    {
        congestion_window = gain * initial_congestion_window_;
    }

    return std::max(congestion_window, kMinimumCongestionWindow);
}

void BbrSender::EnterStartupMode()
{
    mode_ = STARTUP;
    //pacing_gain_ = kHighGain;                 // by dd
    //congestion_window_gain_ = kHighGain;      // by dd
    pacing_gain_ = high_gain_;                  // by dd
    congestion_window_gain_ = high_cwnd_gain_;  // by dd

    if(debug){
        std::cout<<"BbrSender mode "<< int(mode_) <<" time " << Simulator::Now().GetMilliSeconds()<< std::endl;
    }
}

void BbrSender::EnterProbeBandwidthMode(uint64_t now)
{
    //return;
    mode_ = PROBE_BW;
    congestion_window_gain_ = congestion_window_gain_constant_;

    // Pick a random offset for the gain cycle out of {0, 2..7} range. 1 is
    // excluded because in that case increased gain and decreased gain would not
    // follow each other.
    cycle_current_offset_ = random_->GetInteger() % (kGainCycleLength - 1);
    if (cycle_current_offset_ >= 1)
    {
        cycle_current_offset_ += 1;
    }

    last_cycle_start_ = now;
    pacing_gain_ = kPacingGain[cycle_current_offset_];

    if(debug){
        std::cout<<"BbrSender mode "<< int(mode_) <<" time " << Simulator::Now().GetMilliSeconds()<< std::endl;
    }
}

void BbrSender::DiscardLostPackets(const CongestionVector &lost_packets)
{
    for (const auto &packet : lost_packets)
    {
        sampler_->OnPacketLost(packet.first);
    }
}

bool BbrSender::UpdateRoundTripCounter(PacketNumber last_acked_packet)
{
    if (last_acked_packet > current_round_trip_end_)
    {
        round_trip_count_++;
        current_round_trip_end_ = last_sent_packet_;
        return true;
    }

    return false;
}

bool BbrSender::UpdateBandwidthAndMinRtt(
    uint64_t now,
    const CongestionVector &acked_packets)
{
    uint64_t sample_min_rtt = INFINITETIME;
    for (const auto &packet : acked_packets)
    {   /*-------------------------------------add by dd start---------------------------------------*/
        if (packet.second == 0) {
            // Skip acked packets with 0 in flight bytes when updating bandwidth.
            continue;
        }
        /*-------------------------------------add by dd stop---------------------------------------*/

        BandwidthSample bandwidth_sample = sampler_->OnPacketAcknowledged(now, packet.first);
        last_sample_is_app_limited_ = bandwidth_sample.is_app_limited;

        has_non_app_limited_sample_ |= !bandwidth_sample.is_app_limited; // add by dd

        if (bandwidth_sample.rtt != 0)
        {
            sample_min_rtt = std::min(sample_min_rtt, bandwidth_sample.rtt);
        }

        if (!bandwidth_sample.is_app_limited || bandwidth_sample.bandwidth > BandwidthEstimate())
        {
            max_bandwidth_.Update(bandwidth_sample.bandwidth, round_trip_count_);
        }
    }

    // If none of the RTT samples are valid, return immediately.
    if (sample_min_rtt == INFINITETIME)
    {
        return false;
    }

    // Do not expire min_rtt if none was ever available.
    bool min_rtt_expired = min_rtt_ != 0 && (now > (min_rtt_timestamp_ + kMinRttExpiry));

    if (min_rtt_expired || sample_min_rtt < min_rtt_ || min_rtt_ == 0)
    {
        NS_LOG_DEBUG("Min RTT updated, old value: " << min_rtt_
                                                    << ", new value: " << sample_min_rtt
                                                    << ", current time: " << now);

        min_rtt_ = sample_min_rtt;
        min_rtt_timestamp_ = now;
    }

    return min_rtt_expired;
}

void BbrSender::UpdateGainCyclePhase(uint64_t now, ByteCount prior_in_flight, bool has_losses)
{
    const ByteCount bytes_in_flight = unacked_packets_->bytes_in_flight(); // added by dd
    // In most cases, the cycle is advanced after an RTT passes.
    bool should_advance_gain_cycling = now - last_cycle_start_ > GetMinRtt();

    // If the pacing gain is above 1.0, the connection is trying to probe the
    // bandwidth by increasing the number of bytes in flight to at least
    // pacing_gain * BDP.  Make sure that it actually reaches the target, as long
    // as there are no losses suggesting that the buffers are not able to hold
    // that much.
    if (pacing_gain_ > 1.0 && !has_losses && prior_in_flight < GetTargetCongestionWindow(pacing_gain_))
    {
        should_advance_gain_cycling = false;
    }

    // If pacing gain is below 1.0, the connection is trying to drain the extra
    // queue which could have been incurred by probing prior to it.  If the number
    // of bytes in flight falls down to the estimated BDP value earlier, conclude
    // that the queue has been successfully drained and exit this cycle early.
    //if (pacing_gain_ < 1.0 && prior_in_flight <= GetTargetCongestionWindow(1)) // comment by dd
    if (pacing_gain_ < 1.0 && bytes_in_flight <= GetTargetCongestionWindow(1))  // changed by dd
    {
        should_advance_gain_cycling = true;
    }

    if (should_advance_gain_cycling)
    {
        cycle_current_offset_ = (cycle_current_offset_ + 1) % kGainCycleLength;
        last_cycle_start_ = now;

        /*-------------------------------------add by dd start---------------------------------------*/
        // Stay in low gain mode until the target BDP is hit.
        // Low gain mode will be exited immediately when the target BDP is achieved.
        if (drain_to_target_ && pacing_gain_ < 1 &&
            kPacingGain[cycle_current_offset_] == 1 &&
            bytes_in_flight > GetTargetCongestionWindow(1)) {
            return;
        }
        /*-------------------------------------add by dd stop---------------------------------------*/

        pacing_gain_ = kPacingGain[cycle_current_offset_];  
    }

    if(debug)
    {
        int paceInt;
        if(pacing_gain_>1.0){
            paceInt = 2;
        }else if(pacing_gain_<1.0){
            paceInt = 0;
        }else{
            paceInt = 1;
        }
        std::cout<<"BbrSender pacing_gain "<< paceInt <<" time " << Simulator::Now().GetMilliSeconds()<< std::endl;
    }
}

void BbrSender::CheckIfFullBandwidthReached()
{
    if (last_sample_is_app_limited_)
    {
        return;
    }

    Bandwidth target = bandwidth_at_last_round_ * kStartupGrowthTarget;
    if (BandwidthEstimate() >= target)
    {
        bandwidth_at_last_round_ = BandwidthEstimate();
        rounds_without_bandwidth_gain_ = 0;
        return;
    }

    rounds_without_bandwidth_gain_++;
    if ((rounds_without_bandwidth_gain_ >= num_startup_rtts_) || (exit_startup_on_loss_ && InRecovery()))
    {
        is_at_full_bandwidth_ = true;
    }
}

void BbrSender::MaybeExitStartupOrDrain(uint64_t now)
{
    if (mode_ == STARTUP && is_at_full_bandwidth_)
    {
        mode_ = DRAIN;
        //pacing_gain_ = kDrainGain;                // comment by dd
        //congestion_window_gain_ = kHighGain;      // comment by dd
        pacing_gain_ = drain_gain_;                 // changed by dd
        congestion_window_gain_ = high_cwnd_gain_;  // changed by dd
    }
    if (mode_ == DRAIN && unacked_packets_->bytes_in_flight() <= GetTargetCongestionWindow(1))
    {
        EnterProbeBandwidthMode(now);
    }

    if(debug){
        std::cout<<"BbrSender mode "<< int(mode_) <<" time " << Simulator::Now().GetMilliSeconds()<< std::endl;
    }
}

void BbrSender::MaybeEnterOrExitProbeRtt(uint64_t now, bool is_round_start, bool min_rtt_expired)
{
    //return;
    if (min_rtt_expired && !exiting_quiescence_ && mode_ != PROBE_RTT)
    {
        mode_ = PROBE_RTT;
        pacing_gain_ = 1;
        // Do not decide on the time to exit PROBE_RTT until the |bytes_in_flight|
        // is at the target small value.
        exit_probe_rtt_at_ = 0;
    }

    if (mode_ == PROBE_RTT)
    {
        sampler_->OnAppLimited();

        if (exit_probe_rtt_at_ == 0)
        {
            // If the window has reached the appropriate size, schedule exiting
            // PROBE_RTT.  The CWND during PROBE_RTT is kMinimumCongestionWindow, but
            // we allow an extra packet since QUIC checks CWND before sending a packet.
            if (unacked_packets_->bytes_in_flight() < kMinimumCongestionWindow + kMaxPacketSize)
            {
                exit_probe_rtt_at_ = now + kProbeRttTime;
                probe_rtt_round_passed_ = false;
            }
        }
        else
        {
            if (is_round_start)
            {
                probe_rtt_round_passed_ = true;
            }
            if (now >= exit_probe_rtt_at_ && probe_rtt_round_passed_)
            {
                min_rtt_timestamp_ = now;
                if (!is_at_full_bandwidth_)
                {
                    EnterStartupMode();
                }
                else
                {
                    EnterProbeBandwidthMode(now);
                }
            }
        }
    }

    exiting_quiescence_ = false;

    if(debug){
        std::cout<<"BbrSender mode "<< int(mode_) <<" time " << Simulator::Now().GetMilliSeconds()<< std::endl;
    }
}

void BbrSender::UpdateRecoveryState(PacketNumber last_acked_packet, bool has_losses, bool is_round_start)
{
    // Exit recovery when there are no losses for a round.
    if (has_losses)
    {
        end_recovery_at_ = last_sent_packet_;
    }

    switch (recovery_state_)
    {
    case NOT_IN_RECOVERY:
        // Enter conservation on the first loss.
        if (has_losses)
        {
            recovery_state_ = CONSERVATION;
            // This will cause the |recovery_window_| to be set to the correct
            // value in CalculateRecoveryWindow().
            
            //------------------------------add by dd start-----------------------------//
            if (mode_ == STARTUP) {
                recovery_state_ = initial_conservation_in_startup_;
            }
            //------------------------------add by dd stop-----------------------------//

            recovery_window_ = 0;
            // Since the conservation phase is meant to be lasting for a whole
            // round, extend the current round as if it were started right now.
            current_round_trip_end_ = last_sent_packet_;
            
            //------------------------------add by dd start-----------------------------//
            if (last_sample_is_app_limited_) {
                is_app_limited_recovery_ = true;
            }
            //------------------------------add by dd stop-----------------------------//
        }
        break;

    case CONSERVATION:
    case MEDIUM_GROWTH:         // add by dd
        if (is_round_start)
        {
            recovery_state_ = GROWTH;
        }

    case GROWTH:
        // Exit recovery if appropriate.
        if (!has_losses && last_acked_packet > end_recovery_at_)
        {
            recovery_state_ = NOT_IN_RECOVERY;
            is_app_limited_recovery_ = false;   // add by dd
        }

        break;
    }

    //------------------------------add by dd start-----------------------------//
    if (recovery_state_ != NOT_IN_RECOVERY && is_app_limited_recovery_) {
        sampler_->OnAppLimited();
    }
    //------------------------------add by dd stop-----------------------------//
}

// TODO(ianswett): Move this logic into BandwidthSampler.
// void BbrSender::UpdateAckAggregationBytes(uint64_t ack_time, ByteCount newly_acked_bytes)    // comment by dd
ByteCount BbrSender::UpdateAckAggregationBytes(uint64_t ack_time, ByteCount newly_acked_bytes)  // changed by dd
{
    // Compute how many bytes are expected to be delivered, assuming max bandwidth is correct.
    ByteCount expected_bytes_acked = max_bandwidth_.GetBest() * (ack_time - aggregation_epoch_start_time_);
    // Reset the current aggregation epoch as soon as the ack arrival rate is less
    // than or equal to the max bandwidth.
    if (aggregation_epoch_bytes_ <= expected_bytes_acked)
    {
        // Reset to start measuring a new aggregation epoch.
        aggregation_epoch_bytes_ = newly_acked_bytes;
        aggregation_epoch_start_time_ = ack_time;
        //return; // comment by dd
        return 0; // change by dd
    }

    // Compute how many extra bytes were delivered vs max bandwidth.
    // Include the bytes most recently acknowledged to account for stretch acks.
    aggregation_epoch_bytes_ += newly_acked_bytes;
    max_ack_height_.Update(aggregation_epoch_bytes_ - expected_bytes_acked, round_trip_count_);

    return aggregation_epoch_bytes_ - expected_bytes_acked; // add by dd
}

void BbrSender::CalculatePacingRate()
{
    if (BandwidthEstimate().IsZero())
    {
        return;
    }

    Bandwidth target_rate = pacing_gain_ * BandwidthEstimate();
    // if (rate_based_recovery_ && InRecovery())                                // by dd
    // {                                                                        // by dd
    //     pacing_rate_ = pacing_gain_ * max_bandwidth_.GetThirdBest();         // by dd
    // }                                                                        // by dd
    if (is_at_full_bandwidth_)
    {
        pacing_rate_ = target_rate;
        return;
    }

    // Pace at the rate of initial_window / RTT as soon as RTT measurements are available.
    if (pacing_rate_.IsZero() && rtt_stats_->min_rtt() != 0)
    {
        pacing_rate_ = Bandwidth::FromBytesAndTimeDelta(initial_congestion_window_, rtt_stats_->min_rtt());
        return;
    }

    //------------------------------add by dd start-----------------------------//
    // Slow the pacing rate in STARTUP once loss has ever been detected.
    const bool has_ever_detected_loss = end_recovery_at_ > 0;
    if (slower_startup_ && has_ever_detected_loss && has_non_app_limited_sample_) {
        pacing_rate_ = kStartupAfterLossGain * BandwidthEstimate();
        return;
    }
    //------------------------------add by dd stop-----------------------------//

    // Do not decrease the pacing rate during the startup.
    pacing_rate_ = std::max(pacing_rate_, target_rate);

    // for test
//    Bandwidth BWmax = Bandwidth::FromBitsPerSecond(2000000);
//    Bandwidth max_pacing_rate = pacing_gain_ * BWmax;
//    pacing_rate_ = std::min(pacing_rate_, max_pacing_rate);
}

//void BbrSender::CalculateCongestionWindow(ByteCount bytes_acked)  // by dd
void BbrSender::CalculateCongestionWindow(ByteCount bytes_acked, ByteCount excess_acked)    // added a parameter by dd
{
    if (mode_ == PROBE_RTT)
    {
        return;
    }

    ByteCount target_window = GetTargetCongestionWindow(congestion_window_gain_);

    /*---------------------------------------------------Comment by dd start------------------------------------------------------------*/
    // if (rtt_variance_weight_ > 0.f && !BandwidthEstimate().IsZero())
    // {
    //     target_window += rtt_variance_weight_ * BandwidthEstimate().ToBytesPerPeriod(rtt_stats_->mean_deviation());
    // }
    // else if (max_aggregation_bytes_multiplier_ > 0 && is_at_full_bandwidth_)
    // {
    //     // Subtracting only half the bytes_acked_since_queue_drained ensures sending
    //     // doesn't completely stop for a long period of time if the queue hasn't
    //     // been drained recently.
    //     if (max_aggregation_bytes_multiplier_ * max_ack_height_.GetBest() > bytes_acked_since_queue_drained_ / 2)
    //     {
    //         target_window +=
    //             max_aggregation_bytes_multiplier_ * max_ack_height_.GetBest() -
    //             bytes_acked_since_queue_drained_ / 2;
    //     }
    // }
    // else if (is_at_full_bandwidth_)
    // {
    //     target_window += max_ack_height_.GetBest();
    // }

    // if (kBbrAddTsoCwnd)
    // {
    //     // QUIC doesn't have TSO, but it does have similarly quantized pacing, so
    //     // allow extra CWND to make QUIC's BBR CWND identical to TCP's.
    //     ByteCount tso_segs_goal = 0;
    //     if (pacing_rate_ < Bandwidth::FromKBitsPerSecond(1200))
    //     {
    //         tso_segs_goal = kDefaultTCPMSS;
    //     }
    //     else if (pacing_rate_ < Bandwidth::FromKBitsPerSecond(24000))
    //     {
    //         tso_segs_goal = 2 * kDefaultTCPMSS;
    //     }
    //     else
    //     {
    //         tso_segs_goal =
    //             std::min(pacing_rate_ * MILLISECOND(1),
    //                      /* 64k */ static_cast<ByteCount>(1 << 16));
    //     }
    //     target_window += 3 * tso_segs_goal;
    // }

    /*---------------------------------------------------Comment by dd stop------------------------------------------------------------*/
    /*---------------------------------------------------change by dd start------------------------------------------------------------*/
    if (is_at_full_bandwidth_) {
        // Add the max recently measured ack aggregation to CWND.
        target_window += max_ack_height_.GetBest();
    } else if (enable_ack_aggregation_during_startup_) {
        // Add the most recent excess acked.  Because CWND never decreases in
        // STARTUP, this will automatically create a very localized max filter.
        target_window += excess_acked;
    }
    /*---------------------------------------------------change by dd stop------------------------------------------------------------*/

    // Instead of immediately setting the target CWND as the new one, BBR grows
    // the CWND towards |target_window| by only increasing it |bytes_acked| at a
    // time.
    if (is_at_full_bandwidth_)
    {
        congestion_window_ = std::min(target_window, congestion_window_ + bytes_acked);
    }
    else if (congestion_window_ < target_window || sampler_->total_bytes_acked() < initial_congestion_window_)
    {
        // If the connection is not yet out of startup phase, do not decrease the window.
        congestion_window_ = congestion_window_ + bytes_acked;
    }

    // Enforce the limits on the congestion window.
    congestion_window_ = std::max(congestion_window_, kMinimumCongestionWindow);
    congestion_window_ = std::min(congestion_window_, max_congestion_window_);


//    ByteCount MaxCongestionWindow = 25000 * congestion_window_gain_;
//    congestion_window_ = std::min(congestion_window_, MaxCongestionWindow);
}

void BbrSender::CalculateRecoveryWindow(ByteCount bytes_acked, ByteCount bytes_lost)
{
    //if (rate_based_recovery_) // comment by dd
    if (rate_based_startup_ && mode_ == STARTUP) {
        return;
    }

    if (recovery_state_ == NOT_IN_RECOVERY)
    {
        return;
    }

    // Set up the initial recovery window.
    if (recovery_window_ == 0)
    {
        recovery_window_ = unacked_packets_->bytes_in_flight() + bytes_acked;
        recovery_window_ = std::max(kMinimumCongestionWindow, recovery_window_);
        return;
    }

    // Remove losses from the recovery window, while accounting for a potential
    // integer underflow.
    recovery_window_ = recovery_window_ >= bytes_lost
                           ? recovery_window_ - bytes_lost
                           : kMaxSegmentSize;

    // In CONSERVATION mode, just subtracting losses is sufficient.  In GROWTH,
    // release additional |bytes_acked| to achieve a slow-start-like behavior.
    if (recovery_state_ == GROWTH)
    {
        recovery_window_ += bytes_acked;
    }

    // Sanity checks.  Ensure that we always allow to send at least
    // |bytes_acked| in response.
    recovery_window_ = std::max(recovery_window_, unacked_packets_->bytes_in_flight() + bytes_acked);
    /*---------------------------------------------------add by dd start------------------------------------------------------------*/
    // // Sanity checks.  Ensure that we always allow to send at least an MSS or
    // // |bytes_acked| in response, whichever is larger.
    // if (GetQuicReloadableFlag(quic_bbr_one_mss_conservation)) {
    //     recovery_window_ = std::max(recovery_window_, unacked_packets_->bytes_in_flight() + kMaxSegmentSize);
    // }
    /*---------------------------------------------------add by dd stop------------------------------------------------------------*/
    recovery_window_ = std::max(kMinimumCongestionWindow, recovery_window_);
}

std::string BbrSender::GetDebugState() const
{
    std::ostringstream stream;
    stream << ExportDebugState();
    return stream.str();
}

void BbrSender::OnApplicationLimited(ByteCount bytes_in_flight)
{
    if (bytes_in_flight >= GetCongestionWindow())
    {
        return;
    }

    sampler_->OnAppLimited();
    NS_LOG_DEBUG("Becoming application limited. Last sent packet: "
                 << last_sent_packet_ << ", CWND: " << GetCongestionWindow());
}

BbrSender::DebugState BbrSender::ExportDebugState() const
{
    return DebugState(*this);
}

static std::string ModeToString(BbrSender::Mode mode)
{
    switch (mode)
    {
    case BbrSender::STARTUP:
        return "STARTUP";
    case BbrSender::DRAIN:
        return "DRAIN";
    case BbrSender::PROBE_BW:
        return "PROBE_BW";
    case BbrSender::PROBE_RTT:
        return "PROBE_RTT";
    }
    return "???";
}

std::ostream &operator<<(std::ostream &os, const BbrSender::Mode &mode)
{
    os << ModeToString(mode);
    return os;
}

std::ostream &operator<<(std::ostream &os, const BbrSender::DebugState &state)
{
    os << "Mode: " << ModeToString(state.mode) << std::endl;
    os << "Maximum bandwidth: " << state.max_bandwidth.ToDebugValue() << std::endl;
    os << "Round trip counter: " << state.round_trip_count << std::endl;
    os << "Gain cycle index: " << static_cast<int>(state.gain_cycle_index)
       << std::endl;
    os << "Congestion window: " << state.congestion_window << " bytes"
       << std::endl;

    if (state.mode == BbrSender::STARTUP)
    {
        os << "(startup) Bandwidth at last round: "
           << state.bandwidth_at_last_round.ToDebugValue()
           << std::endl;
        os << "(startup) Rounds without gain: "
           << state.rounds_without_bandwidth_gain << std::endl;
    }

    os << "Minimum RTT: " << state.min_rtt << std::endl;
    os << "Minimum RTT timestamp: " << state.min_rtt_timestamp << " us"
       << std::endl;

    os << "Last sample is app-limited: "
       << (state.last_sample_is_app_limited ? "yes" : "no");

    return os;
}
}
}