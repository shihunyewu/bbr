/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/core-module.h"
#include "pacing-sender.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("PacingSender");
namespace bbr
{
// The estimated system alarm granularity.
static const uint64_t kAlarmGranularity = MILLISECOND(1);

// Configured maximum size of the burst coming out of quiescence.  The burst
// is never larger than the current CWND in packets.
static const uint32_t kInitialUnpacedBurst = 10;

PacingSender::PacingSender()
    : sender_(nullptr),
      max_pacing_rate_(Bandwidth::Zero()),
      burst_tokens_(kInitialUnpacedBurst),
      last_delayed_packet_sent_time_(0),
      ideal_next_packet_send_time_(0),
      was_last_send_delayed_(false) {}

PacingSender::~PacingSender() {}

void PacingSender::set_sender(SendAlgorithmInterface *sender)
{
    NS_ASSERT(sender != nullptr);
    sender_ = sender;
}

void PacingSender::OnCongestionEvent(
    bool rtt_updated,
    ByteCount bytes_in_flight,
    uint64_t event_time,
    const SendAlgorithmInterface::CongestionVector &acked_packets,
    const SendAlgorithmInterface::CongestionVector &lost_packets)
{
    NS_ASSERT(sender_ != nullptr);
    if (!lost_packets.empty())
    {
        // Clear any burst tokens when entering recovery.
        burst_tokens_ = 0;
    }
    sender_->OnCongestionEvent(rtt_updated, bytes_in_flight, event_time, acked_packets, lost_packets);
}

bool PacingSender::OnPacketSent(
    uint64_t sent_time,
    ByteCount bytes_in_flight,
    PacketNumber packet_number,
    ByteCount bytes,
    HasRetransmittableData is_retransmittable)
{
    NS_ASSERT(sender_ != nullptr);
    const bool in_flight = sender_->OnPacketSent(sent_time, bytes_in_flight, packet_number, bytes, is_retransmittable);

    // If in recovery, the connection is not coming out of quiescence.
    if (bytes_in_flight == 0 && !sender_->InRecovery())
    {
        // Add more burst tokens anytime the connection is leaving quiescence, but
        // limit it to the equivalent of a single bulk write, not exceeding the
        // current CWND in packets.
        burst_tokens_ = std::min(kInitialUnpacedBurst, static_cast<uint32_t>(sender_->GetCongestionWindow() / kDefaultTCPMSS));
    }
    if (burst_tokens_ > 0)
    {
        --burst_tokens_;
        was_last_send_delayed_ = false;
        last_delayed_packet_sent_time_ = 0;
        ideal_next_packet_send_time_ = 0;
        return in_flight;
    }
    // The next packet should be sent as soon as the current packet has been
    // transferred.  PacingRate is based on bytes in flight including this packet.
    uint64_t delay = PacingRate(bytes_in_flight + bytes).TransferTime(bytes);
    // If the last send was delayed, and the alarm took a long time to get
    // invoked, allow the connection to make up for lost time.
    if (was_last_send_delayed_)
    {
        ideal_next_packet_send_time_ = ideal_next_packet_send_time_ + delay;
        // The send was application limited if it takes longer than the
        // pacing delay between sent packets.
        const bool application_limited = last_delayed_packet_sent_time_ > 0 && sent_time > last_delayed_packet_sent_time_ + delay;
        const bool making_up_for_lost_time = ideal_next_packet_send_time_ <= sent_time;
        // As long as we're making up time and not application limited,
        // continue to consider the packets delayed, allowing the packets to be
        // sent immediately.
        if (making_up_for_lost_time && !application_limited)
        {
            last_delayed_packet_sent_time_ = sent_time;
        }
        else
        {
            was_last_send_delayed_ = false;
            last_delayed_packet_sent_time_ = 0;
        }
    }
    else
    {
        ideal_next_packet_send_time_ = std::max(ideal_next_packet_send_time_ + delay, sent_time + delay);
    }
    return in_flight;
}

uint64_t PacingSender::TimeUntilSend(uint64_t now, ByteCount bytes_in_flight)
{
    NS_ASSERT(sender_ != nullptr);
    uint64_t time_until_send = sender_->TimeUntilSend(now, bytes_in_flight);
    if (burst_tokens_ > 0 || bytes_in_flight == 0)
    {
        // Don't pace if we have burst tokens available or leaving quiescence.
        return time_until_send;
    }

    if (time_until_send != 0)
    {
        NS_ASSERT(time_until_send == INFINITETIME);
        // The underlying sender prevents sending.
        return time_until_send;
    }

    // If the next send time is within the alarm granularity, send immediately.
    if (ideal_next_packet_send_time_ > now + kAlarmGranularity)
    {
        NS_LOG_DEBUG("Delaying packet us: " << (ideal_next_packet_send_time_ - now));
        was_last_send_delayed_ = true;
        return ideal_next_packet_send_time_ - now;
    }

    NS_LOG_DEBUG("Sending packet now");
    return 0;
}

Bandwidth PacingSender::PacingRate(ByteCount bytes_in_flight) const
{
    NS_ASSERT(sender_ != nullptr);
    if (!max_pacing_rate_.IsZero())
    {
        return Bandwidth::FromBitsPerSecond(std::min(max_pacing_rate_.ToBitsPerSecond(), sender_->PacingRate(bytes_in_flight).ToBitsPerSecond()));
    }
    return sender_->PacingRate(bytes_in_flight);
}
}
}