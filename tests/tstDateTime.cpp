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

#include <string>
#include <iostream>

#include <gtest/gtest.h>

#include "../Sloppy/DateTime/DateAndTime.h"

using namespace std;
using namespace Sloppy::DateTime;

TEST(CommonTimestamp, ValidDate)
{
  ASSERT_FALSE(isValidDate(2000, 0, 10));
  ASSERT_FALSE(isValidDate(2000, 10, 0));
  ASSERT_FALSE(isValidDate(1900, 2, 29));
  ASSERT_TRUE(isValidDate(2000, 2, 29));
  ASSERT_FALSE(isValidDate(2000, 2, 30));
  ASSERT_FALSE(isValidDate(2000, 4, 31));
  ASSERT_FALSE(isValidDate(2000, 6, 31));
  ASSERT_FALSE(isValidDate(2000, 9, 31));
  ASSERT_FALSE(isValidDate(2000, 11, 31));
  ASSERT_FALSE(isValidDate(2000, 1, 32));
  ASSERT_FALSE(isValidDate(2000, 3, 32));
  ASSERT_FALSE(isValidDate(2000, 5, 32));
  ASSERT_FALSE(isValidDate(2000, 7, 32));
  ASSERT_FALSE(isValidDate(2000, 8, 32));
  ASSERT_FALSE(isValidDate(2000, 10, 32));
  ASSERT_FALSE(isValidDate(2000, 12, 32));
  ASSERT_TRUE(isValidDate(2000, 3, 26));
}

//----------------------------------------------------------------------------

TEST(CommonTimestamp, Comparison)
{
  WallClockTimepoint_ms t1(date::year{2009} / 01 / 01, 0h, 0min, 9s, "Europe/Berlin");
  WallClockTimepoint_ms t2(date::year{2009} / 01 / 01, 0h, 0min, 10s, "Europe/Berlin");
  WallClockTimepoint_ms t2a(date::year{2009} / 01 / 01, 0h, 0min, 10s, "Europe/Berlin");

  // less than
  ASSERT_TRUE(t1 < t2);
  ASSERT_FALSE(t2 < t1);
  ASSERT_FALSE(t2 < t2a);
  ASSERT_FALSE(t2 < t2);

  // greater than
  ASSERT_TRUE(t2 > t1);
  ASSERT_FALSE(t1 > t2);
  ASSERT_FALSE(t2 > t2a);
  ASSERT_FALSE(t2 > t2);

  // less or equal
  ASSERT_TRUE(t1 <= t2);
  ASSERT_FALSE(t2 <= t1);
  ASSERT_TRUE(t2 <= t2a);
  ASSERT_TRUE(t2 <= t2);

  // greater or equal
  ASSERT_TRUE(t2 >= t1);
  ASSERT_FALSE(t1 >= t2);
  ASSERT_TRUE(t2 >= t2a);
  ASSERT_TRUE(t2 >= t2);

  // equal
  ASSERT_FALSE(t2 == t1);
  ASSERT_FALSE(t1 == t2);
  ASSERT_TRUE(t2 == t2a);
  ASSERT_TRUE(t2 == t2);

  // not equal
  ASSERT_TRUE(t2 != t1);
  ASSERT_TRUE(t1 != t2);
  ASSERT_FALSE(t2 != t2a);
  ASSERT_FALSE(t2 != t2);
}

//----------------------------------------------------------------------------

TEST(CommonTimestamp, DateFromString)
{
  auto d = parseDateString("01.02.2012", "%d.%m.%Y");
  ASSERT_TRUE(d);
  ASSERT_TRUE(d->ok());
  ASSERT_EQ(date::day{1}, d->day());
  ASSERT_EQ(date::month{2}, d->month());
  ASSERT_EQ(date::year{2012}, d->year());
  ASSERT_EQ(date::year{2012} / 2 / 1, d);

  d = parseDateString("2016-04-23");
  ASSERT_TRUE(d);
  ASSERT_TRUE(d->ok());
  ASSERT_EQ(date::day{23}, d->day());
  ASSERT_EQ(date::month{4}, d->month());
  ASSERT_EQ(date::year{2016}, d->year());
  ASSERT_EQ(date::year{2016} / 4 / 23, d);
  
  d = parseDateString("01x02.2012", "%d.%m.%Y");
  ASSERT_FALSE(d);

  d = parseDateString("kjsfdgjkdfhg", "%d.%m.%Y");
  ASSERT_FALSE(d);
  
  d = parseDateString("2016.02.17", "%d.%m.%Y");
  ASSERT_FALSE(d);
}

//----------------------------------------------------------------------------

TEST(DateTimeFuncs, Conversion)
{
  time_t raw = time(nullptr);
  const auto tzp = date::locate_zone("Europe/Berlin");
  ASSERT_TRUE(tzp != nullptr);
  
  WallClockTimepoint_secs nowUtc;
  WallClockTimepoint_secs nowLocal{tzp};
  ASSERT_EQ(raw, nowUtc.to_time_t());
  ASSERT_EQ(raw, nowLocal.to_time_t());
  
  const WallClockTimepoint_secs localTime{date::year{2000} / 1 / 1, 0h, 30min, 0s, "Europe/Berlin"};
  const WallClockTimepoint_secs utcTime{localTime.utc()};
  ASSERT_EQ(localTime, utcTime);
  const auto localDate = localTime.ymd();
  const auto utcDate = utcTime.ymd();
  ASSERT_NE(localDate.year(), utcDate.year());
  ASSERT_NE(localDate.month(), utcDate.month());
  ASSERT_NE(localDate.day(), utcDate.day());
}
