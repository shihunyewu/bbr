/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#ifndef LOSS_DETECTION_INTERFACE_H
#define LOSS_DETECTION_INTERFACE_H

#include "send-algorithm-interface.h"

namespace ns3
{
namespace bbr
{
class UnackedPacketMap;
class RttStats;

class LossDetectionInterface
{
  public:
    virtual ~LossDetectionInterface() {}
    virtual LossDetectionType GetLossDetectionType() const = 0;

    // Called when a new ack arrives or the loss alarm fires.
    virtual void DetectLosses(
        const UnackedPacketMap &unacked_packets,
        uint64_t time,
        const RttStats &rtt_stats,
        PacketNumber largest_newly_acked,
        SendAlgorithmInterface::CongestionVector *packets_lost) = 0;

    // Get the time the LossDetectionAlgorithm wants to re-evaluate losses.
    // Returns Time::Zero if no alarm needs to be set.
    virtual uint64_t GetLossTimeout() const = 0;

    // Called when a |spurious_retransmission| is detected.  The original
    // transmission must have been caused by DetectLosses.
    virtual void SpuriousRetransmitDetected(
        const UnackedPacketMap &unacked_packets,
        uint64_t time,
        const RttStats &rtt_stats,
        PacketNumber spurious_retransmission) = 0;
};
}
}

#endif