/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef BBR_COMMON_H
#define BBR_COMMON_H

#include <stdint.h>
#include "ns3/packet.h"

#include "bbr-constants.h"

namespace ns3
{
namespace bbr
{

const uint32_t kStopWaitingThreshold = 3;

#define SEQ_LT(a, b) (int64_t((a) - (b)) < 0)
#define SEQ_LE(a, b) (int64_t((a) - (b)) <= 0)
#define SEQ_GT(a, b) (int64_t((a) - (b)) > 0)
#define SEQ_GE(a, b) (int64_t((a) - (b)) >= 0)

// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName) \
  TypeName(const TypeName &);                \
  void operator=(const TypeName &)

// An alternate name that leaves out the moral judgment... :-)
#define DISALLOW_COPY_AND_ASSIGN(TypeName) DISALLOW_EVIL_CONSTRUCTORS(TypeName)

#define MILLISECOND(ms) (uint64_t(ms))
#define SECOND(s) (uint64_t(s * kNumMillisPerSecond))
#define INFINITETIME std::numeric_limits<uint64_t>::max()

// Test to see if a set or map contains a particular key.
// Returns true if the key is in the collection.
template <typename Collection, typename Key>
bool ContainsKey(const Collection &collection, const Key &key)
{
  return collection.find(key) != collection.end();
}

enum LossDetectionType
{
  kNack,         // Used to mimic TCP's loss detection.
  kTime,         // Time based loss detection.
  kAdaptiveTime, // Adaptive time based loss detection.
  kLazyFack,     // Nack based but with FACK disabled for the first ack.
};

enum CongestionControlType
{
  kCubic,
  kCubicBytes,
  kReno,
  kRenoBytes,
  kBBR,
  kPCC
};

enum PacketType
{
  kPaddingPacket = 0,
  kStreamPacket,
  kAckPacket,
  kStopWaiting,
};

inline uint8_t PeekPackeType(Ptr<Packet> packet)
{
  uint8_t type = kPaddingPacket;
  packet->CopyData(&type, 1);
  return type;
}

enum TransmissionType : int8_t {
  NOT_RETRANSMISSION,
  FIRST_TRANSMISSION_TYPE = NOT_RETRANSMISSION,
  HANDSHAKE_RETRANSMISSION,    // Retransmits due to handshake timeouts.
  ALL_UNACKED_RETRANSMISSION,  // Retransmits all unacked packets.
  ALL_INITIAL_RETRANSMISSION,  // Retransmits all initially encrypted packets.
  LOSS_RETRANSMISSION,         // Retransmits due to loss detection.
  RTO_RETRANSMISSION,          // Retransmits due to retransmit time out.
  TLP_RETRANSMISSION,          // Tail loss probes.
  LAST_TRANSMISSION_TYPE = TLP_RETRANSMISSION,
};

enum HasRetransmittableData : int8_t {
  NO_RETRANSMITTABLE_DATA,
  HAS_RETRANSMITTABLE_DATA,
};

}
}

#endif