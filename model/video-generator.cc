/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "video-generator.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("VideoGenerator");

VideoGenerator::VideoGenerator()
    : m_state(false),
      m_current_clip(NULL),
      m_current_index(0)
{
}

void VideoGenerator::GenerateFrame()
{
    NS_ASSERT(m_state == true);
    NS_LOG_INFO("Generate Frame m_current_index " << m_current_index);
    FrameInfo frameInfo = m_current_clip->m_frame_array[m_current_index];
    m_current_index++;
    if (m_current_index >= m_current_clip->m_frame_array.size())
    {
        m_current_index = 0;
    }

    if (m_frame_queue.size() >= kMaxFrameBufferSize)
    {
        NS_LOG_UNCOND("VideoGenerator frame queue full (max: " << kMaxFrameBufferSize << ")");
    }
    else
    {
        m_frame_queue.push_back(frameInfo);
    }

    Time delay = Seconds(1.0 / m_current_clip->m_frame_rate);
    m_event = Simulator::Schedule(delay, &VideoGenerator::GenerateFrame, this);
}

VideoClipInfo *
VideoGenerator::GetBestClip(int width, int height,
                            int frame_rate, int gop_size, int code_rate)
{
    if (width == 0 || height == 0)
    {
        return g_video_clips[0];
    }

    VideoClipInfo target;
    target.m_width = width;
    target.m_height = height;
    target.m_frame_rate = frame_rate;
    target.m_gop_size = gop_size;
    target.m_code_rate = code_rate;

    int best = 0;
    for (uint32_t i = 1; i < g_video_clips.size(); i++)
    {
        if (VideoClipDiff(target, *g_video_clips[i]) <
            VideoClipDiff(target, *g_video_clips[best]))
        {
            best = i;
        }
    }
    return g_video_clips[best];
}

void VideoGenerator::Start()
{
    if (m_state)
    {
        return;
    }
    m_state = true;
    m_current_clip = GetBestClip(0, 0, 0, 0, 0);
    m_current_index = 0;

    GenerateFrame();
}

void VideoGenerator::Stop()
{
    if (!m_state)
    {
        return;
    }

    m_state = false;
    if (m_event.IsRunning())
    {
        m_event.Cancel();
    }
}

bool VideoGenerator::GetNextFrame(FrameInfo &frame)
{
    bool ret = false;
    if (m_frame_queue.size() > 0)
    {
        frame = m_frame_queue[0];
        m_frame_queue.pop_front();
        ret = true;
    }

    return ret;
}

bool VideoGenerator::SetVideoParameters(int width, int height,
                                        int frame_rate, int gop_size, int code_rate)
{
    VideoClipInfo *best_clip = GetBestClip(width, height, frame_rate,
                                           gop_size, code_rate);

    if (best_clip != m_current_clip)
    {
        m_current_clip = best_clip;
        m_current_index = 0;

        return true;
    }
    return false;
}
}