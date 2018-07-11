/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#ifndef VIDEO_COMMON_H
#define VIDEO_COMMON_H

#include <vector>
#include <stdint.h>

#include "bbr-common.h"
#include "packets.h"

using namespace ns3::bbr;

#define TRACES_DIR_PATH "/home/shilei/software/ns3/src/bbr/model/videocodecs/video_traces/chat_firefox_h264/"
#define TRACES_FILE_PREFIX "chat"

namespace ns3
{
enum FrameType
{
    XFrame = 0,
    IFrame,
    PFrame,
    BFrame,
};

struct FrameInfo
{
    int m_type;
    int m_size;

    FrameInfo() = default;
};

struct PESInfo
{
    PacketNumber m_pes_seq;
    uint32_t m_size;
    static const int kVideoPacketSize = 1024;
};

class FrameListenerInterface
{
public:
    virtual void OnNewFrame(FrameInfo &frame) = 0;
};
}

#endif
