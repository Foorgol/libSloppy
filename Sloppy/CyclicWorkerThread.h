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

#ifndef __LIBSLOPPY_CYCLIC_WORKER_THREAD_H
#define __LIBSLOPPY_CYCLIC_WORKER_THREAD_H

#include <atomic>              // for atomic
#include <condition_variable>  // for condition_variable
#include <mutex>               // for mutex, lock_guard, unique_lock
#include <thread>              // for thread

#include "ThreadStats.h"       // for CyclicThreadStats

namespace Sloppy
{

  /** \brief Base class for a thread that cyclically calls
   * a worker function.
   *
   * The worker function has to be implemented in a class
   * derived from this abstract base class.
   *
   * The execution of the worker can be externally controlled,
   * e.g., started, stopped, paused, resumed or terminated.
   *
   */
  class CyclicWorkerThread
  {
  public:
    static constexpr int WaitForStateChangePollingTime_ms = 2;

    /** \brief The basic states of a cyclic worker */
    enum class CyclicWorkerThreadState
    {
      Initialized,   ///< the object has been created and the worker has never been executed so far

      Preparing,   ///< we're currently executing `onFirstRun()` and are about to enter `Running` state
      Running,   ///< the worker is being cyclically executed

      Suspending, ///< we're currently executing `onSuspend()` and are about to enter `Suspended` state
      Suspended,   ///< execution of the worker is currently suspended

      Resuming,   ///< we're currently executing `onResume()` and are about to re-enter `Running` state

      Terminating,   ///< we're currently executing `onTerminate()` and are about to enter `Finished' state
      Finished   ///< terminal state and dead end; no exit from here
    };

    /** \brief Ctor that sets up the cyclic behaviour of the thread
     */
    CyclicWorkerThread(
        int minWorkerCycle_ms ///< minimum time in millisecs between two calls of the worker function
        //int minIdleCycle_ms = -1 ///< minimum time in millisecs between checks for a status change if the worker is not currently running (-1 = use the worker cycle)
        );

    /** \brief Dtor, calls `join()` on the underlying thread for proper clean-up*/
    virtual ~CyclicWorkerThread();

    /** \returns the current state of the cyclic thread
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     */
    CyclicWorkerThreadState state()
    {
      std::lock_guard<std::mutex> lk{stateMutex};

      CyclicWorkerThreadState tmp = curState;
      return tmp;
    }

    /** \brief (Re-)enables the cyclic execution of the worker
     *
     * The object must be either in `Initialized` or in `Suspended` state,
     * otherwise the command has no effect.
     *
     * This is an async call that doesn't wait for completion of the
     * actual state transition.
     *
     * if the current state is `Initialized`, the `onFirstRun()` hook is
     * executed and immediately afterwards the worker is called.
     *
     * if the current state is `Suspended`, the `onResume()` hook is
     * executed and immediately afterwards the worker is called.
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     *
     * \returns `true` if the state change request was valid, `false` otherwise
     */
    bool run();

    /** \brief Suspends the cyclic execution of the worker
     *
     * The object must be in `Running` state,
     * otherwise the command has no effect.
     *
     * This is an async call that doesn't wait for completion of the
     * actual state transition. The transition doesn't take place before
     * an ongoing iteration of the worker has finished.
     *
     * Before we enter `Suspended` state, `onSuspend()` is executed.
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     *
     * \returns `true` if the state change request was valid or the thread is already paused, `false` otherwise
     */
    bool pause();

    /** \brief Resume the cyclic execution of the worker
     *
     * The object must be in `Suspended` state,
     * otherwise the command has no effect.
     *
     * This is an async call that doesn't wait for completion of the
     * actual state transition. The transition doesn't take place before
     * an ongoing iteration of the worker has finished.
     *
     * Before we re-enter `Running` state, `onResume()` is executed.
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     *
     * \returns `true` if the state change request was valid or if the thread is already `Running`, `false` otherwise
     */
    bool resume();

    /** \brief Ultimately stops the cyclic execution of the worker
     *
     * A request for termination will always succeed, regardless of
     * the current state.
     *
     * This is an async call that doesn't wait for completion of the
     * actual state transition. The transition doesn't take place before
     * an ongoing iteration of the worker has finished.
     *
     * Before we enter `Finished` state, `onTerminate()` is executed.
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     */
    void terminate();

    /** \brief Ultimately stops the cyclic execution of the worker and
     * deletes the worker thread in order to free up resources.
     *
     * A request for termination will always succeed, regardless of
     * the current state.
     *
     * This is blocking call that doesn't return until the worker is
     * stopped and the thread is join()ed.
     *
     * Before we enter `Finished` state, `onTerminate()` is executed.
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     */
    void terminateAndJoin();

    /** \brief Blocks until pending state transitions are completed
     *
     * if there is a state change pending, we check every `minIdleCycle`
     * (see ctor parameters) whether the transition is complete.
     *
     * \note This function is for execution in the CONTROLLER THREAD CONTEXT only!
     */
    void waitForStateChange();

    /** \returns Some basic stats about the worker thread
     */
    CyclicThreadStats workerStats();

  protected:

    /** \brief Overloadable hook that is called before the first execution of the worker
     *
     * \note This function is for execution in the WORKER THREAD CONTEXT only!
     */
    virtual void onFirstRun() {}

    /** \brief Overloadable hook that is called before we enter the "Suspended" state
     *
     * \note This function is for execution in the WORKER THREAD CONTEXT only!
     */
    virtual void onSuspend() {}

    /** \brief Overloadable hook that is called before we resume the worker
     *
     * \note This function is for execution in the WORKER THREAD CONTEXT only!
     */
    virtual void onResume() {}

    /** \brief Overloadable hook that is called
     * after the worker has been executed for the last time
     *
     * \note This function is for execution in the WORKER THREAD CONTEXT only!
     */
    virtual void onTerminate() {}

    /** \brief The actual worker that has to be implemented by a derived class
     *
     * \note This function is for execution in the WORKER THREAD CONTEXT only!
     */
    virtual void worker() {};

  private:
    // the actual low-level worker that is run by the thread
    //
    // NOTE: This function is for execution in the WORKER THREAD CONTEXT only!
    void mainLoop();

    // processes state transitions and calls hooks;
    // is cyclically called by the mainLoop()
    //
    // NOTE: This function is for execution in the WORKER THREAD CONTEXT only!
    //
    // Returns `true` if the worker should be forcefully executing afterwards,
    // regardless of any timer values
    void doStateMachine(std::unique_lock<std::mutex>& lk);

    std::mutex stateMutex;
    std::condition_variable cvState;

    CyclicWorkerThreadState curState{CyclicWorkerThreadState::Initialized};   ///< our current state
    CyclicWorkerThreadState reqState{CyclicWorkerThreadState::Initialized};   ///< the next state requested by the owner
    std::thread workerThread;
    int workerCycle_ms;

    std::atomic<bool> isTransitionPending{false};  ///< not strictly necessary, but makes sync easier
    bool forceQuitThreadFromDtor{false}; // may only be set by the dtor!

    CyclicThreadStats stats;
  };
}

#endif
