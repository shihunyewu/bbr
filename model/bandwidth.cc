/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include <inttypes.h>
#include <string>

#include "ns3/core-module.h"
#include "bandwidth.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Bandwidth");
namespace bbr
{

std::string Bandwidth::ToDebugValue() const
{
    char buf[100] = {0};
    if (bits_per_second_ < 80000)
    {
        snprintf(buf, 100, "%" PRId64 " bits/s (%" PRId64 " bytes/s)",
                            bits_per_second_, bits_per_second_ / 8);
        std::string str(buf);
        return str;
    }

    double divisor;
    char unit;
    if (bits_per_second_ < 8 * 1000 * 1000)
    {
        divisor = 1e3;
        unit = 'k';
    }
    else if (bits_per_second_ < INT64_C(8) * 1000 * 1000 * 1000)
    {
        divisor = 1e6;
        unit = 'M';
    }
    else
    {
        divisor = 1e9;
        unit = 'G';
    }

    double bits_per_second_with_unit = bits_per_second_ / divisor;
    double bytes_per_second_with_unit = bits_per_second_with_unit / 8;

    snprintf(buf, 100, "%.2f %cbits/s (%.2f %cbytes/s)",
                        bits_per_second_with_unit, unit,
                        bytes_per_second_with_unit, unit);
    std::string str(buf);
    return str;
}
}
}