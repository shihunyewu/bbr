/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef GENERAL_LOSS_ALGORITHM_H
#define GENERAL_LOSS_ALGORITHM_H

#include <algorithm>
#include <map>

#include "bbr-common.h"
#include "loss-detection-interface.h"

namespace ns3
{
namespace bbr
{
class RttStats;

class GeneralLossAlgorithm : public LossDetectionInterface
{
  public:
    // TCP retransmits after 3 nacks.
    static const PacketCount kNumberOfNacksBeforeRetransmission = 3;

    GeneralLossAlgorithm();
    explicit GeneralLossAlgorithm(LossDetectionType loss_type);
    ~GeneralLossAlgorithm() override {}

    LossDetectionType GetLossDetectionType() const override;

    // Switches the loss detection type to |loss_type| and resets the loss
    // algorithm.
    void SetLossDetectionType(LossDetectionType loss_type);

    // Uses |largest_acked| and time to decide when packets are lost.
    void DetectLosses(
        const UnackedPacketMap &unacked_packets,
        uint64_t time,
        const RttStats &rtt_stats,
        PacketNumber largest_newly_acked,
        SendAlgorithmInterface::CongestionVector *packets_lost) override;

    // Returns a non-zero value when the early retransmit timer is active.
    uint64_t GetLossTimeout() const override;

    // Increases the loss detection threshold for time loss detection.
    void SpuriousRetransmitDetected(
        const UnackedPacketMap &unacked_packets,
        uint64_t time,
        const RttStats &rtt_stats,
        PacketNumber spurious_retransmission) override;

    int reordering_shift() const { return reordering_shift_; }

  private:
    uint64_t loss_detection_timeout_;
    // Largest sent packet when a spurious retransmit is detected.
    // Prevents increasing the reordering threshold multiple times per epoch.
    // TODO(ianswett): Deprecate when
    // quic_reloadable_flag_quic_fix_adaptive_time_loss is deprecated.
    PacketNumber largest_sent_on_spurious_retransmit_;
    LossDetectionType loss_type_;
    // Fraction of a max(SRTT, latest_rtt) to permit reordering before declaring
    // loss.  Fraction calculated by shifting max(SRTT, latest_rtt) to the right
    // by reordering_shift.
    int reordering_shift_;
    // The largest newly acked from the previous call to DetectLosses.
    PacketNumber largest_previously_acked_;

    DISALLOW_COPY_AND_ASSIGN(GeneralLossAlgorithm);
};
}
}

#endif