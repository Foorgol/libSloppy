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
#include <thread>

#include <gtest/gtest.h>

#include "../Sloppy/ThreadSafeQueue.h"
#include "../Sloppy/Timer.h"

using namespace std;

TEST(ThreadSafeQueue, BasicUsage)
{
  Sloppy::ThreadSafeQueue<int> q;

  ASSERT_EQ(0, q.size());
  ASSERT_TRUE(q.empty());
  ASSERT_FALSE(q.hasData());

  // get with timeout on empty queue
  static constexpr int timeoutMs = 10;
  int data{-42};
  Sloppy::Timer t;
  ASSERT_FALSE(q.get(data, timeoutMs));
  ASSERT_TRUE(t.getTime__ms() >= timeoutMs);
  ASSERT_TRUE(t.getTime__ms() <= (timeoutMs + 1));
  ASSERT_EQ(-42, data);  // variable has not been altered

  // get without timeout on empty queue
  t.restart();
  ASSERT_FALSE(q.get(data, 0));
  ASSERT_TRUE(t.getTime__us() <= 1);
  ASSERT_EQ(-42, data);  // variable has not been altered

  // push some data
  q.put(99);
  ASSERT_EQ(1, q.size());
  ASSERT_FALSE(q.empty());
  ASSERT_TRUE(q.hasData());

  // get the data
  t.restart();
  ASSERT_TRUE(q.get(data, timeoutMs));
  ASSERT_TRUE(t.getTime__ms() < 1);
  ASSERT_EQ(99, data);

  ASSERT_EQ(0, q.size());
  ASSERT_TRUE(q.empty());
  ASSERT_FALSE(q.hasData());
}

//----------------------------------------------------------------------------

TEST(ThreadSafeQueue, Notifications)
{
  Sloppy::ThreadSafeQueue<int> q;

  // a lambda for filling the queue
  // with element by element, each
  // with a delay
  static constexpr int elemCnt = 100;
  static constexpr int insertionDelay_ms = 2;
  auto filler = [&]()
  {
    for (int i =0; i < elemCnt; ++i)
    {
      q.put(i);
      this_thread::sleep_for(chrono::milliseconds{insertionDelay_ms});
    }
  };

  thread fillerThread(filler);

  bool hasData = true;
  int cnt = 0;
  while (hasData)
  {
    Sloppy::Timer t;
    int data{-42};
    hasData = q.get(data, 2 * insertionDelay_ms);

    if (hasData)
    {
      ASSERT_TRUE(t.getTime__ms() <= insertionDelay_ms);
      ASSERT_EQ(data, cnt);
      ++cnt;
    } else {
      ASSERT_TRUE(t.getTime__ms() >= (2 * insertionDelay_ms));
      ASSERT_EQ(elemCnt, cnt);
    }
  }

  fillerThread.join();
}
