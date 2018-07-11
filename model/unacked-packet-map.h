/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef UNACKED_PACKET_MAP_H
#define UNACKED_PACKET_MAP_H

#include <deque>
#include <memory>
#include "bbr-common.h"

namespace ns3
{
namespace bbr
{
struct PacketHeader;
struct PicDataPacket;
struct TransmissionInfo
{
    // Used by STL when assigning into a map.
    TransmissionInfo();

    TransmissionInfo(PacketHeader &packet, TransmissionType transmission_type, bool set_in_flight);                   

    TransmissionInfo(const TransmissionInfo &other);

    ~TransmissionInfo();

    std::shared_ptr<PicDataPacket> data_packet;

    PacketLength bytes_sent;
    uint64_t sent_time;

    // Reason why this packet was transmitted.
    TransmissionType transmission_type;
    // In flight packets have not been abandoned or lost.
    bool in_flight;
    // True if the packet can never be acked, so it can be removed.
    bool is_unackable;
    // Stores the packet number of the next retransmission of this packet.
    // Zero if the packet has not been retransmitted.
    PacketNumber retransmission;
};

// Class which tracks unacked packets for two purposes:
// 1) Track packets and bytes in flight for congestion control.
// 2) Track sent time of packets to provide RTT measurements from acks.
class UnackedPacketMap
{
  public:
    UnackedPacketMap();
    ~UnackedPacketMap(){};

    // Adds |serialized_packet| to the map and marks it as sent at |sent_time|.
    // Marks the packet as in flight if |set_in_flight| is true.
    // Packets marked as in flight are expected to be marked as missing when they
    // don't arrive, indicating the need for retransmission.
    // |old_packet_number| is the packet number of the previous transmission,
    // or 0 if there was none.
    // Any AckNotifierWrappers in |serialized_packet| are swapped from the
    // serialized packet into the QuicTransmissionInfo.
    void AddSentPacket(PacketHeader &serialized_packet,
                       PacketNumber old_packet_number,
                       TransmissionType transmission_type,
                       uint64_t sent_time,
                       bool set_in_flight);
    // Returns true if the packet |packet_number| is unacked.
    bool IsUnacked(PacketNumber packet_number) const;

    // Marks |info| as no longer in flight.
    void RemoveFromInFlight(TransmissionInfo *info);

    // Marks |packet_number| as no longer in flight.
    void RemoveFromInFlight(PacketNumber packet_number);

    // Marks |packet_number| as in flight.  Must not be unackable.
    void RestoreToInFlight(PacketNumber packet_number);

    bool HasRetransmittableFrames(PacketNumber packet_number) const;

    // Returns true if there are any unacked packets.
    bool HasUnackedPackets() const;

    // Returns true if there are any unacked packets which have retransmittable
    // frames.
    bool HasUnackedRetransmittableFrames() const;

    // Returns the largest packet number that has been sent.
    PacketNumber largest_sent_packet() const { return largest_sent_packet_; }

    // Returns the largest retransmittable packet number that has been sent.
    PacketNumber largest_sent_retransmittable_packet() const
    {
        return largest_sent_retransmittable_packet_;
    }

    // Returns the largest packet number that has been acked.
    PacketNumber largest_observed() const { return largest_observed_; }

    // Returns the sum of bytes from all packets in flight.
    ByteCount bytes_in_flight() const { return bytes_in_flight_; }

    // Returns the smallest packet number of a packet which has not
    // been acked by the peer.  If there are no unacked packets, returns 0.
    PacketNumber GetLeastUnacked() const { return least_unacked_; };

    typedef std::deque<TransmissionInfo> UnackedPacketList;

    typedef UnackedPacketList::const_iterator const_iterator;
    typedef UnackedPacketList::iterator iterator;

    const_iterator begin() const { return unacked_packets_.begin(); }
    const_iterator end() const { return unacked_packets_.end(); }
    iterator begin() { return unacked_packets_.begin(); }
    iterator end() { return unacked_packets_.end(); }

    // Returns true if there are unacked packets that are in flight.
    bool HasInFlightPackets() const;

    // Returns the TransmissionInfo associated with |packet_number|, which
    // must be unacked.
    const TransmissionInfo &GetTransmissionInfo(
        PacketNumber packet_number) const;

    // Returns mutable TransmissionInfo associated with |packet_number|, which
    // must be unacked.
    TransmissionInfo *GetMutableTransmissionInfo(
        PacketNumber packet_number);

    // Returns the time that the last unacked packet was sent.
    uint64_t GetLastPacketSentTime() const;

    // Returns the number of unacked packets.
    size_t GetNumUnackedPacketsDebugOnly() const;

    // Returns true if there are multiple packets in flight.
    bool HasMultipleInFlightPackets() const;

    // Removes any retransmittable frames from this transmission or an associated
    // transmission.  It removes now useless transmissions, and disconnects any
    // other packets from other transmissions.
    void RemoveRetransmittability(TransmissionInfo *info);

    // Looks up the TransmissionInfo by |packet_number| and calls
    // RemoveRetransmittability.
    void RemoveRetransmittability(PacketNumber packet_number);

    // Increases the largest observed.  Any packets less or equal to
    // |largest_acked_packet| are discarded if they are only for the RTT purposes.
    void IncreaseLargestObserved(PacketNumber largest_observed);

    // Remove any packets no longer needed for retransmission, congestion, or
    // RTT measurement purposes.
    void RemoveObsoletePackets();

  private:
    // Called when a packet is retransmitted with a new packet number.
    // |old_packet_number| will remain unacked, but will have no
    // retransmittable data associated with it. Retransmittable frames will be
    // transferred to |info| and all_transmissions will be populated.
    void TransferRetransmissionInfo(PacketNumber old_packet_number,
                                    PacketNumber new_packet_number,
                                    TransmissionType transmission_type,
                                    TransmissionInfo *info);

    // Returns true if packet may be useful for an RTT measurement.
    bool IsPacketUsefulForMeasuringRtt(PacketNumber packet_number,
                                       const TransmissionInfo &info) const;

    // Returns true if packet may be useful for congestion control purposes.
    bool IsPacketUsefulForCongestionControl(const TransmissionInfo &info) const;

    // Returns true if packet may be associated with retransmittable data
    // directly or through retransmissions.
    bool IsPacketUsefulForRetransmittableData(const TransmissionInfo &info) const;

    // Returns true if the packet no longer has a purpose in the map.
    bool IsPacketUseless(PacketNumber packet_number,
                         const TransmissionInfo &info) const;

    PacketNumber largest_sent_packet_;
    // The largest sent packet we expect to receive an ack for.
    PacketNumber largest_sent_retransmittable_packet_;
    PacketNumber largest_observed_;

    // Newly serialized retransmittable packets are added to this map, which
    // contains owning pointers to any contained frames.  If a packet is
    // retransmitted, this map will contain entries for both the old and the new
    // packet. The old packet's retransmittable frames entry will be nullptr,
    // while the new packet's entry will contain the frames to retransmit.
    // If the old packet is acked before the new packet, then the old entry will
    // be removed from the map and the new entry's retransmittable frames will be
    // set to nullptr.
    UnackedPacketList unacked_packets_;
    // The packet at the 0th index of unacked_packets_.
    PacketNumber least_unacked_;

    ByteCount bytes_in_flight_;

    DISALLOW_COPY_AND_ASSIGN(UnackedPacketMap);
};
}
}

#endif