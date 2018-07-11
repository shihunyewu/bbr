/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include "ns3/core-module.h"
#include "unacked-packet-map.h"
#include "packet-header.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("UnackedPacketMap");
namespace bbr
{
TransmissionInfo::TransmissionInfo()
    : data_packet(NULL), bytes_sent(0), sent_time(0)
    , transmission_type(NOT_RETRANSMISSION), in_flight(false)
    , is_unackable(true), retransmission(0)
{
}

TransmissionInfo::TransmissionInfo(PacketHeader &packet, TransmissionType transmission_type,
                                   bool set_in_flight)
    : data_packet(packet.m_data_packet), bytes_sent(packet.m_data_length)
    , sent_time(packet.m_sent_time), transmission_type(transmission_type)
    , in_flight(set_in_flight), is_unackable(false), retransmission(0)
{
}

TransmissionInfo::TransmissionInfo(const TransmissionInfo &other) =
    default;

TransmissionInfo::~TransmissionInfo()
{
}

UnackedPacketMap::UnackedPacketMap()
    : largest_sent_packet_(0),
      largest_sent_retransmittable_packet_(0),
      largest_observed_(0),
      least_unacked_(0),
      bytes_in_flight_(0)
{
}

void UnackedPacketMap::AddSentPacket(PacketHeader &packet,
                   PacketNumber old_packet_number,
                   TransmissionType transmission_type,
                   uint64_t sent_time,
                   bool set_in_flight)
{
    PacketNumber packet_number = packet.m_packet_seq;
    PacketLength bytes_sent = packet.m_data_length;
    NS_ASSERT(SEQ_GT(packet_number, largest_sent_packet_));
    NS_ASSERT(SEQ_GE(packet_number, least_unacked_ + unacked_packets_.size()));
    NS_ASSERT_MSG(packet_number == largest_sent_packet_ + 1, "non-consecutive packet number");

    TransmissionInfo info(packet, transmission_type, set_in_flight);

    if (unacked_packets_.size() == 0)
    {
        least_unacked_ = packet_number;
    }

    if (old_packet_number > 0)
    {
        TransferRetransmissionInfo(old_packet_number, packet_number,
                               transmission_type, &info);
    }

    largest_sent_packet_ = packet_number;
    if (set_in_flight)
    {
        bytes_in_flight_ += bytes_sent;
        info.in_flight = true;
        largest_sent_retransmittable_packet_ = packet_number;
    }

    unacked_packets_.push_back(info);
}

void UnackedPacketMap::RemoveObsoletePackets()
{
    while (!unacked_packets_.empty())
    {
        if (!IsPacketUseless(least_unacked_, unacked_packets_.front()))
        {
            break;
        }

        unacked_packets_.pop_front();
        least_unacked_++;
    }
}

void UnackedPacketMap::TransferRetransmissionInfo(PacketNumber old_packet_number,
                                                  PacketNumber new_packet_number,
                                                  TransmissionType transmission_type,
                                                  TransmissionInfo *info)
{
    if (old_packet_number < least_unacked_)
    {
        // This can happen when a retransmission packet is queued because of write
        // blocked socket, and the original packet gets acked before the
        // retransmission gets sent.
        return;
    }
    if (old_packet_number > largest_sent_packet_)
    {
        NS_LOG_INFO("Old TransmissionInfo never existed for :"
                    << old_packet_number
                    << " largest_sent:" 
                    << largest_sent_packet_);
        return;
    }
    NS_ASSERT(new_packet_number == least_unacked_ + unacked_packets_.size());
    NS_ASSERT(NOT_RETRANSMISSION != transmission_type);

    TransmissionInfo *transmission_info =
        &unacked_packets_.at(old_packet_number - least_unacked_);

    // Swap the frames.
    transmission_info->data_packet.reset();

    // Don't link old transmissions to new ones when version or
    // encryption changes.
    if (transmission_type == ALL_INITIAL_RETRANSMISSION ||
        transmission_type == ALL_UNACKED_RETRANSMISSION)
    {
        transmission_info->is_unackable = true;
    }
    else
    {
        transmission_info->retransmission = new_packet_number;
    }
    // Proactively remove obsolete packets so the least unacked can be raised.
    RemoveObsoletePackets();
}

bool UnackedPacketMap::HasRetransmittableFrames(PacketNumber packet_number) const
{
    NS_ASSERT(SEQ_GE(packet_number, least_unacked_));
    NS_ASSERT(SEQ_LT(packet_number, least_unacked_ + unacked_packets_.size()));

    return unacked_packets_[packet_number - least_unacked_].data_packet != nullptr;
}

void UnackedPacketMap::RemoveRetransmittability(TransmissionInfo *info)
{
    while (info->retransmission != 0)
    {
        const PacketNumber retransmission = info->retransmission;
        info->retransmission = 0;
        info = &unacked_packets_[retransmission - least_unacked_];
    }

    info->data_packet.reset();
}

void UnackedPacketMap::RemoveRetransmittability(PacketNumber packet_number)
{
    NS_ASSERT(SEQ_GE(packet_number, least_unacked_));
    NS_ASSERT(SEQ_LT(packet_number, least_unacked_ + unacked_packets_.size()));
    TransmissionInfo *info =
        &unacked_packets_[packet_number - least_unacked_];
    RemoveRetransmittability(info);
}

void UnackedPacketMap::IncreaseLargestObserved(PacketNumber largest_observed)
{
    NS_ASSERT(largest_observed_ <= largest_observed);
    largest_observed_ = largest_observed;
}

bool UnackedPacketMap::IsPacketUsefulForMeasuringRtt(PacketNumber packet_number,
                                                     const TransmissionInfo &info) const
{
    // Packet can be used for RTT measurement if it may yet be acked as the
    // largest observed packet by the receiver.
    return !info.is_unackable && packet_number > largest_observed_;
}

bool UnackedPacketMap::IsPacketUsefulForCongestionControl(const TransmissionInfo &info) const
{
    // Packet contributes to congestion control if it is considered inflight.
    return info.in_flight;
}

bool UnackedPacketMap::IsPacketUsefulForRetransmittableData(const TransmissionInfo& info) const
{
    return info.data_packet || info.retransmission > largest_observed_;
}

bool UnackedPacketMap::IsPacketUseless(PacketNumber packet_number,
                                       const TransmissionInfo &info) const
{
    return !IsPacketUsefulForMeasuringRtt(packet_number, info) &&
           !IsPacketUsefulForCongestionControl(info) &&
           !IsPacketUsefulForRetransmittableData(info);
}

bool UnackedPacketMap::IsUnacked(PacketNumber packet_number) const
{
    if (packet_number < least_unacked_ ||
        packet_number >= least_unacked_ + unacked_packets_.size())
    {
        return false;
    }
    return !IsPacketUseless(packet_number, unacked_packets_[packet_number - least_unacked_]);
}

void UnackedPacketMap::RemoveFromInFlight(TransmissionInfo *info)
{
    if (info->in_flight)
    {
        NS_ASSERT(bytes_in_flight_ >= info->bytes_sent);
        bytes_in_flight_ -= info->bytes_sent;
        info->in_flight = false;
    }
}

void UnackedPacketMap::RemoveFromInFlight(PacketNumber packet_number)
{
    NS_ASSERT(SEQ_GE(packet_number, least_unacked_));
    NS_ASSERT(SEQ_LT(packet_number, least_unacked_ + unacked_packets_.size()));
    TransmissionInfo *info =
        &unacked_packets_[packet_number - least_unacked_];
    RemoveFromInFlight(info);
}

void UnackedPacketMap::RestoreToInFlight(PacketNumber packet_number)
{
    NS_ASSERT(SEQ_GE(packet_number, least_unacked_));
    NS_ASSERT(SEQ_LT(packet_number, least_unacked_ + unacked_packets_.size()));
    TransmissionInfo *info = &unacked_packets_[packet_number - least_unacked_];
    NS_ASSERT(!info->is_unackable);
    bytes_in_flight_ += info->bytes_sent;
    info->in_flight = true;
}

bool UnackedPacketMap::HasUnackedPackets() const
{
    return !unacked_packets_.empty();
}

bool UnackedPacketMap::HasInFlightPackets() const
{
    return bytes_in_flight_ > 0;
}

const TransmissionInfo &UnackedPacketMap::GetTransmissionInfo(
    PacketNumber packet_number) const
{
    return unacked_packets_[packet_number - least_unacked_];
}

TransmissionInfo *UnackedPacketMap::GetMutableTransmissionInfo(
    PacketNumber packet_number)
{
    return &unacked_packets_[packet_number - least_unacked_];
}

uint64_t UnackedPacketMap::GetLastPacketSentTime() const
{
    UnackedPacketList::const_reverse_iterator it = unacked_packets_.rbegin();
    while (it != unacked_packets_.rend())
    {
        if (it->in_flight)
        {
            NS_ASSERT_MSG(it->sent_time > 0,
                          "Sent time can never be zero for a packet in flight.");
            return it->sent_time;
        }
        ++it;
    }
    NS_LOG_WARN("GetLastPacketSentTime requires in flight packets.");
    return 0;
}

size_t UnackedPacketMap::GetNumUnackedPacketsDebugOnly() const
{
    size_t unacked_packet_count = 0;
    PacketNumber packet_number = least_unacked_;
    for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
         it != unacked_packets_.end(); ++it, ++packet_number)
    {
        if (!IsPacketUseless(packet_number, *it))
        {
            ++unacked_packet_count;
        }
    }
    return unacked_packet_count;
}

bool UnackedPacketMap::HasMultipleInFlightPackets() const
{
    if (bytes_in_flight_ > kDefaultTCPMSS)
    {
        return true;
    }
    size_t num_in_flight = 0;
    for (UnackedPacketList::const_reverse_iterator it = unacked_packets_.rbegin();
         it != unacked_packets_.rend(); ++it)
    {
        if (it->in_flight)
        {
            ++num_in_flight;
        }
        if (num_in_flight > 1)
        {
            return true;
        }
    }
    return false;
}

bool UnackedPacketMap::HasUnackedRetransmittableFrames() const
{
    for (UnackedPacketList::const_reverse_iterator it = unacked_packets_.rbegin();
         it != unacked_packets_.rend(); ++it)
    {
        if (it->in_flight && it->data_packet)
        {
            return true;
        }
    }
    return false;
}
}
}