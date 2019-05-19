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

#include "../Sloppy/Memory.h"

using IntArray = Sloppy::ManagedArray<int>;

TEST(ManagedArray, Ctor)
{
  IntArray ia{10};
  ASSERT_EQ(10, ia.size());
  ASSERT_EQ(40, ia.byteSize());

  IntArray ia2;
  ASSERT_EQ(0, ia2.size());
  ASSERT_EQ(0, ia2.byteSize());

}

//----------------------------------------------------------------------------

TEST(ManagedArray, FirstLast)
{
  IntArray ia{5};
  ia[0] = 42;
  ia[4] = 99;

  ASSERT_EQ(42, ia.first());
  ASSERT_EQ(99, ia.last());

  ia.first() = 111;
  ASSERT_EQ(111, ia.first());
}

//----------------------------------------------------------------------------

TEST(ManagedArray, AsView)
{
  IntArray ia{2};
  ia[0] = 42;
  ia[1] = 99;

  auto v = ia.view();
  v.chopLeft(1);
  ASSERT_EQ(99, v.first());

  ASSERT_EQ(42, ia.first());
}

//----------------------------------------------------------------------------

TEST(ManagedArray, DeepCopy)
{
  static constexpr int n = 1000;

  IntArray ia{n};
  ASSERT_EQ(n, ia.size());
  for (int i=0; i < n; ++i) ia[i] = rand();

  // copy ctor
  IntArray ia2{ia};
  ASSERT_EQ(ia.size(), ia2.size());
  for (int i=0; i < n; ++i) ASSERT_EQ(ia[i], ia2[i]);

  // show independence of the two arrays
  ia[0] = 1234;
  ia2[0] = 5678;
  ASSERT_EQ(ia[0], 1234);
  ASSERT_EQ(ia2[0], 5678);
}

//----------------------------------------------------------------------------

TEST(ManagedArray, MoveOpsAndCopyOps)
{
  static constexpr int n = 1000;

  IntArray ia{n};
  ASSERT_EQ(n, ia.size());
  for (int i=0; i < n; ++i) ia[i] = i;

  // move ctor
  IntArray ia2{std::move(ia)};
  ASSERT_EQ(n, ia2.size());
  ASSERT_TRUE(ia.empty());
  for (int i=0; i < n; ++i) ASSERT_EQ(i, ia2[i]);

  // move operator
  ia = std::move(ia2);
  ASSERT_EQ(n, ia.size());
  ASSERT_TRUE(ia2.empty());
  for (int i=0; i < n; ++i) ASSERT_EQ(i, ia[i]);

  // copy assignment
  ia2 = ia;
  ASSERT_EQ(n, ia.size());
  ASSERT_EQ(n, ia2.size());
  for (int i=0; i < n; ++i) ASSERT_EQ(i, ia2[i]);
  for (int i=0; i < n; ++i) ia[i] = -i;  // modify ia
  for (int i=0; i < n; ++i) ASSERT_EQ(ia[i], -ia2[i]);  // show that ia2 has not been modified (--> true copy)
}

//----------------------------------------------------------------------------

TEST(ManagedArray, Conversion)
{
  static constexpr int n = 10;

  IntArray ia{n};
  ASSERT_EQ(n, ia.size());
  for (int i=0; i < n; ++i) ia[i] = 125 + i;

  char* c = ia.to_charPtr();
  ASSERT_EQ(125, *c);
  *c = 42;
  ASSERT_EQ(42, ia[0]);

  uint8_t* u = ia.to_uint8Ptr();
  ASSERT_EQ(134, u[9 * 4]);
  u[9 * 4] = 255;
  ASSERT_EQ(255, ia[9]);
}

//----------------------------------------------------------------------------

TEST(ManagedArray, Resize)
{
  static constexpr int n = 100;

  IntArray ia{n};
  ASSERT_EQ(n, ia.size());
  for (int i=0; i < n; ++i) ia[i] = 125 + i;

  ia.resize(20);
  ASSERT_EQ(20, ia.size());
  for (int i=0; i < 20; ++i) ASSERT_EQ(125 + i, ia[i]);

  ia.resize(0);
  ASSERT_TRUE(ia.empty());

  ASSERT_NO_THROW(ia.resize(200));  // resizing an empty array with invalid source pointers
}

//----------------------------------------------------------------------------

TEST(ManagedArray, CopyOver)
{
  IntArray dst{10};
  IntArray src1{3};
  IntArray src2{11};

  for (int i=0; i < dst.size(); ++i) dst[i] = i;
  for (int i=0; i < src1.size(); ++i) src1[i] = 100 + i;

  // copy without offset
  dst.copyOver(src1.view());
  size_t idx = 0;
  for (int i : {100, 101, 102, 3, 4, 5, 6, 7, 8, 9})
  {
    ASSERT_EQ(i, dst[idx]);
    ++idx;
  }

  // copy with offset
  dst.copyOver(src1.view(), 5);
  idx = 0;
  for (int i : {100, 101, 102, 3, 4, 100, 101, 102, 8, 9})
  {
    ASSERT_EQ(i, dst[idx]);
    ++idx;
  }
  dst.copyOver(src1.view(), 7);
  idx = 0;
  for (int i : {100, 101, 102, 3, 4, 100, 101, 100, 101, 102})
  {
    ASSERT_EQ(i, dst[idx]);
    ++idx;
  }

  // invalid calls
  ASSERT_THROW(dst.copyOver(src1.view(), 8), std::out_of_range);
  ASSERT_THROW(dst.copyOver(src2.view()), std::out_of_range);

}
