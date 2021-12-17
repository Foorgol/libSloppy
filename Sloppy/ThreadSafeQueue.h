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

#pragma once

#include <string>
#include <chrono>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <optional>
#include "Timer.h"

// we include some special file functions for
// non-Windows builds only
#ifndef WIN32
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#endif

namespace Sloppy
{
  template<typename T>
  class AbstractThreadSafeQueue
  {
  public:
    using DataType = T;

    /** \brief Copy-append data to the end of the queue
     */
    void put(
        const T& inData   ///< the data that shall be copy-appended to the queue
        )
    {
      std::lock_guard<std::mutex> lg{listMutex};
      queue.push_back(inData);
      notify();
    }

    /** \brief Append data to the end of the queue using perfect forwaring
     */
    void put(
        T&& inData   ///< the data that shall be moved to the queue
        )
    {
      std::lock_guard<std::mutex> lg{listMutex};
      queue.push_back(std::forward<T>(inData));
      notify();
    }

    /** \brief Wait blockingly until new data is available in the queue and returns / removes
     *  the oldest data entry in the queue
     */
    T get() {
      return get(-1);
    }

    std::optional<T> get(int timeout_ms) {
      if (!waitForData(timeout_ms)) {
        return std::nullopt;
      }
      std::lock_guard<std::mutex> lg{listMutex};
      const T outData = queue.front();
      queue.pop_front();
      return outData;
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

  protected:
    virtual void notify() = 0;  ///< only to be caller by the writer thread; mutex must be in place

    virtual bool waitForData(int timeout_ms) = 0;   ///< mutex is NOT in acquired when we call this!

    /** \brief Direct access to the queue's empty() operator for derived classes
     *
     *  \warning This function does not provide any mutex protection
     *  for accessing the queue!
     *
     *  \pre The caller has to acquire the mutex for accessing the queue!
     */
    bool unprotected_empty() const {
      return queue.empty();
    }

    /** \brief Derived classes need direct access to the mutex */
    std::mutex listMutex;

  private:
    std::deque<T> queue;
  };

  //------------------------------------------------------------------------------------------

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
  class ThreadSafeQueue : public AbstractThreadSafeQueue<T> {

  protected:
    void notify() override {
      cv.notify_one();
    }

    bool waitForData(int timeout_ms) override {
      // block infinitely until data is available
      if (timeout_ms < 0) {
        // condition variables only work with unique_lock, not with lock_guard
        std::unique_lock<std::mutex> lock{this->listMutex};

        // wait for a notification that new data has arrived;
        // ignores spurious wakeups and is guaranteed to not return
        // until there is data in the queue
        cv.wait(lock, [this](){ return !(this->unprotected_empty()); });
        return true;
      }

      // quickly check if there's pending data
      if (timeout_ms == 0) {
        std::lock_guard<std::mutex> lg{this->listMutex};
        return !(this->unprotected_empty());
      }

      // get exclusive access to the queue;
      //
      // condition variables only work with unique_lock, not with lock_guard
      std::unique_lock<std::mutex> lock{this->listMutex};
      bool hasData = !(this->unprotected_empty());

      //
      // wait / sleep if there is no data available and if the
      // caller provided a "real" timeout value > 0
      //
      if (!hasData)
      {
        Sloppy::Timer t;
        t.setTimeoutDuration__ms(timeout_ms);
        while (!t.isElapsed())
        {
          hasData = cv.wait_for(lock, std::chrono::milliseconds{timeout_ms - t.getTime__ms()},
                            [this](){ return !(this->unprotected_empty()); });

          if (hasData) break;

          // sometime we might have "spurious wake-ups" before the
          // timeout has elapsed.
          //
          // by using the timer we make sure that we stick to the
          // demanded waiting time
        }
      }

      // at this point, we either have data or a timeout
      return hasData;
    }

  private:
    std::condition_variable cv;

  };

  //------------------------------------------------------------------------------------------

#ifndef WIN32

  /** \brief Template class for a queue that can be used by independent threads,
   * one thread for reading and one or more for writing.
   *
   * The reading thread is be notified through an internal pipe that contains
   * one token for each element put into the queue. The reading thread can
   * include the pipe's file descriptor in the (e)poll call of the main event queue.
   *
   * Thus, the reading thread can simultaneously wait for events on multiple
   * queues at once.
   *
   * The queue uses the first-in-first-out principle.
   *
   * \note Since we're using pipes, this implementation is not available
   * for Windows.
   *
   */
  template<typename T>
  class ThreadSafeQueue_PipeSynced : public AbstractThreadSafeQueue<T>
  {
  public:
    ThreadSafeQueue_PipeSynced() {
      // create a pipe between the controller thread and the worker thread
      int fd[2];
      int rc = ::pipe(fd);
      if (rc < 0) {
        throw std::runtime_error("ThreadSafeQueue_PipeSynced::ctor(): could not create pipe");
      }
      pipeReadFd = fd[0];
      pipeWriteFd = fd[1];

      // prepare an epoll context and register the pipe's end for reading
      epollContext = epoll_create(10);  // the parameter value isn't used

      if (epollContext < 0) {
        throw std::runtime_error("ThreadSafeQueue_PipeSynced::ctor(): could create new epoll instance");
      }
      epoll_event ev;
      memset(&ev, 0, sizeof (ev));
      ev.data.fd = pipeReadFd;
      ev.events = EPOLLIN;
      rc = epoll_ctl(epollContext, EPOLL_CTL_ADD, pipeReadFd, &ev);
      if (rc != 0) {
        throw std::runtime_error("epoll error: " + std::string{strerror(errno)});
      }
    }

    ~ThreadSafeQueue_PipeSynced() {
      std::lock_guard<std::mutex> lg{this->listMutex};
      ::close(pipeReadFd);
      ::close(pipeWriteFd);
      ::close(epollContext);
    }

    /** \brief Exposes the file descriptor for the pipe's end
     *  to the user. Useful for including it in the user's main
     *  poll() call.
     *
     *  \warning Use the file descriptor for POLLING ONLY! Never
     *  execute any read(), write() or close() operation on the
     *  descriptor.
     */
    int fdForPolling() const { return pipeReadFd; }

  protected:
    void notify() override {
      char token{'x'};
      ::write(pipeWriteFd, &token, 1);
    }

    bool waitForData(int timeout_ms) override {
      // use epoll to handle all variants of
      // timeout values for us
      epoll_event rdyList;
      int nReady = epoll_wait(epollContext, &rdyList, 1, timeout_ms);
      if (nReady != 1) return false;

      if (rdyList.data.fd != pipeReadFd) return false;

      char token;
      int n = ::read(pipeReadFd, &token, 1);
      return (n == 1);
    }

  private:
    int pipeReadFd{-1};
    int pipeWriteFd{-1};
    int epollContext{-1};
  };

#endif
}
