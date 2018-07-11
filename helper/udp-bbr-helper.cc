#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "udp-bbr-helper.h"

namespace ns3
{
UdpBbrReceiverHelper::UdpBbrReceiverHelper()
{
}

UdpBbrReceiverHelper::UdpBbrReceiverHelper(uint16_t port)
{
    m_factory.SetTypeId(UdpBbrReceiver::GetTypeId());
    SetAttribute("Port", UintegerValue(port));
}

void UdpBbrReceiverHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
UdpBbrReceiverHelper::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;

        m_server = m_factory.Create<UdpBbrReceiver>();
        node->AddApplication(m_server);
        apps.Add(m_server);
    }
    return apps;
}

Ptr<UdpBbrReceiver>
UdpBbrReceiverHelper::GetServer(void)
{
    return m_server;
}

UdpBbrSenderHelper::UdpBbrSenderHelper()
{
}

UdpBbrSenderHelper::UdpBbrSenderHelper(Address address, uint16_t port)
{
    m_factory.SetTypeId(UdpBbrSender::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("RemotePort", UintegerValue(port));
}

UdpBbrSenderHelper::UdpBbrSenderHelper(Address address)
{
    m_factory.SetTypeId(UdpBbrSender::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
}

void UdpBbrSenderHelper::SetAttribute(std::string name, const AttributeValue &value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
UdpBbrSenderHelper::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;
        Ptr<UdpBbrSender> client = m_factory.Create<UdpBbrSender>();
        node->AddApplication(client);
        apps.Add(client);
    }
    return apps;
}
}