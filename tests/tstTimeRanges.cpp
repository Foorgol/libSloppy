/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

TEST(TimeRanges, TestConstruction)
{
  WallClockTimepoint_secs now;
  WallClockTimepoint_secs beforeNow{date::year{1990} / 7 / 1};

  // create a valid closed TimeRange_secs
  TimeRange_secs tp{beforeNow, now};
  ASSERT_FALSE(tp.hasOpenEnd());
  ASSERT_EQ(beforeNow, tp.getStart());
  ASSERT_EQ(now, *tp.getEnd());

  // create a valid open TimeRange_secs
  tp = TimeRange_secs{now};
  ASSERT_TRUE(tp.hasOpenEnd());
  ASSERT_EQ(now, tp.getStart());
  ASSERT_FALSE(tp.getEnd().has_value());

  // create invalid period
  ASSERT_ANY_THROW(TimeRange_secs(now, beforeNow));

  // a null-duration period, which is legit
  ASSERT_NO_THROW(TimeRange_secs(now, now));
}

//----------------------------------------------------------------------------

TEST(TimeRanges, TestRelations)
{
  WallClockTimepoint_secs s{date::year{2010} / 01 / 01};
  WallClockTimepoint_secs e{date::year{2011} / 01 / 01};
  WallClockTimepoint_secs before{date::year{2009} / 12 / 31};
  WallClockTimepoint_secs after{date::year{2011} / 01 / 02};
  WallClockTimepoint_secs inbetween{date::year{2010} / 07 / 01};

  // create a valid closed TimeRange_secs
  TimeRange_secs cp{s, e};
  ASSERT_FALSE(cp.hasOpenEnd());
  ASSERT_EQ(s, cp.getStart());
  ASSERT_EQ(e, *cp.getEnd());

  // create a valid open TimeRange_secs
  TimeRange_secs op{s};
  ASSERT_TRUE(op.hasOpenEnd());
  ASSERT_EQ(s, op.getStart());
  ASSERT_FALSE(op.getEnd().has_value());

  // test relations for closed periods
  ASSERT_FALSE(cp.isInRange(before));
  ASSERT_TRUE(cp.isInRange(s));
  ASSERT_TRUE(cp.isInRange(inbetween));
  ASSERT_TRUE(cp.isInRange(e));
  ASSERT_FALSE(cp.isInRange(after));
  ASSERT_EQ(Sloppy::RelationToRange::isBefore, cp.determineRelationToRange(before));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, cp.determineRelationToRange(s));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, cp.determineRelationToRange(inbetween));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, cp.determineRelationToRange(e));
  ASSERT_EQ(Sloppy::RelationToRange::isAfter, cp.determineRelationToRange(after));
  ASSERT_FALSE(cp.startsEarlierThan(before));
  ASSERT_FALSE(cp.startsEarlierThan(s));
  ASSERT_TRUE(cp.startsEarlierThan(inbetween));
  ASSERT_TRUE(cp.startsLaterThan(before));
  ASSERT_FALSE(cp.startsLaterThan(s));
  ASSERT_FALSE(cp.startsLaterThan(inbetween));

  // test relations for open periods
  ASSERT_FALSE(op.isInRange(before));
  ASSERT_TRUE(op.isInRange(s));
  ASSERT_TRUE(op.isInRange(inbetween));
  ASSERT_TRUE(op.isInRange(e));
  ASSERT_TRUE(op.isInRange(after));
  ASSERT_EQ(Sloppy::RelationToRange::isBefore, op.determineRelationToRange(before));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, op.determineRelationToRange(s));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, op.determineRelationToRange(inbetween));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, op.determineRelationToRange(e));
  ASSERT_EQ(Sloppy::RelationToRange::isIn, op.determineRelationToRange(after));
  ASSERT_FALSE(op.startsEarlierThan(before));
  ASSERT_FALSE(op.startsEarlierThan(s));
  ASSERT_TRUE(op.startsEarlierThan(inbetween));
  ASSERT_TRUE(op.startsLaterThan(before));
  ASSERT_FALSE(op.startsLaterThan(s));
  ASSERT_FALSE(op.startsLaterThan(inbetween));
}

//----------------------------------------------------------------------------

TEST(TimeRanges, TestLengths)
{
  using namespace std::chrono_literals;
  
  WallClockTimepoint_secs s{date::year{2010} / 01 / 01, 12h, 0min, 0s};
  WallClockTimepoint_secs e{date::year{2010} / 01 / 01, 12h, 30min, 30s};

  TimeRange_secs cp{s, e};
  ASSERT_EQ(30*60s+30s, cp.length_secs());
  ASSERT_EQ(30.5min, cp.length_minutes());

  TimeRange_secs op{s};
  ASSERT_FALSE(op.length_secs());
  ASSERT_FALSE(op.length_minutes());
}

//----------------------------------------------------------------------------

TEST(TimeRanges, TestOffsets)
{
  using namespace std::chrono_literals;
  
  WallClockTimepoint_secs s{date::year{2010} / 01 / 01, 12h, 0min, 0s};
  WallClockTimepoint_secs e{date::year{2010} / 01 / 01, 14h, 0min, 0s};
  auto openR = TimeRange_secs{s};   // open range
  auto closedR = TimeRange_secs{s, e};  // closed range
  
  // test offsets on open ranges
  ASSERT_TRUE(openR.applyOffsetToStart(1h));
  ASSERT_EQ(WallClockTimepoint_secs(date::year(2010) / 01 / 01, 13h, 0min, 0s), openR.getStart());
  ASSERT_FALSE(openR.applyOffsetToEnd(20s));
  ASSERT_EQ(WallClockTimepoint_secs(date::year(2010) / 01 / 01, 13h, 0min, 0s), openR.getStart());
  
  // test offsets on closed ranges
  ASSERT_TRUE(closedR.applyOffsetToStart(1h));
  ASSERT_EQ(WallClockTimepoint_secs(date::year(2010) / 01 / 01, 13h, 0min, 0s), closedR.getStart());
  ASSERT_TRUE(closedR.applyOffsetToStart(1h));
  ASSERT_EQ(e, closedR.getStart());
  ASSERT_FALSE(closedR.applyOffsetToStart(1s));
  ASSERT_EQ(e, closedR.getStart());
  ASSERT_TRUE(closedR.applyOffsetToStart(-2h));
  ASSERT_EQ(s, closedR.getStart());
  
  ASSERT_TRUE(closedR.applyOffsetToEnd(1h));
  ASSERT_EQ(WallClockTimepoint_secs(date::year(2010) / 01 / 01, 15h, 0min, 0s), closedR.getEnd());
  ASSERT_TRUE(closedR.applyOffsetToEnd(-3h));
  ASSERT_EQ(s, closedR.getEnd());
  ASSERT_FALSE(closedR.applyOffsetToEnd(-1s));
  ASSERT_EQ(s, closedR.getEnd());
}
  
//----------------------------------------------------------------------------
