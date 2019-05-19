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

using IntPeriod = Sloppy::DateTime::GenericPeriod<int>;

TEST(GenericPeriod, Ctor)
{
  IntPeriod ip{3, 4};
  ASSERT_FALSE(ip.hasOpenEnd());
  ASSERT_EQ(3, ip.getStart());
  ASSERT_EQ(4, ip.getEnd());

  ASSERT_THROW(IntPeriod(4,3), std::invalid_argument);

  ip = IntPeriod{3,3};
  ASSERT_FALSE(ip.hasOpenEnd());

  ip = IntPeriod{4};
  ASSERT_TRUE(ip.hasOpenEnd());
}

//----------------------------------------------------------------------------

TEST(GenericPeriod, Relations)
{
  IntPeriod ip{3, 5};
  for (int i : {3,4,5}) ASSERT_TRUE(ip.isInPeriod(i));
  for (int i : {-1,2,6}) ASSERT_FALSE(ip.isInPeriod(i));

  for (int i : {-1,2,}) ASSERT_EQ(IntPeriod::Relation::isBefore, ip.determineRelationToPeriod(i));
  for (int i : {3,4,5}) ASSERT_EQ(IntPeriod::Relation::isIn, ip.determineRelationToPeriod(i));
  for (int i : {6,7,8}) ASSERT_EQ(IntPeriod::Relation::isAfter, ip.determineRelationToPeriod(i));

  IntPeriod ipOpen{3};
  for (int i : {3,4,5}) ASSERT_TRUE(ipOpen.isInPeriod(i));
  for (int i : {-1,2}) ASSERT_FALSE(ipOpen.isInPeriod(i));

  for (int i : {-1,2,}) ASSERT_EQ(IntPeriod::Relation::isBefore, ipOpen.determineRelationToPeriod(i));
  for (int i : {3,4,5,6,7,8}) ASSERT_EQ(IntPeriod::Relation::isIn, ipOpen.determineRelationToPeriod(i));

  ASSERT_FALSE(ip.startsEarlierThan(ipOpen));
  ASSERT_FALSE(ip.startsLaterThan(ipOpen));

  ipOpen = IntPeriod{4};
  ASSERT_TRUE(ip.startsEarlierThan(ipOpen));
  ASSERT_FALSE(ip.startsLaterThan(ipOpen));

  ipOpen = IntPeriod{1};
  ASSERT_FALSE(ip.startsEarlierThan(ipOpen));
  ASSERT_TRUE(ip.startsLaterThan(ipOpen));
}

//----------------------------------------------------------------------------

TEST(GenericPeriod, SettersGetters)
{
  IntPeriod ip{3, 5};
  ASSERT_EQ(3, ip.getStart());
  ASSERT_TRUE(ip.getEnd().has_value());
  ASSERT_EQ(5, ip.getEnd());

  ASSERT_FALSE(ip.setStart(6));
  ASSERT_EQ(3, ip.getStart());

  ASSERT_TRUE(ip.setStart(2));
  ASSERT_EQ(2, ip.getStart());

  ASSERT_FALSE(ip.setEnd(1));
  ASSERT_EQ(5, ip.getEnd());

  ASSERT_TRUE(ip.setEnd(6));
  ASSERT_EQ(6, ip.getEnd());

  ip = IntPeriod{666};
  ASSERT_EQ(666, ip.getStart());
  ASSERT_FALSE(ip.getEnd().has_value());
  ASSERT_TRUE(ip.hasOpenEnd());

  ASSERT_FALSE(ip.setEnd(665));
  ASSERT_FALSE(ip.getEnd().has_value());
  ASSERT_TRUE(ip.hasOpenEnd());

  ASSERT_TRUE(ip.setEnd(666));
  ASSERT_EQ(666, ip.getEnd());
  ASSERT_FALSE(ip.hasOpenEnd());
}

//----------------------------------------------------------------------------

