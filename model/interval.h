// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An SeqInterval<T> is a data structure used to represent a contiguous, mutable
// range over an ordered type T. Supported operations include testing a value to
// see whether it is included in the interval, comparing two intervals, and
// performing their union, intersection, and difference. For the purposes of
// this library, an "ordered type" is any type that induces a total order on its
// values via its less-than operator (operator<()). Examples of such types are
// basic arithmetic types like int and double as well as class types like
// string.
//
// An SeqInterval<T> is represented using the usual C++ STL convention, namely as
// the half-open interval [min, max). A point p is considered to be contained in
// the interval iff p >= min && p < max. One consequence of this definition is
// that for any non-empty interval, min is contained in the interval but max is
// not. There is no canonical representation for the empty interval; rather, any
// interval where max <= min is regarded as empty. As a consequence, two empty
// intervals will still compare as equal despite possibly having different
// underlying min() or max() values. Also beware of the terminology used here:
// the library uses the terms "min" and "max" rather than "begin" and "end" as
// is conventional for the STL.
//
// T is required to be default- and copy-constructable, to have an assignment
// operator, and the full complement of comparison operators (<, <=, ==, !=, >=,
// >).  A difference operator (operator-()) is required if SeqInterval<T>::Length
// is used.
//
// For equality comparisons, SeqInterval<T> supports an Equals() method and an
// operator==() which delegates to it. Two intervals are considered equal if
// either they are both empty or if their corresponding min and max fields
// compare equal. For ordered comparisons, SeqInterval<T> also provides the
// comparator SeqInterval<T>::Less and an operator<() which delegates to it.
// Unfortunately this comparator is currently buggy because its behavior is
// inconsistent with Equals(): two empty ranges with different representations
// may be regarded as equivalent by Equals() but regarded as different by
// the comparator. Bug 9240050 has been created to address this.
//
// This class is thread-compatible if T is thread-compatible. (See
// go/thread-compatible).
//
// Examples:
//   SeqInterval<int> r1(0, 100);  // The interval [0, 100).
//   EXPECT_TRUE(r1.Contains(0));
//   EXPECT_TRUE(r1.Contains(50));
//   EXPECT_FALSE(r1.Contains(100));  // 100 is just outside the interval.
//
//   SeqInterval<int> r2(50, 150);  // The interval [50, 150).
//   EXPECT_TRUE(r1.Intersects(r2));
//   EXPECT_FALSE(r1.Contains(r2));
//   EXPECT_TRUE(r1.IntersectWith(r2));  // Mutates r1.
//   EXPECT_EQ(SeqInterval<int>(50, 100), r1);  // r1 is now [50, 100).
//
//   SeqInterval<int> r3(1000, 2000);  // The interval [1000, 2000).
//   EXPECT_TRUE(r1.IntersectWith(r3));  // Mutates r1.
//   EXPECT_TRUE(r1.Empty());  // Now r1 is empty.
//   EXPECT_FALSE(r1.Contains(r1.min()));  // e.g. doesn't contain its own min.

#ifndef INTERVAL_H
#define INTERVAL_H

#include <algorithm>
#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "bbr-common.h"

namespace ns3
{
namespace bbr
{

class SeqInterval
{
  public:
    // Compatibility alias.
    using Less = std::less<SeqInterval>;

    // Construct an SeqInterval representing an empty interval.
    SeqInterval() : min_(), max_() {}

    // Construct an SeqInterval representing the interval [min, max). If min < max,
    // the constructed object will represent the non-empty interval containing all
    // values from min up to (but not including) max. On the other hand, if min >=
    // max, the constructed object will represent the empty interval.
    SeqInterval(const PacketNumber &min, const PacketNumber &max) : min_(min), max_(max) {}

    const PacketNumber &min() const { return min_; }
    const PacketNumber &max() const { return max_; }
    void SetMin(const PacketNumber &t) { min_ = t; }
    void SetMax(const PacketNumber &t) { max_ = t; }

    void Set(const PacketNumber &min, const PacketNumber &max)
    {
        SetMin(min);
        SetMax(max);
    }

    void Clear() { *this = {}; }
    void CopyFrom(const SeqInterval &i) { *this = i; }
    bool Equals(const SeqInterval &i) const { return *this == i; }
    bool Empty() const { return SEQ_GE(min(), max()); }

    // Returns the length of this interval. The value returned is zero if
    // IsEmpty() is true; otherwise the value returned is max() - min().
    const int32_t Length() const { return (SEQ_GE(min_, max_) ? min_ : max_) - min_; }

    // Returns true iff t >= min() && t < max().
    bool Contains(const PacketNumber &t) const { return SEQ_LE(min(), t) && SEQ_GT(max(), t); }

    // Returns true iff *this and i are non-empty, and *this includes i. "*this
    // includes i" means that for all t, if i.Contains(t) then this->Contains(t).
    // Note the unintuitive consequence of this definition: this method always
    // returns false when i is the empty interval.
    bool Contains(const SeqInterval &i) const
    {
        return !Empty() && !i.Empty() && SEQ_LE(min(), i.min()) && SEQ_GE(max(), i.max());
    }

    // Returns true iff there exists some point t for which this->Contains(t) &&
    // i.Contains(t) evaluates to true, i.e. if the intersection is non-empty.
    bool Intersects(const SeqInterval &i) const
    {
        return !Empty() && !i.Empty() && SEQ_LT(min(), i.max()) && SEQ_GT(max(), i.min());
    }

    // Returns true iff there exists some point t for which this->Contains(t) &&
    // i.Contains(t) evaluates to true, i.e. if the intersection is non-empty.
    // Furthermore, if the intersection is non-empty and the intersection pointer
    // is not null, this method stores the calculated intersection in
    // *intersection.
    bool Intersects(const SeqInterval &i, SeqInterval *out) const;

    // Sets *this to be the intersection of itself with i. Returns true iff
    // *this was modified.
    bool IntersectWith(const SeqInterval &i);

    // Calculates the smallest interval containing both *this i, and updates *this
    // to represent that interval, and returns true iff *this was modified.
    bool SpanningUnion(const SeqInterval &i);

    // Determines the difference between two intervals as in
    // Difference(SeqInterval&, vector*), but stores the results directly in out
    // parameters rather than dynamically allocating an SeqInterval* and appending
    // it to a vector. If two results are generated, the one with the smaller
    // value of min() will be stored in *lo and the other in *hi. Otherwise (if
    // fewer than two results are generated), unused arguments will be set to the
    // empty interval (it is possible that *lo will be empty and *hi non-empty).
    // The method returns true iff the intersection of *this and i is non-empty.
    bool Difference(const SeqInterval &i, SeqInterval *lo, SeqInterval *hi) const;

    friend bool operator==(const SeqInterval &a, const SeqInterval &b)
    {
        bool ae = a.Empty();
        bool be = b.Empty();
        if (ae && be)
            return true; // All empties are equal.
        if (ae != be)
            return false; // Empty cannot equal nonempty.
        return a.min() == b.min() && a.max() == b.max();
    }

    friend bool operator!=(const SeqInterval &a, const SeqInterval &b)
    {
        return !(a == b);
    }

    // Defines a comparator which can be used to induce an order on Intervals, so
    // that, for example, they can be stored in an ordered container such as
    // std::set. The ordering is arbitrary, but does provide the guarantee that,
    // for non-empty intervals X and Y, if X contains Y, then X <= Y.
    // TODO(kosak): The current implementation of this comparator has a problem
    // because the ordering it induces is inconsistent with that of Equals(). In
    // particular, this comparator does not properly consider all empty intervals
    // equivalent. Bug b/9240050 has been created to track this.
    friend bool operator<(const SeqInterval &a, const SeqInterval &b)
    {
        return SEQ_LT(a.min(), b.min()) || (a.min() == b.min() && SEQ_GT(a.max(), b.max()));
    }

    friend std::ostream &operator<<(std::ostream &out, const SeqInterval &i)
    {
        return out << "[" << i.min() << ", " << i.max() << ")";
    }

  private:
    PacketNumber min_; // Inclusive lower bound.
    PacketNumber max_; // Exclusive upper bound.
};
}
} // namespace net

#endif
