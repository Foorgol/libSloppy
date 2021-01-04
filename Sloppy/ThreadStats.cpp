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

#include "ThreadStats.h"

namespace Sloppy
{

  double AsyncWorkerStats::avgWorkerExecTime_ms() const
  {
    return (nCalls == 0) ? 0 : (double(totalRuntime_ms) / nCalls);
  }

  //----------------------------------------------------------------------------

  void AsyncWorkerStats::update(int execTime_ms)
  {
    nCalls++;
    totalRuntime_ms += execTime_ms;
    lastRuntime_ms = execTime_ms;
    if (execTime_ms > maxWorkerTime_ms) maxWorkerTime_ms = execTime_ms;
    if (execTime_ms < minWorkerTime_ms) minWorkerTime_ms = execTime_ms;
  }

  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------

  double CyclicThreadStats::dutyPercentage() const
  {
    return avgWorkerExecTime_ms() / workerCycleTime_ms;
  }



}
