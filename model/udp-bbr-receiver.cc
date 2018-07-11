/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

#include "packet-header.h"
#include "udp-bbr-receiver.h"
#include "ack-frame.h"
#include "stop-waiting-frame.h"
#include "received-packet-manager.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("UdpBbrReceiverApplication");

NS_OBJECT_ENSURE_REGISTERED(UdpBbrReceiver);

TypeId
UdpBbrReceiver::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::UdpBbrReceiver")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpBbrReceiver>()
                            .AddAttribute("Port",
                                          "Port on which we listen for incoming packets.",
                                          UintegerValue(100),
                                          MakeUintegerAccessor(&UdpBbrReceiver::m_port),
                                          MakeUintegerChecker<uint16_t>());
    return tid;
}

UdpBbrReceiver::UdpBbrReceiver()
    : m_lossCounter(248),
      m_timer(Timer::REMOVE_ON_DESTROY),
      m_received(0),
      m_num_packets_received_since_last_ack_sent(0)
{
    NS_LOG_FUNCTION(this);
    m_timer.SetDelay(MilliSeconds(10));//10ms
    m_timer.SetFunction(&UdpBbrReceiver::OnTimer, this);
}

UdpBbrReceiver::~UdpBbrReceiver()
{
    NS_LOG_FUNCTION(this);
}

uint16_t
UdpBbrReceiver::GetPacketWindowSize() const
{
    NS_LOG_FUNCTION(this);
    return m_lossCounter.GetBitMapSize();
}

void UdpBbrReceiver::SetPacketWindowSize(uint16_t size)
{
    NS_LOG_FUNCTION(this << size);
    m_lossCounter.SetBitMapSize(size);
}

uint32_t
UdpBbrReceiver::GetLost(void) const
{
    NS_LOG_FUNCTION(this);
    return m_lossCounter.GetLost();
}

uint64_t
UdpBbrReceiver::GetReceived(void) const
{
    NS_LOG_FUNCTION(this);
    return m_received;
}

void UdpBbrReceiver::DoDispose(void)
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void UdpBbrReceiver::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_socket == 0)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(),
                                                    m_port);
        m_socket->Bind(local);
    }

    m_socket->SetRecvCallback(MakeCallback(&UdpBbrReceiver::HandleRead, this));

    if (m_socket6 == 0)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket6 = Socket::CreateSocket(GetNode(), tid);
        Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(),
                                                      m_port);
        m_socket6->Bind(local);
    }

    m_socket6->SetRecvCallback(MakeCallback(&UdpBbrReceiver::HandleRead, this));
    m_receivedPacketManager = new ReceivedPacketManager();
    m_timer.Schedule();
}

void UdpBbrReceiver::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket != 0)
    {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    m_timer.Cancel();
    delete m_receivedPacketManager;
}

void UdpBbrReceiver::OnTimer()
{
    uint64_t now_ms = Simulator::Now().GetMilliSeconds();
    if (m_ack_alarm.IsExpired(now_ms))
    {
        SendAck();
    }
    m_timer.Schedule();
}

void UdpBbrReceiver::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;

    while ((packet = socket->RecvFrom(m_from)))
    {
        int type = PeekPackeType(packet);
        int size = packet->GetSize();
        switch (type)
        {
        case kStreamPacket:
        {
            PacketHeader header;
            packet->RemoveHeader(header);
            OnStreamPacket(header, size);
            break;
        }
        case kStopWaiting:
        {
            StopWaitingFrame header;
            packet->RemoveHeader(header);
            m_receivedPacketManager->DontWaitForPacketsBefore(header.least_unacked);
            break;
        }
        default:
            NS_LOG_WARN("unsupported packet type: " << type);
        }
    }
}

void UdpBbrReceiver::OnStreamPacket(const PacketHeader &header, int size)
{
    uint32_t currentSequenceNumber = header.m_data_seq;
    uint64_t now = Simulator::Now().GetMilliSeconds();

    m_lossCounter.NotifyReceived(currentSequenceNumber);
    m_received++;
    ++m_num_packets_received_since_last_ack_sent;

    m_receivedPacketManager->RecordPacketReceived(header, now);

//    NS_LOG_INFO("RecvData " << this
//    << " Seq:("
//    << header.m_data_seq
//    << ", "
//    << header.m_packet_seq
//    << ") bytes "
//    << size
//    << " time "
//    << Simulator::Now().GetMilliSeconds()
//    << " RecvCount "
//    << m_received);



    std::cout<<"RecvData " << this
                            << " Seq:("
                            << header.m_data_seq
                            << ", "
                            << header.m_packet_seq
                            << ") bytes "
                            << size
                            << " time "
                            << Simulator::Now().GetMilliSeconds()
                            << " RecvCount "
                            << m_received
                            << " gen time "
                            << header.PicGenTime
                            << std::endl;

    //std::shared_ptr<PicDataPacket> pic_data_packet(new PicDataPacket());
    //pic_data_packet = header.m_data_packet;

//    uint8_t      PicType;        // Frame Type for this encoded picture
//    PacketNumber PicIndex;       // Global frame index for this picture
//    uint16_t     PicPktNum;      // How many pkt needed for transmit this picture
//    uint16_t     PicCurPktSeq;   // Current pkt seq for this pic
//    uint64_t     PicGenTime;     // Current pkt data len

//    header.PicType = data_packet->PicType;
//    header.PicIndex = data_packet->PicIndex;
//    header.PicPktNum = data_packet->PicPktNum;
//    header.PicCurPktSeq = data_packet->PicCurPktSeq;
//    header.PicGenTime = data_packet->PicGenTime;
    if(header.PicPktNum - header.PicCurPktSeq==1){
        std::cout<< "RcvSide PicIndex "<< header.PicIndex
                 << " PicPktNum "<< header.PicPktNum
                 << " PicCurPktSeq "<< header.PicCurPktSeq
                 << " PicGenTime "<< header.PicGenTime
                 << " PicRcvTime "<< Simulator::Now().GetMilliSeconds()
                 << " PicSize "<< header.PicDataLen
                 << " E2eDelay "<< Simulator::Now().GetMilliSeconds() - header.PicGenTime
                 << std::endl;
    }

    if (m_receivedPacketManager->ack_frame_updated())
    {
        MaybeSendAck();
    }
}

void UdpBbrReceiver::MaybeSendAck()
{
    bool should_send = false;
    if (m_received < kMinReceivedBeforeAckDecimation)
    {
        should_send = true;
    }
    else
    {
        if (m_num_packets_received_since_last_ack_sent >= kMaxRetransmittablePacketsBeforeAck)
        {
            should_send = true;
        }
        else if (!m_ack_alarm.IsSet())
        {
            uint64_t ack_delay = std::min(kMaxDelayedAckTimeMs, kMinRetransmissionTimeMs / 2);
            m_ack_alarm.Update(Simulator::Now().GetMilliSeconds()  + ack_delay);
        }
    }

    if (should_send)
    {
        SendAck();
    }
}

void UdpBbrReceiver::SendAck()
{
    m_num_packets_received_since_last_ack_sent = 0;

    const AckFrame *ack_frame = m_receivedPacketManager->GetUpdatedAckFrame(Simulator::Now().GetMilliSeconds());

    Ptr<Packet> p = Create<Packet>();
    p->AddHeader(*ack_frame);

    if ((m_socket->SendTo(p, 0, m_from)) >= 0)
    {
        NS_LOG_INFO("Send Ack: "
                    << *ack_frame
                    << " to address: "
                    << InetSocketAddress::ConvertFrom(m_from).GetIpv4());

        NS_LOG_INFO("Lost pkt num : " << m_lossCounter.GetLost());
    }
}
}
