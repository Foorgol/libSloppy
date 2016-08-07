#include <iostream>

#include <gtest/gtest.h>

#include "../Sloppy/DateTime/DateAndTime.h"

using namespace Sloppy::DateTime;

TEST(DatePeriods, TestLengths)
{
  using namespace boost::gregorian;
  date s{2010, 1, 1};
  date e{2010, 1, 15};

  DatePeriod cp{s, e};
  ASSERT_EQ(15, cp.getLength_Days());
}

//----------------------------------------------------------------------------

TEST(DatePeriods, TestBoostConversion)
{
  UTCTimestamp s{20100101, 12, 0, 0};
  UTCTimestamp e{20100101, 12, 30, 30};

  // a normal, closed period
  TimePeriod cp{s, e};
  auto boostTp = cp.toBoost();
  ASSERT_FALSE(boostTp.is_null());
  ASSERT_EQ(e.getRawPtime(), boostTp.last());
  ASSERT_TRUE(e.getRawPtime() < boostTp.end());

  auto td = boostTp.length();
  ASSERT_EQ(30*60+30, td.total_seconds());

  // an open period
  TimePeriod op{s};
  boostTp = op.toBoost();
  ASSERT_TRUE(boostTp.is_null());

  // a closed period of zero length
  cp = TimePeriod{s, s};
  boostTp = cp.toBoost();
  ASSERT_FALSE(boostTp.is_null());
  td = boostTp.length();
  ASSERT_EQ(0, td.total_seconds());
}
