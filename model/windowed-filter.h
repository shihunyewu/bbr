/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef WINDOWED_FILTER_H
#define WINDOWED_FILTER_H

namespace ns3
{
namespace bbr
{
// Compares two values and returns true if the first is less than or equal
// to the second.
template <class T>
struct MinFilter
{
    bool operator()(const T &lhs, const T &rhs) const { return lhs <= rhs; }
};

// Compares two values and returns true if the first is greater than or equal
// to the second.
template <class T>
struct MaxFilter
{
    bool operator()(const T &lhs, const T &rhs) const { return lhs >= rhs; }
};

// Use the following to construct a windowed filter object of type T.
// For example, a min filter using QuicTime as the time type:
//   WindowedFilter<T, MinFilter<T>, QuicTime, QuicTime::Delta> ObjectName;
// A max filter using 64-bit integers as the time type:
//   WindowedFilter<T, MaxFilter<T>, uint64_t, int64_t> ObjectName;
// Specifically, this template takes four arguments:
// 1. T -- type of the measurement that is being filtered.
// 2. Compare -- MinFilter<T> or MaxFilter<T>, depending on the type of filter
//    desired.
// 3. TimeT -- the type used to represent timestamps.
// 4. TimeDeltaT -- the type used to represent continuous time intervals between
//    two timestamps.  Has to be the type of (a - b) if both |a| and |b| are
//    of type TimeT.
template <class T, class Compare, typename TimeT, typename TimeDeltaT>
class WindowedFilter
{
  public:
    // |window_length| is the period after which a best estimate expires.
    // |zero_value| is used as the uninitialized value for objects of T.
    // Importantly, |zero_value| should be an invalid value for a true sample.
    WindowedFilter(TimeDeltaT window_length, T zero_value, TimeT zero_time)
        : window_length_(window_length),
          zero_value_(zero_value),
          estimates_{Sample(zero_value_, zero_time),
                     Sample(zero_value_, zero_time),
                     Sample(zero_value_, zero_time)} {}

    // Updates best estimates with |sample|, and expires and updates best
    // estimates as necessary.
    void Update(T new_sample, TimeT new_time)
    {
        // Reset all estimates if they have not yet been initialized, if new sample
        // is a new best, or if the newest recorded estimate is too old.
        if (estimates_[0].sample == zero_value_ ||
            Compare()(new_sample, estimates_[0].sample) ||
            static_cast<TimeDeltaT>(new_time - estimates_[2].time) > window_length_)
        {
            Reset(new_sample, new_time);
            return;
        }

        if (Compare()(new_sample, estimates_[1].sample))
        {
            estimates_[1] = Sample(new_sample, new_time);
            estimates_[2] = estimates_[1];
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif
        }
        else if (Compare()(new_sample, estimates_[2].sample))
        {
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
            estimates_[2] = Sample(new_sample, new_time);
        }

        // Expire and update estimates as necessary.
        if (static_cast<TimeDeltaT>(new_time - estimates_[0].time) > window_length_)
        {
            // The best estimate hasn't been updated for an entire window, so promote
            // second and third best estimates.
            estimates_[0] = estimates_[1];
            estimates_[1] = estimates_[2];
            estimates_[2] = Sample(new_sample, new_time);
            // Need to iterate one more time. Check if the new best estimate is
            // outside the window as well, since it may also have been recorded a
            // long time ago. Don't need to iterate once more since we cover that
            // case at the beginning of the method.
            if (static_cast<TimeDeltaT>(new_time - estimates_[0].time) > window_length_)
            {
                estimates_[0] = estimates_[1];
                estimates_[1] = estimates_[2];
            }
            return;
        }
        if (estimates_[1].sample == estimates_[0].sample &&
            static_cast<TimeDeltaT>(new_time - estimates_[1].time) > window_length_ >> 2)
        {
            // A quarter of the window has passed without a better sample, so the
            // second-best estimate is taken from the second quarter of the window.
            estimates_[2] = estimates_[1] = Sample(new_sample, new_time);
            return;
        }

        if (estimates_[2].sample == estimates_[1].sample &&
            static_cast<TimeDeltaT>(new_time - estimates_[2].time) > window_length_ >> 1)
        {
            // We've passed a half of the window without a better estimate, so take
            // a third-best estimate from the second half of the window.
            estimates_[2] = Sample(new_sample, new_time);
        }
    }

    // Resets all estimates to new sample.
    void Reset(T new_sample, TimeT new_time)
    {
        estimates_[0] = estimates_[1] = estimates_[2] =
            Sample(new_sample, new_time);
    }

    T GetBest() const { return estimates_[0].sample; }
    T GetSecondBest() const { return estimates_[1].sample; }
    T GetThirdBest() const { return estimates_[2].sample; }

  private:
    struct Sample
    {
        T sample;
        TimeT time;
        Sample(T init_sample, TimeT init_time)
            : sample(init_sample), time(init_time) {}
    };

    TimeDeltaT window_length_; // Time length of window.
    T zero_value_;             // Uninitialized value of T.
    Sample estimates_[3];      // Best estimate is element 0.
};
}
}

#endif