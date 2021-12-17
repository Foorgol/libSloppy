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

#include <gtest/gtest.h>

#include "../Sloppy/ResultOrError.h"


TEST(ResultOrErr, BasicUsage)
{
  enum class MyErr{
    One,
    Two,
    Three
  };

  struct DummyResult {
    int i{42};
    double x{3.14};
  };

  using RoE = Sloppy::ResultOrError<DummyResult, MyErr>;

  const RoE r1{MyErr::Two};
  ASSERT_TRUE(r1.isErr());
  ASSERT_FALSE(r1.isOk());
  ASSERT_EQ(MyErr::Two, r1.err());
  ASSERT_FALSE(r1);

  const RoE r2{DummyResult{33}};
  ASSERT_FALSE(r2.isErr());
  ASSERT_TRUE(r2.isOk());
  ASSERT_EQ(33, r2->i);
  ASSERT_EQ(3.14, r2->x);
  ASSERT_EQ(33, (*r2).i);
  ASSERT_EQ(3.14, (*r2).x);
  ASSERT_TRUE(r2);

  RoE r3{DummyResult{33}};
  ASSERT_FALSE(r2.isErr());
  ASSERT_TRUE(r2.isOk());
  ASSERT_EQ(33, r2->i);
  ASSERT_EQ(3.14, r2->x);
  ASSERT_EQ(33, (*r3).i);
  ASSERT_EQ(3.14, (*r3).x);
  ASSERT_TRUE(r3);
}
