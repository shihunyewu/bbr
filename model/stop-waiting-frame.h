/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef STOP_WAITING_H
#define STOP_WAITING_H

#include "ns3/header.h"

#include "bbr-common.h"

namespace ns3
{
namespace bbr
{
class StopWaitingFrame : public Header
{
  public:
    StopWaitingFrame();
    virtual ~StopWaitingFrame() {}

    void SetLeastSeq(PacketNumber seq)
    {
        least_unacked = seq;
    }

    PacketNumber GetLeastSeq(void) const
    {
        return least_unacked;
    }

  PacketNumber least_unacked; // by dd
  private:
    //PacketNumber least_unacked; // by dd

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