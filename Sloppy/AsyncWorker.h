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

#ifndef __LIBSLOPPY_ASYNCWORKER_H
#define __LIBSLOPPY_ASYNCWORKER_H

#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <climits>
#include <type_traits>

#include "ThreadSafeQueue.h"
#include "ThreadStats.h"

using namespace std;

namespace Sloppy
{
  /** \brief Template class for a "data consumer" or "worker" that takes input
   * data from an input queue, processes it async to the "feeder thread"
   * and stores the result in an output queue.
   *
   * Input data is processed according to the FIFO principle.
   *
   * Each input element produces exactly one output element.
   *
   * \note `InputDataType` has to be default constructible and copy assignable.
   * `OutputDataType` has to be copy assignable.
   */
  template<typename InputDataType, typename OutputDataType>
  class AsyncWorkerWithOutput
  {
  public:
    /** \brief Default ctor; creates the worker thread
     */
    AsyncWorkerWithOutput(
        int preemptionTime_ms_ = 200   ///< time after which we check for pause / run / join requests, if our input queue is empty
        )
      :preemptionTime_ms{preemptionTime_ms_}
    {
      static_assert (std::is_default_constructible<InputDataType>::value,
                     "The input data type for the AsyncWorkerWithOutput has to be default constructible!");
      static_assert (std::is_copy_assignable<InputDataType>::value,
                     "The input data type for the AsyncWorkerWithOutput has to be copy assignable!");
      static_assert (std::is_copy_assignable<OutputDataType>::value,
                     "The output data type for the AsyncWorkerWithOutput has to be copy assignable!");

      workerThread = thread([&]{mainLoop();});
    }

    /** \brief Dtor; stops the worker loop at the next occastion and `join`s
     * the worker thread
     */
    ~AsyncWorkerWithOutput()
    {
      join();
    }

    /** \brief Copy-append data to the end of the input queue
     */
    void put(
        const InputDataType& inData   ///< the data that shall be copy-appended to the queue
        )
    {
      inQueue.put(inData);
    }

    /** \brief Move-append data to the end of the input queue
     */
    void put(
        InputDataType&& inData   ///< the data that shall be moved to the queue
        )
    {
      inQueue.put(std::move(inData));
    }

    /** \brief Reads data from the out queue into a caller-provided reference; blocks
     * until data is available
     */
    void get(
        OutputDataType& outData   ///< the reference to copy the data to (will be transfered by copy-assignment)
        )
    {
      outQueue.get(outData);
    }

    /** \brief Reads data from the output queue into a caller-provided reference
     *
     * \returns `true` if data was returned, `false` otherwise
     */
    bool get(
        OutputDataType& outData,   ///< the reference to copy the data to
        int timeout_ms   /// an optional timeout after which the function returns without value if the queue remains empty (< 0 means: wait forever)
        )
    {
      return outQueue.get(outData, timeout_ms);
    }

    /** \returns `true` if the input queue has data available
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    bool hasInputData()
    {
      return inQueue.hasData();
    }

    /** \returns `true` if the output queue has data available
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    bool hasOutputData()
    {
      return outQueue.hasData();
    }

    /** \returns the number of items that are currently in the input queue
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    int inputQueueSize()
    {
      return inQueue.size();
    }

    /** \returns the number of items that are currently in the input queue
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    int outputQueueSize()
    {
      return outQueue.size();
    }

    /** \brief Drops all pending elements from the input queue
     *
     */
    void clearInput()
    {
      inQueue.clear();
    }

    /** \brief Drops all pending elements from the output queue
     *
     */
    void clearOutput()
    {
      outQueue.clear();
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
      lock_guard<mutex> lg{statsMutex};

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
          this_thread::sleep_for(chrono::milliseconds{preemptionTime_ms});
          continue;
        }

        // execute the worker
        InputDataType inData;
        bool hasData = inQueue.get(inData, preemptionTime_ms);
        if (hasData)
        {
          Sloppy::Timer t;
          OutputDataType outData = worker(inData);
          int execTime = t.getTime__ms();
          outQueue.put(outData);

          lock_guard<mutex> lgStats{statsMutex};
          statData.update(execTime);
        }
      }
    }

    int preemptionTime_ms;
    thread workerThread;
    ThreadSafeQueue<InputDataType> inQueue;
    ThreadSafeQueue<OutputDataType> outQueue;
    atomic_bool isRunning{true};
    atomic_bool joinRequested{false};
    atomic_bool suspendRequested{false};
    mutex statsMutex;
    AsyncWorkerStats statData;
  };
}

#endif
