/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef SEND_ALGORITHM_INTERFACE_H
#define SEND_ALGORITHM_INTERFACE_H

#include <algorithm>
#include <map>

#include "bbr-common.h"
#include "bandwidth.h"
#include "unacked-packet-map.h"
#include "connection-stats.h"

namespace ns3
{
namespace bbr
{
class RttStats;

const PacketCount kDefaultMaxCongestionWindowPackets = 2000;

class SendAlgorithmInterface
{
  public:
    // A sorted vector of packets.
    typedef std::vector<std::pair<PacketNumber, PacketLength>> CongestionVector;

    static SendAlgorithmInterface *Create(
        const RttStats *rtt_stats,
        const UnackedPacketMap *unacked_packets,
        CongestionControlType congestion_control_type,
        ConnectionStats *stats,
        PacketCount initial_congestion_window);

    virtual ~SendAlgorithmInterface() {}

    // Indicates an update to the congestion state, caused either by an incoming
    // ack or loss event timeout.  |rtt_updated| indicates whether a new
    // latest_rtt sample has been taken, |prior_in_flight| the bytes in flight
    // prior to the congestion event.  |acked_packets| and |lost_packets| are any
    // packets considered acked or lost as a result of the congestion event.
    virtual void OnCongestionEvent(bool rtt_updated,
                                   ByteCount prior_in_flight,
                                   uint64_t event_time,
                                   const CongestionVector &acked_packets,
                                   const CongestionVector &lost_packets) = 0;

    // Inform that we sent |bytes| to the wire, and if the packet is
    // retransmittable. Returns true if the packet should be tracked by the
    // congestion manager and included in bytes_in_flight, false otherwise.
    // |bytes_in_flight| is the number of bytes in flight before the packet was
    // sent.
    // Note: this function must be called for every packet sent to the wire.
    virtual bool OnPacketSent(uint64_t sent_time,
                              ByteCount bytes_in_flight,
                              PacketNumber packet_number,
                              ByteCount bytes,
                              HasRetransmittableData is_retransmittable) = 0;

    // Called when the retransmission timeout fires.  Neither OnPacketAbandoned
    // nor OnPacketLost will be called for these packets.
    virtual void OnRetransmissionTimeout(bool packets_retransmitted) = 0;

    // Called when connection migrates and cwnd needs to be reset.
    virtual void OnConnectionMigration() = 0;

    // Calculate the time until we can send the next packet.
    virtual uint64_t TimeUntilSend(uint64_t now, ByteCount bytes_in_flight) = 0;

    // The pacing rate of the send algorithm.  May be zero if the rate is unknown.
    virtual Bandwidth PacingRate(ByteCount bytes_in_flight) const = 0;

    // What's the current estimated bandwidth in bytes per second.
    // Returns 0 when it does not have an estimate.
    virtual Bandwidth BandwidthEstimate() const = 0;

    // Returns the size of the current congestion window in bytes.  Note, this is
    // not the *available* window.  Some send algorithms may not use a congestion
    // window and will return 0.
    virtual ByteCount GetCongestionWindow() const = 0;

    // Whether the send algorithm is currently in slow start.  When true, the
    // BandwidthEstimate is expected to be too low.
    virtual bool InSlowStart() const = 0;

    // Whether the send algorithm is currently in recovery.
    virtual bool InRecovery() const = 0;

    // Returns the size of the slow start congestion window in bytes,
    // aka ssthresh.  Some send algorithms do not define a slow start
    // threshold and will return 0.
    virtual ByteCount GetSlowStartThreshold() const = 0;

    virtual CongestionControlType GetCongestionControlType() const = 0;

    // Notifies the congestion control algorithm of an external network
    // measurement or prediction.  Either |bandwidth| or |rtt| may be zero if no
    // sample is available.
    virtual void AdjustNetworkParameters(Bandwidth bandwidth, uint64_t rtt) = 0;

    // Retrieves debugging information about the current state of the
    // send algorithm.
    virtual std::string GetDebugState() const = 0;

    // Called when the connection has no outstanding data to send. Specifically,
    // this means that none of the data streams are write-blocked, there are no
    // packets in the connection queue, and there are no pending retransmissins,
    // i.e. the sender cannot send anything for reasons other than being blocked
    // by congestion controller. This includes cases when the connection is
    // blocked by the flow controller.
    //
    // The fact that this method is called does not necessarily imply that the
    // connection would not be blocked by the congestion control if it actually
    // tried to send data. If the congestion control algorithm needs to exclude
    // such cases, it should use the internal state it uses for congestion control
    // for that.
    virtual void OnApplicationLimited(ByteCount bytes_in_flight) = 0;

    // True when the congestion control is probing for more bandwidth and needs
    // enough data to not be app-limited to do so.
    virtual bool IsProbingForMoreBandwidth() const = 0;

    // Sets the initial congestion window in number of packets.  May be ignored
    // if called after the initial congestion window is no longer relevant.
    virtual void SetInitialCongestionWindowInPackets(PacketCount packets) = 0;
};
}
}

#endif