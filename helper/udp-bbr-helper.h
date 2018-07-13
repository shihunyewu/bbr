/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * Author: daibo <daibo@yy.com>
 */
#ifndef UDP_BBR_HELPER_H
#define UDP_BBR_HELPER_H

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"

#include "../model/udp-bbr-sender.h"
#include "../model/udp-bbr-receiver.h"

namespace ns3
{
class UdpBbrReceiverHelper
{
  public:
  UdpBbrReceiverHelper();
  UdpBbrReceiverHelper(uint16_t port);

  void SetAttribute(std::string name, const AttributeValue &value);
  ApplicationContainer Install(NodeContainer c);
  Ptr<UdpBbrReceiver> GetServer(void);
  private:
  ObjectFactory m_factory; //!< Object factory.
  Ptr<UdpBbrReceiver> m_server; //!< The last created application
};

class UdpBbrSenderHelper
{
  public:
  UdpBbrSenderHelper();
  UdpBbrSenderHelper(uint32_t id, Address ip, uint16_t port);
  UdpBbrSenderHelper(Address addr);

  void SetAttribute(std::string name, const AttributeValue &value);
  ApplicationContainer Install(NodeContainer c);
  private:
  ObjectFactory m_factory; //!< Object factory.
};
}

#endif
