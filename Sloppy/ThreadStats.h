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

#ifndef __LIBSLOPPY_THREADSTATS_H
#define __LIBSLOPPY_THREADSTATS_H

#include <climits>


namespace Sloppy
{
  /** \brief A simple struct that contains some statistics
   * about a async running task
   */
  struct AsyncWorkerStats
  {
    unsigned long long nCalls{0};   ///< the number of calls to the worker function
    unsigned long long totalRuntime_ms{0};   ///< the accumulated execution time of all worker function calls
    int lastRuntime_ms{0};   ///< the number of millisecs the last worker function call lasted
    int minWorkerTime_ms{INT_MAX};
    int maxWorkerTime_ms{0};

    /** \returns the average execution time across all
     * worker calls so far (0 if no calls were performed so far)
     *
     * \warning If this struct is accessed from different threads, proper
     * locking (e.g., through a mutex) has to be guaranteed by the caller!
     */
    double avgWorkerExecTime_ms() const;

    /** \brief Updates the stats with the execution time of the
     * last worker call
     *
     * \warning If this struct is accessed from different threads, proper
     * locking (e.g., through a mutex) has to be guaranteed by the caller!
     */
    void update(int execTime_ms);
  };

  //----------------------------------------------------------------------------

  /** \brief A simple struct that contains some statistics
   * about a cyclically running task
   */
  struct CyclicThreadStats : public AsyncWorkerStats
  {
    int workerCycleTime_ms{0};

    /** \returns a value between 0...1 that represents the average
     * duty percentage of the worker loop (0 if no calls were performed so far)
     */
    double dutyPercentage() const;
  };
}

#endif
