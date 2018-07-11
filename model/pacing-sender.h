/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef PACING_SENDER_H
#define PACING_SENDER_H

#include "bbr-common.h"
#include "send-algorithm-interface.h"
#include "bandwidth.h"

namespace ns3
{
namespace bbr
{
class PacingSender
{
  public:
    PacingSender();
    ~PacingSender();

    // Sets the underlying sender. Does not take ownership of |sender|. |sender|
    // must not be null. This must be called before any of the
    // SendAlgorithmInterface wrapper methods are called.
    void set_sender(SendAlgorithmInterface *sender);

    void set_max_pacing_rate(Bandwidth max_pacing_rate)
    {
        max_pacing_rate_ = max_pacing_rate;
    }

    void OnCongestionEvent(
        bool rtt_updated,
        ByteCount bytes_in_flight,
        uint64_t event_time,
        const SendAlgorithmInterface::CongestionVector &acked_packets,
        const SendAlgorithmInterface::CongestionVector &lost_packets);

    bool OnPacketSent(uint64_t sent_time,
                      ByteCount bytes_in_flight,
                      PacketNumber packet_number,
                      ByteCount bytes,
                      HasRetransmittableData is_retransmittable);

    uint64_t TimeUntilSend(uint64_t now, ByteCount bytes_in_flight);

    Bandwidth PacingRate(ByteCount bytes_in_flight) const;

  private:
    // Underlying sender. Not owned.
    SendAlgorithmInterface *sender_;
    // If not BandWidth::Zero, the maximum rate the PacingSender will use.
    Bandwidth max_pacing_rate_;

    // Number of unpaced packets to be sent before packets are delayed.
    uint32_t burst_tokens_;
    // Send time of the last packet considered delayed.
    uint64_t last_delayed_packet_sent_time_;
    uint64_t ideal_next_packet_send_time_; // When can the next packet be sent.
    bool was_last_send_delayed_;       // True when the last send was delayed.

    DISALLOW_COPY_AND_ASSIGN(PacingSender);
};
}
}

#endif