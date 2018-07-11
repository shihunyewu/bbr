/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"

#include "../helper/udp-bbr-helper.h"
//#include "../helper/video-helper.h"


#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"

// Default Network Topology
//
// Number of csma nodes can be increased up to 250
//                          |
//                 Rank 0   |   Rank 1
// -------------------------|----------------------------
//   LAN 10.1.3.0
//  4==============1
//  |    |    |    |1   10.1.1.0     2
// n7   n6   n5   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   1==============4
//                                     LAN 10.1.2.0

using namespace ns3;

static const int kServerPort = 9;
static const float kServerStart = 1.0f;
static const float kDuration = 200.0f;
static const float kClientStart = kServerStart + 1.0;
static const float kClientStop = kClientStart + kDuration;
static const float kServerStop = kClientStop + 1.0;


NodeContainer p2pNodes;
PointToPointHelper pointToPoint;
NetDeviceContainer p2pDevices;

bool  need_tcp = false;

void PrintIPAddress(Ptr<Node> node)
{
    Ptr<Ipv4> ipv4;
    Ipv4Address ipv4Address;

    ipv4 = node->GetObject<Ipv4>();

    for (uint32_t i_interface = 0; i_interface < ipv4->GetNInterfaces(); i_interface++)
    {
        for (uint32_t count = 0; count < ipv4->GetNAddresses(i_interface); count++)
        {
            Ipv4InterfaceAddress iaddr = ipv4->GetAddress(i_interface, count);
            ipv4Address = iaddr.GetLocal();

            //each node has a local ip address: 127.0.0.1
            //if (ipv4Address != Ipv4Address("127.0.0.1"))
            {
                std::ostringstream addrOss;
                ipv4Address.Print(addrOss);
                std::cout << i_interface << ": " << addrOss.str().c_str() << std::endl;
            }
        }
    }
}

#define printNodes(container)                                        \
  do                                                                 \
  {                                                                  \
    int n = container.GetN();                                        \
    std::cout << #container " has " << n << " nodes: " << std::endl; \
    for (int i = 0; i < n; i++)                                      \
    {                                                                \
      int k = container.Get(i)->GetNDevices();                       \
      std::cout << "node index: " << i << " has "                    \
                << k << " devices" << std::endl;                     \
      PrintIPAddress(container.Get(i));                              \
    }                                                                \
  } while (0)

NS_LOG_COMPONENT_DEFINE("BbrTestNormal1");

int times = 0;
Timer m_timer; //10s
void OnTimer()
{
    times ++;
    std::cout << "In OnTimer(): times  = " << times << std::endl;
    if(times%2){
        //pointToPoint.SetDeviceAttribute("DataRate", StringValue("1.0Mbps"));
        //pointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));

        Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue("2.2Mbps"));
        //Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/Delay", StringValue("100ms"));
        //Config::Set("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue("1.0Mbps"));
        //p2pDevices.Get(0)->SetAttribute("Delay",StringValue("100ms"));
        //p2pDevices.Get(1)->SetAttribute("DataRate",StringValue("1.0Mbps"));

        //pointToPoint.SetChannelAttribute("Delay", StringValue("100ms"));
        m_timer.SetDelay(MilliSeconds(50000));//20s

    }else{
        //pointToPoint.SetDeviceAttribute("DataRate", StringValue("1.0Mbps"));
        //pointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));

        Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue("2.2Mbps"));
        //Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/Delay", StringValue("50ms"));
        //Config::Set("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue("1.0Mbps"));
        //p2pDevices.Get(0)->SetAttribute("DataRate",StringValue("2.0Mbps"));
        //p2pDevices.Get(0)->SetAttribute("Delay",StringValue("50ms"));
        //p2pDevices.Get(1)->SetAttribute("DataRate",StringValue("2.0Mbps"));
        //pointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));
        m_timer.SetDelay(MilliSeconds(50000));//20s
    }

    m_timer.Schedule();
}

int main(int argc, char *argv[])
{
    bool verbose = true;
    uint32_t nCsmaLeft = 3;
    uint32_t nCsmaRight = 3;
    bool tracing = true;

    //m_timer.SetDelay(MilliSeconds(50000));//20s
    //m_timer.SetFunction(&OnTimer);
    //m_timer.Schedule();

    CommandLine cmd;
    cmd.AddValue("nCsmaLeft", "Number of CSMA nodes/devices at left", nCsmaLeft);
    cmd.AddValue("nCsmaRight", "Number of CSMA nodes/devices at right", nCsmaRight);
    cmd.AddValue("verbose", "Tell bbr applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    // Check for valid number of csma nodes
    // 250 should be enough, otherwise IP addresses
    // soon become an issue
    if (nCsmaLeft > 250 || nCsmaRight > 250)
    {
        std::cout << "Too many csma nodes(" << nCsmaLeft << ","
                  << nCsmaRight << "), no more than 250 each." << std::endl;
        return 1;
    }

    if (verbose)
    {
        LogComponentEnable("UdpBbrSenderApplication", (LogLevel)(LOG_LEVEL_INFO | LOG_PREFIX_FUNC));
        LogComponentEnable("UdpBbrReceiverApplication",(LogLevel)(LOG_LEVEL_INFO | LOG_PREFIX_FUNC));
    }

    //NodeContainer p2pNodes;
    p2pNodes.Create(2);

    //PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));

    //NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel>(
            //"ErrorRate", DoubleValue(0.01),
            "ErrorRate", DoubleValue(0.000),
            "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100.0Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.5)));

    NodeContainer csmaNodesRight;
    csmaNodesRight.Add(p2pNodes.Get(1));
    csmaNodesRight.Create(nCsmaRight);

    NetDeviceContainer csmaDevicesRight;
    csmaDevicesRight = csma.Install(csmaNodesRight);

    NodeContainer csmaNodesLeft;
    csmaNodesLeft.Add(p2pNodes.Get(0));
    csmaNodesLeft.Create(nCsmaLeft);

    NetDeviceContainer csmaDevicesLeft;
    csmaDevicesLeft = csma.Install(csmaNodesLeft);

    InternetStackHelper stack;
    stack.Install(csmaNodesRight);
    stack.Install(csmaNodesLeft);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfacesRight;
    csmaInterfacesRight = address.Assign(csmaDevicesRight);

    address.SetBase("10.1.3.0", "255.255.255.0");
    address.Assign(csmaDevicesLeft);

    UdpBbrReceiverHelper bbrServer(kServerPort);
    //VideoReceiverHelper bbrServer(kServerPort);
    ApplicationContainer serverApps = bbrServer.Install(csmaNodesRight.Get(nCsmaRight));

    UdpBbrSenderHelper bbrClient(csmaInterfacesRight.GetAddress(nCsmaLeft), kServerPort);
    //VideoSenderHelper bbrClient(csmaInterfacesRight.GetAddress(nCsmaLeft), kServerPort);
    bbrClient.SetAttribute("Duration", TimeValue(Seconds(0)));
    bbrClient.SetAttribute("DataRate", DataRateValue(DataRate("1.0Mb/s")));
    bbrClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = bbrClient.Install(csmaNodesLeft.Get(nCsmaLeft));
    Names::Add("/Names/BbrSender", clientApps.Get(0));


//    if(need_tcp){
//        //
//        // Create a BulkSendApplication and install it on node 0
//        //
//        uint16_t port = 9;  // well-known echo port number
//
//        BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (p2pInterfaces.GetAddress (1), port));
//        // Set the amount of data to send in bytes.  Zero is unlimited.
//        source.SetAttribute ("MaxBytes", UintegerValue (0));
//        ApplicationContainer sourceApps = source.Install (p2pNodes.Get (0));
//
//        sourceApps.Start (Seconds (kServerStart));
//        sourceApps.Stop (Seconds (kServerStop));
//        //
//        // Create a PacketSinkApplication and install it on node 1
//        //
//        PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
//        ApplicationContainer sinkApps = sink.Install (p2pNodes.Get (1));
//
//        sinkApps.Start(Seconds(kServerStart));
//        sinkApps.Stop(Seconds(kServerStop));
//    }


    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create the gnuplot helper.
    GnuplotHelper plotHelper;

    // Configure the plot.  Arguments include file prefix, plot title,
    // x-label, y-label, and output file type
    plotHelper.ConfigurePlot("plot-rtt",
                             "rtt vs. Time",
                             "Time (s)",
                             "Rtt (ms)",
                             "png");

    // Create a probe.  Because the trace source we are interested in is
    // of type uint32_t, we specify the type of probe to use by the first
    // argument specifying its ns3 TypeId.
    plotHelper.PlotProbe("ns3::Uinteger32Probe",
                         "/Names/BbrSender/Rtt",
                         "Output",
                         "rtt",
                         GnuplotAggregator::KEY_INSIDE);

    // Create the gnuplot helper.
    GnuplotHelper plotHelper2;

    // Configure the plot.  Arguments include file prefix, plot title,
    // x-label, y-label, and output file type
    plotHelper2.ConfigurePlot("plot-bif",
                              "bytes vs. Time",
                              "Time (s)",
                              "bytes",
                              "png");

    // Create a probe.  Because the trace source we are interested in is
    // of type uint32_t, we specify the type of probe to use by the first
    // argument specifying its ns3 TypeId.
    plotHelper2.PlotProbe("ns3::Uinteger32Probe",
                          "/Names/BbrSender/BytesInFlight",
                          "Output",
                          "bif",
                          GnuplotAggregator::KEY_INSIDE);

    // Create the gnuplot helper.
    GnuplotHelper plotHelper3;

    // Configure the plot.  Arguments include file prefix, plot title,
    // x-label, y-label, and output file type
    plotHelper3.ConfigurePlot("plot-bandwidth",
                              "bandwidth vs. Time",
                              "Time (s)",
                              "bandwidth",
                              "png");

    // Create a probe.  Because the trace source we are interested in is
    // of type uint32_t, we specify the type of probe to use by the first
    // argument specifying its ns3 TypeId.
    plotHelper3.PlotProbe("ns3::Uinteger32Probe",
                          "/Names/BbrSender/Bandwidth",
                          "Output",
                          "Bandwidth",
                          GnuplotAggregator::KEY_INSIDE);

    serverApps.Start(Seconds(kServerStart));
    serverApps.Stop(Seconds(kServerStop));
    clientApps.Start(Seconds(kClientStart));
    clientApps.Stop(Seconds(kClientStop));

    Simulator::Stop(Seconds(kServerStop));

    if (tracing == true)
    {
        pointToPoint.EnablePcapAll("bbrtest");
        csma.EnablePcap("bbrtest", csmaDevicesRight.Get(0), true);
        csma.EnablePcap("bbrtest", csmaDevicesLeft.Get(0), true);
    }

#if 0
    printNodes(p2pNodes);
  printNodes(csmaNodesRight);
  printNodes(csmaNodesLeft);
#endif

    Simulator::Run();
    Simulator::Destroy();

//    if(need_tcp){
//        Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
//        std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << std::endl;
//    }


    return 0;
}