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

using namespace Sloppy;

TEST(ManagedMem, InitAndMove)
{
  ManagedBuffer buf{20};
  ManagedBuffer buf1{10};

  ASSERT_EQ(20, buf.getSize());
  ASSERT_EQ(10, buf1.getSize());

  // fill the memory
  char* ptr = (char *)buf1.get();
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // make sure that the move assignment compiles
  buf = std::move(buf1);

  // check that moving was successfull
  ASSERT_EQ(0, buf1.getSize());
  ASSERT_EQ(nullptr, buf1.get());
  ASSERT_EQ(10, buf.getSize());
  ASSERT_TRUE(buf.get() != nullptr);
  ptr = (char *)buf.get();
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ASSERT_EQ('A' + idx, ptr[idx]);
  }
}
