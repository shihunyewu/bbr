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
static const float kDuration = 60.0f;
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

NS_LOG_COMPONENT_DEFINE("BbrTestNormal");

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

void LossRateSim() {
    double lossrate = std::rand() % 1001 / 1001.0 * 50.0 / 100.0; //0-50 %
    std::cout << "lossrate : " << lossrate << std::endl;
    Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel>(
            "ErrorRate", DoubleValue(lossrate),
            "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    Config::Set("/NodeList/0/DeviceList/1/$ns3::PointToPointNetDevice/ReceiveErrorModel", PointerValue(em)); 
    m_timer.SetDelay(MilliSeconds(1000));
}

int main(int argc, char *argv[])
{
    bool verbose = true;
    uint32_t nCsmaLeft = 3;
    uint32_t nCsmaRight = 3;
    bool tracing = true;
    double lossrate = 0.00;
    std::string linkrate = "2Mbps";
    int testcase = 1;


    CommandLine cmd;
    cmd.AddValue("nCsmaLeft", "Number of CSMA nodes/devices at left", nCsmaLeft);
    cmd.AddValue("nCsmaRight", "Number of CSMA nodes/devices at right", nCsmaRight);
    cmd.AddValue("verbose", "Tell bbr applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("linkrate", "Tell p2p linkrate", linkrate);
    cmd.AddValue("lossrate", "Tell p2p lossrate", lossrate);
    cmd.AddValue("testcase", "Tell which test case", testcase);

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
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(linkrate));
    pointToPoint.SetChannelAttribute("Delay", StringValue("100ms"));

    //NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    switch (testcase) {
        case 1:
            break;
        case 2:
            break;
        case 3:
            lossrate = std::rand() % 1001 / 1001.0 * 50.0 / 100.0; //0-50 %
            m_timer.SetDelay(MilliSeconds(3000));
            m_timer.SetFunction(&LossRateSim);
            m_timer.Schedule();
            break; 
        case 4:
            break;
    }
    lossrate = std::rand() % 1001 / 1001.0 * 50.0 / 100.0; //0-50 %
    Ptr<RateErrorModel> em = CreateObjectWithAttributes<RateErrorModel>(
            //"ErrorRate", DoubleValue(0.01),
            "ErrorRate", DoubleValue(lossrate),
            "ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));
    p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

    // CsmaHelper csma;
    // csma.SetChannelAttribute("DataRate", StringValue("100.0Mbps"));
    // csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.5)));

    // NodeContainer csmaNodesRight;
    // csmaNodesRight.Add(p2pNodes.Get(1));
    // csmaNodesRight.Create(nCsmaRight);

    // NetDeviceContainer csmaDevicesRight;
    // csmaDevicesRight = csma.Install(csmaNodesRight);

    // NodeContainer csmaNodesLeft;
    // csmaNodesLeft.Add(p2pNodes.Get(0));
    // csmaNodesLeft.Create(nCsmaLeft);

    // NetDeviceContainer csmaDevicesLeft;
    // csmaDevicesLeft = csma.Install(csmaNodesLeft);

    // InternetStackHelper stack;
    // stack.Install(csmaNodesRight);
    // stack.Install(csmaNodesLeft);

    InternetStackHelper stack;
    stack.Install(p2pNodes.Get(0));
    stack.Install(p2pNodes.Get(1));

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    // address.SetBase("10.1.2.0", "255.255.255.0");
    // Ipv4InterfaceContainer csmaInterfacesRight;
    // csmaInterfacesRight = address.Assign(csmaDevicesRight);

    // address.SetBase("10.1.3.0", "255.255.255.0");
    // address.Assign(csmaDevicesLeft);

    UdpBbrReceiverHelper bbrServer(kServerPort);
    //VideoReceiverHelper bbrServer(kServerPort);
    //ApplicationContainer serverApps = bbrServer.Install(csmaNodesRight.Get(nCsmaRight));

    ApplicationContainer serverApps = bbrServer.Install(p2pNodes.Get(1));

    UdpBbrSenderHelper bbrClient(p2pInterfaces.GetAddress(1), kServerPort);
    //UdpBbrSenderHelper bbrClient(csmaInterfacesRight.GetAddress(nCsmaLeft), kServerPort);
    //VideoSenderHelper bbrClient(csmaInterfacesRight.GetAddress(nCsmaLeft), kServerPort);
    bbrClient.SetAttribute("Duration", TimeValue(Seconds(0)));
    bbrClient.SetAttribute("DataRate", DataRateValue(DataRate("1.0Mb/s")));
    bbrClient.SetAttribute("PacketSize", UintegerValue(1024));
    //ApplicationContainer clientApps = bbrClient.Install(csmaNodesLeft.Get(nCsmaLeft));
    ApplicationContainer clientApps = bbrClient.Install(p2pNodes.Get(0));
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
        //csma.EnablePcap("bbrtest", csmaDevicesRight.Get(0), true);
        //csma.EnablePcap("bbrtest", csmaDevicesLeft.Get(0), true);
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

// Network topology   PointToPoint
//    mytfrc mytfrc  udp/tcp
//    client client client
//       h0    h1   h2
//       |0    |1    |2
//       ==s0(6)======          s0\s1 is bridgeliuliu
//          |3    
//          |3
//       ==s1(7)======
//       |0    |1    |2
//       h3    h4   h5
//    mytfrc  mytfrc udp/tcp
//    server server server
// - UDP flows from h0 to h2

// #include <fstream>
// #include <iostream>
// #include <string>

// #include "ns3/bigolive-module.h"
// #include "ns3/core-module.h"
// #include "ns3/point-to-point-module.h"
// #include "ns3/internet-module.h"
// #include "ns3/applications-module.h"
// #include "ns3/bridgeliu-module.h"
// #include "ns3/network-module.h"
// #include "ns3/ipv4-header.h"
// #include "ns3/error-model.h"
// #include "ns3/drop-tail-queue.h"
// #include "ns3/data-rate.h"
// #include "ns3/traffic-control-module.h"
// #include "ns3/nstime.h"
// #include "ns3/stats-module.h"

// #include "../helper/udp-bbr-helper.h"

// using namespace ns3;

// NS_LOG_COMPONENT_DEFINE ("MyTFRCExample4");

// static void
// Recordvalue (Ptr<OutputStreamWrapper> stream, int oldv, int newv)
// {
//     //NS_LOG_UNCOND(Simulator::Now().GetMilliSeconds()<<"\t"<<newv);
//     *stream->GetStream()<< Simulator::Now().GetMilliSeconds()<<"\t"<<newv<<std::endl;
// }


// int
// main (int argc, char *argv[])
// {
//     //
//     // Enable logging 
//     //
//     // LogComponentEnable ("MyTFRC", LOG_LEVEL_INFO);
//     // LogComponentEnable ("MyTFRCExample3", LOG_LEVEL_INFO);
//     // LogComponentEnable ("BridgeLiuNetDevice", LOG_LEVEL_DEBUG);
//     CommandLine cmd;
//     std::string outName = "my-tfrc-example1.tr";
//     cmd.AddValue("OutputFile", "the output file name with .tr", outName);


//     double error_p=0.0;
//     cmd.AddValue("Error_P", "the random loss rate (double)", error_p);
//     std::string bandwidth = "10Mbps";
//     std::string delay = "5ms";
//     std::string bottleneck_bandwidth = "1Mbps";
//     std::string bottleneck_delay = "50ms";
//     uint32_t MTU = 1400;
//     uint32_t queueSize = 1000; 
//     uint32_t bottleneck_queue_length = 50;
//     cmd.AddValue("Bottleneck_Bandwidth", "string format of bottleneck bandwidth", bottleneck_bandwidth);
//     cmd.AddValue("Bottleneck_Queue_Length", "number of queue length", bottleneck_queue_length);

//     // set random seed
//     RngSeedManager::SetSeed(3);
//     RngSeedManager::SetRun(7);

//     double tmp_mytfrc_start = 20.0;
//     double tmp_mytfrc_end = 220.0;
//     double tmp_mytfrc2_start = 50.0;
//     double tmp_mytfrc2_end = 350.0;
//     double tmp_tcp_start = 100.0;
//     double tmp_tcp_end = 200.0;
//     double tmp_udp_start = 100.0;
//     double tmp_udp_end = 200.0;
//     cmd.AddValue("MYTFRC_start", "start time of MyTFRC1", tmp_mytfrc_start);
//     cmd.AddValue("MYTFRC_end", "end time of MyTFRC1", tmp_mytfrc_end);
//     cmd.AddValue("MYTFRC2_start", "start time of MyTFRC2", tmp_mytfrc2_start);
//     cmd.AddValue("MYTFRC2_end", "end time of MyTFRC2", tmp_mytfrc2_end);
//     cmd.AddValue("TCP_start", "start time of TCP agent", tmp_tcp_start);
//     cmd.AddValue("TCP_end", "end time of TCP agent", tmp_tcp_end);
//     cmd.AddValue("UDP_start", "start time of UDP agent", tmp_udp_start);
//     cmd.AddValue("UDP_end", "end time of UDP agent", tmp_udp_end);

//     // use the application
//     bool isBBR = true;
//     bool isUDP = false;
//     bool isTCP = false;
//     bool isMYTFRC = false; 
//     bool isMYTFRC2 = false;
//     //bool isBBR = false;
//     cmd.AddValue("isMYTFRC", "start MYTFRC agent1 or not", isMYTFRC);
//     cmd.AddValue("isMYTFRC2", "start MYTFRC agent2 or not", isMYTFRC2);
//     cmd.AddValue("isUDP", "start UDP agent or not, UDP and TCP agent cannot open simultaneously, UDP occupy TCP", isUDP);
//     cmd.AddValue("isTCP", "start TCP agent or not, UDP and TCP agent cannot open simultaneously, UDP occupy TCP", isTCP);

//     // UDP configure
//     uint32_t UDPmaxPacketCount = 0;
//     uint32_t UDPmaxPacketSize = 1024;
//     std::string UDPspeed = "0.2Mbps";
//     NS_LOG_INFO("UDP configure: UDPenabled="<<isUDP<<
//                 " MaxPacketCount="<<UDPmaxPacketCount<<
//                 " MaxPacketSize="<<UDPmaxPacketSize<<
//                 " UDPsendspeed="<<UDPspeed 
//             );
   
    
//     // TCP configure
//     // TcpBic TcpNewReno TcpYeah
//     std::string transport_prot = "TcpBic";
//     uint32_t TCPbufferSize = 87380;   
//     NS_LOG_INFO("TCP configure:"
//                 " TCPenabled="<<isTCP <<
//                 " TCPbufferSize="<<TCPbufferSize
//             );
//     Ptr<PacketSink> sink1;
   
//     // MYTFRC configure
//     // server & client use port 100,101 for transferring
//     // so we donot change this
//     // also the generate frame function will be called 
//     // 5ms once, we donot change this
//     // Send function will be called every 5ms, Mytfrc_SInterval
//     bool isLossFilter = true;
//     bool isLossFilter2 = true;
//     cmd.AddValue("isLossFilter", "MYTFRC agent use Loss Filter or not", isLossFilter);
//     cmd.AddValue("isLossFilter2", "MYTFRC2 agent use Loss Filter or not", isLossFilter2);
//     int TFRCSLOPSMode = 1;
//     int TFRCSLOPSMode2 = 1;
//     cmd.AddValue("TFRCSLOPSMode", "MYTFRC agent use TFRC or SLOPS", TFRCSLOPSMode);
//     cmd.AddValue("TFRCSLOPSMode2", "MYTFRC2 agent use TFRC or SLOPS", TFRCSLOPSMode2);




//     // BBR configure
//     // isBBR is true the BBR algorithm will conquer TCP algorithm
//     //uint32_t BBRkServerPort = 9;
//     //std::string BBRspeed = "2Mbps";

//     // tracing
//     bool isTracing = true;
//     bool isPcap = false;
//     bool isPlot = true;

//     // Calculate the ADU size
//     Header* temp_header = new Ipv4Header ();
//     uint32_t ip_header = temp_header->GetSerializedSize ();
//     NS_LOG_LOGIC ("IP Header size is: " << ip_header);
//     delete temp_header;
//     temp_header = new TcpHeader ();
//     uint32_t tcp_header = temp_header->GetSerializedSize ();
//     NS_LOG_LOGIC ("TCP Header size is: " << tcp_header);
//     delete temp_header;
//     uint32_t tcp_adu_size = MTU - 20 - (ip_header + tcp_header);
//     NS_LOG_LOGIC ("TCP ADU size is: " << tcp_adu_size);
//     temp_header = new UdpHeader ();
//     uint32_t udp_header = temp_header->GetSerializedSize ();
//     NS_LOG_LOGIC ("UDP Header size is: " << tcp_header);
//     delete temp_header;
//     uint32_t udp_adu_size = MTU - 20 - (ip_header + udp_header);
//     NS_LOG_LOGIC ("udp ADU size is: " << udp_adu_size);
    
//     // parse the arguements
//     cmd.Parse (argc, argv);

//     // start end time
//     // start time must bigger than Seconds(1.0)
//     // server start time = start - Seconds(1.0)
//     // server end time   = end   + Seconds(5.0)
//     Time TCP_start = Seconds(tmp_tcp_start);
//     Time TCP_end   = Seconds(tmp_tcp_end);
//     Time UDP_start = Seconds(tmp_udp_start);
//     Time UDP_end   = Seconds(tmp_udp_end);
//     Time MYTFRC_start = Seconds(tmp_mytfrc_start);
//     Time MYTFRC_end   = Seconds(tmp_mytfrc_end);
//     Time MYTFRC2_start = Seconds(tmp_mytfrc2_start);
//     Time MYTFRC2_end   = Seconds(tmp_mytfrc2_end);
//     Time BBR_start = Seconds(110.0);
//     Time BBR_end  = Seconds(220.0);

//     //
//     // Explicitly create the nodes required by the topology (shown above).
//     //
//     NS_LOG_INFO ("Create nodes.");
//     NodeContainer hosts;
//     hosts.Create (6);

//     NodeContainer switches;
//     switches.Create(2);


//     NS_LOG_INFO ("Build Topology.");
//     //
//     // Explicitly create the channels required by the topology (shown above).
//     //


//     NetDeviceContainer hostDevices;
//     NetDeviceContainer switchDevices1;
//     NetDeviceContainer switchDevices2;

//     PointToPointHelper p2p;
//     p2p.SetDeviceAttribute ("Mtu", UintegerValue (MTU));
//     p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queueSize));
    
//     p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (delay));
//     NetDeviceContainer l00 = p2p.Install(hosts.Get(0), switches.Get(0));
//     hostDevices.Add(l00.Get(0));
//     switchDevices1.Add(l00.Get(1));

//     p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (delay));
//     NetDeviceContainer l10 = p2p.Install (hosts.Get(1), switches.Get(0));
//     hostDevices.Add(l10.Get(0));
//     switchDevices1.Add(l10.Get(1));

//     p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (delay));
//     NetDeviceContainer l20 = p2p.Install (hosts.Get(2), switches.Get(0));
//     hostDevices.Add(l20.Get(0));
//     switchDevices1.Add(l20.Get(1));

//     p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (delay));
//     NetDeviceContainer l31 = p2p.Install (hosts.Get(3), switches.Get(1));
//     hostDevices.Add(l31.Get(0));
//     switchDevices2.Add(l31.Get(1));

//     p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (delay));
//     NetDeviceContainer l41 = p2p.Install (hosts.Get(4), switches.Get(1));
//     hostDevices.Add(l41.Get(0));
//     switchDevices2.Add(l41.Get(1));

//     p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (delay));
//     NetDeviceContainer l51 = p2p.Install (hosts.Get(5), switches.Get(1));
//     hostDevices.Add(l51.Get(0));
//     switchDevices2.Add(l51.Get(1));

//     p2p.SetDeviceAttribute ("DataRate", StringValue (bottleneck_bandwidth));
//     p2p.SetChannelAttribute ("Delay", StringValue (bottleneck_delay));
//     NetDeviceContainer bottleneck = p2p.Install (switches.Get(0), switches.Get(1)); 
//     switchDevices1.Add(bottleneck.Get(0));
//     switchDevices2.Add(bottleneck.Get(1));
//     // Configure the error model
//     // Here we use RateErrorModel with packet error rate
//     Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
//     uv->SetStream (50);
//     RateErrorModel error_model;
//     error_model.SetRandomVariable (uv);
//     error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
//     error_model.SetRate (error_p);
//     bottleneck.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (&error_model));


//     // set queue length
// #if 1
//     // this line is for ns3.26
//     Ptr<DropTailQueue> queue = DynamicCast<DropTailQueue>(
//             DynamicCast<PointToPointNetDevice>(bottleneck.Get(0))->GetQueue());
// #else
//     // this line is for ns3.27
//     Ptr<DropTailQueue<Packet>> queue = DynamicCast<DropTailQueue<Packet>>(
//             DynamicCast<PointToPointNetDevice>(bottleneck.Get(0))->GetQueue());
// #endif
//     queue->SetAttribute("MaxPackets", UintegerValue(bottleneck_queue_length));


//     // add bridgeliu
//     //
//     Ptr<Node> switchNode1 = switches.Get(0);
//     Ptr<Node> switchNode2 = switches.Get(1);
//     BridgeLiuHelper bridgeliu;
//     NetDeviceContainer tmp1 = bridgeliu.Install(switchNode1, switchDevices1);
//     NetDeviceContainer tmp2 = bridgeliu.Install(switchNode2, switchDevices2);

//     //
//     // We've got the "hardware" in place.  Now we need to add IP addresses.
//     //
//     InternetStackHelper internet;
//     internet.Install(hosts);
//     // add IP address
//     Ipv4Address hostAddress[6];
//     Ipv4AddressHelper ipv4;
    
//     ipv4.SetBase("10.1.1.0", "255.255.255.0");
//     Ipv4InterfaceContainer ipInterfs = ipv4.Assign(hostDevices);
//     for(int i=0; i<6; i++){
//         hostAddress[i] = ipInterfs.GetAddress(i);
//     }
//     // set learning switch table
//     //
//     Ptr<NetDevice> bbb;
//     Ptr<BridgeLiuNetDevice> br1 = tmp1.Get(0)->GetObject<BridgeLiuNetDevice>();
//     for(int i=0;i<3;i++){
//         bbb = switchDevices1.Get(i);
//         br1->StaticLearn( hostAddress[i], bbb);
//     }
//     bbb = switchDevices1.Get(3);
//     for(int i=0;i<3;i++){
//         br1->StaticLearn( hostAddress[i+3], bbb);
//     }

//     //br1->PrintStaticMap();

//     Ptr<BridgeLiuNetDevice> br2 = tmp2.Get(0)->GetObject<BridgeLiuNetDevice>();
//     for(int i=0;i<3;i++){
//         bbb = switchDevices2.Get(i);
//         br2->StaticLearn( hostAddress[i+3], bbb);
//     }
//     bbb = switchDevices2.Get(3);
//     for(int i=0;i<3;i++){
//         br2->StaticLearn( hostAddress[i], bbb);
//     }
//     //br2->PrintStaticMap();
    

//     NS_LOG_INFO("mytfrc serverAddr:"<<hostAddress[3]<< 
//             ", mytfrc clientAddr:"<<hostAddress[0]);
//     NS_LOG_INFO("tcp serverAddr:"<<hostAddress[4]<< 
//             ", tcp clientAddr:"<<hostAddress[1]);
//     NS_LOG_INFO("udp serverAddr:"<<hostAddress[5]<< 
//             ", udp clientAddr:"<<hostAddress[2]);


//     NS_LOG_INFO ("Create Applications.");

//     ApplicationContainer logapps;

//     if(isBBR){
//         uint16_t kServerPort = 9;

//         UdpBbrReceiverHelper bbrServer(kServerPort);
//         ApplicationContainer serverApps = bbrServer.Install(hosts.Get(4));

//         UdpBbrSenderHelper bbrClient(hostAddress[4], kServerPort);
//         ApplicationContainer clientApps = bbrClient.Install(hosts.Get(1));

//         Names::Add("/Names/BbrSender", clientApps.Get(0));

//         serverApps.Start(Seconds(20.0));
//         serverApps.Stop(Seconds(220.0));
//         clientApps.Start(Seconds(21.0));
//         clientApps.Stop(Seconds(219.0));
//     }



//     //
//     // Now, do the actual simulation.
//     //
//     NS_LOG_INFO ("Run Simulation.");
//     Simulator::Run ();
//     Simulator::Destroy ();
//     NS_LOG_INFO ("Done.");
// }
