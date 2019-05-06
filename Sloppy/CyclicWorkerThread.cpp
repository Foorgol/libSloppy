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

#include "CyclicWorkerThread.h"

namespace Sloppy
{

  CyclicWorkerThread::CyclicWorkerThread(int minWorkerCycle_ms, int minIdleCycle_ms)
    :workerCycle_ms{minWorkerCycle_ms}, idleCycle_ms{minIdleCycle_ms}
  {
    if (minIdleCycle_ms < 0)
    {
      minIdleCycle_ms = minWorkerCycle_ms;
    }

    workerThread = thread([&]{mainLoop();});
  }

  //----------------------------------------------------------------------------

  CyclicWorkerThread::~CyclicWorkerThread()
  {
    this->terminate();
    waitForStateChange();
    workerThread.join();
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
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::waitForStateChange() const
  {
    while (isTransitionPending)
    {
      this_thread::sleep_for(chrono::milliseconds(idleCycle_ms));
    }
  }

  //----------------------------------------------------------------------------

  void CyclicWorkerThread::mainLoop()
  {
    // short-cut
    using Clock = chrono::high_resolution_clock;

    // action after the next sleep()
    enum class NextAction
    {
      StateMachine,
      Worker
    };
    NextAction next{NextAction::StateMachine};

    // time points when to trigger which action
    chrono::high_resolution_clock::time_point nextStateMachineExecution;
    chrono::high_resolution_clock::time_point nextWorkerExecution;

    while (true)
    {
      bool forceRunWorker{false};

      if (next == NextAction::StateMachine)
      {
        // handle pending state changes
        Clock::time_point start = Clock::now();
        forceRunWorker = doStateMachine();
        nextStateMachineExecution = start + chrono::milliseconds(idleCycle_ms);

        // terminate this thread function and
        // make the thread joinable
        //
        // no need for locking here because "Finished" is
        // a dead end; once we're in this state there is no
        // way of changing that
        if (curState == CyclicWorkerThreadState::Finished) return;
      }

      if ((next == NextAction::Worker) || forceRunWorker)
      {
        unique_lock<mutex> lk{stateMutex};  // implicitly calls `lock()`
        Clock::time_point workerStart = Clock::now();

        if (curState == CyclicWorkerThreadState::Running)
        {
          lk.unlock();  // release the lock while we're executing the worker

          // execute the worker and measure its runtime
          worker();
          int workerTime = chrono::duration_cast<chrono::milliseconds>(Clock::now() - workerStart).count();
        }

        nextWorkerExecution = workerStart + chrono::milliseconds(workerCycle_ms);
      }

      // what is the next action? state machine or worker?
      if (nextStateMachineExecution < nextWorkerExecution)
      {
        this_thread::sleep_until(nextStateMachineExecution);
        next = NextAction::StateMachine;
      } else {
        this_thread::sleep_until(nextWorkerExecution);
        next = NextAction::Worker;
      }
    }
  }

  //----------------------------------------------------------------------------

  bool CyclicWorkerThread::doStateMachine()
  {
    if (!isTransitionPending) return false;

    // use a unique_lock here instead of a lock_guard, because
    // a unique_lock allows for multiple locking / unlocking operations
    unique_lock<mutex> lk{stateMutex};  // implicitly calls `lock()`

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

      return false;
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

      return true;  // force-run the worker NOW
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

      return false;
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

      return true;  // force-run the worker NOW
    }

    // consistency check
    //
    // we should never reach this point
    throw std::runtime_error("CyclicWorkerThread: state machine inconsistency or missing state transition!");
  }

  //----------------------------------------------------------------------------



  //----------------------------------------------------------------------------

}
