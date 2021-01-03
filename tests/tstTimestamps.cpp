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
#include "BasicTestClass.h"

using namespace Sloppy::DateTime;

TEST(Timestamps, testTimeConversion)
{
  // create a fake localtime object
  const auto testZone = date::locate_zone("Europe/Berlin");
  WallClockTimepoint_secs lt(date::year{2000} / 1 / 1, 12h, 0min, 0s, testZone);

  // convert to raw time
  time_t raw1 = lt.to_time_t();

  // construct UTC from this raw time
  WallClockTimepoint_secs utc(raw1);

  // convert to raw again
  time_t raw2 = utc.to_time_t();

  // both raws should be identical
  ASSERT_EQ(raw1, raw2);
}

//----------------------------------------------------------------------------

TEST(Timestamps, testEpoch)
{
  const auto tzCEST = date::locate_zone("Europe/Berlin");
  ASSERT_TRUE(tzCEST != nullptr);

  // create a fake localtime object (CEST)
  WallClockTimepoint_secs lt(date::year{2015} / 6 / 27, 12h, 0min, 0s, tzCEST);

  // 2015-06-27 is in summer time, so CEST applies
  //
  // CEST is 2 hours ahead of UTC / GMT, so the
  // equivalent UTC time is 2015-06-27, 10:00:00
  WallClockTimepoint_secs utc(date::year{2015} / 6 / 27, 10h, 0min, 0s);

  // the epoch value for this UTC date is, according
  // to an internet converter:
  time_t expectedEpochVal = 1435399200;

  // convert local time to raw
  time_t raw1 = lt.to_time_t();
  ASSERT_EQ(expectedEpochVal, raw1);

  // convert UTC time to raw
  time_t raw2 = utc.to_time_t();
  ASSERT_EQ(expectedEpochVal, raw2);

  // create a timestamp from the epoch value
  lt = WallClockTimepoint_secs(expectedEpochVal, tzCEST);
  ASSERT_EQ("2015-06-27 12:00:00", lt.timestampString());

}

//----------------------------------------------------------------------------

TEST(Timestamps, testGetters)
{
  // create a fake localtime object
  const auto testZone = date::locate_zone("Europe/Berlin");
  WallClockTimepoint_secs lt(date::year{2000} / 1 / 1, 8h, 3min, 2s, testZone);

  ASSERT_EQ("2000-01-01", lt.isoDateString());
  ASSERT_EQ("08:03:02", lt.timeString());
  ASSERT_EQ("2000-01-01 08:03:02", lt.timestampString());
  ASSERT_EQ(20000101, lt.ymdInt());
}

//----------------------------------------------------------------------------

TEST(Timestamps, testParseDateString)
{
  for (const auto& s : {"skjfh", "2000","20000","2000-", "2000-03","2000-05-","2000-05-sdfd","200-05-03", "2000-15-03"}) {
    ASSERT_FALSE(parseDateString(s));
  }
  
  const auto ymd = parseDateString("2000-05-03");
  ASSERT_TRUE(ymd);
  ASSERT_EQ(date::year(2000) / 5 / 3, ymd);  
}

//----------------------------------------------------------------------------

TEST(Timestamps, CommonTimestampSetTime)
{
  WallClockTimepoint_secs cs{date::year{2018} / 2 / 24, 12h, 0min, 0s};

  cs.setTimeSinceMidnight(13h,14min,15s);
  ASSERT_EQ("13:14:15", cs.timeString());

  cs.setTimeSinceMidnight(13h,14min,62s);
  ASSERT_EQ("13:15:02", cs.timeString());

  cs.setTimeSinceMidnight(25h,14min,59s);
  ASSERT_EQ("01:14:59", cs.timeString());  // NEXT DAY!!

  cs.setTimeSinceMidnight(-3h,14min,59s);
  ASSERT_EQ("21:14:59", cs.timeString());  // PREVIOUS DAY (relative) / original day (absolute)

  cs.setTimeSinceMidnight(0h,0min,0s);
  ASSERT_EQ("00:00:00", cs.timeString());

  cs.setTimeSinceMidnight(23h,59min,59s);
  ASSERT_EQ("23:59:59", cs.timeString());
}
