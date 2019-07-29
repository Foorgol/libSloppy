/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

#ifndef __LIBSLOPPY_THREADSAFEQUEUE_H
#define __LIBSLOPPY_THREADSAFEQUEUE_H

#include <string>
#include <chrono>
#include <mutex>
#include <deque>
#include <condition_variable>
#include "Timer.h"

namespace Sloppy
{
  /** \brief Template class for a queue that can be used by independent threads,
   * one thread for reading and one or more for writing.
   *
   * The reading thread is be notified through a condition variable
   * that new data has arrived. This allows for more efficient synchronization
   * compared to cyclic polling.
   *
   * The queue uses the first-in-first-out principle.
   */
  template<typename T>
  class ThreadSafeQueue
  {
  public:
    /** \brief Copy-append data to the end of the queue
     */
    void put(
        const T& inData   ///< the data that shall be copy-appended to the queue
        )
    {
      std::lock_guard<std::mutex> lg{listMutex};
      queue.push_back(inData);
      cv.notify_one();
    }

    /** \brief Move-append data to the end of the queue
     */
    void put(
        T&& inData   ///< the data that shall be moved to the queue
        )
    {
      std::lock_guard<std::mutex> lg{listMutex};
      queue.push_back(std::move(inData));
      cv.notify_one();
    }

    /** \brief Reads data from the queue into a caller-provided reference; blocks
     * until data is available
     */
    void get(
        T& outData   ///< the reference to copy the data to (will be transfered by copy-assignment)
        )
    {
      // condition variables only work with unique_lock, not with lock_guard
      std::unique_lock<std::mutex> lock{listMutex};

      // wait for a notification that new data has arrived;
      // ignores spurious wakeups and is guaranteed to not return
      // until there is data in the queue
      cv.wait(lock, [this](){ return !queue.empty(); });

      outData = queue.front();
      queue.pop_front();
    }

    /** \brief Reads data from the queue into a caller-provided reference
     *
     * \returns `true` if data was returned, `false` otherwise
     */
    bool get(
        T& outData,   ///< the reference to copy the data to
        int timeout_ms   /// an optional timeout after which the function returns without value if the queue remains empty (< 0 means: wait forever)
        )
    {
      // special case: wait forever
      if (timeout_ms < 0)
      {
        get(outData);
        return true;
      }

      //
      // use "wait" with a timeout
      //

      // condition variables only work with unique_lock, not with lock_guard
      std::unique_lock<std::mutex> lock{listMutex};

      Sloppy::Timer t;
      t.setTimeoutDuration__ms(timeout_ms);
      bool hasData = false;
      while (!t.isElapsed())
      {
        hasData = cv.wait_for(lock, std::chrono::milliseconds{timeout_ms - t.getTime__ms()},
                          [this](){ return !queue.empty(); });

        if (hasData) break;

        // sometime we might have "spurious wake-ups" before the
        // timeout has elapsed.
        //
        // by using the timer we make sure that we stick to the
        // demanded waiting time
      }

      // at this point, we either have data or a timeout
      if (!hasData) return false;

      outData = queue.front();
      queue.pop_front();

      return true;
    }

    /** \returns `true` if the queue has data available
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    bool hasData()
    {
      std::lock_guard<std::mutex> lg{listMutex};
      return !queue.empty();
    }

    /** \returns `true` if the queue is empty
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    bool empty()
    {
      std::lock_guard<std::mutex> lg{listMutex};
      return queue.empty();
    }

    /** \returns the number of items that are currently in the queue
     *
     * \note It is not recommended to cyclically poll this function
     * to wait for fresh data, because each call requires getting
     * and releasing a lock on a mutex.
     */
    int size()
    {
      std::lock_guard<std::mutex> lg{listMutex};
      return queue.size();
    }

    /** \brief Erases all elements from the queue
     */
    void clear()
    {
      std::lock_guard<std::mutex> lg{listMutex};
      queue.clear();
    }

  private:
    std::mutex listMutex;
    std::condition_variable cv;
    std::deque<T> queue;
  };
}

#endif
