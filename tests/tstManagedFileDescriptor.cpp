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
#include <future>

#include <gtest/gtest.h>

#include "../Sloppy/ManagedFileDescriptor.h"

using namespace Sloppy;
using namespace std;

TEST(ManagedFileDescr, BasicReadWrite)
{
  constexpr int numChars = 10;
  constexpr int charsPerSec = 3;

  constexpr int totalRuntime_us = (numChars * 1000000) / charsPerSec;

  string expectedResult;
  for (int i = 0; i < numChars; ++i) expectedResult += char(65 + i);

  //
  // prepare two lambdas that act as "independent" readers and writers for a pipe
  //

  // task1: write numChars bytes with charsPerSec byte / sec
  auto writer = [](ManagedFileDescriptor& fd)
  {
    constexpr int delay_us = 1000000 / charsPerSec;

    char c = 'A';

    for (int i = 0; i < numChars; i++)
    {
      string s{c};
      fd.blockingWrite(s);
      cout << "\t\t\tWriter: " << s << " written" << endl;
      ++c;
      this_thread::sleep_for(chrono::microseconds{delay_us});
    }
  };

  // task2: read numChars bytes
  auto reader = [](ManagedFileDescriptor& fd, size_t timeout_ms) -> MemArray
  {
    MemArray d;

    try
    {
      cout << "Reader: starting read!" << endl;
      d = fd.blockingRead(numChars, numChars, timeout_ms);
      cout << "Reader: completed read!" << endl;
    }
    catch (IOError e)
    {
      cerr << "Reader: Reading raised an IOError: " << e.getErrString() << endl;
      throw;
    }
    catch(ReadTimeout e)
    {
      cerr << "Reader: Timeout while waiting for data. Data read so far: " << string{e.getIncompleteData().to_charPtr(), e.getIncompleteData().size()} << endl;
      throw;
    }

    return d;
  };

  //
  // the actual test case
  //


  // create a pipe
  int fd[2];
  pipe(fd);

  // wrap the descriptors in classes
  ManagedFileDescriptor fdRead{fd[0]};
  ManagedFileDescriptor fdWrite{fd[1]};

  // start a thread for writing
  thread tWrite{writer, ref(fdWrite)};

  // start an async thread for reading, allow for sufficient
  // time to read all data
  auto a = async(launch::async, reader, ref(fdRead), (totalRuntime_us * 1.1) / 1000);
  MemArray d;
  ASSERT_NO_THROW(d = a.get());
  string dStr{d.to_charPtr(), d.size()};
  ASSERT_EQ(expectedResult, dStr);

  // wait for the writer to finish
  tWrite.join();


  // start another writer
  tWrite = thread{writer, ref(fdWrite)};

  // start an async thread for reading, with INSUFFICIENT
  // time to read all data
  a = async(launch::async, reader, ref(fdRead), (totalRuntime_us * 0.8) / 1000);
  ASSERT_THROW(d = a.get(), ReadTimeout);

  // wait for the writer to finish
  tWrite.join();

}

//----------------------------------------------------------------------------

TEST(ManagedFileDescr, MoveOps)
{
  // create a pipe
  int fd[2];
  pipe(fd);

  // wrap the descriptors in classes
  ManagedFileDescriptor fdRead{fd[0]};
  ManagedFileDescriptor fdWrite{fd[1]};

  // test basic operation prior to moving
  ASSERT_TRUE(fdWrite.blockingWrite("abcd"));
  MemArray data = fdRead.blockingRead_FixedSize(4, 100);
  string sDat{data.to_charPtr(), data.size()};
  ASSERT_EQ("abcd", sDat);

  // try move assignment
  ManagedFileDescriptor fdMoved = std::move(fdWrite);
  ASSERT_EQ(ManagedFileDescriptor::State::Closed, fdWrite.getState());
  ASSERT_FALSE(fdWrite.blockingWrite("xxx"));
  ASSERT_THROW(fdRead.blockingRead(0, 0, 10), ReadTimeout);  // test that nothing has been written
  ASSERT_TRUE(fdMoved.blockingWrite("AfterMove"));
  data = fdRead.blockingRead_FixedSize(9, 100);
  sDat = string{data.to_charPtr(), data.size()};
  ASSERT_EQ("AfterMove", sDat);

  // try move ctor
  ManagedFileDescriptor fdNew{std::move(fdMoved)};
  ASSERT_EQ(ManagedFileDescriptor::State::Closed, fdMoved.getState());
  ASSERT_FALSE(fdMoved.blockingWrite("xxx"));
  ASSERT_THROW(fdRead.blockingRead(0, 0, 10), ReadTimeout);  // test that nothing has been written
  ASSERT_TRUE(fdNew.blockingWrite("AfterMoveCtor"));
  data = fdRead.blockingRead_FixedSize(13, 100);
  sDat = string{data.to_charPtr(), data.size()};
  ASSERT_EQ("AfterMoveCtor", sDat);
}
