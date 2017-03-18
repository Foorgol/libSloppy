/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
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

#include "../Sloppy/libSloppy.h"
#include "BasicTestClass.h"

using namespace Sloppy;

TEST(StringFuncs, StringArgs)
{
  string s{"abc %0 def %3xyz%3 %%4%"};

  ASSERT_EQ(1, strArg(s, "_"));
  ASSERT_EQ("abc _ def %3xyz%3 %%4%", s);
  ASSERT_EQ(2, strArg(s, "::"));
  ASSERT_EQ("abc _ def ::xyz:: %%4%", s);
  ASSERT_EQ(1, strArg(s, "*"));
  ASSERT_EQ("abc _ def ::xyz:: %*%", s);
  ASSERT_EQ(0, strArg(s, ""));
  ASSERT_EQ("abc _ def ::xyz:: %*%", s);
}

//----------------------------------------------------------------------------

TEST(StringFuncs, StringArgsInt)
{
  string s{"abc %0 def %1 %2 %3"};

  ASSERT_EQ(1, strArg(s, 42));
  ASSERT_EQ("abc 42 def %1 %2 %3", s);
  ASSERT_EQ(1, strArg(s, 666, 5));
  ASSERT_EQ("abc 42 def 00666 %2 %3", s);
  ASSERT_EQ(1, strArg(s, -23));
  ASSERT_EQ("abc 42 def 00666 -23 %3", s);
  ASSERT_EQ(1, strArg(s, -23, 6));
  ASSERT_EQ("abc 42 def 00666 -23 -00023", s);
}


//----------------------------------------------------------------------------

TEST(StringFuncs, StringArgsDouble)
{
  string s{"abc %0 def %1"};
  double d{3.1415927};

  ASSERT_EQ(1, strArg(s, d));
  ASSERT_EQ("abc 3.141593 def %1", s);
  ASSERT_EQ(1, strArg(s, d, 3));
  ASSERT_EQ("abc 3.141593 def 3.142", s);
}
