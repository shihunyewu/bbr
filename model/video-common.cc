/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#include "video-common.h"

#include "clips/clip_1280x720_24fps_68_4350kbps.h"

namespace ns3
{
std::vector<VideoClipInfo *> g_video_clips = {
    &g_clip_1280x720_24fps_68_4350kbps};

uint32_t VideoClipDiff(VideoClipInfo &left, VideoClipInfo &right)
{
    uint32_t diff = 0;
    //width, height, frame_rate
    double diff_ratio = (left.m_width - right.m_width) / double(left.m_width) +
                        (left.m_height - right.m_height) / double(left.m_height) +
                        (left.m_frame_rate - right.m_frame_rate) / double(left.m_frame_rate);
    uint32_t diff_high = diff_ratio >= 1 ? 0xFFFFFFFF : static_cast<uint16_t>(diff_ratio * 0xFFFFFFFF);

    diff = diff_high << 16;

    //gop_size, code_rate

    diff_ratio = (left.m_gop_size - right.m_gop_size) / double(left.m_gop_size) +
                 (left.m_code_rate - right.m_code_rate) / double(left.m_code_rate);
    uint32_t diff_low = diff_ratio >= 1 ? 0xFFFFFFFF : static_cast<uint16_t>(diff_ratio * 0xFFFFFFFF);
    diff += diff_low;

    return diff;
}
}