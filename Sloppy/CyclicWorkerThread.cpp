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

#include <assert.h>

#include "CyclicWorkerThread.h"

namespace Sloppy
{

  CyclicWorkerThread::CyclicWorkerThread(int minWorkerCycle_ms)
    :workerCycle_ms{minWorkerCycle_ms}
  {
    workerThread = thread([&]{mainLoop();});
  }

  //----------------------------------------------------------------------------

  CyclicWorkerThread::~CyclicWorkerThread()
  {
    //this->terminate();
    //waitForStateChange();
    forceQuitThreadFromDtor = true;
    workerThread.join();
  }

  //----------------------------------------------------------------------------

  bool CyclicWorkerThread::run()
  {
    //if (isTransitionPending) return false;

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
    //if (isTransitionPending) return false;

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
    //if (isTransitionPending) return false;

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

  void CyclicWorkerThread::waitForStateChange()
  {
    while (isTransitionPending)
    {
      /*lock_guard<mutex> lk{stateMutex};
      if (reqState == curState) return;*/

      this_thread::sleep_for(chrono::milliseconds(WaitForStateChangePollingTime_ms));
    }
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::mainLoop()
  {
    // short-cut
    using Clock = chrono::high_resolution_clock;

    // initially lock the mutex because that's a pre-condition
    // when starting the while loop
    unique_lock<mutex> lk{stateMutex};  // implicitly calls `lock()`

    while (!forceQuitThreadFromDtor)
    {
      // just for testing
      assert(lk.owns_lock());

      Sloppy::Timer t;

      if (reqState == CyclicWorkerThreadState::Running)
      {
        lk.unlock();  // release the lock while we're executing the worker

        // execute the worker and measure its runtime
        worker();
        int workerTime = t.getTime__ms();

        // re-lock after the worker
        lk.lock();

        // check after every (potentially long) operation
        if (forceQuitThreadFromDtor) return;

        // process pending state machine events
        // at least once in every cycle, regardless
        // of how much time is left after executing the worker
        if (isTransitionPending)
        {
          // process the pending notification
          //
          // should unlock, relock and return immediately
          cvState.wait(lk);

          // process the event
          doStateMachine(lk);
        }

        // at this point, lk is always locked
      }

      // at this point, lk is always locked

      // wait / process state machine events
      // for the remaining time until the next worker call
      while (!forceQuitThreadFromDtor)
      {
        int remainTime = workerCycle_ms - t.getTime__ms();
        if (remainTime <= 0) break;

        // wait for any state machine event triggered
        // by the controlling thread
        cv_status eventStatus = cvState.wait_for(lk, chrono::milliseconds(remainTime));
        if (eventStatus == cv_status::timeout) break;

        doStateMachine(lk);
        if (curState == CyclicWorkerThreadState::Finished) return;  // leave the thread func to prepare for join()

        // doStateMachine guarantees that it returnes with
        // lk being (re-)locked!

        // just for testing
        assert(lk.owns_lock());

        // start all over waiting for events, if there is
        // still enough time left
      }

      // lk is ALWAYS locked at this point

      // just for testing
      assert(lk.owns_lock());
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

}
