/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include <algorithm>

#include "ns3/core-module.h"
#include "received-packet-manager.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("ReceivedPacketManager");
namespace bbr
{
// The maximum number of packets to ack immediately after a missing packet for
// fast retransmission to kick in at the sender.  This limit is created to
// reduce the number of acks sent that have no benefit for fast retransmission.
// Set to the number of nacks needed for fast retransmit plus one for protection
// against an ack loss
const size_t kMaxPacketsAfterNewMissing = 4;

ReceivedPacketManager::ReceivedPacketManager()
    : peer_least_packet_awaiting_ack_(0),
      ack_frame_updated_(false),
      max_ack_ranges_(0),
      time_largest_observed_(0)
{
    ack_frame_.largest_observed = 0;
}

ReceivedPacketManager::~ReceivedPacketManager() {}

void ReceivedPacketManager::RecordPacketReceived(
    const PacketHeader &header,
    uint64_t receipt_time)
{
    PacketNumber packet_number = header.m_packet_seq;
    if (!IsAwaitingPacket(packet_number))
    {
        NS_LOG_WARN("discard old packet " << packet_number);
        return;
    }
    if (!ack_frame_updated_)
    {
        ack_frame_.received_packet_times.clear();
    }
    ack_frame_updated_ = true;
    ack_frame_.packets.Add(header.m_packet_seq);

    if (SEQ_GT(packet_number, ack_frame_.largest_observed))
    {
        ack_frame_.largest_observed = packet_number;
        time_largest_observed_ = receipt_time;
    }

    ack_frame_.received_packet_times.push_back(std::make_pair(packet_number, receipt_time));
}

bool ReceivedPacketManager::IsMissing(PacketNumber packet_number)
{
    return SEQ_LT(packet_number, ack_frame_.largest_observed) &&
           !ack_frame_.packets.Contains(packet_number);
}

bool ReceivedPacketManager::IsAwaitingPacket(
    PacketNumber packet_number)
{
    return bbr::IsAwaitingPacket(ack_frame_, packet_number, peer_least_packet_awaiting_ack_);
}

const AckFrame* ReceivedPacketManager::GetUpdatedAckFrame(uint64_t approximate_now)
{
    ack_frame_updated_ = false;
    ack_frame_.last_update_time = approximate_now;
    if (time_largest_observed_ == 0)
    {
        // We have received no packets.
        ack_frame_.ack_delay_time = INFINITETIME;
    }
    else
    {
        // Ensure the delta is zero if approximate now is "in the past".
        ack_frame_.ack_delay_time = approximate_now < time_largest_observed_
                                        ? 0
                                        : approximate_now - time_largest_observed_;
    }
    while (max_ack_ranges_ > 0 && ack_frame_.packets.NumIntervals() > max_ack_ranges_)
    {
        ack_frame_.packets.RemoveSmallestInterval();
    }

    // Clear all packet times if any are too far from largest observed.
    for (PacketTimeVector::iterator it = ack_frame_.received_packet_times.begin();
         it != ack_frame_.received_packet_times.end();)
    {
        if (ack_frame_.largest_observed - it->first >= std::numeric_limits<uint8_t>::max())
        {
            it = ack_frame_.received_packet_times.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return &ack_frame_;
}

void ReceivedPacketManager::DontWaitForPacketsBefore(PacketNumber least_unacked)
{
    // ValidateAck() should fail if peer_least_packet_awaiting_ack shrinks.
    NS_ASSERT(SEQ_LE(peer_least_packet_awaiting_ack_, least_unacked));

    if (SEQ_GT(least_unacked, peer_least_packet_awaiting_ack_))
    {
        peer_least_packet_awaiting_ack_ = least_unacked;
        bool packets_updated = ack_frame_.packets.RemoveUpTo(least_unacked);
        if (packets_updated)
        {
            // Ack frame gets updated because packets set is updated because of stop waiting frame.
            ack_frame_updated_ = true;
        }
    }
    NS_ASSERT(ack_frame_.packets.Empty() || SEQ_GE(ack_frame_.packets.Min(), peer_least_packet_awaiting_ack_));
}

bool ReceivedPacketManager::HasMissingPackets() const
{
    return ack_frame_.packets.NumIntervals() > 1 ||
           (!ack_frame_.packets.Empty() &&
            ack_frame_.packets.Min() > std::max(PacketNumber(1), peer_least_packet_awaiting_ack_));
}

bool ReceivedPacketManager::HasNewMissingPackets() const
{
    return HasMissingPackets() && ack_frame_.packets.LastIntervalLength() <= kMaxPacketsAfterNewMissing;
}

bool ReceivedPacketManager::ack_frame_updated() const
{
    return ack_frame_updated_;
}

PacketNumber ReceivedPacketManager::GetLargestObserved() const
{
    return ack_frame_.largest_observed;
}
}
}
