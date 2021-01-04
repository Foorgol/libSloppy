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

#include "CyclicWorkerThread.h"

using namespace std;

namespace Sloppy
{

  CyclicWorkerThread::CyclicWorkerThread(int minWorkerCycle_ms)
    :workerCycle_ms{minWorkerCycle_ms}
  {
    stats.workerCycleTime_ms = workerCycle_ms;
    workerThread = thread([&]{mainLoop();});
  }

  //----------------------------------------------------------------------------

  CyclicWorkerThread::~CyclicWorkerThread()
  {
    if (workerThread.joinable())
    {
      forceQuitThreadFromDtor = true;
      workerThread.join();
    }
  }

  //----------------------------------------------------------------------------

  bool CyclicWorkerThread::run()
  {
    if (isTransitionPending) return false;

    lock_guard<mutex> lk{stateMutex};

    // is there anything to do at all?
    if (curState == CyclicWorkerThreadState::Running)
    {
      return true;   // nothing to do for us
    }

    // are we in a valid state for the transition?
    if ((curState != CyclicWorkerThreadState::Initialized) && (curState != CyclicWorkerThreadState::Suspended))
    {
      return false;   // invalid call
    }

    reqState = CyclicWorkerThreadState::Running;
    isTransitionPending = true;
    cvState.notify_one();
    return true;
  }

  //----------------------------------------------------------------------------

  bool CyclicWorkerThread::pause()
  {
    if (isTransitionPending) return false;

    lock_guard<mutex> lk{stateMutex};

    // is there anything to do at all?
    if (curState == CyclicWorkerThreadState::Suspended)
    {
      return true;   // nothing to do for us
    }

    // are we in a valid state for the transition?
    if (curState != CyclicWorkerThreadState::Running)
    {
      return false;   // invalid call
    }

    reqState = CyclicWorkerThreadState::Suspended;
    isTransitionPending = true;
    cvState.notify_one();
    return true;
  }

  //----------------------------------------------------------------------------

  bool CyclicWorkerThread::resume()
  {
    if (isTransitionPending) return false;

    lock_guard<mutex> lk{stateMutex};

    // is there anything to do at all?
    if (curState == CyclicWorkerThreadState::Running)
    {
      return true;   // nothing to do for us
    }

    // are we in a valid state for the transition?
    if (curState != CyclicWorkerThreadState::Suspended)
    {
      return false;   // invalid call
    }

    reqState = CyclicWorkerThreadState::Running;
    isTransitionPending = true;
    cvState.notify_one();
    return true;
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::terminate()
  {
    lock_guard<mutex> lk{stateMutex};

    // is there anything to do at all?
    if (curState == CyclicWorkerThreadState::Finished)
    {
      return;   // nothing to do for us
    }

    reqState = CyclicWorkerThreadState::Finished;
    isTransitionPending = true;
    cvState.notify_one();
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::terminateAndJoin()
  {
    if (workerThread.joinable())
    {
      terminate();
      workerThread.join();
    }
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::waitForStateChange()
  {
    while (isTransitionPending)
    {
      this_thread::sleep_for(chrono::milliseconds(WaitForStateChangePollingTime_ms));
    }
  }

  //----------------------------------------------------------------------------

  CyclicThreadStats CyclicWorkerThread::workerStats()
  {
    lock_guard<mutex> lk{stateMutex};
    return stats;
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::mainLoop()
  {
    // short-cut
    using Clock = chrono::steady_clock;

    // initially lock the mutex because that's a pre-condition
    // when starting the while loop
    unique_lock<mutex> lk{stateMutex};  // implicitly calls `lock()`

    Sloppy::Timer t;
    bool isNewWorkerCycle{true};

    while (!forceQuitThreadFromDtor && (curState != CyclicWorkerThreadState::Finished))
    {
      if (isNewWorkerCycle) t.restart();

      if ((reqState == CyclicWorkerThreadState::Running) && isNewWorkerCycle)
      {
        // release the lock while we're executing the worker
        lk.unlock();

        // execute the worker and measure its runtime
        worker();
        int workerTime = t.getTime__ms();

        // re-lock after the worker
        lk.lock();

        // now that we own the lock, we can update
        // the internal stats
        stats.update(workerTime);

        // check after every (potentially long) operation
        if (forceQuitThreadFromDtor || (curState == CyclicWorkerThreadState::Finished)) return;

        // at this point, lk is always locked
      }

      // at this point, lk is always locked

      // process pending state machine events
      // at least once in every cycle, regardless
      // of how much time is left after executing the worker
      doStateMachine(lk);

      cv_status eventStatus{cv_status::timeout};
      int remainTime = workerCycle_ms - t.getTime__ms();
      if (remainTime > 0)
      {
        // wait for any state machine event triggered
        // by the controlling thread
        eventStatus = cvState.wait_for(lk, chrono::milliseconds(remainTime));
        if (eventStatus != cv_status::timeout) doStateMachine(lk);
      }

      isNewWorkerCycle = ((remainTime < 0) || (eventStatus == cv_status::timeout));

      // lk is ALWAYS locked at this point
    }
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::doStateMachine(unique_lock<mutex>& lk)
  {
    // PRECONDITION:
    // lk is locked by the caller! In our case this is guaranteed
    // by calling this method ONLY after we have been notified
    // through the condition variable

    // POSTCONDITION:
    // lk is re-locked by this method or even left untouched.


    //
    // Transition logic
    //

    // is there anything to do at all?
    // this check is necessary because after every worker
    // cycle we call the statemachine, regardless of any
    // pending events
    if (reqState == curState) return;

    // Termination takes precedence, check that first
    if (reqState == CyclicWorkerThreadState::Finished)
    {
      curState = CyclicWorkerThreadState::Terminating;

      lk.unlock();
      onTerminate();
      lk.lock();

      curState = CyclicWorkerThreadState::Finished;
      isTransitionPending = false;
      return;
    }

    // Initialized --> Preparing --> Running
    if ((curState == CyclicWorkerThreadState::Initialized) &&
        (reqState == CyclicWorkerThreadState::Running))
    {
      curState = CyclicWorkerThreadState::Preparing;

      lk.unlock();
      onFirstRun();
      lk.lock();

      curState = CyclicWorkerThreadState::Running;

      isTransitionPending = false;
      return;
    }

    // Running --> Suspending --> Suspended
    if ((curState == CyclicWorkerThreadState::Running) &&
        (reqState == CyclicWorkerThreadState::Suspended))
    {
      curState = CyclicWorkerThreadState::Suspending;

      lk.unlock();
      onSuspend();
      lk.lock();

      curState = CyclicWorkerThreadState::Suspended;

      isTransitionPending = false;
      return;
    }

    // Suspended --> Resuming --> Running
    if ((curState == CyclicWorkerThreadState::Suspended) &&
        (reqState == CyclicWorkerThreadState::Running))
    {
      curState = CyclicWorkerThreadState::Resuming;

      lk.unlock();
      onResume();
      lk.lock();

      curState = CyclicWorkerThreadState::Running;

      isTransitionPending = false;
      return;
    }

    // consistency check
    //
    // we should never reach this point
    throw std::runtime_error("CyclicWorkerThread: state machine inconsistency or missing state transition!");
  }

  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------


  //----------------------------------------------------------------------------

}
