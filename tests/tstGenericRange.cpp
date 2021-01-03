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

#include "../Sloppy/GenericRange.h"

TEST(GenericRange, Ctor)
{
  Sloppy::IntRange ir{3, 4};
  ASSERT_FALSE(ir.hasOpenEnd());
  ASSERT_EQ(3, ir.getStart());
  ASSERT_EQ(4, ir.getEnd());

  ASSERT_THROW(Sloppy::IntRange(4,3), std::invalid_argument);

  ir = Sloppy::IntRange{3,3};
  ASSERT_FALSE(ir.hasOpenEnd());

  ir = Sloppy::IntRange{4};
  ASSERT_TRUE(ir.hasOpenEnd());
}

//----------------------------------------------------------------------------

TEST(GenericRange, Relations)
{
  Sloppy::IntRange ir{3, 5};
  for (int i : {3,4,5}) ASSERT_TRUE(ir.isInRange(i));
  for (int i : {-1,2,6}) ASSERT_FALSE(ir.isInRange(i));

  for (int i : {-1,2,}) ASSERT_EQ(Sloppy::RelationToRange::isBefore, ir.determineRelationToRange(i));
  for (int i : {3,4,5}) ASSERT_EQ(Sloppy::RelationToRange::isIn, ir.determineRelationToRange(i));
  for (int i : {6,7,8}) ASSERT_EQ(Sloppy::RelationToRange::isAfter, ir.determineRelationToRange(i));

  Sloppy::IntRange irOpen{3};
  for (int i : {3,4,5}) ASSERT_TRUE(irOpen.isInRange(i));
  for (int i : {-1,2}) ASSERT_FALSE(irOpen.isInRange(i));

  for (int i : {-1,2,}) ASSERT_EQ(Sloppy::RelationToRange::isBefore, irOpen.determineRelationToRange(i));
  for (int i : {3,4,5,6,7,8}) ASSERT_EQ(Sloppy::RelationToRange::isIn, irOpen.determineRelationToRange(i));

  ASSERT_FALSE(ir.startsEarlierThan(irOpen));
  ASSERT_FALSE(ir.startsLaterThan(irOpen));

  irOpen = Sloppy::IntRange{4};
  ASSERT_TRUE(ir.startsEarlierThan(irOpen));
  ASSERT_FALSE(ir.startsLaterThan(irOpen));

  irOpen = Sloppy::IntRange{1};
  ASSERT_FALSE(ir.startsEarlierThan(irOpen));
  ASSERT_TRUE(ir.startsLaterThan(irOpen));
}

//----------------------------------------------------------------------------

TEST(GenericRange, SettersGetters)
{
  Sloppy::IntRange ir{3, 5};
  ASSERT_EQ(3, ir.getStart());
  ASSERT_TRUE(ir.getEnd().has_value());
  ASSERT_EQ(5, ir.getEnd());

  ASSERT_FALSE(ir.setStart(6));
  ASSERT_EQ(3, ir.getStart());

  ASSERT_TRUE(ir.setStart(2));
  ASSERT_EQ(2, ir.getStart());

  ASSERT_FALSE(ir.setEnd(1));
  ASSERT_EQ(5, ir.getEnd());

  ASSERT_TRUE(ir.setEnd(6));
  ASSERT_EQ(6, ir.getEnd());

  ir = Sloppy::IntRange{666};
  ASSERT_EQ(666, ir.getStart());
  ASSERT_FALSE(ir.getEnd().has_value());
  ASSERT_TRUE(ir.hasOpenEnd());

  ASSERT_FALSE(ir.setEnd(665));
  ASSERT_FALSE(ir.getEnd().has_value());
  ASSERT_TRUE(ir.hasOpenEnd());

  ASSERT_TRUE(ir.setEnd(666));
  ASSERT_EQ(666, ir.getEnd());
  ASSERT_FALSE(ir.hasOpenEnd());
}

//----------------------------------------------------------------------------

