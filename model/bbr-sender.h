/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef BBR_SENDER_H
#define BBR_SENDER_H

#include <ostream>
#include <memory>
#include <assert.h>

#include "bbr-common.h"
#include "send-algorithm-interface.h"
#include "rtt-stats.h"
#include "bandwidth-sampler.h"
#include "unacked-packet-map.h"
#include "windowed-filter.h"

namespace ns3
{
namespace bbr
{
class RttStats;
typedef uint64_t RoundTripCount;

// BbrSender implements BBR congestion control algorithm.  BBR aims to estimate
// the current available Bottleneck Bandwidth and RTT (hence the name), and
// regulates the pacing rate and the size of the congestion window based on
// those signals.
//
// BBR relies on pacing in order to function properly.  Do not use BBR when
// pacing is disabled.
//
// TODO(vasilvv): implement traffic policer (long-term sampling) mode.
class BbrSender : public SendAlgorithmInterface
{
  public:
    enum Mode
    {
        // Startup phase of the connection.
        STARTUP,
        // After achieving the highest possible bandwidth during the startup, lower
        // the pacing rate in order to drain the queue.
        DRAIN,
        // Cruising mode.
        PROBE_BW,
        // Temporarily slow down sending in order to empty the buffer and measure
        // the real minimum RTT.
        PROBE_RTT,
    };

    // Indicates how the congestion control limits the amount of bytes in flight.
    enum RecoveryState
    {
        // Do not limit.
        NOT_IN_RECOVERY,
        // Allow an extra outstanding byte for each byte acknowledged.
        CONSERVATION,

        // Allow 1.5 extra outstanding bytes for each byte acknowledged.
        MEDIUM_GROWTH,                                                   // added by dd

        // Allow two extra outstanding bytes for each byte acknowledged (slow
        // start).
        GROWTH
    };

    // Debug state can be exported in order to troubleshoot potential congestion
    // control issues.
    struct DebugState
    {
        explicit DebugState(const BbrSender &sender);
        DebugState(const DebugState &state);

        Mode mode;
        Bandwidth max_bandwidth;
        RoundTripCount round_trip_count;
        int gain_cycle_index;
        ByteCount congestion_window;

        bool is_at_full_bandwidth;
        Bandwidth bandwidth_at_last_round;
        RoundTripCount rounds_without_bandwidth_gain;

        uint64_t min_rtt;
        uint64_t min_rtt_timestamp;

        RecoveryState recovery_state;
        ByteCount recovery_window;

        bool last_sample_is_app_limited;
        PacketNumber end_of_app_limited_phase;
    };

    BbrSender(const RttStats *rtt_stats,
              const UnackedPacketMap *unacked_packets,
              PacketCount initial_tcp_congestion_window,
              PacketCount max_tcp_congestion_window);
    ~BbrSender() override;

    // Start implementation of SendAlgorithmInterface.
    bool InSlowStart() const override;
    bool InRecovery() const override;
    
    bool IsProbingForMoreBandwidth() const override; //by dd
    void SetInitialCongestionWindowInPackets(PacketCount congestion_window) override; // by dd

    void AdjustNetworkParameters(Bandwidth bandwidth, uint64_t rtt) override;

    void OnCongestionEvent(bool rtt_updated,
                           ByteCount prior_in_flight,
                           uint64_t event_time,
                           const CongestionVector &acked_packets,
                           const CongestionVector &lost_packets) override;
    bool OnPacketSent(uint64_t sent_time,
                      ByteCount bytes_in_flight,
                      PacketNumber packet_number,
                      ByteCount bytes,
                      HasRetransmittableData is_retransmittable) override;
    void OnRetransmissionTimeout(bool packets_retransmitted) override {}
    void OnConnectionMigration() override {}
    uint64_t TimeUntilSend(uint64_t now, ByteCount bytes_in_flight) override;
    Bandwidth PacingRate(ByteCount bytes_in_flight) const override;
    Bandwidth BandwidthEstimate() const override;
    ByteCount GetCongestionWindow() const override;
    ByteCount GetSlowStartThreshold() const override;
    CongestionControlType GetCongestionControlType() const override;
    std::string GetDebugState() const override;
    void OnApplicationLimited(ByteCount bytes_in_flight) override;
    // End implementation of SendAlgorithmInterface.

    // Gets the number of RTTs BBR remains in STARTUP phase.
    RoundTripCount num_startup_rtts() const {
        return num_startup_rtts_;
    }
    ///---------------------------------------By dd add new function start-------------------------------///
    bool has_non_app_limited_sample() const {
        return has_non_app_limited_sample_;
    }
 
    // Sets the pacing gain used in STARTUP.  Must be greater than 1.
    void set_high_gain(float high_gain) {
        //DCHECK_LT(1.0f, high_gain);
        assert(high_gain > 1.0f);
        high_gain_ = high_gain;
        if (mode_ == STARTUP) {
            pacing_gain_ = high_gain;
        }
    }
 
    // Sets the CWND gain used in STARTUP.  Must be greater than 1.
    void set_high_cwnd_gain(float high_cwnd_gain) {
        //DCHECK_LT(1.0f, high_cwnd_gain);
        assert(high_cwnd_gain > 1.0f);
        high_cwnd_gain_ = high_cwnd_gain;
        if (mode_ == STARTUP) {
            congestion_window_gain_ = high_cwnd_gain;
        }
    }
 
    // Sets the gain used in DRAIN.  Must be less than 1.
    void set_drain_gain(float drain_gain) {
        //DCHECK_GT(1.0f, drain_gain);
        assert(drain_gain < 1.0f);
        drain_gain_ = drain_gain;
    }
    ///---------------------------------------By dd add new function stop-------------------------------///
    DebugState ExportDebugState() const;

  private:
    typedef WindowedFilter<Bandwidth,
                           MaxFilter<Bandwidth>,
                           RoundTripCount,
                           int64_t>
        MaxBandwidthFilter;

    typedef WindowedFilter<uint64_t,
                           MaxFilter<uint64_t>,
                           RoundTripCount,
                           int64_t>
        MaxAckDelayFilter;

    typedef WindowedFilter<ByteCount,
                           MaxFilter<ByteCount>,
                           RoundTripCount,
                           int64_t>
        MaxAckHeightFilter;

    // Returns the current estimate of the RTT of the connection.  Outside of the
    // edge cases, this is minimum RTT.
    uint64_t GetMinRtt() const;
    
    // Returns whether the connection has achieved full bandwidth required to exit
    // the slow start.
    bool IsAtFullBandwidth() const;
    
    // Computes the target congestion window using the specified gain.
    ByteCount GetTargetCongestionWindow(float gain) const;

    // Enters the STARTUP mode.
    void EnterStartupMode();
    
    // Enters the PROBE_BW mode.
    void EnterProbeBandwidthMode(uint64_t now);

    // Discards the lost packets from BandwidthSampler state.
    void DiscardLostPackets(const CongestionVector &lost_packets);
    
    // Updates the round-trip counter if a round-trip has passed.  Returns true if
    // the counter has been advanced.
    bool UpdateRoundTripCounter(PacketNumber last_acked_packet);
    
    // Updates the current bandwidth and min_rtt estimate based on the samples for
    // the received acknowledgements.  Returns true if min_rtt has expired.
    bool UpdateBandwidthAndMinRtt(uint64_t now, const CongestionVector &acked_packets);
    
    // Updates the current gain used in PROBE_BW mode.
    void UpdateGainCyclePhase(uint64_t now, ByteCount prior_in_flight, bool has_losses);
    
    // Tracks for how many round-trips the bandwidth has not increased significantly.
    void CheckIfFullBandwidthReached();
    
    // Transitions from STARTUP to DRAIN and from DRAIN to PROBE_BW if appropriate.
    void MaybeExitStartupOrDrain(uint64_t now);
    
    // Decides whether to enter or exit PROBE_RTT.
    void MaybeEnterOrExitProbeRtt(uint64_t now, bool is_round_start, bool min_rtt_expired);                                
    
    // Determines whether BBR needs to enter, exit or advance state of the recovery.
    void UpdateRecoveryState(PacketNumber last_acked_packet, bool has_losses, bool is_round_start);                           

    // Updates the ack aggregation max filter in bytes.
    // void UpdateAckAggregationBytes(uint64_t ack_time, ByteCount newly_acked_bytes); // com by dd
    ByteCount UpdateAckAggregationBytes(uint64_t ack_time, ByteCount newly_acked_bytes);    // change return value by dd: void-->ByteCount

    // Determines the appropriate pacing rate for the connection.
    void CalculatePacingRate();

    // Determines the appropriate congestion window for the connection.
    // void CalculateCongestionWindow(ByteCount bytes_acked); // comment by dd
    void CalculateCongestionWindow(ByteCount bytes_acked,     // by dd
                                 ByteCount excess_acked);     // by dd , add this paramter

    // Determines the approriate window that constrains the in-flight during recovery.
    void CalculateRecoveryWindow(ByteCount bytes_acked, ByteCount bytes_lost);

    const RttStats *rtt_stats_;
    const UnackedPacketMap *unacked_packets_;

    Ptr<UniformRandomVariable> random_;

    Mode mode_;

    // Bandwidth sampler provides BBR with the bandwidth measurements at
    // individual points.
    std::unique_ptr<BandwidthSamplerInterface> sampler_;

    // The number of the round trips that have occurred during the connection.
    RoundTripCount round_trip_count_;

    // The packet number of the most recently sent packet.
    PacketNumber last_sent_packet_;
    // Acknowledgement of any packet after |current_round_trip_end_| will cause
    // the round trip counter to advance.
    PacketCount current_round_trip_end_;

    // The filter that tracks the maximum bandwidth over the multiple recent
    // round-trips.
    MaxBandwidthFilter max_bandwidth_;

    // Tracks the maximum number of bytes acked faster than the sending rate.
    MaxAckHeightFilter max_ack_height_;

    // The time this aggregation started and the number of bytes acked during it.
    uint64_t aggregation_epoch_start_time_;
    ByteCount aggregation_epoch_bytes_;

    // The number of bytes acknowledged since the last time bytes in flight
    // dropped below the target window.
    ByteCount bytes_acked_since_queue_drained_;

    // The muliplier for calculating the max amount of extra CWND to add to
    // compensate for ack aggregation.
    float max_aggregation_bytes_multiplier_;

    // Minimum RTT estimate.  Automatically expires within 10 seconds (and
    // triggers PROBE_RTT mode) if no new value is sampled during that period.
    uint64_t min_rtt_;
    // The time at which the current value of |min_rtt_| was assigned.
    uint64_t min_rtt_timestamp_;

    // The maximum allowed number of bytes in flight.
    ByteCount congestion_window_;

    // The initial value of the |congestion_window_|.
    ByteCount initial_congestion_window_;

    // The largest value the |congestion_window_| can achieve.
    ByteCount max_congestion_window_;

    //-----------------------------------------add new para by dd start ------------------------------//
    // The smallest value the |congestion_window_| can achieve.
    ByteCount min_congestion_window_;
 
    // The pacing gain applied during the STARTUP phase.
    float high_gain_;
 
    // The CWND gain applied during the STARTUP phase.
    float high_cwnd_gain_;
 
    // The pacing gain applied during the DRAIN phase.
    float drain_gain_;


    //-----------------------------------------add new para by dd stop ------------------------------//

    // The current pacing rate of the connection.
    Bandwidth pacing_rate_;

    // The gain currently applied to the pacing rate.
    float pacing_gain_;
    // The gain currently applied to the congestion window.
    float congestion_window_gain_;

    // The gain used for the congestion window during PROBE_BW.  Latched from
    // quic_bbr_cwnd_gain flag.
    const float congestion_window_gain_constant_;
    // The coefficient by which mean RTT variance is added to the congestion
    // window.  Latched from quic_bbr_rtt_variation_weight flag.
    const float rtt_variance_weight_;
    // The number of RTTs to stay in STARTUP mode.  Defaults to 3.
    RoundTripCount num_startup_rtts_;
    // If true, exit startup if 1RTT has passed with no bandwidth increase and
    // the connection is in recovery.
    bool exit_startup_on_loss_;

    // Number of round-trips in PROBE_BW mode, used for determining the current pacing gain cycle.
    int cycle_current_offset_;
    // The time at which the last pacing gain cycle was started.
    uint64_t last_cycle_start_;

    // Indicates whether the connection has reached the full bandwidth mode.
    bool is_at_full_bandwidth_;
    // Number of rounds during which there was no significant bandwidth increase.
    RoundTripCount rounds_without_bandwidth_gain_;
    // The bandwidth compared to which the increase is measured.
    Bandwidth bandwidth_at_last_round_;

    // Set to true upon exiting quiescence.
    bool exiting_quiescence_;

    // Time at which PROBE_RTT has to be exited.  Setting it to zero indicates
    // that the time is yet unknown as the number of packets in flight has not
    // reached the required value.
    uint64_t exit_probe_rtt_at_;
    // Indicates whether a round-trip has passed since PROBE_RTT became active.
    bool probe_rtt_round_passed_;

    // Indicates whether the most recent bandwidth sample was marked as
    // app-limited.
    bool last_sample_is_app_limited_;
    //-----------------------------------------add new para by dd start ------------------------------//
    // Indicates whether any non app-limited samples have been recorded.
    bool has_non_app_limited_sample_;

    // When true, add the most recent ack aggregation measurement during STARTUP.
    bool enable_ack_aggregation_during_startup_;
    
    // If true, will not exit low gain mode until bytes_in_flight drops below BDP
    // or it's time for high gain mode.
    bool drain_to_target_;

    // If true, consider all samples in recovery app-limited.
    bool is_app_limited_recovery_;

    // When true, pace at 1.5x and disable packet conservation in STARTUP.
    bool slower_startup_;
    // When true, disables packet conservation in STARTUP.
    bool rate_based_startup_;
    // Used as the initial packet conservation mode when first entering recovery.
    RecoveryState initial_conservation_in_startup_;
    // When true, add the most recent ack aggregation measurement during STARTUP.
    //bool enable_ack_aggregation_during_startup_;

    // If true, will not exit low gain mode until bytes_in_flight drops below BDP
    // or it's time for high gain mode.
    // bool drain_to_target_;

    // If true, use a CWND of 0.75*BDP during probe_rtt instead of 4 packets.
    bool probe_rtt_based_on_bdp_;
    // If true, skip probe_rtt and update the timestamp of the existing min_rtt to
    // now if min_rtt over the last cycle is within 12.5% of the current min_rtt.
    // Even if the min_rtt is 12.5% too low, the 25% gain cycling and 2x CWND gain
    // should overcome an overly small min_rtt.
    bool probe_rtt_skipped_if_similar_rtt_;
    // If true, disable PROBE_RTT entirely as long as the connection was recently
    // app limited.
    bool probe_rtt_disabled_if_app_limited_;
    bool app_limited_since_last_probe_rtt_;
    uint64_t min_rtt_since_last_probe_rtt_;
    //-----------------------------------------add new para by dd stop ------------------------------//

    // Current state of recovery.
    RecoveryState recovery_state_;
    // Receiving acknowledgement of a packet after |end_recovery_at_| will cause
    // BBR to exit the recovery mode.
    PacketNumber end_recovery_at_;
    // A window used to limit the number of bytes in flight during loss recovery.
    ByteCount recovery_window_;

    // When true, recovery is rate based rather than congestion window based.
    bool rate_based_recovery_;



    DISALLOW_COPY_AND_ASSIGN(BbrSender);
};

std::ostream &operator<<(std::ostream &os, const BbrSender::Mode &mode);
std::ostream &operator<<(std::ostream &os, const BbrSender::DebugState &state);
}
}

#endif