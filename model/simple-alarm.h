/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef SIMPLE_ALARM_H
#define SIMPLE_ALARM_H

namespace ns3
{
namespace bbr
{
class SimpleAlarm
{
  public:
    SimpleAlarm() : m_deadline(0) {}
    bool IsSet() const { return m_deadline > 0; }
    bool IsExpired(uint64_t now_ms)
    {
        bool ret = false;
        if (IsSet())
        {
            ret = now_ms >= m_deadline;
            if (ret)
            {
                m_deadline = 0;
            }
        }
        return ret;
    }
    void Update(uint64_t new_deadline_ms)
    {
        m_deadline = new_deadline_ms;
    }

  private:
    uint64_t m_deadline;
};
}
}

#endif