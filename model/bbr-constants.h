/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef BBR_CONSTANTS_H
#define BBR_CONSTANTS_H

namespace ns3
{
namespace bbr
{
typedef uint32_t PacketLength;
typedef uint64_t ByteCount;
typedef uint64_t PacketNumber;
typedef uint64_t PacketCount;

// Simple time constants.
const uint64_t kNumSecondsPerMinute = 60;
const uint64_t kNumSecondsPerHour = kNumSecondsPerMinute * 60;
const uint64_t kNumSecondsPerWeek = kNumSecondsPerHour * 24 * 7;
const uint64_t kNumMillisPerSecond = 1000;

// Default maximum packet size used in the Linux TCP implementation.
// Used in QUIC for congestion window computations in bytes.
const ByteCount kDefaultTCPMSS = 1460;

// The maximum packet size of any QUIC packet, based on ethernet's max size,
// minus the IP and UDP headers. IPv6 has a 40 byte header, UDP adds an
// additional 8 bytes.  This is a total overhead of 48 bytes.  Ethernet's
// max packet size is 1500 bytes,  1500 - 48 = 1452.
const ByteCount kMaxPacketSize = 1452;

// We match SPDY's use of 32 (since we'd compete with SPDY).
const PacketCount kInitialCongestionWindow = 32;

// Maximum number of tracked packets.
const PacketCount kMaxTrackedPackets = 10000;

// Minimum number of packets received before ack decimation is enabled.
// This intends to avoid the beginning of slow start, when CWNDs may be
// rapidly increasing.
const PacketCount kMinReceivedBeforeAckDecimation = 100;

// Wait for up to 10 retransmittable packets before sending an ack.
const PacketCount kMaxRetransmittablePacketsBeforeAck = 10;

// Don't allow a client to suggest an RTT shorter than 10ms.
const uint32_t kMinInitialRoundTripTimeMs = 10;

// Don't allow a client to suggest an RTT longer than 15 seconds.
const uint32_t kMaxInitialRoundTripTimeMs = 15 * kNumMillisPerSecond;

// Maximum delayed ack time, in ms.
const int64_t kMaxDelayedAckTimeMs = 25;

// Minimum tail loss probe time in ms.
static const int64_t kMinTailLossProbeTimeoutMs = 10;

// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int64_t kMinRetransmissionTimeMs = 200;

static const int64_t kDefaultRetransmissionTimeMs = 500;
static const int64_t kMaxRetransmissionTimeMs = 60000;
// Maximum number of exponential backoffs used for RTO timeouts.
static const size_t kMaxRetransmissions = 10;
// Maximum number of packets retransmitted upon an RTO.
static const size_t kMaxRetransmissionsOnTimeout = 2;
// Minimum number of consecutive RTOs before path is considered to be degrading.
const size_t kMinTimeoutsBeforePathDegrading = 2;

// Sends up to two tail loss probes before firing an RTO,
// per draft RFC draft-dukkipati-tcpm-tcp-loss-probe.
static const size_t kDefaultMaxTailLossProbes = 2;
}
}

#endif