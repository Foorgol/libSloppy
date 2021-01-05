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

using namespace Sloppy::DateTime;

TEST(DateRanges, TestLengths)
{
  date::year_month_day s = date::year{2010} / 1 / 1;
  date::year_month_day e = date::year{2010} / 1 / 15;

  DateRange dr1{s, e};
  ASSERT_EQ(15, dr1.length_days()->count());
  ASSERT_EQ(15/7.0, dr1.length_weeks()->count());

  // Date periods that start and end on the same day
  // should have length "1"
  DateRange dr2{s,s};
  ASSERT_EQ(1, dr2.length_days()->count());
  
  // test a range that includes the February of a leap year
  date::year_month_day sLeap = date::year{2020} / 2 / 27;
  date::year_month_day eLeap = date::year{2020} / 3 / 1;
  DateRange dr3{sLeap, eLeap};
  ASSERT_EQ(4, dr3.length_days()->count());  // 27., 28., 29., 1.
  
}

