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

#include "../Sloppy/CyclicWorkerThread.h"

static constexpr int WorkerCycle_ms = 20;
static constexpr int IdleCycle_ms = 1;
static constexpr int HookDelay_ms = 5;

class TestWorker : public Sloppy::CyclicWorkerThread
{
public:
  TestWorker()
    : Sloppy::CyclicWorkerThread{WorkerCycle_ms, IdleCycle_ms} {}

  atomic<int> onPrepCnt{0};
  atomic<int> onResumeCnt{0};
  atomic<int> onSuspendCnt{0};
  atomic<int> onTermCnt{0};
  atomic<int> workerCnt{0};

  bool assertCntAndState(Sloppy::CyclicWorkerThread::CyclicWorkerThreadState st, int prep, int suspend, int resume, int term, int w, int tolerance = 1)
  {
    if (st != state())
    {
      cerr << "State!" << endl;
      return false;
    }

    if (onPrepCnt != prep)
    {
      cerr << "Prep! " << endl;
      return false;
    }

    if (onResumeCnt != resume)
    {
      cerr << "Resume!" << endl;
      return false;
    }

    if (onSuspendCnt != suspend)
    {
      cerr << "Suspend!" << endl;
      return false;
    }

    if (onTermCnt != term)
    {
      cerr << "Term!" << endl;
      return false;
    }

    if ((w == 0) && (workerCnt > 0))
    {
      cerr << "worker count not zero!" << endl;
      return false;
    }
    if ((w > 0) && (abs(workerCnt - w) > tolerance))
    {
      cerr << "Counter, expected: " << w << ", actual: " << workerCnt << endl;
      return false;
    }

    return true;
  }

protected:
  void onFirstRun() override
  {
    ++onPrepCnt;
    this_thread::sleep_for(chrono::milliseconds(HookDelay_ms));
  }

  void onSuspend() override
  {
    ++onSuspendCnt;
    this_thread::sleep_for(chrono::milliseconds(HookDelay_ms));
  }

  void onResume() override
  {
    ++onResumeCnt;
    this_thread::sleep_for(chrono::milliseconds(HookDelay_ms));
  }

  void onTerminate() override
  {
    ++onTermCnt;
    this_thread::sleep_for(chrono::milliseconds(HookDelay_ms));
  }

  void worker() override
  {
    ++workerCnt;
    this_thread::sleep_for(chrono::milliseconds(HookDelay_ms));
  }

};

TEST(CyclicThread, BasicUsage)
{
  using CTS = Sloppy::CyclicWorkerThread::CyclicWorkerThreadState;

  TestWorker tw;

  this_thread::sleep_for(chrono::milliseconds(10 * WorkerCycle_ms));
  ASSERT_TRUE(tw.assertCntAndState(CTS::Initialized, 0, 0, 0, 0, 0));

  // start the thread
  Sloppy::Timer t;
  ASSERT_TRUE(tw.run());
  this_thread::sleep_for(chrono::milliseconds(2 * IdleCycle_ms));
  ASSERT_EQ(CTS::Preparing, tw.state());
  tw.waitForStateChange();
  ASSERT_TRUE(t.getTime__ms() <= (HookDelay_ms + IdleCycle_ms));
  ASSERT_TRUE(tw.assertCntAndState(CTS::Running, 1, 0, 0, 0, 1));

  // do some worker cycles
  this_thread::sleep_for(chrono::milliseconds(10 * WorkerCycle_ms));
  ASSERT_TRUE(tw.assertCntAndState(CTS::Running, 1, 0, 0, 0, 10, 2));

  return;

  // stop execution
  t.restart();
  ASSERT_TRUE(tw.pause());
  tw.waitForStateChange();
  ASSERT_TRUE(t.getTime__ms() <= 2); // 1 ms idle cycle plus tolerance
  ASSERT_TRUE(tw.assertCntAndState(CTS::Suspended, 1, 1, 0, 0, 11, 2));

  // make sure no more cycles are executed
  this_thread::sleep_for(chrono::milliseconds(200));
  ASSERT_TRUE(tw.assertCntAndState(CTS::Suspended, 1, 1, 0, 0, 11, 2));

  // resume operation
  t.restart();
  ASSERT_TRUE(tw.resume());
  tw.waitForStateChange();
  ASSERT_TRUE(t.getTime__ms() <= 2); // 1 ms idle cycle plus tolerance
  ASSERT_TRUE(tw.assertCntAndState(CTS::Running, 1, 1, 1, 0, 12, 2));

  // do some worker cycles
  this_thread::sleep_for(chrono::milliseconds(100));
  ASSERT_TRUE(tw.assertCntAndState(CTS::Running, 1, 1, 1, 0, 22, 2));

  // finish
  t.restart();
  tw.terminate();
  tw.waitForStateChange();
  ASSERT_TRUE(t.getTime__ms() <= 2); // 1 ms idle cycle plus tolerance
  ASSERT_TRUE(tw.assertCntAndState(CTS::Finished, 1, 1, 1, 1, 22, 2));

  // always wait at the end of the test case, otherwise
  // we might interfere with the dtor!
  this_thread::sleep_for(chrono::milliseconds(100));
}

//----------------------------------------------------------------------------

