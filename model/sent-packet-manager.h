/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef SENT_PACKET_MANAGER_H
#define SENT_PACKET_MANAGER_H
#include <memory>

#include "video-common.h"
#include "bandwidth.h"
#include "rtt-stats.h"
#include "connection-stats.h"
#include "unacked-packet-map.h"
#include "send-algorithm-interface.h"
#include "general-loss-algorithm.h"
#include "pacing-sender.h"
#include "packet-header.h"
#include "ack-frame.h"
#include "connection-stats.h"
#include "linked-hash-map.h"

namespace ns3
{
namespace bbr
{
class SentPacketManager
{
public:
  SentPacketManager(ConnectionStats *stats, CongestionControlType congestion_control_type, LossDetectionType loss_type);
                                      
  virtual ~SentPacketManager();

  void SetMaxPacingRate(Bandwidth max_pacing_rate);

  // Processes the incoming ack.
  void OnIncomingAck(const AckFrame &ack_frame, uint64_t receive_time);

  // Retransmits the oldest pending packet there is still a tail loss probe
  // pending.  Invoked after OnRetransmissionTimeout.
  bool MaybeRetransmitTailLossProbe();

  // Returns true if there are pending retransmissions.
  // Not const because retransmissions may be cancelled before returning.
  bool HasPendingRetransmissions() const;

  // Retrieves the next pending retransmission.  You must ensure that
  // there are pending retransmissions prior to calling this function.
  PacketHeader NextPendingRetransmission();

  bool HasUnackedPackets() const;

  // Returns the smallest packet number of a serialized packet which has not
  // been acked by the peer.
  PacketNumber GetLeastUnacked() const;

  // Called when we have sent bytes to the peer.  This informs the manager both
  // the number of bytes sent and if they were retransmitted.  Returns true if
  // the sender should reset the retransmission timer.
  bool OnPacketSent(PacketHeader& header,
                    PacketNumber original_packet_number,
                    uint64_t sent_time,
                    TransmissionType transmission_type,
                    HasRetransmittableData has_retransmittable_data);

  // Called when the retransmission timer expires.
  void OnRetransmissionTimeout();

  // Calculate the time until we can send the next packet to the wire.
  // Note 1: When kUnknownWaitTime is returned, there is no need to poll
  // TimeUntilSend again until we receive an OnIncomingAckFrame event.
  // Note 2: Send algorithms may or may not use |retransmit| in their
  // calculations.
  uint64_t TimeUntilSend(uint64_t now);

  // Returns the current delay for the retransmission timer, which may send
  // either a tail loss probe or do a full RTO.  Returns Time::Zero() if
  // there are no retransmittable packets.
  const uint64_t GetRetransmissionTime() const;

  const RttStats *GetRttStats() const;

  // Returns the estimated bandwidth calculated by the congestion algorithm.
  Bandwidth BandwidthEstimate() const;

  // Returns the number of bytes that are considered in-flight, i.e. not lost or acknowledged.
  ByteCount GetBytesInFlight() const;

  PacketNumber GetLargestObserved() const;

  PacketNumber GetLargestSentPacket() const;

  PacketNumber GetLeastPacketAwaitedByPeer() const;

  bool InSlowStart() const;

  size_t GetConsecutiveRtoCount() const;

  size_t GetConsecutiveTlpCount() const;

  void OnApplicationLimited();

  const SendAlgorithmInterface *GetSendAlgorithm() const;

private:

  // The retransmission timer is a single timer which switches modes depending
  // upon connection state.
  enum RetransmissionTimeoutMode {
    // A conventional TCP style RTO.
    RTO_MODE,
    // A tail loss probe.  By default, QUIC sends up to two before RTOing.
    TLP_MODE,
    // Re-invoke the loss detection when a packet is not acked before the
    // loss detection algorithm expects.
    LOSS_MODE,
  };

  typedef linked_hash_map<PacketNumber, TransmissionType> PendingRetransmissionMap;

  // Updates the least_packet_awaited_by_peer.
  void UpdatePacketInformationReceivedByPeer(const AckFrame& ack_frame);

  // Process the incoming ack looking for newly ack'd data packets.
  void HandleAckForSentPackets(const AckFrame &ack_frame);

  // Returns the current retransmission mode.
  RetransmissionTimeoutMode GetRetransmissionMode() const;

  // Retransmits two packets for an RTO and removes any non-retransmittable
  // packets from flight.
  void RetransmitRtoPackets();

  // Returns the timer for a new tail loss probe.
  const uint64_t GetTailLossProbeDelay() const;

  // Returns the retransmission timeout, after which a full RTO occurs.
  const uint64_t GetRetransmissionDelay() const;

  // Returns the newest transmission associated with a packet.
  PacketNumber GetNewestRetransmission(PacketNumber packet_number, const TransmissionInfo &transmission_info) const;
                                      
  // Update the RTT if the ack is for the largest acked packet number.
  // Returns true if the rtt was updated.
  bool MaybeUpdateRTT(const AckFrame &ack_frame, uint64_t ack_receive_time);

  // Invokes the loss detection algorithm and loses and retransmits packets if
  // necessary.
  void InvokeLossDetection(uint64_t time);

  // Invokes OnCongestionEvent if |rtt_updated| is true, there are pending acks,
  // or pending losses.  Clears pending acks and pending losses afterwards.
  // |prior_in_flight| is the number of bytes in flight before the losses or
  // acks, |event_time| is normally the timestamp of the ack packet which caused
  // the event, although it can be the time at which loss detection was
  // triggered.
  void MaybeInvokeCongestionEvent(bool rtt_updated, ByteCount prior_in_flight, uint64_t event_time);                               

  // Removes the retransmittability and in flight properties from the packet at
  // |info| due to receipt by the peer.
  void MarkPacketHandled(PacketNumber packet_number, TransmissionInfo *info, uint64_t ack_delay_time);
                        
  // Request that |packet_number| be retransmitted after the other pending
  // retransmissions.  Does not add it to the retransmissions if it's already
  // a pending retransmission.
  void MarkForRetransmission(PacketNumber packet_number, TransmissionType transmission_type);                           

  // Notify observers that packet with TransmissionInfo |info| is a spurious
  // retransmission. It is caller's responsibility to guarantee the packet with
  // TransmissionInfo |info| is a spurious retransmission before calling
  // this function.
  void RecordOneSpuriousRetransmission(const TransmissionInfo& info);

  // Notify observers about spurious retransmits of packet with
  // TransmissionInfo |info|.
  void RecordSpuriousRetransmissions(const TransmissionInfo& info, PacketNumber acked_packet_number);                                   

  // Sets the send algorithm to the given congestion control type and points the
  // pacing sender at |send_algorithm_|. Can be called any number of times.
  void SetSendAlgorithm(CongestionControlType congestion_control_type);

  // Sets the send algorithm to |send_algorithm| and points the pacing sender at
  // |send_algorithm_|. Takes ownership of |send_algorithm|. Can be called any
  // number of times.
  void SetSendAlgorithm(SendAlgorithmInterface *send_algorithm);

private:
  // Newly serialized retransmittable packets are added to this map, which
  // contains owning pointers to any contained frames.  If a packet is
  // retransmitted, this map will contain entries for both the old and the new
  // packet. The old packet's retransmittable frames entry will be nullptr,
  // while the new packet's entry will contain the frames to retransmit.
  // If the old packet is acked before the new packet, then the old entry will
  // be removed from the map and the new entry's retransmittable frames will be
  // set to nullptr.
  UnackedPacketMap unacked_packets_;

  // Pending retransmissions which have not been packetized and sent yet.
  PendingRetransmissionMap pending_retransmissions_;

  ConnectionStats* stats_;

  const PacketCount initial_congestion_window_;

  RttStats rtt_stats_;

  std::unique_ptr<SendAlgorithmInterface> send_algorithm_;

  GeneralLossAlgorithm general_loss_algorithm_;
  LossDetectionInterface* loss_algorithm_;

  // Least packet number which the peer is still waiting for.
  PacketNumber least_packet_awaited_by_peer_;

  // Tracks the first RTO packet.  If any packet before that packet gets acked,
  // it indicates the RTO was spurious and should be reversed(F-RTO).
  PacketNumber first_rto_transmission_;
  // Number of times the RTO timer has fired in a row without receiving an ack.
  size_t consecutive_rto_count_;
  // Number of times the tail loss probe has been sent.
  size_t consecutive_tlp_count_;
  // Number of pending transmissions of TLP, RTO packets.
  size_t pending_timer_transmission_count_;
  // Maximum number of tail loss probes to send before firing an RTO.
  size_t max_tail_loss_probes_;
  // If true, send the TLP at 0.5 RTT.
  bool enable_half_rtt_tail_loss_probe_;
  bool using_pacing_;
  // If true, use the new RTO with loss based CWND reduction instead of the send
  // algorithms's OnRetransmissionTimeout to reduce the congestion window.
  bool use_new_rto_;
  // If true, cancel pending retransmissions if they're larger than
  // largest_newly_acked.
  bool undo_pending_retransmits_;

  // Vectors packets acked and lost as a result of the last congestion event.
  SendAlgorithmInterface::CongestionVector packets_acked_;
  SendAlgorithmInterface::CongestionVector packets_lost_;

  // Largest newly acknowledged packet.
  PacketNumber largest_newly_acked_;

  // Replaces certain calls to |send_algorithm_| when |using_pacing_| is true.
  // Calls into |send_algorithm_| for the underlying congestion control.
  PacingSender pacing_sender_;

  // The largest acked value that was sent in an ack, which has then been acked.
  PacketNumber largest_packet_peer_knows_is_acked_;

};
}
}

#endif