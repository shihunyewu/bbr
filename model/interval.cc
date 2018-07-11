#include "interval.h"

namespace ns3
{
namespace bbr
{
//==============================================================================
// Implementation details: Clients can stop reading here.
bool SeqInterval::Intersects(const SeqInterval &i, SeqInterval *out) const
{
    if (!Intersects(i))
        return false;
    if (out != nullptr)
    {
        *out = SeqInterval(SEQ_LE(min(), i.min()) ? i.min() : min(),
                           SEQ_GE(max(), i.max()) ? i.max() : max());
    }
    return true;
}

bool SeqInterval::IntersectWith(const SeqInterval &i)
{
    if (Empty())
        return false;
    bool modified = false;
    if (SEQ_GT(i.min(), min()))
    {
        SetMin(i.min());
        modified = true;
    }
    if (SEQ_LT(i.max(), max()))
    {
        SetMax(i.max());
        modified = true;
    }
    return modified;
}

bool SeqInterval::SpanningUnion(const SeqInterval &i)
{
    if (i.Empty())
        return false;
    if (Empty())
    {
        *this = i;
        return true;
    }
    bool modified = false;
    if (SEQ_LT(i.min(), min()))
    {
        SetMin(i.min());
        modified = true;
    }
    if (SEQ_GT(i.max(), max()))
    {
        SetMax(i.max());
        modified = true;
    }
    return modified;
}

bool SeqInterval::Difference(const SeqInterval &i,
                             SeqInterval *lo,
                             SeqInterval *hi) const
{
    // Initialize *lo and *hi to empty
    *lo = {};
    *hi = {};
    if (Empty())
        return false;
    if (i.Empty())
    {
        *lo = *this;
        return false;
    }
    if (SEQ_LT(min(), i.max()) && SEQ_GE(min(), i.min()) && SEQ_GT(max(), i.max()))
    {
        //            [------ this ------)
        // [------ i ------)
        //                 [-- result ---)
        *hi = SeqInterval(i.max(), max());
        return true;
    }
    if (SEQ_GT(max(), i.min()) && SEQ_LE(max(), i.max()) && SEQ_LT(min(), i.min()))
    {
        // [------ this ------)
        //            [------ i ------)
        // [- result -)
        *lo = SeqInterval(min(), i.min());
        return true;
    }
    if (SEQ_LT(min(), i.min()) && SEQ_GT(max(), i.max()))
    {
        // [------- this --------)
        //      [---- i ----)
        // [ R1 )           [ R2 )
        // There are two results: R1 and R2.
        *lo = SeqInterval(min(), i.min());
        *hi = SeqInterval(i.max(), max());
        return true;
    }
    if (SEQ_GE(min(), i.min()) && SEQ_LE(max(), i.max()))
    {
        //   [--- this ---)
        // [------ i --------)
        // Intersection is <this>, so difference yields the empty interval.
        return true;
    }
    *lo = *this; // No intersection.
    return false;
}
}
}