/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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
#include <atomic>

#include <gtest/gtest.h>

#include "../Sloppy/AsyncWorker.h"
#include "../Sloppy/Timer.h"

static constexpr int PreemptionTime_ms = 100;
static constexpr int WorkerDuration_ms = 10;

struct AsyncWorkInput
{
  int add1;
  int add2;

  AsyncWorkInput() = default;
};

class AsyncTestWorker : public Sloppy::AsyncWorker<AsyncWorkInput, int>
{
public:
  AsyncTestWorker(
      Sloppy::ThreadSafeQueue<AsyncWorkInput>* inQueue,
      Sloppy::ThreadSafeQueue<int>* outQueue
      )
    : Sloppy::AsyncWorker<AsyncWorkInput, int>{inQueue, outQueue, PreemptionTime_ms} {}

  int worker(const AsyncWorkInput& inData) override
  {
    this_thread::sleep_for(chrono::milliseconds(WorkerDuration_ms));
    return inData.add1 + inData.add2;
  }

};

TEST(AsyncWorker, BasicUsage)
{
  // create two queues
  Sloppy::ThreadSafeQueue<AsyncWorkInput> iq;  // input queue
  Sloppy::ThreadSafeQueue<int> oq;  // output queue

  // create a new worker and make sure it's "running"
  AsyncTestWorker w{&iq, &oq};
  ASSERT_TRUE(w.running());

  // push some simple data and wait for the result
  AsyncWorkInput inData{20, 30};
  Sloppy::Timer t;
  iq.put(inData);

  int out{-42};
  oq.get(out);
  int execTime = t.getTime__ms();
  ASSERT_TRUE(execTime < (PreemptionTime_ms + WorkerDuration_ms));
  ASSERT_EQ(50, out);

  // suspend the work execution
  w.suspend();
  this_thread::sleep_for(chrono::milliseconds{PreemptionTime_ms * 2});
  ASSERT_FALSE(w.running());

  // fill the queue with a lot of work
  // and make sure it's not yet getting processed
  const int nElem = 50;
  for (int i = 0; i < nElem; ++i)
  {
    AsyncWorkInput inData{i + 100, 2 * i};
    iq.put(inData);
  }
  ASSERT_EQ(nElem, iq.size());
  ASSERT_EQ(0, oq.size());
  this_thread::sleep_for(chrono::milliseconds{PreemptionTime_ms * 2});
  ASSERT_FALSE(w.running());
  ASSERT_EQ(nElem, iq.size());
  ASSERT_EQ(0, oq.size());

  // re-enable the worker and let it run
  t.restart();
  w.resume();
  int cnt{0};
  while (true)
  {
    int out;
    bool hasData = oq.get(out, PreemptionTime_ms + WorkerDuration_ms);
    if (!hasData) break;

    ASSERT_EQ(cnt + 100 + 2 * cnt, out);
    ++cnt;
  }
  ASSERT_FALSE(iq.hasData());
  ASSERT_FALSE(oq.hasData());
  ASSERT_EQ(nElem, cnt);
  this_thread::sleep_for(chrono::milliseconds{PreemptionTime_ms * 2});
  ASSERT_FALSE(iq.hasData());
  ASSERT_FALSE(oq.hasData());

  w.join();

  // show stats
  const auto& stats = w.stats();
  cout << "Number of worker calls: " << stats.nCalls << endl;
  cout << "Avg. worker duration: " << stats.avgWorkerExecTime_ms() << endl;
  cout << "Min / max exec time: " << stats.minWorkerTime_ms << " / " << stats.maxWorkerTime_ms << endl;
}

//----------------------------------------------------------------------------

