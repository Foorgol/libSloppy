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

#include <string>
#include <iostream>

#include <boost/date_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/local_time/local_time.hpp>

#include <gtest/gtest.h>

#include "../Sloppy/DateTime/DateAndTime.h"

using namespace std;
using namespace Sloppy::DateTime;

TEST(CommonTimestamp, ValidDate)
{
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 0, 10));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 10, 0));
  ASSERT_FALSE(CommonTimestamp::isValidDate(1900, 2, 29));
  ASSERT_TRUE(CommonTimestamp::isValidDate(2000, 2, 29));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 2, 30));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 4, 31));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 6, 31));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 9, 31));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 11, 31));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 1, 32));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 3, 32));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 5, 32));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 7, 32));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 8, 32));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 10, 32));
  ASSERT_FALSE(CommonTimestamp::isValidDate(2000, 12, 32));
  ASSERT_TRUE(CommonTimestamp::isValidDate(2000, 3, 26));
}

//----------------------------------------------------------------------------

TEST(CommonTimestamp, Comparison)
{
  using namespace boost::local_time;
  time_zone_ptr testZone{new posix_time_zone("TST-01:00:00")};

  LocalTimestamp t1(2000, 01, 01, 0, 0, 9, testZone);
  LocalTimestamp t2(2000, 01, 01, 0, 0, 10, testZone);
  LocalTimestamp t2a(2000, 01, 01, 0, 0, 10, testZone);

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

TEST(CommonTimestamp, BoostTime)
{
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  // make sure that boost's ptime uses UTC by default
  ptime ptUtc(date(1970, 1, 1));
  ASSERT_TRUE(to_time_t(ptUtc) == 0);

  // create a dummy time zone (UTC+2, no DST)
  typedef boost::date_time::local_adjustor<ptime, 2, no_dst> dummyAdjustor;

  // convert the ptUtc to local time
  ptime ptLocal = dummyAdjustor::utc_to_local(ptUtc);
  time_duration td = ptLocal.time_of_day();
  ASSERT_TRUE(td.hours() == 2);
  cout << ptLocal << endl;
}

//----------------------------------------------------------------------------

TEST(CommonTimestamp, DateFromString)
{
  using namespace boost::gregorian;

  date d = parseDateString("01.02.2012", "%d.%m.%Y");
  ASSERT_FALSE(d.is_special());
  ASSERT_EQ(1, d.day());
  ASSERT_EQ(2, d.month());
  ASSERT_EQ(2012, d.year());

  d = parseDateString("2016-04-23");
  ASSERT_FALSE(d.is_special());
  ASSERT_EQ(23, d.day());
  ASSERT_EQ(4, d.month());
  ASSERT_EQ(2016, d.year());

  d = parseDateString("01x02.2012", "%d.%m.%Y");
  ASSERT_TRUE(d.is_not_a_date());

  d = parseDateString("kjsfdgjkdfhg", "%d.%m.%Y");
  ASSERT_TRUE(d.is_not_a_date());

  d = parseDateString("2016.02.17", "%d.%m.%Y");
  ASSERT_TRUE(d.is_not_a_date());
}

//----------------------------------------------------------------------------

TEST(DateTimeFuncs, PopulatedTzDatabase)
{
  auto db = Sloppy::DateTime::getPopulatedTzDatabase();

  ASSERT_TRUE(db.region_list().size() > 0);

  auto tst = db.time_zone_from_region("Europe/Berlin");
  ASSERT_TRUE(tst != nullptr);
  ASSERT_TRUE(tst->has_dst());
}
