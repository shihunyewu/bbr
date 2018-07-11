/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/core-module.h"

#include "bbr-common.h"
#include "bandwidth-sampler.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("BandwidthSampler");
namespace bbr
{
BandwidthSampler::BandwidthSampler()
    : total_bytes_sent_(0),
      total_bytes_acked_(0),
      total_bytes_sent_at_last_acked_packet_(0),
      last_acked_packet_sent_time_(0),
      last_acked_packet_ack_time_(0),
      last_sent_packet_(0),
      is_app_limited_(false),
      end_of_app_limited_phase_(0),
      connection_state_map_() {}

BandwidthSampler::~BandwidthSampler() {}

void BandwidthSampler::OnPacketSent(
    uint64_t sent_time,
    PacketNumber packet_number,
    ByteCount bytes,
    ByteCount bytes_in_flight,
    HasRetransmittableData is_retransmittable)
{
    last_sent_packet_ = packet_number;

    total_bytes_sent_ += bytes;

    // If there are no packets in flight, the time at which the new transmission
    // opens can be treated as the A_0 point for the purpose of bandwidth
    // sampling. This underestimates bandwidth to some extent, and produces some
    // artificially low samples for most packets in flight, but it provides with
    // samples at important points where we would not have them otherwise, most
    // importantly at the beginning of the connection.
    if (bytes_in_flight == 0)
    {
        last_acked_packet_ack_time_ = sent_time;
        total_bytes_sent_at_last_acked_packet_ = total_bytes_sent_;

        // In this situation ack compression is not a concern, set send rate to
        // effectively infinite.
        last_acked_packet_sent_time_ = sent_time;
    }

    if (!connection_state_map_.IsEmpty() && packet_number > connection_state_map_.last_packet() + kMaxTrackedPackets)
    {
        NS_LOG_DEBUG("BandwidthSampler in-flight packet map has exceeded maximum "
                     "number "
                     "of tracked packets.");
    }

    bool success = connection_state_map_.Emplace(packet_number, sent_time, bytes, *this);       
    NS_ASSERT_MSG(success, "BandwidthSampler failed to insert the packet "
                           "into the map, most likely because it's already "
                           "in it.");
}

BandwidthSample BandwidthSampler::OnPacketAcknowledged(uint64_t ack_time, PacketNumber packet_number) 
{
    ConnectionStateOnSentPacket *sent_packet_pointer = connection_state_map_.GetEntry(packet_number);      
    if (sent_packet_pointer == nullptr)
    {
        // See the TODO below.
        return BandwidthSample();
    }
    BandwidthSample sample = OnPacketAcknowledgedInner(ack_time, packet_number, *sent_packet_pointer);
    connection_state_map_.Remove(packet_number);
    return sample;
}

BandwidthSample BandwidthSampler::OnPacketAcknowledgedInner(
    uint64_t ack_time,
    PacketNumber packet_number,
    const ConnectionStateOnSentPacket &sent_packet)
{
    total_bytes_acked_ += sent_packet.size;
    total_bytes_sent_at_last_acked_packet_ = sent_packet.total_bytes_sent;
    last_acked_packet_sent_time_ = sent_packet.sent_time;
    last_acked_packet_ack_time_ = ack_time;

    // Exit app-limited phase once a packet that was sent while the connection is
    // not app-limited is acknowledged.
    if (is_app_limited_ && packet_number > end_of_app_limited_phase_)
    {
        is_app_limited_ = false;
    }

    // There might have been no packets acknowledged at the moment when the
    // current packet was sent. In that case, there is no bandwidth sample to
    // make.
    if (sent_packet.last_acked_packet_sent_time == 0)
    {
        return BandwidthSample();
    }

    // Infinite rate indicates that the sampler is supposed to discard the
    // current send rate sample and use only the ack rate.
    Bandwidth send_rate = Bandwidth::Infinite();
    if (sent_packet.sent_time > sent_packet.last_acked_packet_sent_time)
    {
        send_rate = Bandwidth::FromBytesAndTimeDelta(
            sent_packet.total_bytes_sent - sent_packet.total_bytes_sent_at_last_acked_packet,
            sent_packet.sent_time - sent_packet.last_acked_packet_sent_time);
    }

    // During the slope calculation, ensure that ack time of the current packet is
    // always larger than the time of the previous packet, otherwise division by
    // zero or integer underflow can occur.
    if (ack_time <= sent_packet.last_acked_packet_ack_time)
    {
        NS_LOG_DEBUG("Time of the previously acked packet is larger than the time of the current packet.");                     
        return BandwidthSample();
    }
    Bandwidth ack_rate = Bandwidth::FromBytesAndTimeDelta(
        total_bytes_acked_ - sent_packet.total_bytes_acked_at_the_last_acked_packet,           
        ack_time - sent_packet.last_acked_packet_ack_time);

    BandwidthSample sample;
    sample.bandwidth = std::min(send_rate, ack_rate);
    // Note: this sample does not account for delayed acknowledgement time.  This
    // means that the RTT measurements here can be artificially high, especially
    // on low bandwidth connections.
    sample.rtt = ack_time - sent_packet.sent_time;
    // A sample is app-limited if the packet was sent during the app-limited
    // phase.
    sample.is_app_limited = sent_packet.is_app_limited;
    return sample;
}

void BandwidthSampler::OnPacketLost(PacketNumber packet_number)
{
    // TODO(vasilvv): see the comment for the case of missing packets in
    // BandwidthSampler::OnPacketAcknowledged on why this does not raise a
    // QUIC_BUG when removal fails.
    connection_state_map_.Remove(packet_number);
}

void BandwidthSampler::OnAppLimited()
{
    is_app_limited_ = true;
    end_of_app_limited_phase_ = last_sent_packet_;
}

void BandwidthSampler::RemoveObsoletePackets(PacketNumber least_unacked)
{
    while (!connection_state_map_.IsEmpty() && connection_state_map_.first_packet() < least_unacked)          
    {
        connection_state_map_.Remove(connection_state_map_.first_packet());
    }
}

ByteCount BandwidthSampler::total_bytes_acked() const
{
    return total_bytes_acked_;
}

bool BandwidthSampler::is_app_limited() const
{
    return is_app_limited_;
}

PacketNumber BandwidthSampler::end_of_app_limited_phase() const
{
    return end_of_app_limited_phase_;
}
}
}