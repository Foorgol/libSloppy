/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
