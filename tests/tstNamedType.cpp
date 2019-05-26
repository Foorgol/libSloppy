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
#include <thread>

#include <gtest/gtest.h>

#include "../Sloppy/NamedType.h"

TEST(NamedType, BasicUsage)
{
  using StrongInt = Sloppy::NamedType<int, struct StrongIntegerTag>;

  StrongInt si{42};
  ASSERT_EQ(si, 42);

  // try passing as a const-ref to a function
  auto dummyFunc = [](const StrongInt& i)
  {
    return i < 100;
  };

  ASSERT_TRUE(dummyFunc(si));
  ASSERT_TRUE(dummyFunc(StrongInt{42}));
  ASSERT_FALSE(dummyFunc(StrongInt{200}));
}

//----------------------------------------------------------------------------

TEST(NamedType, Comparison)
{
  using StrongInt = Sloppy::NamedType<int, struct StrongIntegerTag>;

  StrongInt si1{42};
  StrongInt si2{666};
  StrongInt si3{666};
  ASSERT_TRUE(si1 < si2);
  ASSERT_FALSE(si1 > si2);
  ASSERT_TRUE(si1 <= si2);
  ASSERT_FALSE(si1 >= si2);
  ASSERT_FALSE(si1 == si2);
  ASSERT_TRUE(si1 != si2);

  ASSERT_FALSE(si3 < si2);
  ASSERT_FALSE(si3 > si2);
  ASSERT_TRUE(si3 <= si2);
  ASSERT_TRUE(si3 >= si2);
  ASSERT_TRUE(si3 == si2);
  ASSERT_FALSE(si3 != si2);
}

//----------------------------------------------------------------------------

