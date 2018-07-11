/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include <algorithm>
#include "ns3/core-module.h"
#include "ack-frame.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("AckFrame");
namespace bbr
{
PacketNumberQueue::const_iterator::const_iterator(const const_iterator &other) =
    default;
PacketNumberQueue::const_iterator::const_iterator(const_iterator &&other) =
    default;
PacketNumberQueue::const_iterator::~const_iterator() {}

PacketNumberQueue::const_reverse_iterator::const_reverse_iterator(
    const const_reverse_iterator &other) = default;
PacketNumberQueue::const_reverse_iterator::const_reverse_iterator(
    const_reverse_iterator &&other) = default;
PacketNumberQueue::const_reverse_iterator::~const_reverse_iterator() {}

PacketNumberQueue::const_iterator::const_iterator(
    typename std::deque<SeqInterval>::const_iterator it)
    : deque_it_(it) {}

PacketNumberQueue::const_reverse_iterator::const_reverse_iterator(
    const typename std::deque<SeqInterval>::const_reverse_iterator &it)
    : deque_it_(it) {}

bool IsAwaitingPacket(const AckFrame &ack_frame,
                      PacketNumber packet_number,
                      PacketNumber peer_least_packet_awaiting_ack)
{
    return SEQ_GE(packet_number, peer_least_packet_awaiting_ack) &&
           !ack_frame.packets.Contains(packet_number);
}

const PacketType AckFrame::m_type = kAckPacket;

AckFrame::AckFrame()
    : largest_observed(0), ack_delay_time(INFINITETIME), last_update_time(0) {}

AckFrame::AckFrame(const AckFrame &other) = default;

AckFrame::~AckFrame() {}

std::ostream &operator<<(std::ostream &os, const AckFrame &ack_frame)
{
    os << "{ largest_observed: " << ack_frame.largest_observed
       << ", ack_delay_time: " << ack_frame.ack_delay_time
       << ", last_update_time: " << ack_frame.last_update_time
       << ", packets: [ " << ack_frame.packets << " ]"
       << ", received_packets: [ ";
    for (const std::pair<PacketNumber, uint64_t> &p :
         ack_frame.received_packet_times)
    {
        os << p.first << " at " << p.second << " ";
    }
    os << " ] }";
    return os;
}
PacketNumberQueue::PacketNumberQueue() {}

PacketNumberQueue::PacketNumberQueue(const PacketNumberQueue &other) = default;
PacketNumberQueue::PacketNumberQueue(PacketNumberQueue &&other) = default;
PacketNumberQueue::~PacketNumberQueue() {}

PacketNumberQueue &PacketNumberQueue::operator=(
    const PacketNumberQueue &other) = default;
PacketNumberQueue &PacketNumberQueue::operator=(PacketNumberQueue &&other) =
    default;

void PacketNumberQueue::Add(PacketNumber packet_number)
{
    // Check if the deque is empty
    if (packet_number_deque_.empty())
    {
        packet_number_deque_.push_front(
            SeqInterval(packet_number, packet_number + 1));
        return;
    }

    // Check for the typical case,
    // when the next packet in order is acked
    if ((packet_number_deque_.back()).max() == packet_number)
    {
        (packet_number_deque_.back()).SetMax(packet_number + 1);
        return;
    }
    // Check if the next packet in order is skipped
    if (SEQ_LT((packet_number_deque_.back()).max(), packet_number))
    {
        packet_number_deque_.push_back(
            SeqInterval(packet_number, packet_number + 1));
        return;
    }
    // Check if the packet can be  popped on the front
    if (SEQ_GT((packet_number_deque_.front()).min(), packet_number + 1))
    {
        packet_number_deque_.push_front(
            SeqInterval(packet_number, packet_number + 1));
        return;
    }
    if ((packet_number_deque_.front()).min() == packet_number + 1)
    {
        (packet_number_deque_.front()).SetMin(packet_number);
        return;
    }

    int i = packet_number_deque_.size() - 1;
    // Iterating through the queue backwards
    // to find a proper place for the packet
    while (i >= 0)
    {
        // Check if the packet is contained in an interval already
        if (SEQ_GT(packet_number_deque_[i].max(), packet_number) &&
            SEQ_LE(packet_number_deque_[i].min(), packet_number))
        {
            return;
        }

        // Check if the packet can extend an interval
        // and merges two intervals if needed
        if (packet_number_deque_[i].max() == packet_number)
        {
            packet_number_deque_[i].SetMax(packet_number + 1);
            if (static_cast<size_t>(i) < packet_number_deque_.size() - 1 &&
                packet_number_deque_[i].max() ==
                    packet_number_deque_[i + 1].min())
            {
                packet_number_deque_[i].SetMax(packet_number_deque_[i + 1].max());
                packet_number_deque_.erase(packet_number_deque_.begin() + i + 1);
            }
            return;
        }
        if (packet_number_deque_[i].min() == packet_number + 1)
        {
            packet_number_deque_[i].SetMin(packet_number);
            if (i > 0 && packet_number_deque_[i].min() == packet_number_deque_[i - 1].max())
            {
                packet_number_deque_[i - 1].SetMax(packet_number_deque_[i].max());
                packet_number_deque_.erase(packet_number_deque_.begin() + i);
            }
            return;
        }

        // Check if we need to make a new interval for the packet
        if (SEQ_LT(packet_number_deque_[i].max(), packet_number + 1))
        {
            packet_number_deque_.insert(
                packet_number_deque_.begin() + i + 1,
                SeqInterval(packet_number, packet_number + 1));
            return;
        }
        i--;
    }
}

void PacketNumberQueue::Add(PacketNumber lower, PacketNumber higher)
{
    if (SEQ_GE(lower, higher))
    {
        return;
    }

    if (packet_number_deque_.empty())
    {
        packet_number_deque_.push_front(
            SeqInterval(lower, higher));
    }
    else if ((packet_number_deque_.back()).max() == lower)
    {
        // Check for the typical case,
        // when the next packet in order is acked
        (packet_number_deque_.back()).SetMax(higher);
    }
    else if (SEQ_LT((packet_number_deque_.back()).max(), lower))
    {
        // Check if the next packet in order is skipped
        packet_number_deque_.push_back(SeqInterval(lower, higher));

        // Check if the packets are being added in reverse order
    }
    else if ((packet_number_deque_.front()).min() == higher)
    {
        (packet_number_deque_.front()).SetMin(lower);
    }
    else if (SEQ_GT((packet_number_deque_.front()).min(), higher))
    {
        packet_number_deque_.push_front(
            SeqInterval(lower, higher));
    }
    else
    {
        // Iterating through the interval and adding packets one by one
        for (PacketNumber i = lower; i != higher; i++)
        {
            PacketNumberQueue::Add(i);
        }
    }
}

bool PacketNumberQueue::RemoveUpTo(PacketNumber higher)
{
    if (Empty())
    {
        return false;
    }
    const PacketNumber old_min = Min();
    {
        while (!packet_number_deque_.empty())
        {
            if (SEQ_LT(packet_number_deque_.front().max(), higher))
            {
                packet_number_deque_.pop_front();
            }
            else if (SEQ_LT(packet_number_deque_.front().min(), higher) &&
                     SEQ_GE(packet_number_deque_.front().max(), higher))
            {
                packet_number_deque_.front().SetMin(higher);
                if (packet_number_deque_.front().max() ==
                    packet_number_deque_.front().min())
                {
                    packet_number_deque_.pop_front();
                }
                break;
            }
            else
            {
                break;
            }
        }
    }

    return Empty() || old_min != Min();
}

void PacketNumberQueue::RemoveSmallestInterval()
{
    {
        NS_ASSERT_MSG(packet_number_deque_.size() >= 2, (Empty() ? "No intervals to remove."
                                                                 : "Can't remove the last interval."));
        packet_number_deque_.pop_front();
    }
}

bool PacketNumberQueue::Contains(PacketNumber packet_number) const
{
    {
        if (packet_number_deque_.empty())
        {
            return false;
        }
        int low = 0;
        int high = packet_number_deque_.size() - 1;

        while (low <= high)
        {
            int mid = (low + high) / 2;
            if (SEQ_GT(packet_number_deque_[mid].min(), packet_number))
            {
                high = mid - 1;
                continue;
            }
            if (SEQ_LE(packet_number_deque_[mid].max(), packet_number))
            {
                low = mid + 1;
                continue;
            }
            NS_ASSERT(SEQ_GT(packet_number_deque_[mid].max(), packet_number));
            NS_ASSERT(SEQ_LE(packet_number_deque_[mid].min(), packet_number));
            return true;
        }
        return false;
    }
}

bool PacketNumberQueue::Empty() const
{
    return packet_number_deque_.empty();
}

PacketNumber PacketNumberQueue::Min() const
{
    NS_ASSERT(!Empty());
    return packet_number_deque_.front().min();
}

PacketNumber PacketNumberQueue::Max() const
{
    NS_ASSERT(!Empty());
    return packet_number_deque_.back().max() - 1;
}

size_t PacketNumberQueue::NumPacketsSlow() const
{
    {
        size_t n_packets = 0;
        for (size_t i = 0; i < packet_number_deque_.size(); i++)
        {
            n_packets += packet_number_deque_[i].Length();
        }
        return n_packets;
    }
}

size_t PacketNumberQueue::NumIntervals() const
{
    return packet_number_deque_.size();
}

PacketNumberQueue::const_iterator PacketNumberQueue::begin() const
{
    return PacketNumberQueue::const_iterator(packet_number_deque_.begin());
}

PacketNumberQueue::const_iterator PacketNumberQueue::end() const
{
    return const_iterator(packet_number_deque_.end());
}

PacketNumberQueue::const_reverse_iterator PacketNumberQueue::rbegin() const
{
    return const_reverse_iterator(packet_number_deque_.rbegin());
}

PacketNumberQueue::const_reverse_iterator PacketNumberQueue::rend() const
{
    return const_reverse_iterator(packet_number_deque_.rend());
}

PacketNumber PacketNumberQueue::LastIntervalLength() const
{
    NS_ASSERT(!Empty());
    return packet_number_deque_[packet_number_deque_.size() - 1].Length();
}

std::ostream &operator<<(std::ostream &os, const PacketNumberQueue &q)
{
    for (const SeqInterval &interval : q)
    {
        // for (PacketNumber packet_number = interval.min();
        //      packet_number < interval.max(); ++packet_number)
        // {
        //     os << packet_number << " ";
        // }
        os << interval.min() << "-" << interval.max() << " ";
    }
    return os;
}

// header interface
TypeId AckFrame::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::AckFrame")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<AckFrame>();
    return tid;
}

TypeId AckFrame::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

void AckFrame::Print(std::ostream &os) const
{
    os << this;
}

uint32_t AckFrame::GetSerializedSize(void) const
{
    return 1 + 8 + 2 + 1 + 2 + std::min(int(packets.NumIntervals() - 1), 255) * 4 
             + 1 + 8 + 4 * received_packet_times.size();
}

void AckFrame::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_type);
    i.WriteHtonU64(largest_observed);
    NS_ASSERT_MSG(ack_delay_time < 0xFFFF, "ack_delay_time invalid " << ack_delay_time);
    i.WriteHtonU16(ack_delay_time);
    
    NS_ASSERT_MSG(packets.NumIntervals() > 0, "empty ack blocks");
    if (packets.NumIntervals() > 256)
    {
        NS_LOG_WARN("ack blocks " << packets.NumIntervals() << " > 255");
    }
    int max_num_ack_blocks = std::min(PacketNumber(255), packets.NumIntervals() - 1);
    i.WriteU8(max_num_ack_blocks);
    // First ack block
    i.WriteHtonU16(packets.LastIntervalLength());
    // Remaining ack blocks
    auto iter0 = packets.rbegin();
    int num_ack_blocks_written = 0;
    uint16_t delta_seq;
    for (; iter0 != packets.rend() && num_ack_blocks_written < max_num_ack_blocks; ++iter0)
    {
        delta_seq = largest_observed - iter0->min();
        i.WriteHtonU16(delta_seq);
        i.WriteHtonU16(iter0->Length());
        num_ack_blocks_written++;
    }

    // Append Timestamps
    NS_ASSERT(received_packet_times.size() <= std::numeric_limits<uint8_t>::max());

    uint8_t num_received_packets = received_packet_times.size();

    i.WriteU8(num_received_packets);
    if (num_received_packets == 0)
    {
        return;
    }
    i.WriteHtonU64(last_update_time);

    PacketTimeVector::const_iterator iter = received_packet_times.begin();
    PacketNumber packet_number;
    uint16_t delta_from_largest_observed;
    uint16_t delta_time;

    for (; iter != received_packet_times.end(); ++iter)
    {
        packet_number = iter->first;
        delta_from_largest_observed = largest_observed - packet_number;
        i.WriteHtonU16(delta_from_largest_observed);
        delta_time = last_update_time - iter->second;
        i.WriteHtonU16(delta_time);
    }
}

uint32_t AckFrame::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    NS_ASSERT(type == kAckPacket);
    largest_observed = i.ReadNtohU64();
    ack_delay_time = i.ReadNtohU16();
    uint8_t num_ack_blocks = i.ReadU8();
    uint16_t first_block_length = i.ReadNtohU16();
    PacketNumber first_received =
        largest_observed + 1 - first_block_length;
    packets.Add(first_received, largest_observed + 1);

    for (size_t k = 0; k < num_ack_blocks; ++k)
    {
        uint16_t delta_seq = i.ReadNtohU16();
        uint16_t current_block_length = i.ReadNtohU16();
        first_received = largest_observed - delta_seq;
        if (current_block_length > 0)
        {
            packets.Add(first_received, first_received + current_block_length);
        }
    }

    //read timestamps
    uint8_t num_received_packets = i.ReadU8();
    last_update_time = i.ReadNtohU64();
    PacketNumber seq_num;
    while (num_received_packets > 0)
    {
        uint16_t delta_seq = i.ReadNtohU16();
        seq_num = largest_observed - delta_seq;
        uint64_t delta_time = i.ReadNtohU16();
        received_packet_times.push_back(
            std::make_pair(seq_num, delta_time));
        num_received_packets--;
    }

    return GetSerializedSize();
}
}
}