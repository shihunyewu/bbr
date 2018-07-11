/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "../model/packet-number-indexed-queue.h"

// An essential include is test.h
#include "ns3/test.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;
using namespace ns3::bbr;

// This is an example TestCase.
class PNIQTestCase : public TestCase
{
  public:
    PNIQTestCase();
    virtual ~PNIQTestCase() {}

  private:
    virtual void DoRun(void);
};

// Add some help text to this case to describe what it is intended to test
PNIQTestCase::PNIQTestCase()
    : TestCase("packet number indexed queue test")
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void PNIQTestCase::DoRun(void)
{
    //InitialState
    {
        PacketNumberIndexedQueue<std::string> queue_;
        NS_TEST_ASSERT_MSG_EQ(queue_.IsEmpty(), true, "not empty");
        NS_TEST_ASSERT_MSG_EQ(0u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(0u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(0u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(0u, queue_.entry_slots_used(), "");
    }
    //InsertingContinuousElements
    {
        PacketNumberIndexedQueue<std::string> queue_;
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1001, "one"), true, "");
        NS_TEST_ASSERT_MSG_EQ("one", *queue_.GetEntry(1001), "");

        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1002, "two"), true, "");
        NS_TEST_ASSERT_MSG_EQ("two", *queue_.GetEntry(1002), "");

        NS_TEST_ASSERT_MSG_EQ(queue_.IsEmpty(), false, "");
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(1002u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(2u, queue_.entry_slots_used(), "");
    }

    //InsertingOutOfOrder
    {
        PacketNumberIndexedQueue<std::string> queue_;

        queue_.Emplace(1001, "one");

        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1003, "three"), true, "");
        NS_TEST_ASSERT_MSG_EQ(0, queue_.GetEntry(1002), "");
        NS_TEST_ASSERT_MSG_EQ("three", *queue_.GetEntry(1003), "");

        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(1003u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(3u, queue_.entry_slots_used(), "");

        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1002, "two"), false, "");
    }

    //InsertingIntoPast
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1000, "zero"), false, "");
    }

    //InsertingDuplicate
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1001, "one"), false, "");
    }

    //RemoveInTheMiddle
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        queue_.Emplace(1002, "two");
        queue_.Emplace(1003, "three");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1002), true, "");
        NS_TEST_ASSERT_MSG_EQ(0, queue_.GetEntry(1002), "");
      
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(1003u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(3u, queue_.entry_slots_used(), "");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1002, "two"), false, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1004, "four"), true, "");
    }

    //RemoveAtImmediateEdges
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        queue_.Emplace(1002, "two");
        queue_.Emplace(1003, "three");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1001), true, "");
        NS_TEST_ASSERT_MSG_EQ(0, queue_.GetEntry(1001), "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1003), true, "");
        NS_TEST_ASSERT_MSG_EQ(0, queue_.GetEntry(1003), "");
      
        NS_TEST_ASSERT_MSG_EQ(1002u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(1003u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(1u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(2u, queue_.entry_slots_used(), "");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(1004, "four"), true, "");
    }

    //RemoveAtDistantFront
    {
        PacketNumberIndexedQueue<std::string> queue_;
        
        queue_.Emplace(1001, "one");
        queue_.Emplace(1002, "one (kinda)");
        queue_.Emplace(2001, "two");
      
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2001u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(3u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.entry_slots_used(), "");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1002), true, "");
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2001u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.entry_slots_used(), "");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1001), true, "");
        NS_TEST_ASSERT_MSG_EQ(2001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2001u, queue_.last_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(1u, queue_.number_of_present_entries(), "");
        NS_TEST_ASSERT_MSG_EQ(1u, queue_.entry_slots_used(), "");
    }

    //RemoveAtDistantBack
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        queue_.Emplace(2001, "two");
      
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2001u, queue_.last_packet(), "");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(2001), true, "");
        NS_TEST_ASSERT_MSG_EQ(1001u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(2001u, queue_.last_packet(), "");
    }

    //ClearAndRepopulate
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        queue_.Emplace(2001, "two");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1001), true, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(2001), true, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.IsEmpty(), true, "");
        NS_TEST_ASSERT_MSG_EQ(0u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(0u, queue_.last_packet(), "");
      
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(101, "one"), true, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Emplace(201, "two"), true, "");
        NS_TEST_ASSERT_MSG_EQ(101u, queue_.first_packet(), "");
        NS_TEST_ASSERT_MSG_EQ(201u, queue_.last_packet(), "");
    }

    //FailToRemoveElementsThatNeverExisted
    {
        PacketNumberIndexedQueue<std::string> queue_;
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1000), false, "");
        queue_.Emplace(1001, "one");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1000), false, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1002), false, "");
    }

    //FailToRemoveElementsTwice
    {
        PacketNumberIndexedQueue<std::string> queue_;
        queue_.Emplace(1001, "one");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1001), true, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1001), false, "");
        NS_TEST_ASSERT_MSG_EQ(queue_.Remove(1001), false, "");
    }

    //ConstGetter
    {
        PacketNumberIndexedQueue<std::string> queue_;

        queue_.Emplace(1001, "one");
        const auto& const_queue = queue_;
      
        NS_TEST_ASSERT_MSG_EQ("one", *const_queue.GetEntry(1001), "");
        NS_TEST_ASSERT_MSG_EQ(0, const_queue.GetEntry(1002), "");
    }

}