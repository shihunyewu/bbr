/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef RECEIVED_PACKET_MANAGER_H
#define RECEIVED_PACKET_MANAGER_H

#include <deque>

#include "bbr-common.h"
#include "packet-header.h"
#include "ack-frame.h"

namespace ns3
{
namespace bbr
{

class ReceivedPacketManager
{
  public:

    explicit ReceivedPacketManager();
    virtual ~ReceivedPacketManager();
    // Updates the internal state concerning which packets have been received.
    // header: the packet header.
    // timestamp: the arrival time of the packet.
    virtual void RecordPacketReceived(const PacketHeader &header, uint64_t receipt_time);

    // Checks whether |packet_number| is missing and less than largest observed.
    virtual bool IsMissing(PacketNumber packet_number);

    // Checks if we're still waiting for the packet with |packet_number|.
    virtual bool IsAwaitingPacket(PacketNumber packet_number);

    // Retrieves a frame containing a AckFrame.  The ack frame may not be
    // changed outside ReceivedPacketManager and must be serialized before
    // another packet is received, or it will change.
    const AckFrame* GetUpdatedAckFrame(uint64_t approximate_now);

    // Deletes all missing packets before least unacked. The connection won't
    // process any packets with packet number before |least_unacked| that it
    // received after this call.
    void DontWaitForPacketsBefore(PacketNumber least_unacked);

    // Returns true if there are any missing packets.
    bool HasMissingPackets() const;

    // Returns true when there are new missing packets to be reported within 3
    // packets of the largest observed.
    virtual bool HasNewMissingPackets() const;

    PacketNumber peer_least_packet_awaiting_ack()
    {
        return peer_least_packet_awaiting_ack_;
    }

    virtual bool ack_frame_updated() const;

    PacketNumber GetLargestObserved() const;

    // For logging purposes.
    const AckFrame &ack_frame() const { return ack_frame_; }

    void set_max_ack_ranges(size_t max_ack_ranges)
    {
        max_ack_ranges_ = max_ack_ranges;
    }

  private:
    // Least packet number of the the packet sent by the peer for which it
    // hasn't received an ack.
    PacketNumber peer_least_packet_awaiting_ack_;

    // Received packet information used to produce acks.
    AckFrame ack_frame_;

    // True if |ack_frame_| has been updated since UpdateReceivedPacketInfo was
    // last called.
    bool ack_frame_updated_;

    // Maximum number of ack ranges allowed to be stored in the ack frame.
    size_t max_ack_ranges_;

    // The time we received the largest_observed packet number, or zero if
    // no packet numbers have been received since UpdateReceivedPacketInfo.
    // Needed for calculating ack_delay_time.
    uint64_t time_largest_observed_;

    DISALLOW_COPY_AND_ASSIGN(ReceivedPacketManager);
};
}
}

#endif