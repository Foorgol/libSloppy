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

#ifndef __LIBSLOPPY_DATEANDTIME_H
#define __LIBSLOPPY_DATEANDTIME_H

#include <string>
#include <memory>
#include <ctime>
#include <cstring>
#include <tuple>

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

using namespace std;


//
// Extend boost's gregorian date to deal with
// integer date representations, e.g. "20160801" = 2016-08-01
//
namespace boost
{
  namespace gregorian
  {
    inline tuple<int, int, int> to_tuple(const date& d)
    {
      return make_tuple(d.year(), d.month(), d.day());
    }

    inline int to_int(const date& d)
    {
      return d.year() * 10000 + d.month() * 100 + d.day();
    }

    inline date from_int(int ymd)
    {
      return date{(short unsigned)(ymd / 10000),
                  (short unsigned)((ymd % 10000) / 100),
                  (short unsigned)(ymd % 100)};
    }
  }
}

namespace Sloppy
{
  namespace DateTime {
    static constexpr int MIN_YEAR = 1900;

    inline tuple<int, int, int> YearMonthDayFromInt(int ymd)
    {
      return make_tuple(ymd / 10000,  (ymd % 10000) / 100, ymd % 100);
    }

    bool parseDateString(const string& in, boost::gregorian::date& out, const string& fmtString="", bool strictChecking=true);

    // a wrapper class for boost's ptime
    class CommonTimestamp
    {
    public:
      CommonTimestamp(int year, int month, int day, int hour, int min, int sec);
      CommonTimestamp(boost::posix_time::ptime rawTime);
      virtual time_t getRawTime() const;

      string getISODate() const;
      string getTime() const;
      string getTimestamp() const;
      int getDoW() const;
      int getYMD() const;
      string getFormattedString(const string& fmt) const;
      bool setTime(int hour, int min, int sec);
      tuple<int, int, int> getYearMonthDay() const;

      static bool isValidDate(int year, int month, int day);
      static bool isValidTime(int hour, int min, int sec);
      static bool isLeapYear(int year);

      inline boost::posix_time::ptime getRawPtime() const
      {
        return raw;
      }

      inline bool operator< (const CommonTimestamp& other) const
      {
        return (raw < other.raw);   // maybe I should use difftime() here...
      }
      inline bool operator> (const CommonTimestamp& other) const
      {
        return (other < (*this));
      }
      inline bool operator<= (const CommonTimestamp& other) const
      {
        return (!(*this > other));
      }
      inline bool operator>= (const CommonTimestamp& other) const
      {
        return (!(*this < other));
      }
      inline bool operator== (const CommonTimestamp& other) const
      {
        return (raw == other.raw);   // maybe I should use difftime() here...
      }
      inline bool operator!= (const CommonTimestamp& other) const
      {
        return (raw != other.raw);   // maybe I should use difftime() here...
      }
      inline void applyOffset(long secs)
      {
        raw += boost::posix_time::seconds(secs);
      }

    protected:
      boost::posix_time::ptime raw;
    };

    // an extension of struct tm to clearly indicate that local time
    // is stored
    class UTCTimestamp;
    class LocalTimestamp : public CommonTimestamp
    {
    public:
      LocalTimestamp(int year, int month, int day, int hour, int min, int sec, boost::local_time::time_zone_ptr tzp);
      LocalTimestamp(time_t rawTimeInUTC, boost::local_time::time_zone_ptr tzp);
      LocalTimestamp(boost::local_time::time_zone_ptr tzp);
      UTCTimestamp toUTC() const;

      static unique_ptr<LocalTimestamp> fromISODate(const string& isoDate, boost::local_time::time_zone_ptr tzp, int hour=12, int min=0, int sec=0);

      time_t getRawTime() const override;

    protected:
      boost::posix_time::ptime utc;
    };

    // an extension of struct tm to clearly indicate that UTC
    // is stored
    class UTCTimestamp : public CommonTimestamp
    {
    public:
      UTCTimestamp(int year, int month, int day, int hour, int min, int sec);
      UTCTimestamp(int ymd, int hour=12, int min=0, int sec=0);
      UTCTimestamp(time_t rawTimeInUTC);
      UTCTimestamp(boost::posix_time::ptime utcTime);
      UTCTimestamp();
      LocalTimestamp toLocalTime(boost::local_time::time_zone_ptr tzp) const;
    };

    typedef unique_ptr<LocalTimestamp> upLocalTimestamp;
    typedef unique_ptr<UTCTimestamp> upUTCTimestamp;

    // a generic period class
    template <class T>
    class GenericPeriod
    {
    public:
      enum class Relation
      {
        isBefore,
        isIn,
        isAfter,
        _undefined
      };

      GenericPeriod(const T& _start, const T& _end)
        :start{_start}, end{make_unique<T>(_end)}
      {
        if (*end < start)
        {
          throw invalid_argument("GenericPeriod ctor: 'end'' may not be before start!");
        }
      }

      GenericPeriod(const T& _start)
        :start(_start), end(nullptr)
      {
      }

      inline bool hasOpenEnd() const
      {
        return (end == nullptr);
      }

      inline bool isInPeriod(const T& sample)
      {
        if (end != nullptr)
        {
          return ((sample >= start) && (sample <= *end));
        }
        return (sample >= start);
      }

      inline Relation determineRelationToPeriod(const T& sample) const
      {
        if (sample < start) return Relation::isBefore;

        if (end != nullptr)
        {
          return (sample > *end) ? Relation::isAfter : Relation::isIn;
        }

        return Relation::isIn;
      }

      inline bool setStart(const T& newStart)
      {
        if (newStart > *end) return false;
        start = newStart;
        return true;
      }

      inline bool setEnd(const T& newEnd)
      {
        if (newEnd < start) return false;
        end = make_unique<T>(newEnd);
        return true;
      }

      inline T getStart() const
      {
        return start;
      }

      inline unique_ptr<T> getEnd() const
      {
        if (end != nullptr)
        {
          return make_unique<T>(*end);
        }
        return nullptr;
      }

      inline bool startsEarlierThan (const GenericPeriod<T>& other) const
      {
        return (start < other.start);
      }

      inline bool startsLaterThan (const GenericPeriod<T>& other) const
      {
        return (start > other.start);
      }

    protected:
      T start;
      unique_ptr<T> end;
    };

    // a time period
    class TimePeriod : public GenericPeriod<UTCTimestamp>
    {
    public:
      TimePeriod(const UTCTimestamp& _start)
        :GenericPeriod<UTCTimestamp>(_start){}

      TimePeriod(const UTCTimestamp& _start, const UTCTimestamp& _end)
        :GenericPeriod<UTCTimestamp>(_start, _end){}

      inline boost::posix_time::time_period toBoost() const
      {
        boost::posix_time::ptime pStart = start.getRawPtime();

        if (end != nullptr)
        {
          boost::posix_time::ptime pEnd = end->getRawPtime();
          pEnd += boost::posix_time::time_duration(0, 0, 0, 1);
          return boost::posix_time::time_period(pStart, pEnd);
        }

        // return an invalid duration
        return boost::posix_time::time_period(pStart, pStart);
      }

      long getLength_Sec() const;
      double getLength_Minutes() const;
      double getLength_Hours() const;
      double getLength_Days() const;
      double getLength_Weeks() const;

      bool applyOffsetToStart(long secs);
      bool applyOffsetToEnd(long secs);
    };

    // a date period
    class DatePeriod : public GenericPeriod<boost::gregorian::date>
    {
    public:
      DatePeriod(const boost::gregorian::date& _start, const boost::gregorian::date& _end)
        :GenericPeriod<boost::gregorian::date>(_start, _end) {}

      DatePeriod(const boost::gregorian::date& _start)
        :GenericPeriod<boost::gregorian::date>(_start) {}

      inline boost::gregorian::date_period toBoost() const
      {
        if (end != nullptr)
        {
          boost::gregorian::date oneDayAfterEnd = (*end) + boost::gregorian::date_duration(1);
          return boost::gregorian::date_period(start, oneDayAfterEnd);
        }

        // return an invalid duration
        return boost::gregorian::date_period(start, start);
      }

      inline long getLength_Days() const
      {
        if (end == nullptr) return -1;

        auto dp = toBoost();
        return dp.length().days();
      }

      inline double getLength__Weeks() const
      {
        return (end == nullptr) ? -1 : getLength_Days() / 7.0;
      }
    };
  }
}

#endif /* DATEANDTIME_H */
