/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef RTT_STATS_H
#define RTT_STATS_H

#include <algorithm>
#include "ns3/nstime.h"
#include "ns3/log.h"
#include "bbr-common.h"

namespace ns3
{
namespace bbr
{
class RttStats
{
  public:
    RttStats();

    // Updates the RTT from an incoming ack which is received |send_delta| after
    // the packet is sent and the peer reports the ack being delayed |ack_delay|.
    // Time Unit: ms
    void UpdateRtt(int64_t send_delta, int64_t ack_delay, uint64_t now);
    // Causes the smoothed_rtt to be increased to the latest_rtt if the latest_rtt
    // is larger. The mean deviation is increased to the most recent deviation if
    // it's larger.
    void ExpireSmoothedMetrics();

    // Called when connection migrates and rtt measurement needs to be reset.
    void OnConnectionMigration();

    // Returns the EWMA smoothed RTT for the connection.
    // May return Zero if no valid updates have occurred.
    int64_t smoothed_rtt() const { return smoothed_rtt_; }

    // Returns the EWMA smoothed RTT prior to the most recent RTT sample.
    int64_t previous_srtt() const { return previous_srtt_; }

    int64_t initial_rtt_ms() const { return initial_rtt_ms_; }

    // Sets an initial RTT to be used for SmoothedRtt before any RTT updates.
    void set_initial_rtt_ms(int64_t initial_rtt_ms);

    // The most recent rtt measurement.
    // May return Zero if no valid updates have occurred.
    int64_t latest_rtt() const { return latest_rtt_; }

    // Returns the min_rtt for the entire connection.
    // May return Zero if no valid updates have occurred.
    int64_t min_rtt() const { return min_rtt_; }

    int64_t mean_deviation() const { return mean_deviation_; }

  private:
    int64_t latest_rtt_;
    int64_t min_rtt_;
    int64_t smoothed_rtt_;
    int64_t previous_srtt_;
    // Mean RTT deviation during this session.
    // Approximation of standard deviation, the error is roughly 1.25 times
    // larger than the standard deviation, for a normally distributed signal.
    int64_t mean_deviation_;
    int64_t initial_rtt_ms_;

    DISALLOW_COPY_AND_ASSIGN(RttStats);
};
}
}

#endif