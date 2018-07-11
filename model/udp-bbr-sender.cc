/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>

#include "packet-header.h"
#include "sent-packet-manager.h"
#include "udp-bbr-sender.h"
#include "ack-frame.h"
#include "stop-waiting-frame.h"

#include <math.h>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("UdpBbrSenderApplication");

NS_OBJECT_ENSURE_REGISTERED(UdpBbrSender);

    MyVideoCodec::MyVideoCodec(){
        auto innerCodec = new VideoCodecs::TraceBasedCodecWithScaling(TRACES_DIR_PATH, TRACES_FILE_PREFIX, SYNCODEC_DEFAULT_FPS);
        SetCodec(std::shared_ptr<VideoCodecs::Codec>{innerCodec});
    }

    float MyVideoCodec::setTargetRate(float newRateBps)
    {
        VideoCodecs::Codec& codec = *m_codec;
        //m_dataRate(newRateBps);
        return codec.setTargetRate(newRateBps);
        //std::cout << "Increase data rate to " << result / 1000 << " Kbps" << std::endl;
    }

    void MyVideoCodec::Setup(float fps, DataRate max_data_rate, DataRate min_data_rate, DataRate target_data_rate, DataRate step_data_rate, uint64_t timeout, UdpBbrSender* sender)
    {
        m_TimeoutTime = timeout;
        m_dataRate = target_data_rate;
        m_maxDataRate = max_data_rate;
        m_minDataRate = min_data_rate;
        m_stepDataRate = step_data_rate;
        m_pic_seq = 0;
        //Config::SetDefault ("ns3::UniformRandomVariable::Min", DoubleValue (min));
        //Config::SetDefault ("ns3::UniformRandomVariable::Max", DoubleValue (max));
        //m_rng = CreateObject<UniformRandomVariable> ();
/*----------------------------------------------------------*/
        m_pkt_gened = 0;
        m_enqueueEvent = EventId();
        m_sendOversleepEvent = EventId();
        m_rVin = 0.0;
        m_rSend = 0.0;
        m_rateShapingBytes = 0;
        m_nextSendTstmp = 0;
        m_pic_seq = 0;

        m_sender = sender;

//        auto innerCodec = new VideoCodecs::TraceBasedCodec(TRACES_DIR_PATH, TRACES_FILE_PREFIX, fps);
//        float result = innerCodec->setTargetRate(m_dataRate.GetBitRate());
//        std::cout << "Accepted rate " << result / 1000 << " Kbps" << std::endl;
//        SetCodec(std::shared_ptr<VideoCodecs::Codec>{innerCodec});

        float result = setTargetRate(m_dataRate.GetBitRate()*1.0);
        std::cout << "Accepted rate " << result / 1000 << " Kbps" << std::endl;
    }

    void MyVideoCodec::StartApp()
    {
        if (!m_running)
        {
            m_running = true;
            m_startTime = Simulator::Now().GetMilliSeconds();

            EnqueuePic();
        }
    }

    void MyVideoCodec::StopApp()
    {
        m_running = false;
        if (m_enqueueEvent.IsRunning())
        {
            Simulator::Cancel(m_enqueueEvent);
        }
    }

    bool MyVideoCodec::GetRedundantPacket(PicDataPacket &data)
    {
        int size = DEFAULT_PAYLOAD_SIZE;
        data.data_seq = m_seqGen.NextSeq();
        data.data_length = size;
        data.priority = bbr::ProtocolSendPriority::P0;
        data.expire_time = Simulator::Now().GetMilliSeconds() + 10000;
        data.payload.assign(size, 'X');

        data.PicType = pic_type_fake;
        data.PicIndex = 1<<64 - 1;//uint64_t
        data.PicDataLen = size;
        data.PicPktNum = 2;             // Cant be the last one 
        data.PicCurPktSeq = 0;          // Cant be the last one 
        data.PicGenTime = Simulator::Now().GetMilliSeconds();

        return true;
    }

    bool MyVideoCodec::GetNextPacket(PicDataPacket &data)
    {
        if(m_PicDataBuf.size() && m_PicDataBuf.front().PktDataLen.size()){ // Has pic data to send
            int size = m_PicDataBuf.front().PktDataLen.front();
            data.data_seq = m_seqGen.NextSeq();
            data.data_length = size;
            data.priority = bbr::ProtocolSendPriority::P0;
            data.expire_time = Simulator::Now().GetMilliSeconds() + 10000;
            data.payload.assign(size, 'P');

            data.PicType = m_PicDataBuf.front().CurType;
            data.PicIndex = m_PicDataBuf.front().PicSeq;
            data.PicDataLen = m_PicDataBuf.front().PicDataLen;
            data.PicPktNum = m_PicDataBuf.front().PicPktNum;
            data.PicCurPktSeq = m_PicDataBuf.front().PicPktNum - m_PicDataBuf.front().PktDataLen.size();
            data.PicGenTime = m_PicDataBuf.front().PicGenTime;

            m_PicDataBuf.front().PktDataLen.erase(m_PicDataBuf.front().PktDataLen.begin());
            if(m_PicDataBuf.front().PktDataLen.size() == 0){// pkt send over,my be do some statics
                m_PicSendingDataBuf.push_back(m_PicDataBuf.front());
                m_PicDataBuf.erase(m_PicDataBuf.begin());

                //std::cout <<"Sent pic num "<< m_PicSendingDataBuf.size() << std::endl;
                //std::cout <<"Left pic num "<< m_PicDataBuf.size() << std::endl;
            }
            return true;
        }else{
            return false;
        }
    }

    void MyVideoCodec::SetCodec (std::shared_ptr<VideoCodecs::Codec> codec)
    {
        m_codec = codec;
    }

    void MyVideoCodec::SetCodecType (SyncodecType codecType)
    {
        VideoCodecs::Codec* codec = NULL;
        switch (codecType) {
            case SYNCODEC_TYPE_PERFECT:
            {
                codec = new VideoCodecs::PerfectCodec{DEFAULT_PACKET_SIZE};
                break;
            }
            case SYNCODEC_TYPE_FIXFPS:
            {
                const auto fps = SYNCODEC_DEFAULT_FPS;
                auto innerCodec = new VideoCodecs::SimpleFpsBasedCodec{fps};
                codec = new VideoCodecs::ShapedPacketizer{innerCodec, DEFAULT_PACKET_SIZE};
                break;
            }
            case SYNCODEC_TYPE_STATS:
            {
                const auto fps = SYNCODEC_DEFAULT_FPS;
                auto innerStCodec = new VideoCodecs::StatisticsCodec{fps};
                codec = new VideoCodecs::ShapedPacketizer{innerStCodec, DEFAULT_PACKET_SIZE};
                break;
            }
            case SYNCODEC_TYPE_TRACE:
            case SYNCODEC_TYPE_HYBRID:
            {
                const std::vector<std::string> candidatePaths = {
                        ".",      // If run from top directory (e.g., with gdb), from ns-3.26/
                        "../",    // If run from with test_new.py with designated directory, from ns-3.26/2017-xyz/
                        "../..",  // If run with test.py, from ns-3.26/testpy-output/201...
                };

                const std::string traceSubDir{"src/bbr/model/videocodecs/video_traces/chat_firefox_h264"};
                std::string traceDir{};

                for (auto c : candidatePaths) {
                    std::ostringstream currPathOss;
                    currPathOss << c << "/" << traceSubDir;
                    struct stat buffer;
                    if (::stat (currPathOss.str ().c_str (), &buffer) == 0) {
                        //filename exists
                        traceDir = currPathOss.str ();
                        break;
                    }
                }

                NS_ASSERT_MSG (!traceDir.empty (), "Traces file not found in candidate paths");

                auto filePrefix = "chat";
                auto innerCodec = (codecType == SYNCODEC_TYPE_TRACE) ?
                                  new VideoCodecs::TraceBasedCodecWithScaling{
                                          traceDir,        // path to traces directory
                                          filePrefix,      // video filename
                                          SYNCODEC_DEFAULT_FPS,             // Default FPS: 30fps
                                          true} :          // fixed mode: image resolution doesn't change
                                  new VideoCodecs::HybridCodec{
                                          traceDir,        // path to traces directory
                                          filePrefix,      // video filename
                                          SYNCODEC_DEFAULT_FPS,             // Default FPS: 30fps
                                          true};           // fixed mode: image resolution doesn't change

                codec = new VideoCodecs::ShapedPacketizer{innerCodec, DEFAULT_PACKET_SIZE};
                break;
            }
            case SYNCODEC_TYPE_SHARING:
            {
                auto innerShCodec = new VideoCodecs::SimpleContentSharingCodec{};
                codec = new VideoCodecs::ShapedPacketizer{innerShCodec, DEFAULT_PACKET_SIZE};
                break;
            }
            default:  // defaults to perfect codec
                codec = new VideoCodecs::PerfectCodec{DEFAULT_PACKET_SIZE};
        }

        // update member variable
        m_codec = std::shared_ptr<VideoCodecs::Codec>{codec};
    }

    void MyVideoCodec::EnqueuePic()
    {
        uint64_t now = Simulator::Now().GetMilliSeconds();
        VideoCodecs::Codec& codec = *m_codec;

        const auto bytesToSend = codec->first.size ();
        ++codec; // Advance codec/packetizer to next frame/packet
        //std::cout <<"bytesToSend:------------------- "<< bytesToSend<< std::endl;
        NS_ASSERT (bytesToSend > 0);
        uint16_t left_len = bytesToSend;

        uint16_t pic_total_pkts = static_cast<uint16_t>(ceil(bytesToSend*1.0/DEFAULT_PACKET_SIZE));
        uint16_t pic_cur_seq = 0;

        PicData pic_data;
        pic_data.CurType = pic_type_real;
        pic_data.PicSeq = m_pic_seq;
        pic_data.PicGenTime = now;
        pic_data.PicExpireTime = now + m_TimeoutTime;
        pic_data.PicDataLen = bytesToSend;
        pic_data.PicPktNum = pic_total_pkts;

        while(left_len > 0){// Generate pkt from pic data
            //std::cout <<"left len: "<< left_len<< std::endl;
            if(left_len > DEFAULT_PAYLOAD_SIZE){
                pic_data.PktDataLen.push_back(DEFAULT_PAYLOAD_SIZE);
                left_len -= DEFAULT_PAYLOAD_SIZE;
            }else{
                pic_data.PktDataLen.push_back(left_len);
                left_len = 0;
            }
        }
        m_pic_seq++;
        m_PicDataBuf.push_back(pic_data);// queued new-frame

        //NS_ASSERT (bytesToSend <= DEFAULT_PACKET_SIZE);
        m_rateShapingBuf.push_back (bytesToSend);
        m_rateShapingBytes += bytesToSend;

//      NS_LOG_INFO ("MyVideoCodec::EnqueuePic, pic enqueued, pic length: " << bytesToSend
//                                                                          << ", buffer size: " << m_rateShapingBuf.size ()
//                                                                          << ", buffer bytes: " << m_rateShapingBytes);
        auto secsToNextEnqPic = codec->second;
        Time tNext{Seconds (secsToNextEnqPic)};
        m_enqueueEvent = Simulator::Schedule (tNext, &MyVideoCodec::EnqueuePic, this);
        
        m_sender->TryToSendData();
    }

    uint64_t MyVideoCodec::GetCurMaxPicQueueDelay(uint64_t now){

        if(m_PicDataBuf.size() > 0 ){// timeout event occur
            if(m_PicDataBuf.front().PicGenTime > now){
                return 0;
            }else{
                return (now - m_PicDataBuf.front().PicGenTime);
            }
        }else{
            return 0;
        }
    }

TypeId
UdpBbrSender::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::UdpBbrSender")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpBbrSender>()
                            .AddAttribute("Duration",
                                          "Packet Sending Duration",
                                          TimeValue(Seconds(10)),
                                          MakeTimeAccessor(&UdpBbrSender::m_duration),
                                          MakeTimeChecker())
                            .AddAttribute("RemoteAddress",
                                          "The destination Address of the outbound packets",
                                          AddressValue(),
                                          MakeAddressAccessor(&UdpBbrSender::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("RemotePort", "The destination port of the outbound packets",
                                          UintegerValue(100),
                                          MakeUintegerAccessor(&UdpBbrSender::m_peerPort),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("PacketSize",
                                          "Size of packets generated. The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&UdpBbrSender::m_size),
                                          MakeUintegerChecker<uint32_t>(12, 1500))
                            .AddAttribute("DataRate",
                                          "Sending data rate",
                                          DataRateValue(DataRate("1Mib/s")),
                                          MakeDataRateAccessor(&UdpBbrSender::m_dataRate),
                                          MakeDataRateChecker())
                            .AddTraceSource("Rtt",
                                            "round trip time",
                                            MakeTraceSourceAccessor(&UdpBbrSender::m_traceRtt),
                                            "ns3::TracedValueCallback::Uint32")
                            .AddTraceSource("BytesInFlight",
                                            "Bytes in flight",
                                            MakeTraceSourceAccessor(&UdpBbrSender::m_bytesInFlight),
                                            "ns3::TracedValueCallback::Uint32")
                            .AddTraceSource("Bandwidth",
                                            "Bandwidth",
                                            MakeTraceSourceAccessor(&UdpBbrSender::m_bandwidth),
                                            "ns3::TracedValueCallback::Uint32");
    ;

    return tid;
}

static bool app_onoff = false;

UdpBbrSender::UdpBbrSender()
: m_timer(Timer::REMOVE_ON_DESTROY),
  stop_waiting_count_(0)
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_socket = 0;
    m_pending = false;
    m_startTime = Seconds(0);
    m_traceRtt = 0;
    m_bytesInFlight = 0;
    m_bandwidth = 0;

    //m_timer.SetDelay(MilliSeconds(1));//10ms
    //m_timer.SetFunction(&UdpBbrSender::OnTimer, this);
    m_video_codec.Setup(30. , DataRate("2.0Mb/s"), DataRate("2.0Mb/s"), DataRate("0.2Mb/s"), DataRate("0.2Mb/s"), 200, this);
    m_total_bytes_sent = 0;
    m_total_pkts_sent = 0;

    m_sampleQueue = PicSentInfoSampleQueue();
    m_streamStatus = VideoStreamAccessDelayStatus();

    //m_timer_updateStreamStatus.SetDelay(MilliSeconds(100));//100ms
    //m_timer_updateStreamStatus.SetFunction(&UdpBbrSender::OnTimerUpdateStreamStatus, this);
}

UdpBbrSender::~UdpBbrSender()
{
    NS_LOG_FUNCTION(this);
}

void UdpBbrSender::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddress = ip;
    m_peerPort = port;
}

void UdpBbrSender::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddress = addr;
}

void UdpBbrSender::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

float UdpBbrSender::setTargetRate(float newRateBps){
    return m_video_codec.setTargetRate(newRateBps);
}

void UdpBbrSender::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_socket == 0)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
            m_socket->Bind();
            m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
            m_socket->Bind6();
            m_socket->Connect(Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
        {
            m_socket->Bind();
            m_socket->Connect(m_peerAddress);
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true)
        {
            m_socket->Bind6();
            m_socket->Connect(m_peerAddress);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
    }

    m_socket->SetRecvCallback(MakeCallback(&UdpBbrSender::HandleRead, this));
    m_socket->SetAllowBroadcast(true);

    m_sentPacketManager = new bbr::SentPacketManager(&stats_, bbr::kBBR, bbr::kAdaptiveTime);
    //m_timer.Schedule();
    //m_timer_updateStreamStatus.Schedule();

    m_video_codec.StartApp();
}

void UdpBbrSender::StopApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_socket != 0)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket = 0;
    }
    //m_timer.Cancel();
    delete m_sentPacketManager;

    m_video_codec.StopApp();
}

    void UdpBbrSender::OnTimerUpdateStreamStatus()
    {
        return;
        std::cout << "In  UdpBbrSender::OnTimerUpdateStreamStatus()" <<  std::endl;
        uint64_t now_ms = Simulator::Now().GetMilliSeconds();
        //time-based
        if(m_sampleQueue.SampleQueue.size() > 25){ // has samples
            int sampleNum = m_sampleQueue.SampleQueue.size();
            int64_t ProcessTime = m_sampleQueue.NewlyUpdateTime;
            int64_t ProcessAccessDelay = 0;
            while (!m_sampleQueue.SampleQueue.empty()){
                ProcessAccessDelay += m_sampleQueue.SampleQueue.front().PicAccessDelay;
                m_sampleQueue.SampleQueue.pop_front();
            }
            //std::cout << "ProcessAccessDelay1 " << ProcessAccessDelay << "sampleNum " << sampleNum << std::endl;
            ProcessAccessDelay /= sampleNum;
            std::cout << "ProcessAccessDelay " << ProcessAccessDelay << "sampleNum " << sampleNum << std::endl;
            if(m_streamStatus.IsValid){// Common process
                float Acceleration = (ProcessAccessDelay - (int64_t)m_streamStatus.InitProcessAccessDelay)*1.0/
                                     (ProcessTime - (int64_t)m_streamStatus.InitProcessTime);

                std::cout << "ProcessAccessDelay - m_streamStatus.InitProcessAccessDelay " << ProcessAccessDelay - (int64_t)m_streamStatus.InitProcessAccessDelay << std::endl;
                std::cout << "ProcessTime - m_streamStatus.InitProcessTime " << ProcessTime - (int64_t)m_streamStatus.InitProcessTime << std::endl;

                std::cout << "Acceleration " << Acceleration << std::endl;
                std::cout << "m_streamStatus.MaxAcceleration " << m_streamStatus.MaxAcceleration << std::endl;
                std::cout << "m_streamStatus.MinAcceleration " << m_streamStatus.MinAcceleration << std::endl;
                std::cout << "m_streamStatus.MaxAccelerationContinueUpdateCount " << m_streamStatus.MaxAccelerationContinueUpdateCount << std::endl;
                std::cout << "m_streamStatus.MinAccelerationContinueUpdateCount " << m_streamStatus.MinAccelerationContinueUpdateCount << std::endl;
                std::cout << "m_streamStatus.InitProcessAccessDelay " << m_streamStatus.InitProcessAccessDelay <<  std::endl;
                std::cout << "m_streamStatus.InitProcessTime " << m_streamStatus.InitProcessTime <<  std::endl;

                if(Acceleration > m_streamStatus.MaxAcceleration){
                    m_streamStatus.MaxAcceleration = Acceleration;
                    m_streamStatus.MaxAccelerationContinueUpdateCount ++;
                    m_streamStatus.MinAccelerationContinueUpdateCount = 0;

                    //m_streamStatus.InitProcessAccessDelay = m_streamStatus.LatestProcessAccessDelay;
                    //m_streamStatus.InitProcessTime = m_streamStatus.LatestProcessTime;

                    m_streamStatus.MaxAcceleration = 0.0;

                    if(m_streamStatus.MaxAccelerationContinueUpdateCount > 2){
                        float bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond()*1.0;
                        float result = setTargetRate(bandwidth - 100000.0);
                        //float result = setTargetRate(bandwidth);
                        std::cout << "Decrease data rate to " << result / 1000 << " Kbps" << std::endl;
                        m_streamStatus.MaxAccelerationContinueUpdateCount = 0;
                        //m_streamStatus.MaxAcceleration = 0.0;
                    }
                } else if(Acceleration < m_streamStatus.MinAcceleration){
                    m_streamStatus.MinAcceleration = Acceleration;
                    m_streamStatus.MaxAccelerationContinueUpdateCount = 0;
                    m_streamStatus.MinAccelerationContinueUpdateCount ++;

                    //m_streamStatus.InitProcessAccessDelay = m_streamStatus.LatestProcessAccessDelay;
                    //m_streamStatus.InitProcessTime = m_streamStatus.LatestProcessTime;

                    if(m_streamStatus.MinAccelerationContinueUpdateCount > 2){
                        float bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond()*1.0;
                        float result = setTargetRate(bandwidth);
                        std::cout << "Increase 2 data rate to " << result / 1000 << " Kbps" << std::endl;
                    }
                }else{
                    m_streamStatus.MaxAccelerationContinueUpdateCount = 0;
                    m_streamStatus.MinAccelerationContinueUpdateCount = 0;

                    m_streamStatus.InitProcessAccessDelay = m_streamStatus.LatestProcessAccessDelay;
                    m_streamStatus.InitProcessTime = m_streamStatus.LatestProcessTime;
                }

            }else{// Init
                m_streamStatus.InitProcessAccessDelay = ProcessAccessDelay;
                m_streamStatus.InitProcessTime = m_sampleQueue.NewlyUpdateTime;

                std::cout << "m_streamStatus.InitProcessAccessDelay " << m_streamStatus.InitProcessAccessDelay <<  std::endl;
                std::cout << "m_streamStatus.InitProcessTime " << m_streamStatus.InitProcessTime <<  std::endl;

                m_streamStatus.MinAcceleration = 100000;//100s
                m_streamStatus.MaxAcceleration = 0;

                m_streamStatus.IsValid = true;

                std::cout << "In  UdpBbrSender::OnTimerUpdateStreamStatus() init" <<  std::endl;
            }
            m_streamStatus.LatestProcessAccessDelay = ProcessAccessDelay;
            m_streamStatus.LatestProcessTime = m_sampleQueue.NewlyUpdateTime;
        }
        m_timer_updateStreamStatus.Schedule();
    }

void UdpBbrSender::OnTimer()
{
    uint64_t now_ms = Simulator::Now().GetMilliSeconds();
    //time-based
    if (m_resend_alarm.IsExpired(now_ms))
    {
        OnRetransmissionTimeout();
    }

    bool unlimited = SendRetransmissions();

    //
    unlimited |= SendQueuedPackets();

    if (!unlimited)
    {
        m_sentPacketManager->OnApplicationLimited();
        // Get current bandwidth and set it to codec
        float bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond()*1.0;
        //float result = setTargetRate(bandwidth + 100000);
        // if(bandwidth > 100*1000.0 && bandwidth <= 600*1000.0){
        //     bandwidth = 600*1000.0;
        // }else if(bandwidth > 600*1000.0 && bandwidth <= 700*1000.0){
        //     bandwidth = 700*1000.0;
        // }else if(bandwidth > 700*1000.0 && bandwidth <= 800*1000.0){
        //     bandwidth = 800*1000.0;
        // }else if(bandwidth > 800*1000.0 && bandwidth <= 1000*1000.0){
        //     bandwidth = 1000*1000.0;
        // }else if(bandwidth > 1000*1000.0 && bandwidth <= 1200*1000.0){
        //     bandwidth = 1200*1000.0;
        // }else if(bandwidth > 1200*1000.0 && bandwidth <= 1500*1000.0){
        //     bandwidth = 1500*1000.0;
        // }else{
        //     bandwidth = 2000*1000.0;
        // }

        float result = setTargetRate(bandwidth + 100*1000.0);
        std::cout << "Increase data rate to " << result / 1000 << " Kbps" << std::endl;
    }

    if (!m_resend_alarm.IsSet())
    {
        SetRetransmissionAlarm();
    }

    if (stop_waiting_count_ > kStopWaitingThreshold)
    {
        SendStopWaitingFrame();
        stop_waiting_count_ = 0;
    }

    m_timer.Schedule();
}

void UdpBbrSender::TryToSendData(){
    uint64_t now_ms = Simulator::Now().GetMilliSeconds();
    //time-based
    if (m_resend_alarm.IsExpired(now_ms))
    {
        OnRetransmissionTimeout();
    }

    bool unlimited = SendRetransmissions();

    //
    unlimited |= SendQueuedPackets();

    if (!unlimited)
    {
        m_sentPacketManager->OnApplicationLimited();
        float bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond()*1.0;
        float result = setTargetRate(bandwidth + 100*1000.0);
        std::cout << "Increase data rate to " << result / 1000 << " Kbps" << std::endl;
    }

    if (!m_resend_alarm.IsSet())
    {
        SetRetransmissionAlarm();
    }

    if (stop_waiting_count_ > kStopWaitingThreshold)
    {
        SendStopWaitingFrame();
        stop_waiting_count_ = 0;
    }
}


void UdpBbrSender::SetRetransmissionAlarm()
{
    uint64_t retransmission_time = m_sentPacketManager->GetRetransmissionTime();
    m_resend_alarm.Update(retransmission_time);
}

void UdpBbrSender::OnRetransmissionTimeout()
{
    size_t n = m_sentPacketManager->GetConsecutiveRtoCount();
    NS_ASSERT_MSG(n < 3, "consecutive retransmission timeouts: " << n);

    m_sentPacketManager->OnRetransmissionTimeout();

    m_sentPacketManager->MaybeRetransmitTailLossProbe();
}

bool UdpBbrSender::SendRetransmissions()
{
    bool unlimited = true;
    while (!m_sentPacketManager->TimeUntilSend(Simulator::Now().GetMilliSeconds()))
    {
        if (m_sentPacketManager->HasPendingRetransmissions())
        {
            bbr::PacketHeader pending = m_sentPacketManager->NextPendingRetransmission();
            pending.m_packet_seq = m_seqNumGen.NextSeq();
            pending.m_sent_time = Simulator::Now().GetMilliSeconds();

            std::cout<< "Retransmit PicIndex "<< pending.m_data_packet->PicIndex
                     << " PicPktNum "<< pending.m_data_packet->PicPktNum
                     << " PicCurPktSeq "<< pending.m_data_packet->PicCurPktSeq
                     << " PicGenTime "<< pending.m_data_packet->PicGenTime
                     << " PicSentTime "<< Simulator::Now().GetMilliSeconds()
                     << " PicSize "<< pending.m_data_packet->PicDataLen
                     << " AccessDelay "<< Simulator::Now().GetMilliSeconds() - pending.m_data_packet->PicGenTime
                     << std::endl;

            HandleSend(pending);
        }
        else
        {
            unlimited = false;
            break;
        }
    }
    return unlimited;
}

bool UdpBbrSender::SendQueuedPackets()
{
    // First check if queued data has time out
    uint64_t min_access_delay = m_video_codec.GetCurMaxPicQueueDelay(Simulator::Now().GetMilliSeconds());
    if(min_access_delay > 200){// Decrease codec output rate
        // Get current bandwidth and set it to codec
        float bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond()*1.0;

//        if(bandwidth > 800*1000.0 && bandwidth <= 1000*1000.0){
//            bandwidth = 1000*1000.0;
//        }else if(bandwidth > 1000*1000.0 && bandwidth <= 1200*1000.0){
//            bandwidth = 1200*1000.0;
//        }else{
//            bandwidth = 1500*1000.0;
//        }

        // if(bandwidth > 600*1000.0 && bandwidth <= 700*1000.0){
        //     bandwidth = 600*1000.0;
        // }else if(bandwidth > 700*1000.0 && bandwidth <= 800*1000.0){
        //     bandwidth = 700*1000.0;
        // }else if(bandwidth > 800*1000.0 && bandwidth <= 1000*1000.0){
        //     bandwidth = 800*1000.0;
        // }else if(bandwidth > 1000*1000.0 && bandwidth <= 1200*1000.0){
        //     bandwidth = 1000*1000.0;
        // }else if(bandwidth > 1200*1000.0 && bandwidth <= 1500*1000.0){
        //     bandwidth = 1200*1000.0;
        // }else if(bandwidth > 1500*1000.0 && bandwidth <= 2000*1000.0){
        //     bandwidth = 1500*1000.0;
        // }else{
        //     bandwidth = 2000*1000.0;
        // }

        // //bandwidth = 920*1000.0;
        float result = setTargetRate(bandwidth - 100*1000.0);
        // //float result = setTargetRate(bandwidth - 100000);
        std::cout << "Decrease data rate to " << bandwidth / 1000 << " Kbps" << std::endl;
    }

    bool unlimited = true;
    while(!m_sentPacketManager->TimeUntilSend(Simulator::Now().GetMilliSeconds()))
    {
        //std::shared_ptr<DataPacket> data_packet(new DataPacket());
        std::shared_ptr<PicDataPacket> data_packet(new PicDataPacket());
        //bool got = m_app.GetNextPacket(*data_packet);
        bool update_data_rate = false;
        //bool got = m_video_codec.GetNextPacket(*data_packet);
        //if (got) // get real or fake data
        if (true)
        {
            bbr::PacketHeader header;
            header.m_packet_seq = m_seqNumGen.NextSeq();
            header.m_old_packet_seq = 0;
            header.m_transmission_type = bbr::NOT_RETRANSMISSION;
            header.m_sent_time = Simulator::Now().GetMilliSeconds();
            //header.m_data_length = data_packet->data_length;
            header.m_data_length = DEFAULT_PAYLOAD_SIZE;
            header.m_data_packet = data_packet;
            //header.m_data_seq = data_packet->data_seq;
            header.m_data_seq = header.m_packet_seq;


            //header.PicType = data_packet->PicType;
            header.PicType = pic_type_real;
            header.PicIndex = data_packet->PicIndex;
            header.PicDataLen = data_packet->PicDataLen;
            header.PicPktNum = data_packet->PicPktNum;
            header.PicCurPktSeq = data_packet->PicCurPktSeq;
            header.PicGenTime = data_packet->PicGenTime;
            //
            HandleSend(header);
            ;
        }
        else
        {
            // std::cout << "Applimited !" << std::endl;
            // if(!update_data_rate){
            //     update_data_rate = true;

            //     float bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond()*1.0;
            //     if(bandwidth<= 2000*1000.0){// less than max data rate
            //         float result = setTargetRate(bandwidth);
            //         std::cout << "Increase 2 data rate to " << bandwidth / 1000 << " Kbps" << std::endl;
            //     }else{
            //         return false;
            //     }
            // }
            // m_video_codec.GetRedundantPacket(*data_packet);

            unlimited = false;
            break;
        }

        // bbr::PacketHeader header;
        // header.m_packet_seq = m_seqNumGen.NextSeq();
        // header.m_old_packet_seq = 0;
        // header.m_transmission_type = bbr::NOT_RETRANSMISSION;
        // header.m_sent_time = Simulator::Now().GetMilliSeconds();
        // header.m_data_length = data_packet->data_length;
        // //header.m_data_length = DEFAULT_PAYLOAD_SIZE;
        // header.m_data_packet = data_packet;
        // header.m_data_seq = data_packet->data_seq;

        // header.PicType = data_packet->PicType;
        // //header.PicType = pic_type_real;
        // header.PicIndex = data_packet->PicIndex;
        // header.PicDataLen = data_packet->PicDataLen;
        // header.PicPktNum = data_packet->PicPktNum;
        // header.PicCurPktSeq = data_packet->PicCurPktSeq;
        // header.PicGenTime = data_packet->PicGenTime;
        // //
        // HandleSend(header);
        //unlimited = false;
        //break;
    }

    return unlimited;
}

void UdpBbrSender::HandleSend(PacketHeader &header)
{
    Ptr<Packet> packet = Create<Packet>(header.m_data_length);
    packet->AddHeader(header);

    //send
    std::stringstream peerAddressStringStream;
    peerAddressStringStream << Ipv4Address::ConvertFrom(m_peerAddress);
    if ((m_socket->Send(packet)) >= 0)
    {
//        NS_LOG_INFO("SendData " << this
//                                << " Seq:("
//                                << header.m_data_seq
//                                << ", "
//                                << header.m_packet_seq
//                                << ") bytes "
//                                << packet->GetSize()
//                                << " time "
//                                << Simulator::Now().GetMilliSeconds()
//                                << " type "
//                                << int(header.m_transmission_type));
        if(header.m_data_packet&&
           header.m_data_packet->PicPktNum -
           header.m_data_packet->PicCurPktSeq == 1){ //The last pkt for one frame
            std::cout<< "SenderSide PicIndex "<< header.m_data_packet->PicIndex
                     << " PicPktNum "<< header.m_data_packet->PicPktNum
                     << " PicCurPktSeq "<< header.m_data_packet->PicCurPktSeq
                     << " PicGenTime "<< header.m_data_packet->PicGenTime
                     << " PicSentTime "<< Simulator::Now().GetMilliSeconds()
                     << " PicSize "<< header.PicDataLen
                     << " AccessDelay "<< Simulator::Now().GetMilliSeconds() - header.m_data_packet->PicGenTime
                     << std::endl;


//            if(header.PicIndex >= m_sampleQueue.MaxPicIndexSent){// not used the retransmit time
//                PicSentInfo newSample = PicSentInfo();
//                newSample.PicIndex = header.PicIndex;
//                newSample.PicDataLen = header.PicDataLen;
//                newSample.PicAccessDelay = Simulator::Now().GetMilliSeconds() - header.m_data_packet->PicGenTime;
//                newSample.PicAccessTime = Simulator::Now().GetMilliSeconds();
//
//                //
//                m_sampleQueue.SampleQueue.push_back(newSample);
//                m_sampleQueue.MaxPicIndexSent = header.PicIndex;
//                m_sampleQueue.NewlyUpdateTime = Simulator::Now().GetMilliSeconds();
//
//                std::cout<< "SenderSide PicIndex "<< header.m_data_packet->PicIndex
//                         << " PicPktNum "<< header.m_data_packet->PicPktNum
//                         << " PicCurPktSeq "<< header.m_data_packet->PicCurPktSeq
//                         << " PicGenTime "<< header.m_data_packet->PicGenTime
//                         << " PicSentTime "<< Simulator::Now().GetMilliSeconds()
//                         << " PicSize "<< header.PicDataLen
//                         << " AccessDelay "<< Simulator::Now().GetMilliSeconds() - header.m_data_packet->PicGenTime
//                         << std::endl;
//
//
//            }
        }

        std::cout<<"SendData " << this
                                << " Seq:("
                                << header.m_data_seq
                                << ", "
                                << header.m_packet_seq
                                << ") bytes "
                                << packet->GetSize()
                                << " time "
                                << Simulator::Now().GetMilliSeconds()
                                << " type "
                                << int(header.m_transmission_type)
                                << " gen time "
                                << header.m_data_packet->PicGenTime
                                <<std::endl;
    }
    else
    {
        NS_LOG_INFO("Error while sending " << packet->GetSize() << " bytes to "  << peerAddressStringStream.str());
    }
    bool reset_alarm = m_sentPacketManager->OnPacketSent(header,
                                                         header.m_old_packet_seq,
                                                         Simulator::Now().GetMilliSeconds(),
                                                         header.m_transmission_type,
                                                         HAS_RETRANSMITTABLE_DATA);
    if (reset_alarm || !m_resend_alarm.IsSet())
    {
        SetRetransmissionAlarm();
    }

    stats_.bytes_sent += header.m_data_length;
    ++stats_.packets_sent;
    if (header.m_transmission_type != NOT_RETRANSMISSION)
    {
        stats_.bytes_retransmitted += header.m_data_length;
        ++stats_.packets_retransmitted;
    }
}

void UdpBbrSender::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(m_from)))
    {
        int type = PeekPackeType(packet);
        switch (type)
        {
        case kAckPacket:
        {
            AckFrame ack_frame;
            packet->RemoveHeader(ack_frame);
            OnAckPacket(ack_frame);
            // we need plag data from buffer
            TryToSendData();
            break;
        }
        default:
            NS_LOG_WARN("unsupported packet type: " << type);
        }
    }
}

void UdpBbrSender::SendStopWaitingFrame()
{
    StopWaitingFrame frame;
    frame.least_unacked = m_sentPacketManager->GetLeastUnacked();
    Ptr<Packet> packet = Create<Packet>();
    packet->AddHeader(frame);

    if ((m_socket->Send(packet)) >= 0)
    {
        NS_LOG_INFO("send stop-waiting " << frame.least_unacked);
    }
    else
    {
        NS_LOG_INFO("send stop-waiting error " << frame.least_unacked);
    }
}

void UdpBbrSender::OnAckPacket(const AckFrame &ack_frame)
{
    uint64_t now = Simulator::Now().GetMilliSeconds();
    m_sentPacketManager->OnIncomingAck(ack_frame, now);
    SetRetransmissionAlarm();

    if (!ack_frame.packets.Empty() && m_sentPacketManager->GetLeastUnacked() > ack_frame.packets.Min())
    {
        ++stop_waiting_count_;
    }
    else 
    {
        stop_waiting_count_ = 0;
    }

    m_traceRtt = m_sentPacketManager->GetRttStats()->latest_rtt();
    m_bytesInFlight = m_sentPacketManager->GetBytesInFlight();
    m_bandwidth = m_sentPacketManager->BandwidthEstimate().ToBitsPerSecond();

    NS_LOG_INFO("At time " << Simulator::Now().GetSeconds()
                           << " LeastUnacked " << m_sentPacketManager->GetLeastUnacked()
                           << " ack " << ack_frame);
    NS_LOG_INFO("OnAck " << this
                         << " time "
                         << Simulator::Now().GetMilliSeconds()
                         << " m_traceRtt "
                         << m_traceRtt
                         << " m_bytesInFlight "
                         << m_bytesInFlight
                         << " m_bandwidth "
                         << m_bandwidth);

    std::cout<< "Sender" << " time "
                         << Simulator::Now().GetMilliSeconds()
                         << " Rtt "
                         << m_traceRtt
                         << " Bif "
                         << m_bytesInFlight
                         << " Bw "
                         << m_bandwidth
                         << std::endl;
}
}
