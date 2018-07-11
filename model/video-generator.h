/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef VIDEO_GENERATOR_H
#define VIDEO_GENERATOR_H

#include <deque>

#include "ns3/core-module.h"
#include "video-common.h"

namespace ns3
{

class VideoGenerator
{
  public:
    VideoGenerator();

    static VideoClipInfo *GetBestClip(int width, int height,
                                       int frame_rate, int gop_size, int code_rate);

    void Start();
    void Stop();

    void GenerateFrame(void);
    bool GetNextFrame(FrameInfo &frame);
    int GetN() { return m_frame_queue.size(); }
    bool SetVideoParameters(int width, int height,
                            int frame_rate, int gop_size, int code_rate);

  private:

    static const uint32_t kMaxFrameBufferSize = 100;
    bool m_state;

    std::deque<FrameInfo> m_frame_queue;

    VideoClipInfo *m_current_clip;
    uint32_t m_current_index;

    EventId m_event;
};
}

#endif