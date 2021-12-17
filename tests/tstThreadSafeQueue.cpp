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
#include <vector>

#include <gtest/gtest.h>

#include "../Sloppy/ThreadSafeQueue.h"
#include "../Sloppy/Timer.h"

using namespace std;

TEST(ThreadSafeQueue, BasicUsage)
{
  Sloppy::ThreadSafeQueue<int> q1;
  Sloppy::ThreadSafeQueue_PipeSynced<int> q2;

  std::vector<Sloppy::AbstractThreadSafeQueue<int>*> ptrList;
  ptrList.push_back(&q1);
  ptrList.push_back(&q2);

  for (auto qPtr : ptrList) {
    ASSERT_EQ(0, qPtr->size());
    ASSERT_TRUE(qPtr->empty());
    ASSERT_FALSE(qPtr->hasData());

    // get with timeout on empty queue
    static constexpr int timeoutMs = 10;
    Sloppy::Timer t;
    ASSERT_FALSE(qPtr->get(timeoutMs));
    ASSERT_TRUE(t.getTime__ms() >= timeoutMs);
    ASSERT_TRUE(t.getTime__ms() <= (timeoutMs + 1));

    // get without timeout on empty queue
    t.restart();
    ASSERT_FALSE(qPtr->get(0));
    ASSERT_TRUE(t.getTime__us() <= 5);

    // push some data
    qPtr->put(99);
    ASSERT_EQ(1, qPtr->size());
    ASSERT_FALSE(qPtr->empty());
    ASSERT_TRUE(qPtr->hasData());

    // get the data
    t.restart();
    ASSERT_EQ(99, qPtr->get(timeoutMs));
    ASSERT_TRUE(t.getTime__ms() < 1);

    ASSERT_EQ(0, qPtr->size());
    ASSERT_TRUE(qPtr->empty());
    ASSERT_FALSE(qPtr->hasData());
  }
}

//----------------------------------------------------------------------------

TEST(ThreadSafeQueue, Notifications)
{
  Sloppy::ThreadSafeQueue<int> q1;
  Sloppy::ThreadSafeQueue_PipeSynced<int> q2;

  std::vector<Sloppy::AbstractThreadSafeQueue<int>*> ptrList;
  ptrList.push_back(&q1);
  ptrList.push_back(&q2);

  // a lambda for filling the queue
  // with element by element, each
  // with a delay
  static constexpr int elemCnt = 100;
  static constexpr int insertionDelay_ms = 2;
  auto filler = [&](Sloppy::AbstractThreadSafeQueue<int>* qPtr)
  {
    for (int i =0; i < elemCnt; ++i)
    {
      qPtr->put(i);
      this_thread::sleep_for(chrono::milliseconds{insertionDelay_ms});
    }
  };

  for (auto qPtr : ptrList) {
    thread fillerThread(filler, qPtr);

    int cnt = 0;
    while (true)
    {
      Sloppy::Timer t;
      auto optData = qPtr->get(2 * insertionDelay_ms);

      if (optData)
      {
        ASSERT_TRUE(t.getTime__ms() <= insertionDelay_ms);
        ASSERT_EQ(*optData, cnt);
        ++cnt;
      } else {
        ASSERT_TRUE(t.getTime__ms() >= (2 * insertionDelay_ms));
        ASSERT_EQ(elemCnt, cnt);
        break;
      }
    }

    fillerThread.join();
  }

}
