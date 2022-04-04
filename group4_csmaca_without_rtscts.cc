
#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-net-device.h"
#include "ns3/animation-interface.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/qos-txop.h"
#include "ns3/olsr-module.h" //Header file for OLSR routing protocol.
#include "ns3/wifi-mac.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("csma-lab");

int main(int argc, char *argv[])
{
      
      // create nodes
      int nWifi = 20; // total number of nodes
      NodeContainer nodes; 
      nodes.Create(nWifi); 


      // setting up wifi channel
      YansWifiPhyHelper wifiPhy; // for setting physical properties like range, transmission power, antenna, etc.
      YansWifiChannelHelper wifiChannel;                                          
      wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel"); // the propagation speed is constant
      wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel"); // Range Propagation Loss model, signal strength depends on the range
      wifiPhy.SetChannel(wifiChannel.Create());   
      WifiHelper wifi; // for the wifi interface
      wifi.SetStandard(WIFI_STANDARD_80211g); // IEEE802.11g standard for MAC layer - multiple access protocol thats controls access the physical transmission medium
      std::string phyMode("DsssRate11Mbps"); // physical layer control mode.
      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                   "DataMode", StringValue(phyMode),
                                   "ControlMode", StringValue(phyMode));
      // add a MAC
      WifiMacHelper wifiMac;
      wifiMac.SetType("ns3::AdhocWifiMac"); // ad-hoc MAC
      NetDeviceContainer Devices = wifi.Install(wifiPhy, wifiMac, nodes); // install all settings into the created nodes
      

      // position of the nodes
      ObjectFactory pos; // possition allocator
      pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
      pos.Set("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]")); // X axis
      pos.Set("Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]")); // Y axis
      Ptr<PositionAllocator> taPositionAlloc = pos.Create()->GetObject<PositionAllocator>();
      int64_t streamIndex = 200; // set the distance between nodes randomly
      streamIndex += taPositionAlloc->AssignStreams(streamIndex);
      MobilityHelper mobility; // mobilty of the nodes, movement and layout
      mobility.SetPositionAllocator(taPositionAlloc);
      mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // constant position mobility model: the nodes are not moving
      /**
       * If we want to move the nodes, we can use different mobitly models such as random Walk, random Waypint etc. 
       * But in this casewe use constant position model: our nodes are not moving.
       */
      mobility.Install(nodes);


      // setting up internet stack and IP addresses
      OlsrHelper olsr; // Adhoc routing Protocol. OLSR. OPtimized Link Stat Routing protocol.
      Ipv4ListRoutingHelper list;
      list.Add(olsr, 10);
      InternetStackHelper stack; // create an internet stack, we must put all nodes into stack to enable communication.
      stack.SetRoutingHelper(list); // installing adhoc protocol(OLSR) here.
      stack.Install(nodes);


      // assign IPv4 addresses to all devices in the network
      Ipv4AddressHelper address;
      address.SetBase("10.0.0.0", "255.255.255.0");
      Ipv4InterfaceContainer Interfaces;
      Interfaces = address.Assign(Devices); // assign addresses to the devices/nodes

      
      // traffic creation
      OnOffHelper onoff("ns3::UdpSocketFactory", Address()); // UDP on off application for trafic generating
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
      Ipv4GlobalRoutingHelper::PopulateRoutingTables(); // enable default routing: source finds the nearest route to the destination
      uint16_t port = 9; // port number
      double simulationTime = 10; // total simulation time in seconds
      // nodes to destination
      for (int i = 1; i < nWifi; i++)
      {
            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(Interfaces.GetAddress(0), port)); // sink devices (receiver nodes)
            onoff.SetConstantRate(DataRate("300bps")); // data rate of the network. (speed)
            onoff.SetAttribute("PacketSize", UintegerValue(100)); // size of packets in bits
            // set up the packet generating applications
            ApplicationContainer apps = onoff.Install(nodes.Get(i)); // sender nodes
            apps.Start(Seconds(1.0)); // Packet Generating Application start time
            apps.Stop(Seconds(simulationTime)); // Packet Generating Application off time
      }

      
      // statistics
      FlowMonitorHelper flowmon; // flow monitor for statistical results (throughput, delay, packets sent and received)
      Ptr<FlowMonitor> monitor = flowmon.InstallAll(); // install flow monitor on all nodes
      Simulator::Stop(Seconds(simulationTime + 1)); // simulation start t1ime
      Simulator::Run(); // start simulation                             
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
      std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
      // variables for storing the results
      double Delaysum = 0;
      uint64_t txPacketsum = 0;
      uint64_t rxPacketsum = 0;
      double throughput = 0;
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
            NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress); // print source address and destination address 
            txPacketsum += iter->second.txPackets; // total transmitted packets
            rxPacketsum += iter->second.rxPackets; // total received packets
            Delaysum += iter->second.delaySum.GetMilliSeconds(); // delay of the entire network
      }      
      throughput = rxPacketsum * 8 / (simulationTime * 1000000.0); // bit/s throughput calculation
      NS_LOG_UNCOND("Sent Packets = " << txPacketsum); // print Total Transmit Packets
      NS_LOG_UNCOND("Received Packets = " << rxPacketsum); // print total received packets
      NS_LOG_UNCOND("Delay: " << Delaysum / txPacketsum << " ms"); // mean delay of the nework
      std::cout << throughput << " Mbit/s" << std::endl; // mean throughput of the network
      std::cout << "\n";
      
      Simulator::Destroy(); // end the simulation
      

      return 0;
}
