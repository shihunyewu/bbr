/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/core-module.h"

#include "stop-waiting-frame.h"

namespace ns3
{
namespace bbr
{
NS_LOG_COMPONENT_DEFINE("StopWaitingFrame");

const PacketType StopWaitingFrame::m_type = kStopWaiting;

StopWaitingFrame::StopWaitingFrame()
    : least_unacked(0)
{
}

TypeId StopWaitingFrame::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::StopWaitingFrame")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<StopWaitingFrame>();
    return tid;
}

TypeId StopWaitingFrame::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

void StopWaitingFrame::Print(std::ostream &os) const
{
    os << "(least_seq=" << least_unacked << ")";
}

uint32_t StopWaitingFrame::GetSerializedSize(void) const
{
    return sizeof(uint8_t) + sizeof(PacketNumber);
}

void StopWaitingFrame::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_type);
    i.WriteHtonU64(least_unacked);
}

uint32_t StopWaitingFrame::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    NS_ASSERT(type == kStopWaiting);
    least_unacked = i.ReadNtohU64();
    return GetSerializedSize();
}
}
}