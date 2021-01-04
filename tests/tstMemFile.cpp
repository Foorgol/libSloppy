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

#include <gtest/gtest.h>

#include "../Sloppy/Memory.h"

using namespace std;
using namespace Sloppy;

// these tests are only useful for non-Windows builds
#ifndef WIN32

TEST(Memory, MemFile_Ctor)
{
  // test the default ctor
  MemFile mf1;
  ASSERT_TRUE(mf1.size() < 0);

  // test the normal ctor
  MemFile mf2{"../tests/sampleTemplateStore/t1.txt"};
  ASSERT_TRUE(mf2.size() > 0);

  // access the data
  MemView v = mf2.view();
  v = v.slice_byCount(0, 5);
  string s{v.to_charPtr(), v.size()};
  ASSERT_EQ("Hello", s);

  // move assignment
  mf1 = std::move(mf2);
  ASSERT_TRUE(mf1.size() > 0);
  ASSERT_TRUE(mf2.size() < 0);

  // move ctor
  MemFile mf3{std::move(mf1)};
  ASSERT_TRUE(mf1.size() < 0);
  ASSERT_TRUE(mf3.size() > 0);

  v = mf3.view();
  v = v.slice_byCount(0, 5);
  s = string{v.to_charPtr(), v.size()};
  ASSERT_EQ("Hello", s);

  // invalid file names
  ASSERT_THROW(MemFile{""}, std::invalid_argument);
  ASSERT_THROW(MemFile{"sfdljsfdl"}, std::invalid_argument);
}

//----------------------------------------------------------------------------

TEST(Memory, MemFile_Accessors)
{
  MemFile mf{"../tests/sampleTemplateStore/t1.txt"};

  ASSERT_EQ('o', mf.getByte(4));

  // the two bytes at offset 3 are 'l' (108) and 'o' (111)
  // which gives the short int 108 + 111*256 = 28524
  ASSERT_EQ(28524, mf.getShort(3));

  // the four bytes at offset are 'e', 'l', 'l', 'o' (101, 108, 108, 111)
  // which gives the low word 101 + 108*256 = 27749 and the
  // high word 108 + 111*256 = 28524
  ASSERT_EQ(27749 + 28524*65536, mf.getInt(1));

  // string access
  ASSERT_EQ("This", mf.getString(6, 4));

  // zero-terminated string without zero-terminator
  // ==> read up to the file's end
  ASSERT_EQ(" }}\n\n", mf.getString(mf.size()-5));

  // invalid indices
  ASSERT_THROW(mf.getByte(mf.size()), std::out_of_range);
  ASSERT_THROW(mf.getInt(mf.size()-3), std::out_of_range);
}

#endif
