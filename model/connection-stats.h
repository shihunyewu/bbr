/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef CONNECTION_STATS_H
#define CONNECTION_STATS_H

#include "bbr-common.h"
#include "bandwidth.h"

namespace ns3
{
namespace bbr
{
// Structure to hold stats for a Connection.
struct ConnectionStats
{
    ConnectionStats();
    ConnectionStats(const ConnectionStats &other);
    ~ConnectionStats();

    friend std::ostream &operator<<(
        std::ostream &os,
        const ConnectionStats &s);

    ByteCount bytes_sent; // Includes retransmissions.
    PacketCount packets_sent;
    // Non-retransmitted bytes sent in a stream frame.
    ByteCount stream_bytes_sent;
    // Packets serialized and discarded before sending.
    PacketCount packets_discarded;

    // These include version negotiation and public reset packets, which do not
    // have packet numbers or frame data.
    ByteCount bytes_received; // Includes duplicate data for a stream.
    // Includes packets which were not processable.
    PacketCount packets_received;
    // Excludes packets which were not processable.
    PacketCount packets_processed;
    ByteCount stream_bytes_received; // Bytes received in a stream frame.

    ByteCount bytes_retransmitted;
    PacketCount packets_retransmitted;

    ByteCount bytes_spuriously_retransmitted;
    PacketCount packets_spuriously_retransmitted;
    // Number of packets abandoned as lost by the loss detection algorithm.
    PacketCount packets_lost;

    // Number of packets sent in slow start.
    PacketCount slowstart_packets_sent;
    // Number of packets lost exiting slow start.
    PacketCount slowstart_packets_lost;
    // Number of bytes lost exiting slow start.
    ByteCount slowstart_bytes_lost;

    PacketCount packets_dropped; // Duplicate or less than least unacked.
    size_t crypto_retransmit_count;
    // Count of times the loss detection alarm fired.  At least one packet should
    // be lost when the alarm fires.
    size_t loss_timeout_count;
    size_t tlp_count;
    size_t rto_count; // Count of times the rto timer fired.

    int64_t min_rtt_us; // Minimum RTT in microseconds.
    int64_t srtt_us;    // Smoothed RTT in microseconds.
    ByteCount max_packet_size;
    ByteCount max_received_packet_size;
    Bandwidth estimated_bandwidth;

    // Reordering stats for received packets.
    // Number of packets received out of packet number order.
    PacketCount packets_reordered;
    // Maximum reordering observed in packet number space.
    PacketNumber max_sequence_reordering;
    // Maximum reordering observed in microseconds
    int64_t max_time_reordering_us;

    // The following stats are used only in TcpCubicSender.
    // The number of loss events from TCP's perspective.  Each loss event includes
    // one or more lost packets.
    uint32_t tcp_loss_events;

    // Creation time, as reported by the Clock.
    uint64_t connection_creation_time;

    uint64_t blocked_frames_received;
    uint64_t blocked_frames_sent;
};
}
}

#endif