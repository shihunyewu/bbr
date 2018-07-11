/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/core-module.h"
#include "general-loss-algorithm.h"
#include "rtt-stats.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("GeneralLossAlgorithm");
namespace bbr
{
// The minimum delay before a packet will be considered lost,
// regardless of SRTT.  Half of the minimum TLP, since the loss algorithm only
// triggers when a nack has been receieved for the packet.
static const size_t kMinLossDelayMs = 5;

// Default fraction of an RTT the algorithm waits before determining a packet is
// lost due to early retransmission by time based loss detection.
static const int kDefaultLossDelayShift = 2;
// Default fraction of an RTT when doing adaptive loss detection.
static const int kDefaultAdaptiveLossDelayShift = 4;

GeneralLossAlgorithm::GeneralLossAlgorithm()
    : loss_detection_timeout_(0),
      largest_sent_on_spurious_retransmit_(0),
      loss_type_(kNack),
      reordering_shift_(kDefaultLossDelayShift),
      largest_previously_acked_(0) {}

GeneralLossAlgorithm::GeneralLossAlgorithm(LossDetectionType loss_type)
    : loss_detection_timeout_(0),
      largest_sent_on_spurious_retransmit_(0),
      loss_type_(loss_type),
      reordering_shift_(loss_type == kAdaptiveTime
                            ? kDefaultAdaptiveLossDelayShift
                            : kDefaultLossDelayShift),
      largest_previously_acked_(0) {}

LossDetectionType GeneralLossAlgorithm::GetLossDetectionType() const
{
    return loss_type_;
}

void GeneralLossAlgorithm::SetLossDetectionType(LossDetectionType loss_type)
{
    loss_detection_timeout_ = 0;
    largest_sent_on_spurious_retransmit_ = 0;
    loss_type_ = loss_type;
    reordering_shift_ = loss_type == kAdaptiveTime
                            ? kDefaultAdaptiveLossDelayShift
                            : kDefaultLossDelayShift;
    largest_previously_acked_ = 0;
}

// Uses nack counts to decide when packets are lost.
void GeneralLossAlgorithm::DetectLosses(
    const UnackedPacketMap &unacked_packets,
    uint64_t time,
    const RttStats &rtt_stats,
    PacketNumber largest_newly_acked,
    SendAlgorithmInterface::CongestionVector *packets_lost)
{
    loss_detection_timeout_ = 0;
    uint64_t max_rtt = std::max(rtt_stats.previous_srtt(), rtt_stats.latest_rtt());
    uint64_t loss_delay = std::max(kMinLossDelayMs, max_rtt + (max_rtt >> reordering_shift_));

    PacketNumber packet_number = unacked_packets.GetLeastUnacked();
    for (UnackedPacketMap::const_iterator it = unacked_packets.begin();
         it != unacked_packets.end() && packet_number <= largest_newly_acked;
         ++it, ++packet_number)
    {
        if (!it->in_flight)
        {
            continue;
        }

        if (loss_type_ == kNack)
        {
            // FACK based loss detection.
            if (largest_newly_acked - packet_number >= kNumberOfNacksBeforeRetransmission)
            {
                packets_lost->push_back(std::make_pair(packet_number, it->bytes_sent));
                continue;
            }
        }
        else if (loss_type_ == kLazyFack)
        {
            // Require two in order acks to invoke FACK, which avoids spuriously
            // retransmitting packets when one packet is reordered by a large amount.
            if (largest_newly_acked > largest_previously_acked_ &&
                largest_previously_acked_ > packet_number &&
                largest_previously_acked_ - packet_number >=
                    (kNumberOfNacksBeforeRetransmission - 1))
            {
                packets_lost->push_back(std::make_pair(packet_number, it->bytes_sent));
                continue;
            }
        }

        // Only early retransmit(RFC5827) when the last packet gets acked and
        // there are retransmittable packets in flight.
        // This also implements a timer-protected variant of FACK.
        if (loss_type_ == kTime || loss_type_ == kAdaptiveTime)
        {
            uint64_t when_lost = it->sent_time + loss_delay;
            if (time < when_lost)
            {
                loss_detection_timeout_ = when_lost;
                break;
            }
            packets_lost->push_back(std::make_pair(packet_number, it->bytes_sent));
            continue;
        }

        // NACK-based loss detection allows for a max reordering window of 1 RTT.
        if (it->sent_time + rtt_stats.smoothed_rtt() <
            unacked_packets.GetTransmissionInfo(largest_newly_acked).sent_time)
        {
            packets_lost->push_back(std::make_pair(packet_number, it->bytes_sent));
            continue;
        }
    }
    largest_previously_acked_ = largest_newly_acked;
}

uint64_t GeneralLossAlgorithm::GetLossTimeout() const
{
    return loss_detection_timeout_;
}

void GeneralLossAlgorithm::SpuriousRetransmitDetected(
    const UnackedPacketMap &unacked_packets,
    uint64_t time,
    const RttStats &rtt_stats,
    PacketNumber spurious_retransmission)
{
    if (loss_type_ != kAdaptiveTime || reordering_shift_ == 0)
    {
        return;
    }
    // Calculate the extra time needed so this wouldn't have been declared lost.
    // Extra time needed is based on how long it's been since the spurious
    // retransmission was sent, because the SRTT and latest RTT may have changed.
    uint64_t extra_time_needed = time - unacked_packets.GetTransmissionInfo(spurious_retransmission).sent_time;
    // Increase the reordering fraction until enough time would be allowed.
    uint64_t max_rtt = std::max(rtt_stats.previous_srtt(), rtt_stats.latest_rtt());

    if (spurious_retransmission <= largest_sent_on_spurious_retransmit_)
    {
        return;
    }
    largest_sent_on_spurious_retransmit_ = unacked_packets.largest_sent_packet();
    uint64_t proposed_extra_time = 0;
    do
    {
        proposed_extra_time = max_rtt >> reordering_shift_;
        --reordering_shift_;
    } while (proposed_extra_time < extra_time_needed && reordering_shift_ > 0);
}
}
}