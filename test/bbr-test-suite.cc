/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// An essential include is test.h
#include "ns3/test.h"

#include "packet-number-queue-test-suite.h"
#include "packet-number-indexed-queue-test-suite.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// This is an example TestCase.
class BbrTestCase1 : public TestCase
{
public:
  BbrTestCase1 ();
  virtual ~BbrTestCase1 ();

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
BbrTestCase1::BbrTestCase1 ()
  : TestCase ("Bbr test case (does nothing)")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
BbrTestCase1::~BbrTestCase1 ()
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
BbrTestCase1::DoRun (void)
{
  // A wide variety of test macros are available in src/core/test.h
  NS_TEST_ASSERT_MSG_EQ (true, true, "true doesn't equal true for some reason");
  // Use this one for floating point comparisons
  NS_TEST_ASSERT_MSG_EQ_TOL (0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined
//
class BbrTestSuite : public TestSuite
{
public:
  BbrTestSuite ();
};

BbrTestSuite::BbrTestSuite ()
  : TestSuite ("bbr", UNIT)
{
  // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
  AddTestCase (new BbrTestCase1, TestCase::QUICK);
  AddTestCase (new PNQAddRemoveCase, TestCase::QUICK);
  AddTestCase (new PNQWrapAroundCase, TestCase::QUICK);
  AddTestCase (new PNIQTestCase, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static BbrTestSuite bbrTestSuite;

