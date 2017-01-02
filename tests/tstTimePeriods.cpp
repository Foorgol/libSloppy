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

TEST(TimePeriods, TestConstruction)
{
  UTCTimestamp now;
  UTCTimestamp beforeNow{19900701};

  // create a valid closed TimePeriod
  TimePeriod tp{beforeNow, now};
  ASSERT_FALSE(tp.hasOpenEnd());
  ASSERT_EQ(beforeNow, tp.getStart());
  ASSERT_EQ(now, *tp.getEnd());

  // create a valid open TimePeriod
  tp = TimePeriod{now};
  ASSERT_TRUE(tp.hasOpenEnd());
  ASSERT_EQ(now, tp.getStart());
  ASSERT_EQ(nullptr, tp.getEnd());

  // create invalid period
  ASSERT_ANY_THROW(TimePeriod(now, beforeNow));

  // a null-duration period, which is legit
  ASSERT_NO_THROW(TimePeriod(now, now));
}

//----------------------------------------------------------------------------

TEST(TimePeriods, TestRelations)
{
  UTCTimestamp s{20100101};
  UTCTimestamp e{20110101};
  UTCTimestamp before{20091231};
  UTCTimestamp after{20110102};
  UTCTimestamp inbetween{20100701};

  // create a valid closed TimePeriod
  TimePeriod cp{s, e};
  ASSERT_FALSE(cp.hasOpenEnd());
  ASSERT_EQ(s, cp.getStart());
  ASSERT_EQ(e, *cp.getEnd());

  // create a valid open TimePeriod
  TimePeriod op{s};
  ASSERT_TRUE(op.hasOpenEnd());
  ASSERT_EQ(s, op.getStart());
  ASSERT_EQ(nullptr, op.getEnd());

  // test relations for closed periods
  ASSERT_FALSE(cp.isInPeriod(before));
  ASSERT_TRUE(cp.isInPeriod(s));
  ASSERT_TRUE(cp.isInPeriod(inbetween));
  ASSERT_TRUE(cp.isInPeriod(e));
  ASSERT_FALSE(cp.isInPeriod(after));
  ASSERT_EQ(TimePeriod::Relation::isBefore, cp.determineRelationToPeriod(before));
  ASSERT_EQ(TimePeriod::Relation::isIn, cp.determineRelationToPeriod(s));
  ASSERT_EQ(TimePeriod::Relation::isIn, cp.determineRelationToPeriod(inbetween));
  ASSERT_EQ(TimePeriod::Relation::isIn, cp.determineRelationToPeriod(e));
  ASSERT_EQ(TimePeriod::Relation::isAfter, cp.determineRelationToPeriod(after));
  ASSERT_FALSE(cp.startsEarlierThan(before));
  ASSERT_FALSE(cp.startsEarlierThan(s));
  ASSERT_TRUE(cp.startsEarlierThan(inbetween));
  ASSERT_TRUE(cp.startsLaterThan(before));
  ASSERT_FALSE(cp.startsLaterThan(s));
  ASSERT_FALSE(cp.startsLaterThan(inbetween));

  // test relations for open periods
  ASSERT_FALSE(op.isInPeriod(before));
  ASSERT_TRUE(op.isInPeriod(s));
  ASSERT_TRUE(op.isInPeriod(inbetween));
  ASSERT_TRUE(op.isInPeriod(e));
  ASSERT_TRUE(op.isInPeriod(after));
  ASSERT_EQ(TimePeriod::Relation::isBefore, op.determineRelationToPeriod(before));
  ASSERT_EQ(TimePeriod::Relation::isIn, op.determineRelationToPeriod(s));
  ASSERT_EQ(TimePeriod::Relation::isIn, op.determineRelationToPeriod(inbetween));
  ASSERT_EQ(TimePeriod::Relation::isIn, op.determineRelationToPeriod(e));
  ASSERT_EQ(TimePeriod::Relation::isIn, op.determineRelationToPeriod(after));
  ASSERT_FALSE(op.startsEarlierThan(before));
  ASSERT_FALSE(op.startsEarlierThan(s));
  ASSERT_TRUE(op.startsEarlierThan(inbetween));
  ASSERT_TRUE(op.startsLaterThan(before));
  ASSERT_FALSE(op.startsLaterThan(s));
  ASSERT_FALSE(op.startsLaterThan(inbetween));
}

//----------------------------------------------------------------------------

TEST(TimePeriods, TestLengths)
{
  UTCTimestamp s{20100101, 12, 0, 0};
  UTCTimestamp e{20100101, 12, 30, 30};

  TimePeriod cp{s, e};
  ASSERT_EQ(30*60+30, cp.getLength_Sec());
  ASSERT_EQ(30.5, cp.getLength_Minutes());

  TimePeriod op{s};
  ASSERT_EQ(-1, op.getLength_Sec());
  ASSERT_EQ(-1, op.getLength_Minutes());
}

//----------------------------------------------------------------------------

TEST(TimePeriods, TestBoostConversion)
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
