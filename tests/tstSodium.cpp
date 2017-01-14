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
#include "../Sloppy/Crypto/Sodium.h"

using namespace Sloppy;
using namespace Sloppy::Crypto;

TEST(Sodium, SodiumInit)
{
  SodiumLib* sodium{nullptr};

  ASSERT_NO_THROW(sodium = SodiumLib::getInstance());
  ASSERT_TRUE(sodium != nullptr);
}

//----------------------------------------------------------------------------

TEST(Sodium, Bin2Hex)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  ASSERT_EQ("414243", sodium->bin2hex("ABC"));
  ASSERT_EQ("", sodium->bin2hex(""));

  Sloppy::ManagedBuffer buf{4};
  string tmp{"ABCD"};
  memcpy(buf.get(), tmp.c_str(), 4);
  ASSERT_EQ("41424344", sodium->bin2hex(buf));

}

//----------------------------------------------------------------------------

TEST(Sodium, SecureMem)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  SodiumSecureMemory mem{20, SodiumSecureMemType::Guarded};
  ASSERT_EQ(20, mem.getSize());
  ASSERT_EQ(SodiumSecureMemType::Guarded, mem.getType());

  // the following calls will cause segfaults if
  // the memory management doesn't work
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RO));
  char x = ((char*)mem.get())[0];
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RW));
  ((char*)mem.get())[0] = 'x';

  // check that normal memory can't be protected
  SodiumSecureMemory mem1{10, SodiumSecureMemType::Normal};
  ASSERT_EQ(10, mem1.getSize());
  ASSERT_EQ(SodiumSecureMemType::Normal, mem1.getType());
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::RO));
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::RW));
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::NoAccess));

  // fill mem1 with values
  char* ptr = (char *)mem1.get();
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // make sure that the move assignment compiles
  mem = std::move(mem1);

  // check that moving was successfull
  ASSERT_EQ(0, mem1.getSize());
  ASSERT_EQ(nullptr, mem1.get());
  ASSERT_EQ(10, mem.getSize());
  ASSERT_TRUE(mem.get() != nullptr);
  ptr = (char *)mem.get();
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ASSERT_EQ('A' + idx, ptr[idx]);
  }

}

//----------------------------------------------------------------------------

TEST(Sodium, SecureMemCopy)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  SodiumSecureMemory mem{20, SodiumSecureMemType::Guarded};
  char* ptr = (char *)mem.get();
  for (size_t idx = 0; idx < 20; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // protect the memory
  mem.setAccess(SodiumSecureMemAccess::NoAccess);

  // get a copy
  SodiumSecureMemory cpy = SodiumSecureMemory::asCopy(mem);

  // make sure the memory areas are different
  ASSERT_NE(mem.get(), cpy.get());

  // make sure the types are identical
  ASSERT_EQ(mem.getSize(), cpy.getSize());
  ASSERT_EQ(mem.getType(), cpy.getType());

  // make sure protection has been restored
  ASSERT_EQ(mem.getProtection(), cpy.getProtection());
  ASSERT_EQ(SodiumSecureMemAccess::NoAccess, mem.getProtection());

  // check memory contents
  ASSERT_TRUE(cpy.setAccess(SodiumSecureMemAccess::RO));
  ptr = (char *)cpy.get();
  for (size_t idx = 0; idx < 20; ++idx)
  {
    ASSERT_EQ('A' + idx, ptr[idx]);
  }
}

//----------------------------------------------------------------------------

TEST(Sodium, MemCmp)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  SodiumSecureMemory mem1{20, SodiumSecureMemType::Guarded};
  char* ptr = (char *)mem1.get();
  for (size_t idx = 0; idx < mem1.getSize(); ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  ManagedBuffer mem2{mem1.getSize()};
  ptr = (char *)mem2.get();
  for (size_t idx = 0; idx < mem1.getSize(); ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  ASSERT_TRUE(sodium->memcmp(mem1, mem2));
  ptr[0] = 42;
  ASSERT_FALSE(sodium->memcmp(mem1, mem2));
}

//----------------------------------------------------------------------------

TEST(Sodium, Random)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  ManagedBuffer buf{20};
  char* ptr = (char *)buf.get();
  for (size_t idx = 0; idx < buf.getSize(); ++idx)
  {
    ptr[idx] = 0;
  }
  ASSERT_TRUE(sodium->isZero(buf));

  // fill with random data
  sodium->randombytes_buf(buf);
  ASSERT_FALSE(sodium->isZero(buf));

  // check the random generator with upper bound
  uint32_t max = 1000;
  for (int cnt = 0; cnt < 10000 ; ++cnt)
  {
    ASSERT_TRUE(sodium->randombytes_uniform(max) < max);
  }
}
