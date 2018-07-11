/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef UDP_BBR_RECEIVER_H
#define UDP_BBR_RECEIVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/packet-loss-counter.h"
#include "ns3/core-module.h"

#include "packet-header.h"
#include "simple-alarm.h"
#include "packets.h"

namespace ns3
{
namespace bbr
{
class PacketHeader;
class ReceivedPacketManager;
}
class Packet;
class Socket;

using namespace bbr;

class UdpBbrReceiver : public Application
{
public:
  static TypeId GetTypeId(void);

  UdpBbrReceiver();
  virtual ~UdpBbrReceiver();

  uint32_t GetLost(void) const;
  uint64_t GetReceived(void) const;

  uint16_t GetPacketWindowSize() const;

  void SetPacketWindowSize(uint16_t size);

  void OnTimer();

protected:
  virtual void DoDispose(void);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);

  void OnStreamPacket(const PacketHeader &header, int size);
  void MaybeSendAck();
  void SendAck();

  Address m_from;
  uint16_t m_port;                 //!< Port on which we listen for incoming packets.
  Ptr<Socket> m_socket;            //!< IPv4 Socket
  Ptr<Socket> m_socket6;           //!< IPv6 Socket
  PacketLossCounter m_lossCounter; //!< Lost packet counter
  Timer m_timer; //10ms

  uint64_t m_received;             //!< Number of received packets
  // How many consecutive packets have arrived without sending an ack.
  uint32_t m_num_packets_received_since_last_ack_sent;

  ReceivedPacketManager *m_receivedPacketManager;
  bbr::SimpleAlarm m_ack_alarm;
};
}

#endif
