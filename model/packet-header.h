/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef PACKET_HEADER_H
#define PACKET_HEADER_H
#include <memory>
#include "ns3/header.h"
#include "packets.h"
#include "bbr-common.h"

namespace ns3
{
namespace bbr
{
class PacketHeader : public Header
{
  public:
    PacketHeader();
    virtual ~PacketHeader() {}

  public:
    PacketNumber m_packet_seq; //!< current Sequence number
    PacketNumber m_old_packet_seq;
    TransmissionType m_transmission_type;
    uint64_t m_sent_time;
    int m_data_length;
    //std::shared_ptr<DataPacket> m_data_packet;
    std::shared_ptr<PicDataPacket> m_data_packet;
    PacketNumber m_data_seq;

    uint8_t      PicType;        // Frame Type for this encoded picture
    PacketNumber PicIndex;       // Global frame index for this picture
    uint16_t     PicDataLen;
    uint16_t     PicPktNum;      // How many pkt needed for transmit this picture
    uint16_t     PicCurPktSeq;   // Current pkt seq for this pic
    uint64_t     PicGenTime;     // Current pkt data len

    static const PacketType m_type;
  public:
    /**
       * \brief Get the type ID.
       * \return The object TypeId.
       */
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual void Print(std::ostream &os) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);
};
}
}

#endif