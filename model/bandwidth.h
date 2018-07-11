/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef BANDWIDTH_H
#define BANDWIDTH_H

#include <cmath>
#include <cstdint>
#include <limits>

#include "bbr-common.h"

namespace ns3
{
namespace bbr
{
class Bandwidth
{
  public:
    // Creates a new Bandwidth with an internal value of 0.
    static Bandwidth Zero() { return Bandwidth(0); }

    // Creates a new Bandwidth with an internal value of INT64_MAX.
    static Bandwidth Infinite()
    {
        return Bandwidth(std::numeric_limits<int64_t>::max());
    }

    // Create a new Bandwidth holding the bits per second.
    static Bandwidth FromBitsPerSecond(int64_t bits_per_second)
    {
        return Bandwidth(bits_per_second);
    }

    // Create a new Bandwidth holding the kilo bits per second.
    static Bandwidth FromKBitsPerSecond(int64_t k_bits_per_second)
    {
        return Bandwidth(k_bits_per_second * 1000);
    }

    // Create a new Bandwidth holding the bytes per second.
    static Bandwidth FromBytesPerSecond(int64_t bytes_per_second)
    {
        return Bandwidth(bytes_per_second * 8);
    }

    // Create a new Bandwidth holding the kilo bytes per second.
    static Bandwidth FromKBytesPerSecond(int64_t k_bytes_per_second)
    {
        return Bandwidth(k_bytes_per_second * 8000);
    }

    // Create a new Bandwidth based on the bytes per the elapsed delta.
    static inline Bandwidth FromBytesAndTimeDelta(ByteCount bytes,
                                                  uint64_t delta)
    {
        return Bandwidth((bytes * kNumMillisPerSecond) /
                         delta * 8);
    }

    inline int64_t ToBitsPerSecond() const { return bits_per_second_; }

    inline int64_t ToKBitsPerSecond() const { return bits_per_second_ / 1000; }

    inline int64_t ToBytesPerSecond() const { return bits_per_second_ / 8; }

    inline int64_t ToKBytesPerSecond() const { return bits_per_second_ / 8000; }

    inline ByteCount ToBytesPerPeriod(uint64_t time_period) const
    {
        return ToBytesPerSecond() * time_period /
               kNumMillisPerSecond;
    }

    inline int64_t ToKBytesPerPeriod(uint64_t time_period) const
    {
        return ToKBytesPerSecond() * time_period /
               kNumMillisPerSecond;
    }

    inline bool IsZero() const { return bits_per_second_ == 0; }

    inline uint64_t TransferTime(ByteCount bytes) const
    {
        if (bits_per_second_ == 0)
        {
            return 0;
        }
        return bytes * 8 * kNumMillisPerSecond / bits_per_second_;
    }

    std::string ToDebugValue() const;

  private:
    explicit Bandwidth(int64_t bits_per_second)
        : bits_per_second_(bits_per_second >= 0 ? bits_per_second : 0) {}

    int64_t bits_per_second_;

    friend Bandwidth operator+(Bandwidth lhs, Bandwidth rhs);
    friend Bandwidth operator-(Bandwidth lhs, Bandwidth rhs);
    friend Bandwidth operator*(Bandwidth lhs, float factor);
};

// Non-member relational operators for Bandwidth.
inline bool operator==(Bandwidth lhs, Bandwidth rhs)
{
    return lhs.ToBitsPerSecond() == rhs.ToBitsPerSecond();
}
inline bool operator!=(Bandwidth lhs, Bandwidth rhs)
{
    return !(lhs == rhs);
}
inline bool operator<(Bandwidth lhs, Bandwidth rhs)
{
    return lhs.ToBitsPerSecond() < rhs.ToBitsPerSecond();
}
inline bool operator>(Bandwidth lhs, Bandwidth rhs)
{
    return rhs < lhs;
}
inline bool operator<=(Bandwidth lhs, Bandwidth rhs)
{
    return !(rhs < lhs);
}
inline bool operator>=(Bandwidth lhs, Bandwidth rhs)
{
    return !(lhs < rhs);
}

// Non-member arithmetic operators for Bandwidth.
inline Bandwidth operator+(Bandwidth lhs, Bandwidth rhs)
{
    return Bandwidth(lhs.bits_per_second_ + rhs.bits_per_second_);
}
inline Bandwidth operator-(Bandwidth lhs, Bandwidth rhs)
{
    return Bandwidth(lhs.bits_per_second_ - rhs.bits_per_second_);
}
inline Bandwidth operator*(Bandwidth lhs, float rhs)
{
    return Bandwidth(
        static_cast<int64_t>(std::llround(lhs.bits_per_second_ * rhs)));
}
inline Bandwidth operator*(float lhs, Bandwidth rhs)
{
    return rhs * lhs;
}
inline ByteCount operator*(Bandwidth lhs, uint64_t rhs)
{
    return lhs.ToBytesPerPeriod(rhs);
}
inline ByteCount operator*(uint64_t lhs, Bandwidth rhs)
{
    return rhs * lhs;
}
}
}

#endif