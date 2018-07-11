/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#ifndef PACKETS_H
#define PACKETS_H

#include <list>
#include <deque>
#include <memory>

#include "bbr-common.h"

namespace ns3
{

namespace bbr
{

enum ProtocolSendPriority
{
    P0,
    P1,
    END,
};

struct PicData{

    uint8_t      CurType;           // Frame Type for this encoded picture
    PacketNumber PicSeq;            // Global frame index for this picture
    uint16_t     PicDataLen;
    uint16_t     PicPktNum;         // How many pkt needed for transmit this picture
    uint16_t     PicCurPktSeq;      // Current pkt seq for this pic
    uint64_t     PicGenTime;        // Current pkt data len
    uint64_t     PicExpireTime;     // Current pkt data len

    std::deque<uint16_t> PktDataLen;
};

struct DataPacket
{
    DataPacket()
        : data_seq(0), data_length(0), priority(P0)
        , expire_time(0), last_send_time(0), send_count(0), useless(false)
    {
    }
    ~DataPacket() {}

    PacketNumber data_seq;
    PacketLength data_length;
    ProtocolSendPriority priority;
    uint64_t expire_time;
    uint64_t last_send_time;
    int send_count;
    bool useless;
    std::vector<uint8_t> payload;

    std::list<PacketNumber> packet_numbers;
};

struct PicDataPacket
{
    PicDataPacket()
         : data_seq(0), data_length(0), priority(P0)
         , expire_time(0), last_send_time(0), send_count(0), useless(false),
           PicType(0),PicIndex(0),PicPktNum(0),PicCurPktSeq(0)
    {
    }
    ~PicDataPacket() {}

    uint8_t      PicType;        // Frame Type for this encoded picture
    PacketNumber PicIndex;       // Global frame index for this picture
    uint16_t     PicDataLen;
    uint16_t     PicPktNum;      // How many pkt needed for transmit this picture
    uint16_t     PicCurPktSeq;   // Current pkt seq for this pic
    uint64_t     PicGenTime;     // Current pkt data len

    PacketNumber data_seq;
    PacketLength data_length;    // Current pkt data len
    ProtocolSendPriority priority;
    uint64_t expire_time;
    uint64_t last_send_time;
    int send_count;
    bool useless;
    std::vector<uint8_t> payload;

    std::list<PacketNumber> packet_numbers;
};


struct SequenceNumberGenerator
{
    SequenceNumberGenerator() : m_seq(0) {}
    PacketNumber NextSeq() { return ++m_seq; }

  private:
    PacketNumber m_seq;
};
}
}

#endif