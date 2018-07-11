/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "../model/ack-frame.h"

// An essential include is test.h
#include "ns3/test.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;
using namespace ns3::bbr;

// This is an example TestCase.
class PNQAddRemoveCase : public TestCase
{
public:
    PNQAddRemoveCase ();
  virtual ~PNQAddRemoveCase () {}

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
PNQAddRemoveCase::PNQAddRemoveCase ()
  : TestCase ("packet number queue add and remove")
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
PNQAddRemoveCase::DoRun (void)
{
  PacketNumberQueue packets;
  packets.Add(1,3);
  packets.Add(5,9);
  packets.Add(3);
  NS_TEST_ASSERT_MSG_EQ(packets.NumIntervals(), 2, "Number of intervals != 2");
  NS_TEST_ASSERT_MSG_EQ(packets.Min(), 1, "min is not 1");
  NS_TEST_ASSERT_MSG_EQ(packets.Max(), 8, "max is not 8");
  NS_TEST_ASSERT_MSG_EQ(packets.NumPacketsSlow(), 7, "number of packets is not 7");
  NS_TEST_ASSERT_MSG_EQ(packets.Contains(2), true, "packet 2 is not there");
  
  packets.Add(3,5);
  NS_TEST_ASSERT_MSG_EQ(packets.NumIntervals(), 1, "Number of intervals != 1");
  NS_TEST_ASSERT_MSG_EQ(packets.Min(), 1, "min is not 1");
  NS_TEST_ASSERT_MSG_EQ(packets.Max(), 8, "max is not 8");
  NS_TEST_ASSERT_MSG_EQ(packets.NumPacketsSlow(), 8, "number of packets is not 8");
  NS_TEST_ASSERT_MSG_EQ(packets.Contains(4), true, "packet 4 is not there");

  packets.RemoveUpTo(3);
  NS_TEST_ASSERT_MSG_EQ(packets.NumIntervals(), 1, "Number of intervals != 1");
  NS_TEST_ASSERT_MSG_EQ(packets.Min(), 3, "min is not 3");
  NS_TEST_ASSERT_MSG_EQ(packets.Max(), 8, "max is not 8");
  NS_TEST_ASSERT_MSG_EQ(packets.NumPacketsSlow(), 6, "number of packets is not 6");
  NS_TEST_ASSERT_MSG_EQ(packets.Contains(1), false, "packet 1 is there");

}

// This is an example TestCase.
class PNQWrapAroundCase : public TestCase
{
public:
  PNQWrapAroundCase ();
  virtual ~PNQWrapAroundCase () {}

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
PNQWrapAroundCase::PNQWrapAroundCase ()
  : TestCase ("packet number wrap around")
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
PNQWrapAroundCase::DoRun (void)
{
  PacketNumberQueue packets;
  packets.Add(4294967295 - 4, 4294967295);
  packets.Add(2,10);
  NS_TEST_ASSERT_MSG_EQ(packets.NumIntervals(), 2, "Number of intervals != 2");
  NS_TEST_ASSERT_MSG_EQ(packets.Min(), 4294967291, "min is not 4294967291");
  NS_TEST_ASSERT_MSG_EQ(packets.Max(), 9, "max is not 9");
  NS_TEST_ASSERT_MSG_EQ(packets.NumPacketsSlow(), 12, "number of packets is not 12");
  NS_TEST_ASSERT_MSG_EQ(packets.Contains(4294967294), true, "packet 4294967294 is not there:" << packets);

  packets.Add(4294967295,2);
  NS_TEST_ASSERT_MSG_EQ(packets.NumIntervals(), 1, "Number of intervals != 1:" << packets);
  NS_TEST_ASSERT_MSG_EQ(packets.Min(), 4294967291, "min is not 4294967291");
  NS_TEST_ASSERT_MSG_EQ(packets.Max(), 9, "max is not 9");
  NS_TEST_ASSERT_MSG_EQ(packets.NumPacketsSlow(), packets.LastIntervalLength(), "number of packets is not correct");
  NS_TEST_ASSERT_MSG_EQ(packets.Contains(0), true, "packet 0 is not there");
  std::cout << packets;

}


