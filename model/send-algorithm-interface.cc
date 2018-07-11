/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include "ns3/core-module.h"

#include "send-algorithm-interface.h"
#include "bbr-sender.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("SendAlgorithmInterface");
namespace bbr
{
class RttStats;

// Factory for send side congestion control algorithm.
SendAlgorithmInterface *SendAlgorithmInterface::Create(
    const RttStats *rtt_stats,
    const UnackedPacketMap *unacked_packets,
    CongestionControlType congestion_control_type,
    ConnectionStats *stats,
    PacketCount initial_congestion_window)
{
    PacketCount max_congestion_window = kDefaultMaxCongestionWindowPackets;
    switch (congestion_control_type)
    {
    case kBBR:
        return new BbrSender(rtt_stats, unacked_packets, initial_congestion_window, max_congestion_window);
    default:
        break;
    }
    return nullptr;
}
}
}