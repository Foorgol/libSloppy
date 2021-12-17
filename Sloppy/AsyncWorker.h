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

#ifndef __LIBSLOPPY_ASYNCWORKER_H
#define __LIBSLOPPY_ASYNCWORKER_H

#include <atomic>         // for atomic_bool
#include <chrono>         // for milliseconds
#include <mutex>          // for mutex, lock_guard
#include <thread>         // for thread, sleep_for
#include <type_traits>    // for is_copy_assignable, is_default_constructible

#include "ThreadStats.h"  // for AsyncWorkerStats
#include "Timer.h"        // for Timer

namespace Sloppy
{
  template <typename T> class ThreadSafeQueue;

  /** \brief Template class for a "data consumer" or "worker" that takes input
   * data from an input queue, processes it async to the "feeder thread"
   * and stores the result in an output queue.
   *
   * Input data is processed according to the FIFO principle.
   *
   * Each input element produces exactly one output element.
   *
   * The input and output queues have to be provided by the caller.
   * This allows for best synchronization with other threads / workers that
   * produce or consume data for or from this worker.
   *
   * This class guarantees to only call `get` on the input queue and only
   * `put` on the output queue.
   *
   * \note `InputDataType` has to be default constructible and copy assignable.
   * `OutputDataType` has to be copy assignable.
   */
  template<typename InputDataType, typename OutputDataType>
  class AsyncWorker
  {
  public:
    /** \brief Default ctor; creates the worker thread
     */
    AsyncWorker(
        ThreadSafeQueue<InputDataType>* inQueuePtr,   ///< pointer to the queue that we'll take our input data from
        ThreadSafeQueue<OutputDataType>* outQueuePtr,   ///< pointer to the queue to which we'll write our result; if `nullptr`, worker results will be discarded
        int preemptionTime_ms_ = 200   ///< time after which we check for pause / run / join requests, if our input queue is empty
        )
      :inPtr{inQueuePtr}, outPtr{outQueuePtr}, preemptionTime_ms{preemptionTime_ms_}
    {
      static_assert (std::is_default_constructible<InputDataType>::value,
                     "The input data type for the AsyncWorkerWithOutput has to be default constructible!");
      static_assert (std::is_copy_assignable<InputDataType>::value,
                     "The input data type for the AsyncWorkerWithOutput has to be copy assignable!");
      static_assert (std::is_copy_assignable<OutputDataType>::value,
                     "The output data type for the AsyncWorkerWithOutput has to be copy assignable!");

      workerThread = std::thread([&]{mainLoop();});
    }

    /** \brief Dtor; stops the worker loop at the next occastion and `join`s
     * the worker thread
     */
    ~AsyncWorker()
    {
      join();
    }

    /** \returns `true` if the worker is active, `false` otherwise
     *
     * `True` only means that we're waiting for input data and will
     * process it at the next possible occasion. It does not mean
     * that the user-provided worker function is currently being
     * executed.
     */
    bool running()
    {
      return isRunning;
    }

    /** \brief Requests a stop of the worker loop at the next
     * possible occasion and terminates the worker thread
     * afterwards; blocks until the worker thread is joined().
     *
     * \note If the user-provided worker function is currently
     * being executed, it has to finish first before we attempt
     * to terminate the thread. Thus you should make sure the
     * worker function has a reasonably short maximum execution time.
     *
     * \note Calls to `suspend()` or `resume()` won't have any
     * effect once join() has been called. Input and output
     * queue are not affected by `join()`. If required you
     * have to manually clear the queues after the join.
     */
    void join()
    {
      if (workerThread.joinable())
      {
        joinRequested = true;
        workerThread.join();
      }
    }

    /** \brief Requests to suspend the worker execution at the
     * next possible occastion (typically after the current
     * worker function call has finished).
     */
    void suspend()
    {
      suspendRequested = true;
    }

    /** \brief Requests to resume the worker execution at the
     * next possible occastion (typically after the preemption time
     * has elapsed).
     */
    void resume()
    {
      suspendRequested = false;
    }

    /** \returns Some execution statistics about the worker function
     */
    AsyncWorkerStats stats()
    {
      std::lock_guard<std::mutex> lg{statsMutex};

      return statData;
    }

  protected:
    /** \brief Actual implementation should overload this function
     * to provider their actual worker function.
     */
    virtual OutputDataType worker(const InputDataType& inData) {
      return OutputDataType{};
    }

  private:
    void mainLoop()
    {
      while (!joinRequested)
      {
        // state transitions
        isRunning = !suspendRequested;

        if (!isRunning)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds{preemptionTime_ms});
          continue;
        }

        // execute the worker
        const auto optData = inPtr->get(preemptionTime_ms);
        if (optData)
        {
          Sloppy::Timer t;
          OutputDataType outData = worker(*optData);
          int execTime = t.getTime__ms();
          if (outPtr != nullptr) outPtr->put(outData);

          std::lock_guard<std::mutex> lgStats{statsMutex};
          statData.update(execTime);
        }
      }
    }

    int preemptionTime_ms;
    std::thread workerThread;
    ThreadSafeQueue<InputDataType>* inPtr{nullptr};
    ThreadSafeQueue<OutputDataType>* outPtr{nullptr};
    std::atomic_bool isRunning{true};
    std::atomic_bool joinRequested{false};
    std::atomic_bool suspendRequested{false};
    std::mutex statsMutex;
    AsyncWorkerStats statData;
  };
}

#endif
