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

#ifndef __LIBSLOPPY_TIMER_H
#define __LIBSLOPPY_TIMER_H

#include <string>
#include <chrono>

namespace Sloppy
{
  /** \brief A basic timer for measuring durations and checking timeouts
   *
   * It uses the highest available clock for maximum precision.
   */
  class Timer
  {
  public:
    /** \brief Ctor for a new timer that is started immediately and that
     * has no timeout set.
     */
    Timer() : startTime{std::chrono::high_resolution_clock::now()}, isStopped{false}, hasTimeoutSet{false} {}

    /** \brief Stops the timer.
     *
     * If the timer has already been stopped before we do nothing
     * and the original stop time is not modified.
     */
    void stop();

    /** \brief Resets and restarts the timer with a new
     * start time of "now".
     *
     * A potentially set timeout is not reset but it is now
     * applied to the new timer start time of "now". Read:
     * the timeout starts all over.
     */
    void restart();

    /** \returns the elapsed time in a custom resolution.
     */
    template<typename Resolution>
    Resolution getTime() const
    {
      if (isStopped)
      {
        return std::chrono::duration_cast<Resolution>(stopTime - startTime);
      }

      std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
      return std::chrono::duration_cast<Resolution>(now - startTime);
    }

    /** \returns the elapsed time in nanoseconds
     */
    long getTime__ns() const { return getTime<std::chrono::nanoseconds>().count(); }

    /** \returns the elapsed time in microseconds, truncated
     */
    long getTime__us() const { return getTime<std::chrono::microseconds>().count(); }

    /** \returns the elapsed time in milliseconds, truncated
     */
    long getTime__ms() const { return getTime<std::chrono::milliseconds>().count(); }

    /** \returns the elapsed time in seconds, truncated
     */
    long getTime__secs() const { return getTime<std::chrono::seconds>().count(); }

    /** \returns the elapsed time in seconds in `double` resolution incl. decimals
     */
    double getTime__secsDouble() const { return getTime<std::chrono::duration<double>>().count(); }

    /** \returns the elapsed time in milliseconds in `double` resolution incl. decimals
     */
    double getTime__msDouble() const { return getTime<std::chrono::duration<double, std::milli>>().count(); }

    /** \brief Sets or updates the timeout duration in a custom resolution
     */
    template<typename Resolution>
    void setTimeoutDuration(const Resolution& timeout)
    {
      timeoutDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
      hasTimeoutSet = true;
    }

    /** \brief Sets or updates the timeout duration in nanosecons
     */
    void setTimeoutDuration__ns(long ns) { setTimeoutDuration<std::chrono::nanoseconds>(std::chrono::nanoseconds{ns}); }

    /** \brief Sets or updates the timeout duration in microseconds
     */
    void setTimeoutDuration__us(long us) { setTimeoutDuration<std::chrono::microseconds>(std::chrono::microseconds{us}); }

    /** \brief Sets or updates the timeout duration in milliseconds
     */
    void setTimeoutDuration__ms(long ms) { setTimeoutDuration<std::chrono::milliseconds>(std::chrono::milliseconds{ms}); }

    /** \brief Sets or updates the timeout duration in seconds
     */
    void setTimeoutDuration__secs(long s) { setTimeoutDuration<std::chrono::seconds>(std::chrono::seconds{s}); }

    /** \returns `true` if a timeout has been set and if at least the timeout duration
     * has passed since timer construction or the last restart.
     */
    bool isElapsed() const;

    /** \returns the remaining time in a custom resolution until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     */
    template<typename Resolution>
    long getRemainingTime() const
    {
      if (!hasTimeoutSet) return -1;

      std::chrono::nanoseconds elapsed = getTime<std::chrono::nanoseconds>();

      if (elapsed >= timeoutDuration) return 0;

      std::chrono::nanoseconds remain = timeoutDuration - elapsed;

      return (std::chrono::duration_cast<Resolution>(remain)).count();
    }

    /** \returns the remaining time in nanoseconds until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     */
    long getRemainingTime__ns() const { return getRemainingTime<std::chrono::nanoseconds>(); }

    /** \returns the remaining time in microseconds until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     */
    long getRemainingTime__us() const { return getRemainingTime<std::chrono::microseconds>(); }

    /** \returns the remaining time in milliseconds until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     */
    long getRemainingTime__ms() const { return getRemainingTime<std::chrono::milliseconds>(); }

    /** \returns the remaining time in seconds until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     */
    long getRemainingTime__secs() const { return getRemainingTime<std::chrono::seconds>(); }

    /** \returns the remaining time in nanoseconds until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     *
     * \note We're using only microsecond precision here for calculating the fraction seconds.
     */
    double getRemainingTime__secsDouble() const
    {
      long r = getRemainingTime__us();
      return (r > 0) ? (r / 1000000.0) : r;
    }

    /** \returns the remaining time in milliseconds until the timeout occurs;
     * `-1` if no timeout has been set and `0` if the timer has already elapsed
     */
    double getRemainingTime__msDouble() const
    {
      long r = getRemainingTime__ns();
      return (r > 0) ? (r / 1000000.0) : r;
    }

  private:
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point stopTime;
    bool isStopped;
    std::chrono::nanoseconds timeoutDuration;
    bool hasTimeoutSet;
  };
}

#endif
