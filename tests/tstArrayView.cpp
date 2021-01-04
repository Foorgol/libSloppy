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

#include "../Sloppy/Memory.h"

using IntArray = Sloppy::ArrayView<int>;

TEST(ArrayView, Ctor)
{
  int a1[] = {42,23,666};

  IntArray ia{&a1[0], 3};
  ASSERT_EQ(23, ia.elemAt(1));
  ASSERT_EQ(3, ia.size());
  ASSERT_EQ(12, ia.byteSize());

  ASSERT_THROW(IntArray(nullptr, 42), std::invalid_argument);
  ASSERT_THROW(IntArray(&a1[0], 0), std::invalid_argument);

  IntArray iaEmpty;
  ASSERT_EQ(0, iaEmpty.size());
}

//----------------------------------------------------------------------------

TEST(ArrayView, SliceByIndex)
{
  int a1[] = {42,23,666,1,99};
  IntArray ia{&a1[0], 5};

  auto s1 = ia.slice_byIdx(0, 0);
  ASSERT_EQ(1, s1.size());
  ASSERT_EQ(42, s1.elemAt(0));

  auto s2 = ia.slice_byIdx(2, 4);
  ASSERT_EQ(3, s2.size());
  ASSERT_EQ(666, s2.elemAt(0));
  ASSERT_EQ(1, s2.elemAt(1));
  ASSERT_EQ(99, s2.elemAt(2));

  ASSERT_THROW(ia.slice_byIdx(0, 10), std::out_of_range);
  ASSERT_THROW(ia.slice_byIdx(10, 11), std::out_of_range);
  ASSERT_THROW(ia.slice_byIdx(3,2), std::invalid_argument);

  // try to slice an empty view
  IntArray empty{};
  ASSERT_THROW(empty.slice_byIdx(0, 0), std::out_of_range);
  ASSERT_THROW(empty.slice_byIdx(2,3), std::out_of_range);
}

//----------------------------------------------------------------------------

TEST(ArrayView, SliceByCount)
{
  int a1[] = {42,23,666,1,99};
  IntArray ia{&a1[0], 5};

  auto s1 = ia.slice_byCount(0, 1);
  ASSERT_EQ(1, s1.size());
  ASSERT_EQ(42, s1.elemAt(0));

  auto s2 = ia.slice_byCount(2, 3);
  ASSERT_EQ(3, s2.size());
  ASSERT_EQ(666, s2.elemAt(0));
  ASSERT_EQ(1, s2.elemAt(1));
  ASSERT_EQ(99, s2.elemAt(2));

  ASSERT_THROW(ia.slice_byCount(0, 10), std::out_of_range);
  ASSERT_THROW(ia.slice_byCount(10, 1), std::out_of_range);

  auto empty = ia.slice_byCount(2,0);
  ASSERT_TRUE(empty.empty());

  // try to slice an empty view
  ASSERT_THROW(empty.slice_byCount(0, 1), std::out_of_range);
  ASSERT_THROW(empty.slice_byCount(0, 0), std::out_of_range);
}

//----------------------------------------------------------------------------

TEST(ArrayView, ChopLeft)
{
  int a1[] = {42,23,666,1,99};
  IntArray ia{&a1[0], 5};

  ia.chopLeft(1);
  ASSERT_EQ(4, ia.size());
  ASSERT_EQ(23, ia.elemAt(0));

  ia.chopLeft(0);
  ASSERT_EQ(4, ia.size());
  ASSERT_EQ(23, ia.elemAt(0));

  ia.chopLeft(3);
  ASSERT_EQ(1, ia.size());
  ASSERT_EQ(99, ia.elemAt(0));

  ASSERT_THROW(ia.chopLeft(2), std::out_of_range);

  ia.chopLeft(1);
  ASSERT_TRUE(ia.empty());
}

//----------------------------------------------------------------------------

TEST(ArrayView, ChopRight)
{
  int a1[] = {42,23,666,1,99};
  IntArray ia{&a1[0], 5};

  ia.chopRight(1);
  ASSERT_EQ(4, ia.size());
  ASSERT_EQ(1, ia.elemAt(3));

  ia.chopRight(0);
  ASSERT_EQ(4, ia.size());
  ASSERT_EQ(1, ia.elemAt(3));

  ia.chopRight(3);
  ASSERT_EQ(1, ia.size());
  ASSERT_EQ(42, ia.elemAt(0));

  ASSERT_THROW(ia.chopRight(2), std::out_of_range);

  ia.chopRight(1);
  ASSERT_TRUE(ia.empty());
}

//----------------------------------------------------------------------------

TEST(ArrayView, FirstLast)
{
  int a1[] = {42,23,666,1,99};
  IntArray ia{&a1[0], 5};

  ASSERT_EQ(42, ia.first());
  ASSERT_EQ(99, ia.last());

  int x = 666;
  IntArray ia2{&x, 1};
  ASSERT_EQ(666, ia2.first());
  ASSERT_EQ(666, ia2.last());

  IntArray empty{};
  ASSERT_THROW(empty.first(), std::out_of_range);
  ASSERT_THROW(empty.last() , std::out_of_range);

  int* lastItemPtr = &a1[4];
  ASSERT_EQ(lastItemPtr, ia.lastPtr());
}

//----------------------------------------------------------------------------

TEST(ArrayView, OperatorAccess)
{
  int a1[] = {42,23,666,1,99};
  IntArray ia{&a1[0], 5};

  ASSERT_EQ(42, ia[0]);
  ASSERT_EQ(1, ia[3]);
  ASSERT_THROW(ia[5], std::out_of_range);
}

//----------------------------------------------------------------------------

TEST(ArrayView, Conversion)
{
  int a1[] = {42,23,666,200,99};
  IntArray ia{&a1[0], 5};

  const char* p = ia.to_charPtr();
  ASSERT_EQ(42, p[0]);  // int data is stored with lowest byte first
  ASSERT_EQ(0, p[1]);
  ASSERT_EQ(23, p[4]);

  const void* v = ia.to_voidPtr();

  const uint8_t* u = ia.to_uint8Ptr();
  ASSERT_EQ(42, u[0]);  // int data is stored with lowest byte first
  ASSERT_EQ(0, u[1]);
  ASSERT_EQ(200, u[12]);

  const unsigned char* uc = ia.to_ucPtr();
  ASSERT_EQ(42, uc[0]);  // int data is stored with lowest byte first
  ASSERT_EQ(0, uc[1]);
  ASSERT_EQ(200, uc[12]);
}

//----------------------------------------------------------------------------

TEST(ArrayView, CopyOps)
{
  int a1[] = {42,23,666,200,99};
  IntArray ia{&a1[0], 5};

  IntArray ia2{ia};
  ASSERT_EQ(ia[2], ia2[2]);

  IntArray ia3;
  ia3 = ia;
  ASSERT_EQ(ia[2], ia3[2]);
}

//----------------------------------------------------------------------------

TEST(ArrayView, OtherOperators)
{
  int a1[] = {42,23,666,200,99};
  IntArray ia{&a1[0], 5};

  IntArray ia2{ia};

  ASSERT_TRUE(ia2 == ia);
  ASSERT_FALSE(ia2 != ia);

  ia2.chopLeft(1);
  ASSERT_FALSE(ia2 == ia);
  ASSERT_TRUE(ia2 != ia);

  ia.chopLeft(1);
  ASSERT_TRUE(ia2 == ia);
  ASSERT_FALSE(ia2 != ia);

  // comparison with empty array
  IntArray empty;
  ASSERT_FALSE(empty == ia);
  ASSERT_TRUE(empty != ia);

  ASSERT_TRUE(ia > empty);
  ASSERT_FALSE(ia > ia2);

  ASSERT_TRUE(empty < ia);
  ASSERT_FALSE(ia < empty);
  ASSERT_FALSE(ia < ia2);
}

//----------------------------------------------------------------------------

TEST(ArrayView, ByteArrayView)
{
  int a1[] = {42,23,666,200,99};
  IntArray ia{&a1[0], 5};

  uint8_t refVal[] = {
    42, 0, 0, 0,
    23, 0, 0, 0,
    154, 2, 0, 0,
    200, 0, 0, 0,
    99, 0, 0, 0};

  auto bav = ia.toByteArrayView();
  ASSERT_EQ(20, bav.size());

  for (int i = 0; i < bav.size(); ++i)
  {
    ASSERT_EQ(refVal[i], bav[i]);
  }
}
