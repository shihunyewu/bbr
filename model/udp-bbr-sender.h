/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#ifndef UDP_BBR_SENDER_H
#define UDP_BBR_SENDER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-value.h"
#include "ns3/random-variable-stream.h"
#include "ns3/core-module.h"
#include "connection-stats.h"
#include "packet-header.h"
#include "simple-alarm.h"

#include "ns3/socket.h"
#include "bbr-common.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <memory>
#include <string>
#include "ns3/video-codecs.h"
#include "udp-bbr-constants.h"

#include "video-common.h"

namespace ns3
{
class Packet;
class Socket;
namespace bbr
{
class SentPacketManager;
class AckFrame;
class WindowedFilter;
}
using namespace bbr;

class UdpBbrSender;


    class MyVideoCodec{
    public:
        MyVideoCodec();
        void Setup(float fps, DataRate max_data_rate, DataRate min_data_rate, DataRate target_data_rate, DataRate step_data_rate,uint64_t timeout, UdpBbrSender* sender);
        void StartApp();
        void StopApp();
        bool GetNextPacket(PicDataPacket &data);

        bool GetRedundantPacket(PicDataPacket &data); // for fake data

        void SetCodec(std::shared_ptr<VideoCodecs::Codec> codec);
        void SetCodecType(SyncodecType codecType);

        float setTargetRate(float newRateBps);
        uint64_t GetCurMaxPicQueueDelay(uint64_t now);

    private:
        void SendPacket();
        void HandleTimeout();
        void EnqueuePic();

        std::vector<PicData> m_PicDataBuf;
        std::vector<PicData> m_PicSendingDataBuf;
        std::vector<PicData> m_PicACKedDataBuf;
        uint32_t m_NewestPicIndex;
        uint32_t m_SendingPicIndex;
        uint32_t m_NewestSentPicIndex;

        std::vector<PicData> m_DroppedPicDataBuf; // Dropped pic data because of sending timeout
        uint64_t m_TimeoutTime;                   // Timeout time for each pic ms

        DataRate m_dataRate;
        DataRate m_minDataRate;
        DataRate m_maxDataRate;
        DataRate m_stepDataRate;
        EventId m_sendEvent;
        bool m_running;
        uint32_t m_duration;
        uint64_t m_startTime; //ms
        SequenceNumberGenerator m_seqGen;
        bbr::SequenceNumberGenerator m_seqNumGen;

        /*--------------------------------------------------*/
        uint32_t m_pkt_gened;                       // total pkt gene
        uint32_t m_pic_seq;                         // increased pic seq

        std::shared_ptr<VideoCodecs::Codec> m_codec;//add
        EventId m_enqueueEvent;                     //add
        EventId m_sendOversleepEvent;               //add

        UdpBbrSender* m_sender;                     //add
        

        double m_rVin;                              //bps//add
        double m_rSend;                             //bps//add
        std::deque<Ptr<Packet>> m_PktBuf;           //add
        std::deque<uint32_t> m_rateShapingBuf;      //add
        uint32_t m_rateShapingBytes;                //add
        uint64_t m_nextSendTstmp;                   //add
    };


class UdpBbrSender : public Application
{
  public:

    struct PicSentInfo
    {
        PicSentInfo() : PicIndex(0),PicDataLen(0),PicAccessTime(0),PicAccessDelay(0)
        {
        }
        ~PicSentInfo() {}

        PacketNumber PicIndex;           // Global frame index for this picture
        uint16_t     PicDataLen;         // Frame Data len
        uint64_t     PicAccessTime;      // Current pkt first sent time
        uint64_t     PicAccessDelay;     // Current pkt first sent time
    };
    struct PicSentInfoSampleQueue
    {
        PicSentInfoSampleQueue() : MaxPicIndexSent(0),NewlyUpdateTime(0)
        {
        }
        ~PicSentInfoSampleQueue() {}

        PacketNumber MaxPicIndexSent;
        uint64_t     NewlyUpdateTime;
        std::deque<PicSentInfo> SampleQueue;
    };

    struct VideoStreamAccessDelayStatus{
        VideoStreamAccessDelayStatus()
                :InitProcessTime(0),LatestProcessTime(0),
                 InitProcessAccessDelay(0),LatestProcessAccessDelay(0),
                 MaxAcceleration(0.0),MinAcceleration(0.0),
                 MaxAccelerationContinueUpdateCount(0),
                 MinAccelerationContinueUpdateCount(0),
                 IsValid(false)
        {
        }
        ~VideoStreamAccessDelayStatus(){}

        uint64_t InitProcessTime;
        uint64_t InitProcessAccessDelay;
        uint64_t LatestProcessTime;
        uint64_t LatestProcessAccessDelay;
        float MaxAcceleration;
        float MinAcceleration;
        uint16_t MaxAccelerationContinueUpdateCount; //
        uint16_t MinAccelerationContinueUpdateCount; //
        bool IsValid;
    };

    static TypeId GetTypeId(void);

    UdpBbrSender();

    virtual ~UdpBbrSender();

    void SetRemote(Address ip, uint16_t port);

    void SetRemote(Address addr);

    void OnTimer();

    void TryToSendData();

    void OnTimerUpdateStreamStatus();

    float setTargetRate(float newRateBps);

  protected:
    virtual void DoDispose(void);

  private:
    const ConnectionStats& GetStats();

      // Called when a AckFrame has been parsed.
  void OnAckFrame(const AckFrame& frame);

    // Sets the retransmission alarm based on SentPacketManager.
  void SetRetransmissionAlarm();

  // Called when an RTO fires.  Resets the retransmission alarm if there are
  // remaining unacked packets.
  void OnRetransmissionTimeout();

  // send any retransmissions, return true if not data-limited
  bool SendRetransmissions();
  // send any queued packets, return true if not data-limited
  bool SendQueuedPackets();
  // 
  void HandleSend(PacketHeader &header);

  void SendStopWaitingFrame();

  void ConnectionSucceeded(Ptr<Socket> socket); // Called when the connections has succeeded
  void ConnectionFailed(Ptr<Socket> socket); // Called when the connection has failed.

  private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleRead(Ptr<Socket> socket);

    void OnAckPacket(const AckFrame &ack_frame);

  private:
    Ptr<Socket> m_socket;  //!< Socket
    Address m_from;
    Address m_peerAddress; //!< Remote peer address
    uint16_t m_peerPort;   //!< Remote peer port
    bool m_pending;
    Timer m_timer; //10ms
    Timer m_timer_updateStreamStatus; //1000ms

    bbr::SimpleAlarm m_resend_alarm;
    bbr::SentPacketManager *m_sentPacketManager;
    bbr::SequenceNumberGenerator m_seqNumGen;
    bbr::ConnectionStats stats_;
    size_t stop_waiting_count_;

    uint32_t m_size;  //!< Size of the sent packet (including the Header)
    uint32_t m_sent;  //!< Counter for sent packets
    Time m_startTime; //!<
    Time m_duration;  //!< Udp packet sending duration
    DataRate m_dataRate; //!< sending data rate;

    //Trace
    TracedValue<uint32_t> m_traceRtt;
    TracedValue<uint32_t> m_bytesInFlight;
    TracedValue<uint32_t> m_bandwidth;

    MyVideoCodec m_video_codec;
    //static
    uint64_t m_total_bytes_sent;
    uint64_t m_total_pkts_sent;

    // for advance control
    PicSentInfoSampleQueue          m_sampleQueue;
    VideoStreamAccessDelayStatus    m_streamStatus;

    uint32_t m_appId;
};
}

#endif
