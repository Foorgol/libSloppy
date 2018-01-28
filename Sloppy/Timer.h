/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
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
 *    Copyright (C) 2016 - 2017  Volker Knollmann
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

using namespace std;

namespace Sloppy
{
  class Timer
  {
  public:
    Timer() : startTime{chrono::high_resolution_clock::now()}, isStopped{false}, hasTimeoutSet{false} {}

    void stop();
    void restart();

    template<typename Resolution>
    Resolution getTime() const
    {
      if (isStopped)
      {
        return chrono::duration_cast<Resolution>(stopTime - startTime);
      }

      chrono::high_resolution_clock::time_point now = chrono::high_resolution_clock::now();
      return chrono::duration_cast<Resolution>(now - startTime);
    }

    long getTime__ns() const { return getTime<chrono::nanoseconds>().count(); }
    long getTime__us() const { return getTime<chrono::microseconds>().count(); }
    long getTime__ms() const { return getTime<chrono::milliseconds>().count(); }
    long getTime__secs() const { return getTime<chrono::seconds>().count(); }
    double getTime__secsDouble() const { return getTime<chrono::duration<double>>().count(); }
    double getTime__msDouble() const { return getTime<chrono::duration<double, milli>>().count(); }

    template<typename Resolution>
    void setTimeoutDuration(const Resolution& timeout)
    {
      timeoutDuration = chrono::duration_cast<chrono::nanoseconds>(timeout);
      hasTimeoutSet = true;
    }
    void setTimeoutDuration__ns(long ns) { setTimeoutDuration<chrono::nanoseconds>(chrono::nanoseconds{ns}); }
    void setTimeoutDuration__us(long us) { setTimeoutDuration<chrono::microseconds>(chrono::microseconds{us}); }
    void setTimeoutDuration__ms(long ms) { setTimeoutDuration<chrono::milliseconds>(chrono::milliseconds{ms}); }
    void setTimeoutDuration__secs(long s) { setTimeoutDuration<chrono::seconds>(chrono::seconds{s}); }
    bool isElapsed() const;

  private:
    chrono::high_resolution_clock::time_point startTime;
    chrono::high_resolution_clock::time_point stopTime;
    bool isStopped;
    chrono::nanoseconds timeoutDuration;
    bool hasTimeoutSet;
  };
}

#endif
