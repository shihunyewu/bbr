/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include "bbr-common.h"
#include "rtt-stats.h"
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("RttStats");
namespace bbr
{
// Default initial rtt used before any samples are received.

const int kInitialRttMs = 100;
const float kAlpha = 0.125f;
const float kOneMinusAlpha = (1 - kAlpha);
const float kBeta = 0.25f;
const float kOneMinusBeta = (1 - kBeta);

RttStats::RttStats()
    : latest_rtt_(0),
      min_rtt_(0),
      smoothed_rtt_(0),
      previous_srtt_(0),
      mean_deviation_(0),
      initial_rtt_ms_(kInitialRttMs) {}

void RttStats::set_initial_rtt_ms(int64_t initial_rtt_ms)
{
    if (initial_rtt_ms <= 0)
    {
        NS_LOG_WARN("Attempt to set initial rtt to <= 0.");
        return;
    }
    initial_rtt_ms_ = initial_rtt_ms;
}

void RttStats::ExpireSmoothedMetrics()
{
    mean_deviation_ = std::max(mean_deviation_,
                               std::abs(smoothed_rtt_ - latest_rtt_));
    smoothed_rtt_ = std::max(smoothed_rtt_, latest_rtt_);
}

// Updates the RTT based on a new sample.
void RttStats::UpdateRtt(int64_t send_delta,
                         int64_t ack_delay,
                         uint64_t now)
{
    if (send_delta > 60 * 1000 || send_delta <= 0)
    {
        NS_LOG_WARN("Ignoring measured send_delta, because it's is "
                    << "either too large, zero, or negative.  send_delta = "
                    << send_delta);
        return;
    }

    // Update min_rtt_ first. min_rtt_ does not use an rtt_sample corrected for
    // ack_delay but the raw observed send_delta, since poor clock granularity at
    // the client may cause a high ack_delay to result in underestimation of the
    // min_rtt_.
    if (min_rtt_ == 0 || min_rtt_ > send_delta)
    {
        min_rtt_ = send_delta;
    }

    // Correct for ack_delay if information received from the peer results in a
    // positive RTT sample. Otherwise, we use the send_delta as a reasonable
    // measure for smoothed_rtt.
    int64_t rtt_sample = send_delta;
    previous_srtt_ = smoothed_rtt_;

    if (rtt_sample > ack_delay)
    {
        rtt_sample = rtt_sample - ack_delay;
    }
    latest_rtt_ = rtt_sample;
    // First time call.
    if (smoothed_rtt_ == 0)
    {
        smoothed_rtt_ = rtt_sample;
        mean_deviation_ = rtt_sample / 2;
    }
    else
    {
        mean_deviation_ = static_cast<int64_t>(kOneMinusBeta * mean_deviation_ +
                                               kBeta * std::abs(smoothed_rtt_ - rtt_sample));
        smoothed_rtt_ = kOneMinusAlpha * smoothed_rtt_ + kAlpha * rtt_sample;
        NS_LOG_INFO("smoothed_rtt(ms):" << smoothed_rtt_
                                        << " mean_deviation(ms):" << mean_deviation_);
    }
}

void RttStats::OnConnectionMigration()
{
    latest_rtt_ = 0;
    min_rtt_ = 0;
    smoothed_rtt_ = 0;
    mean_deviation_ = 0;
    initial_rtt_ms_ = kInitialRttMs;
}
}
}