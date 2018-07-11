/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef ACK_FRAME_H
#define ACK_FRAME_H

#include <deque>
#include <ostream>
#include <string>

#include "ns3/header.h"

#include "interval.h"

namespace ns3
{
namespace bbr
{
typedef std::vector<std::pair<PacketNumber, uint64_t>> PacketTimeVector;

// A sequence of packet numbers where each number is unique. Intended to be used
// in a sliding window fashion, where smaller old packet numbers are removed and
// larger new packet numbers are added, with the occasional random access.
class PacketNumberQueue
{
  public:
    PacketNumberQueue();
    PacketNumberQueue(const PacketNumberQueue &other);
    PacketNumberQueue(PacketNumberQueue &&other);
    ~PacketNumberQueue();

    PacketNumberQueue &operator=(const PacketNumberQueue &other);
    PacketNumberQueue &operator=(PacketNumberQueue &&other);

    class const_iterator
    {
      public:
        const_iterator(const const_iterator &other);
        const_iterator(const_iterator &&other);
        ~const_iterator();

        explicit const_iterator(
            typename std::deque<SeqInterval>::const_iterator it);

        typedef std::input_iterator_tag iterator_category;
        typedef SeqInterval value_type;
        typedef value_type &reference;
        typedef value_type *pointer;
        typedef typename std::vector<value_type>::iterator::difference_type
            difference_type;

        inline const SeqInterval &operator*()
        {
            return *deque_it_;
        }

        inline const_iterator &operator++()
        {
            deque_it_++;
            return *this;
        }

        inline const_iterator &operator--()
        {
            deque_it_--;
            return *this;
        }

        inline const_iterator &operator++(int)
        {
            ++deque_it_;
            return *this;
        }

        inline bool operator==(const const_iterator &other)
        {
            return deque_it_ == other.deque_it_;
        }

        inline bool operator!=(const const_iterator &other)
        {
            return !(*this == other);
        }

      private:
        typename std::deque<SeqInterval>::const_iterator deque_it_;
    };

    class const_reverse_iterator
    {
      public:
        const_reverse_iterator(const const_reverse_iterator &other);
        const_reverse_iterator(const_reverse_iterator &&other);
        ~const_reverse_iterator();

        explicit const_reverse_iterator(
            const typename std::deque<SeqInterval>::const_reverse_iterator &it);

        typedef std::input_iterator_tag iterator_category;
        typedef SeqInterval value_type;
        typedef value_type &reference;
        typedef value_type *pointer;
        typedef typename std::vector<value_type>::iterator::difference_type
            difference_type;

        inline const SeqInterval &operator*()
        {
            return *deque_it_;
        }

        inline const SeqInterval *operator->()
        {
            return &*deque_it_;
        }

        inline const_reverse_iterator &operator++()
        {
            deque_it_++;
            return *this;
        }

        inline const_reverse_iterator &operator--()
        {
            deque_it_--;
            return *this;
        }

        inline const_reverse_iterator &operator++(int)
        {
            ++deque_it_;
            return *this;
        }

        inline bool operator==(const const_reverse_iterator &other)
        {
            return deque_it_ == other.deque_it_;
        }

        inline bool operator!=(const const_reverse_iterator &other)
        {
            return !(*this == other);
        }

      private:
        typename std::deque<SeqInterval>::const_reverse_iterator
            deque_it_;
    };

    // Adds |packet_number| to the set of packets in the queue.
    void Add(PacketNumber packet_number);

    // Adds packets between [lower, higher) to the set of packets in the queue. It
    // is undefined behavior to call this with |higher| < |lower|.
    void Add(PacketNumber lower, PacketNumber higher);

    // Removes packets with values less than |higher| from the set of packets in
    // the queue. Returns true if packets were removed.
    bool RemoveUpTo(PacketNumber higher);

    // Removes the smallest interval in the queue.
    void RemoveSmallestInterval();

    // Returns true if the queue contains |packet_number|.
    bool Contains(PacketNumber packet_number) const;

    // Returns true if the queue is empty.
    bool Empty() const;

    // Returns the minimum packet number stored in the queue. It is undefined
    // behavior to call this if the queue is empty.
    PacketNumber Min() const;

    // Returns the maximum packet number stored in the queue. It is undefined
    // behavior to call this if the queue is empty.
    PacketNumber Max() const;

    // Returns the number of unique packets stored in the queue. Inefficient; only
    // exposed for testing.
    size_t NumPacketsSlow() const;

    // Returns the number of disjoint packet number intervals contained in the
    // queue.
    size_t NumIntervals() const;

    // Returns the length of last interval.
    PacketNumber LastIntervalLength() const;

    // Returns iterators over the packet number intervals.
    const_iterator begin() const;
    const_iterator end() const;
    const_reverse_iterator rbegin() const;
    const_reverse_iterator rend() const;

    friend std::ostream &operator<<(
        std::ostream &os,
        const PacketNumberQueue &q);

  private:
    std::deque<SeqInterval> packet_number_deque_;
};

class AckFrame : public Header
{
  public:
    AckFrame();
    AckFrame(const AckFrame &other);
    ~AckFrame();

    friend std::ostream &operator<<(
        std::ostream &os,
        const AckFrame &ack_frame);

    /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
    static TypeId GetTypeId(void);

    virtual TypeId GetInstanceTypeId(void) const;
    virtual void Print(std::ostream &os) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);

    // The highest packet number we've observed from the peer.
    PacketNumber largest_observed;
    // Time elapsed since largest_observed was received until this Ack frame was sent.
    uint64_t ack_delay_time;

    uint64_t last_update_time;

    // Vector of <packet_number, time> for when packets arrived.
    PacketTimeVector received_packet_times;

    // Set of packets.
    PacketNumberQueue packets;

    static const PacketType m_type;
};

// True if the packet number is greater than largest_observed or is listed as missing.
// Always returns false for packet numbers less than least_unacked.
bool IsAwaitingPacket(
    const AckFrame &ack_frame,
    PacketNumber packet_number,
    PacketNumber peer_least_packet_awaiting_ack);
}
}

#endif