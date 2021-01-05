/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2021  Volker Knollmann
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

TEST(WallclockTimepoint, ctorNow)
{
  // trigger loading the TZ database so that
  // subsequent zone lookups finish immediately
  const auto tzPtr = date::locate_zone("Europe/Berlin");
  
  const auto rawNow = time(nullptr);
  WallClockTimepoint_secs now1;
  WallClockTimepoint_secs now2{"Europe/Berlin"};
  WallClockTimepoint_secs now3{tzPtr};
  
  ASSERT_EQ(rawNow, now1.to_time_t());
  ASSERT_FALSE(now1.usesLocalTime());
  ASSERT_TRUE(now1.usesUnixTime());
  
  ASSERT_EQ(rawNow, now2.to_time_t());
  ASSERT_TRUE(now2.usesLocalTime());
  ASSERT_FALSE(now2.usesUnixTime());
  
  ASSERT_EQ(rawNow, now3.to_time_t());
  ASSERT_TRUE(now3.usesLocalTime());
  ASSERT_FALSE(now3.usesUnixTime());
  
  // now2 and now3 should contain the same local time
  const auto hms2 = now2.hms();
  const auto hms3 = now3.hms();
  ASSERT_EQ(std::get<0>(hms2), std::get<0>(hms3));
  ASSERT_EQ(std::get<1>(hms2), std::get<1>(hms3));
  ASSERT_EQ(std::get<2>(hms2), std::get<2>(hms3));
  
  // now 1 and now2 should contain different hour values,
  // due to the time offset between Germany and UTC.
  // Minutes and seconds should be identical
  const auto hms1 = now1.hms();
  ASSERT_NE(std::get<0>(hms2), std::get<0>(hms1));
  ASSERT_EQ(std::get<1>(hms2), std::get<1>(hms1));
  ASSERT_EQ(std::get<2>(hms2), std::get<2>(hms1));
  
  // all three objects should represent the same
  // physical timepoint
  ASSERT_EQ(now1, now2);
  ASSERT_EQ(now2, now3);
  ASSERT_EQ(now1, now3);
  
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, ctorFromTimeT)
{
  // trigger loading the TZ database so that
  // subsequent zone lookups finish immediately
  const auto tzPtr = date::locate_zone("Europe/Berlin");
  
  const auto raw = time(nullptr) + 424242;
  WallClockTimepoint_secs r1{raw};
  WallClockTimepoint_secs r2{raw, "Europe/Berlin"};
  WallClockTimepoint_secs r3{raw, tzPtr};
  
  ASSERT_EQ(raw, r1.to_time_t());
  ASSERT_FALSE(r1.usesLocalTime());
  ASSERT_TRUE(r1.usesUnixTime());
  
  ASSERT_EQ(raw, r2.to_time_t());
  ASSERT_TRUE(r2.usesLocalTime());
  ASSERT_FALSE(r2.usesUnixTime());
  
  ASSERT_EQ(raw, r3.to_time_t());
  ASSERT_TRUE(r3.usesLocalTime());
  ASSERT_FALSE(r3.usesUnixTime());
  
  // now2 and now3 should contain the same local time
  const auto hms2 = r2.hms();
  const auto hms3 = r3.hms();
  ASSERT_EQ(std::get<0>(hms2), std::get<0>(hms3));
  ASSERT_EQ(std::get<1>(hms2), std::get<1>(hms3));
  ASSERT_EQ(std::get<2>(hms2), std::get<2>(hms3));
  
  // now 1 and now2 should contain different hour values,
  // due to the time offset between Germany and UTC.
  // Minutes and seconds should be identical
  const auto hms1 = r1.hms();
  ASSERT_NE(std::get<0>(hms2), std::get<0>(hms1));
  ASSERT_EQ(std::get<1>(hms2), std::get<1>(hms1));
  ASSERT_EQ(std::get<2>(hms2), std::get<2>(hms1));
  
  // all three objects should represent the same
  // physical timepoint
  ASSERT_EQ(r1, r2);
  ASSERT_EQ(r2, r3);
  ASSERT_EQ(r1, r3);
  
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, ctorFromTimepoint)
{
  // trigger loading the TZ database so that
  // subsequent zone lookups finish immediately
  const auto tzPtr = date::locate_zone("Europe/Berlin");
  
  const auto tp = std::chrono::system_clock::now() + 666s;
  WallClockTimepoint<> r1{tp};
  WallClockTimepoint<> r2{tp, tzPtr};
  
  ASSERT_EQ(std::chrono::time_point_cast<std::chrono::seconds>(tp).time_since_epoch().count(), r1.to_time_t());
  ASSERT_FALSE(r1.usesLocalTime());
  ASSERT_TRUE(r1.usesUnixTime());
  
  ASSERT_EQ(std::chrono::time_point_cast<std::chrono::seconds>(tp).time_since_epoch().count(), r2.to_time_t());
  ASSERT_TRUE(r2.usesLocalTime());
  ASSERT_FALSE(r2.usesUnixTime());
  
  // r1 and r2 should contain different hour values,
  // due to the time offset between Germany and UTC.
  // Minutes and seconds should be identical
  const auto hms1 = r1.hms();
  const auto hms2 = r2.hms();
  ASSERT_NE(std::get<0>(hms2), std::get<0>(hms1));
  ASSERT_EQ(std::get<1>(hms2), std::get<1>(hms1));
  ASSERT_EQ(std::get<2>(hms2), std::get<2>(hms1));
  
  // however they should represent the same
  // physical timepoint
  ASSERT_EQ(r1, r2);  
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, ctorFromDateTime)
{
  // trigger loading the TZ database so that
  // subsequent zone lookups finish immediately
  const auto tzPtr = date::locate_zone("Europe/Berlin");
  
  // ctor with local date/time values
  const WallClockTimepoint_secs w1{
    date::year{2020} / 12 / 21,
    17h, 59min, 20s,
    tzPtr
  };
  ASSERT_TRUE(w1.usesLocalTime());
  ASSERT_FALSE(w1.usesUnixTime());

  // read the values back and make sure
  // they're still the same
  //
  // Using old-style type casts for brevity...
  const auto ymd1 = w1.ymd();
  ASSERT_EQ(2020, (int)ymd1.year());
  ASSERT_EQ(12, (unsigned)ymd1.month());
  ASSERT_EQ(21, (unsigned)ymd1.day());
  const auto hms1 = w1.hms();
  ASSERT_EQ(17h, std::get<0>(hms1));
  ASSERT_EQ(59min, std::get<1>(hms1));
  ASSERT_EQ(20s, std::get<2>(hms1));
  
  // construct the same timepoint in UTC which
  // is one hour behind
  const WallClockTimepoint_secs w2{
    date::year{2020} / 12 / 21,
    16h, 59min, 20s
  };
  ASSERT_FALSE(w2.usesLocalTime());
  ASSERT_TRUE(w2.usesUnixTime());
  
  // read the values back and make sure
  // they're still the same
  //
  // Using old-style type casts for brevity...
  const auto ymd2 = w2.ymd();
  ASSERT_EQ(2020, (int)ymd1.year());
  ASSERT_EQ(12, (unsigned)ymd1.month());
  ASSERT_EQ(21, (unsigned)ymd1.day());
  const auto hms2 = w2.hms();
  ASSERT_EQ(16h, std::get<0>(hms2));
  ASSERT_EQ(59min, std::get<1>(hms2));
  ASSERT_EQ(20s, std::get<2>(hms2));
  
  // construct using the timezone name
  // instead of a direct pointer
  const WallClockTimepoint_secs w3{
    date::year{2020} / 12 / 21,
    17h, 59min, 20s,
    "Europe/Berlin"
  };
  ASSERT_TRUE(w3.usesLocalTime());
  ASSERT_FALSE(w3.usesUnixTime());
  
  // read the values back and make sure
  // they're still the same
  //
  // Using old-style type casts for brevity...
  const auto ymd3 = w3.ymd();
  ASSERT_EQ(2020, (int)ymd3.year());
  ASSERT_EQ(12, (unsigned)ymd3.month());
  ASSERT_EQ(21, (unsigned)ymd3.day());
  const auto hms3 = w3.hms();
  ASSERT_EQ(17h, std::get<0>(hms3));
  ASSERT_EQ(59min, std::get<1>(hms3));
  ASSERT_EQ(20s, std::get<2>(hms3));
  
  // assert equality of all timestamps
  ASSERT_EQ(w1, w2);
  ASSERT_EQ(w1, w3);
  ASSERT_EQ(w3, w2);
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, isoDateTimeOut)
{
  // trigger loading the TZ database so that
  // subsequent zone lookups finish immediately
  const auto tzPtr = date::locate_zone("Europe/Berlin");
  
  // ctor with local date/time values
  const WallClockTimepoint_secs w1{
    date::year{2020} / 1 / 1,
    0h, 2min, 3s,
    tzPtr
  };
  
  // make sure we get the local date although the
  // UTC date is still in the previous year
  ASSERT_EQ("2020-01-01", w1.isoDateString());
  ASSERT_EQ("00:02:03", w1.timeString());
  ASSERT_EQ("2020-01-01 00:02:03", w1.timestampString());
  
  // construct the same date in UTC
  const WallClockTimepoint_secs w2{
    date::year{2020} / 1 / 1,
    0h, 2min, 3s
  };
  ASSERT_EQ("2020-01-01", w2.isoDateString());
  ASSERT_EQ("00:02:03", w2.timeString());
  ASSERT_EQ("2020-01-01 00:02:03", w2.timestampString());
  
  // both timestamps denote different points in time
  ASSERT_NE(w1, w2);
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, sinceMidnight)
{
  // trigger loading the TZ database so that
  // subsequent zone lookups finish immediately
  const auto tzPtr = date::locate_zone("Europe/Berlin");
  
  // ctor with local date/time values
  const WallClockTimepoint_secs w1{
    date::year{2020} / 12 / 21,
    18h, 24min, 42s,
    tzPtr
  };
  ASSERT_EQ(18h + 24min + 42s, w1.sinceMidnight());
  
  // ctor with UTC
  const WallClockTimepoint_secs w2{
    date::year{2020} / 12 / 21,
    18h, 24min, 42s
  };
  ASSERT_EQ(18h + 24min + 42s, w2.sinceMidnight());
  
  // both timestamps denote different points in time
  ASSERT_NE(w1, w2);
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, offsets)
{
  // ctor with UTC
  WallClockTimepoint_secs w1{
    date::year{2020} / 12 / 21,
    18h, 28min, 11s
  };
  ASSERT_EQ(18h + 28min + 11s, w1.sinceMidnight());
  
  w1.applyOffset(5min);
  ASSERT_EQ(18h + 33min + 11s, w1.sinceMidnight());
  
  // go past midnight --> next day
  w1.applyOffset(7h + 2min);
  ASSERT_EQ(1h + 35min + 11s, w1.sinceMidnight());
  
  // negative offsets
  w1.applyOffset(-12s);
  ASSERT_EQ(1h + 34min + 59s, w1.sinceMidnight());
  
  // operators "+=" and "-="
  w1 += 10min;
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, setTimeSinceMidnight)
{
  // ctor with UTC
  WallClockTimepoint_secs w1{
    date::year{2021} / 1 / 3,
    13h, 10min, 42s
  };
  ASSERT_EQ(13h + 10min + 42s, w1.sinceMidnight());
  
  w1.setTimeSinceMidnight(1h, 2min, 3s);
  ASSERT_EQ(1h + 2min + 3s, w1.sinceMidnight());  
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, getYMD)
{
  // ctor with local time
  WallClockTimepoint_secs w1{
    date::year{2021} / 1 / 1,
    0h, 1min, 2s,
    "Europe/Berlin"
  };
  const auto ymd1 = w1.ymd();
  ASSERT_EQ(2021, static_cast<int>(ymd1.year()));
  ASSERT_EQ(1, static_cast<unsigned>(ymd1.month()));
  ASSERT_EQ(1, static_cast<unsigned>(ymd1.day()));
  ASSERT_EQ(20210101, w1.ymdInt());
  ASSERT_EQ(5, w1.dow());  // 2021-01-01 was a Friday
  
  // convert to unix time
  //
  // YMD in unix time should be "2020 / 12 /31"
  WallClockTimepoint_secs w2{w1.to_time_t()};
  const auto ymd2 = w2.ymd();
  ASSERT_EQ(2020, static_cast<int>(ymd2.year()));
  ASSERT_EQ(12, static_cast<unsigned>(ymd2.month()));
  ASSERT_EQ(31, static_cast<unsigned>(ymd2.day()));
  ASSERT_EQ(20201231, w2.ymdInt());
  ASSERT_EQ(4, w2.dow());  // 2020-12-31 was a Thursday
}

//-------------------------------------------------------------------------------

TEST(WallclockTimepoint, comparisons)
{
  // construct two timepoints for comparison
  WallClockTimepoint_secs w1{
    date::year{2021} / 1 / 3,
    13h, 40min, 0s,
  };
  WallClockTimepoint_secs w2{
    date::year{2021} / 1 / 3,
    13h, 41min, 0s,
  };
  
  // less than
  ASSERT_TRUE(w1 < w2);
  ASSERT_FALSE(w2 < w1);
  ASSERT_FALSE(w2 < w2);
  
  // greater than
  ASSERT_FALSE(w1 > w2);
  ASSERT_TRUE(w2 > w1);
  ASSERT_FALSE(w2 > w2);
  
  // greater or equal
  ASSERT_FALSE(w1 >= w2);
  ASSERT_TRUE(w2 >= w1);
  ASSERT_TRUE(w2 >= w2);
  
  // less or equal
  ASSERT_TRUE(w1 <= w2);
  ASSERT_FALSE(w2 <= w1);
  ASSERT_TRUE(w2 <= w2);
  
  // equal
  ASSERT_FALSE(w1 == w2);
  ASSERT_TRUE(w2 == w2);
  ASSERT_TRUE(w1 != w2);
  ASSERT_FALSE(w2 != w2);
}

