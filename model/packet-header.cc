/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/core-module.h"

#include "packet-header.h"

namespace ns3
{
namespace bbr
{
NS_LOG_COMPONENT_DEFINE("PacketHeader");

NS_OBJECT_ENSURE_REGISTERED(PacketHeader);

const PacketType PacketHeader::m_type = kStreamPacket;

PacketHeader::PacketHeader()
    : m_packet_seq(0),
      m_old_packet_seq(0),
      m_transmission_type(NOT_RETRANSMISSION),
      m_sent_time(0),
      m_data_length(0),
      m_data_packet(nullptr),
      m_data_seq(0)
{
}

TypeId PacketHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::PacketHeader")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<PacketHeader>();
    return tid;
}

TypeId PacketHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

void PacketHeader::Print(std::ostream &os) const
{
    os << "(seq=" << m_data_seq << ", " << m_packet_seq << ")";
}

uint32_t PacketHeader::GetSerializedSize(void) const
{
    return sizeof(uint8_t) + sizeof(PacketNumber) + sizeof(PacketNumber) + sizeof(uint64_t) +
           sizeof(uint8_t) + sizeof(PacketNumber) + 3*sizeof(uint16_t) + sizeof(uint64_t);
}

void PacketHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(m_type);
    i.WriteHtonU64(m_packet_seq);
    i.WriteHtonU64(m_sent_time);
    i.WriteHtonU64(m_data_seq);

    i.WriteU8(PicType);
    i.WriteHtonU64(PicIndex);
    i.WriteHtonU16(PicDataLen);
    i.WriteHtonU16(PicPktNum);
    i.WriteHtonU16(PicCurPktSeq);
    i.WriteHtonU64(PicGenTime);

}

uint32_t PacketHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    uint8_t type = i.ReadU8();
    NS_ASSERT(type == kStreamPacket);
    m_packet_seq = i.ReadNtohU64();
    m_sent_time = i.ReadNtohU64();
    m_data_seq = i.ReadNtohU64();


    PicType = i.ReadU8();
    PicIndex = i.ReadNtohU64();
    PicDataLen = i.ReadNtohU16();
    PicPktNum = i.ReadNtohU16();
    PicCurPktSeq = i.ReadNtohU16();
    PicGenTime = i.ReadNtohU64();


    return GetSerializedSize();
}
}
}