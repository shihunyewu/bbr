/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "../model/video-generator.h"

// An essential include is test.h
#include "ns3/test.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;
using namespace ns3::bbr;

// This is an example TestCase.
class VideoGeneratorTestCase : public TestCase
{
public:
    VideoGeneratorTestCase ();
  virtual ~VideoGeneratorTestCase () {}

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
VideoGeneratorTestCase::VideoGeneratorTestCase ()
  : TestCase ("video generator test case")
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
VideoGeneratorTestCase::DoRun (void)
{
    VideoGenerator video_generator;

    video_generator.Start();
    Simulator::Stop(Seconds(1));
    Simulator::Run();
    Simulator::Destroy();
    int num = video_generator.GetN();
    NS_TEST_ASSERT_MSG_EQ_TOL(num, 24, 1, "total frames != 24");

    FrameInfo frame;
    NS_TEST_ASSERT_MSG_EQ(video_generator.GetNextFrame(frame), true, "");
    NS_TEST_ASSERT_MSG_EQ(video_generator.GetN(), num - 1, "");
    NS_TEST_ASSERT_MSG_EQ(frame.m_type, IFrame, "");
    NS_TEST_ASSERT_MSG_EQ(frame.m_size, 130261, "");
}