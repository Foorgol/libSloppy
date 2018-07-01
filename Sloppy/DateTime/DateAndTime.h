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
#include <optional>
#include <type_traits>

#include <boost/date_time/local_time/local_time.hpp>

using namespace std;


//
// Extend boost's gregorian date to deal with
// integer date representations, e.g. "20160801" = 2016-08-01
//
namespace boost
{
  namespace gregorian
  {
    class date;

    /** \brief Converts a gregorian date into a tuple of ints for year, month, days
     *
     * \returns a 3-tuple of ints containing <year, month, day>
     */
    inline tuple<int, int, int> to_tuple(const date& d     ///< the boost::gregorian::date to convert
                                         )
    {
      return make_tuple(d.year(), d.month(), d.day());
    }

    /** \brief Converts a gregorian date into a single integer
     *
     * The integer representation is quite simple, e.g. "20160801" = 2016-08-01
     *
     * The earliest date supported by Boost is 1400-01-01, thus the
     *
     * \returns a single integer representing a date
     */
    inline int to_int(const date& d     ///< the boost::gregorian::date to convert
                      )
    {
      return d.year() * 10000 + d.month() * 100 + d.day();
    }

    /** \brief Converts a single integer into a gregorian date
     *
     * The integer representation is quite simple, e.g. "20160801" = 2016-08-01
     *
     * The dates supported by Boost are 1400-01-01 -- 9999-12-31, thus the integer
     * parameter has to be 14000101 <= ymd <= 99991231
     *
     * \throws std::out_of_range if the integer value is not within the valid range
     * \throws all possible exceptions from the boost::gregorian::date ctor
     *
     * \returns a boost::gregorian::date
     */
    date from_int(int ymd    ///< the integer-date to convert
                  );
  }

}

namespace Sloppy
{
  namespace DateTime {

    /** \brief Converts a single integer into a 3-tuple of <year, month, day>
     *
     * The integer representation is quite simple, e.g. "20160801" = 2016-08-01
     *
     * Assuming a minimum date of 1-1-1, the minimum value of ymd is 10000101.
     *
     * \note Besides the check for a minimun value, this function does not apply any
     * other consistency checks of the resulting date! Garbage in, garbage out...
     *
     * \throws std::out_of_range if the parameter value is less than 100000101
     */
    tuple<unsigned short, unsigned short, unsigned short> YearMonthDayFromInt(
        int ymd  ///< the integer-date to convert
        );

    /** \brief Converts a string with custom formatting into a boost::gregorian::date
     *
     *  The conversion uses a `boost::gregorian::date_input_facet` for the conversion. The
     *  format of the conversion string can be found
     *  [here](http://www.boost.org/doc/libs/1_66_0/doc/html/date_time/date_time_io.html#format_strings).
     *
     *  If strict checking is requested, the resulting date object will be converted back into a string
     *  and this string will be compared to the input string. The conversion is deemed successful if
     *  both strings are equal.
     *
     *  \returns the parsed date or the special value "not_a_date" is the conversion failed
     *
     */
    boost::gregorian::date parseDateString(
        const string& in,            ///< the string to parse
        const string& fmtString="",  ///< the format of the string; if empty, "yyyy-mm-dd" will be used
        bool strictChecking=true     ///< enable or disable strict checking
        );

    /** \brief A wrapper class for boost's ptime
     *
     * The documentation for `ptime` is [here](http://www.boost.org/doc/libs/1_66_0/doc/html/date_time/posix_time.html).
     *
     * \note The timestamp stored herein is agnostic of a specific time zone
     */
    class CommonTimestamp
    {
    public:
      /** \brief Ctor from a 6-tuple of year, month, day, hour, minute and second
       *
       * \throws std::out_of_range if not 1400 <= year <= 9999, because that's a Boost limitation
       * \throws std::invalid_argument if the values if year, month, day do not form a valid date
       * \throws std::invalid_argument if the values if hour, minute, second do not form a valid time in 24h format
       */
      CommonTimestamp(
          int year,   ///< the year of the timestamp; shall be >= 1400 and <= 9999
          int month,  ///< the month of the timestamp
          int day,    ///< the day of the timestamp
          int hour,   ///< hours
          int min,    ///< minutes
          int sec     ///< seconds
          );

      /** \brief Ctor that converts an existing `ptime` object into a CommonTimestamp
       *
       * The documentation for `ptime` is [here](http://www.boost.org/doc/libs/1_66_0/doc/html/date_time/posix_time.html).
       */
      CommonTimestamp(boost::posix_time::ptime rawTime);

      /** \brief Dtor, currently empty.
       */
      virtual ~CommonTimestamp(){}

      /** \brief Converts the timestamp into a `time_t` value (seconds since the UNIX epoch) in UTC
       *
       * This is a `virtual` function because it needs to be overridden by derived classes for
       * timestamps in local time.
       *
       * \returns A `time_t` value of the timestamp with seconds since the epoch, in UTC
       */
      virtual time_t getRawTime() const;

      /** \brief Gets the date-part of the timestamp as string in "yyyy-mm-dd" format
       *
       * \returns a string in "yyyy-mm-dd" format, representing the date-part of the timestamp
       */
      string getISODate() const;

      /** \brief Gets the time-part of the timestamp as string in "hh:mm:ss" format
       *
       * \returns a string in "hh:mm:ss" format, representing the time-part of the timestamp
       */
      string getTime() const;

      /** \brief Gets a string representation of the timestamp in "yyyy-mm-dd hh:mm:ss" format
       *
       * \returns a string in "yyyy-mm-dd hh:mm:ss" format representing timestamp
       */
      string getTimestamp() const;

      /** \brief Gets the day-of-week for the timestamp as an int
       *
       * \returns an integer representing the day-of-week (0 = Sunday, 1 = Monday, ...)
       */
      int getDoW() const;

      /** \brief Gets the timestamp's date as a single integer
       *
       * \returns an integer representing the timestamp's date (e.g., 2018-02-24 = 20180224)
       */
      int getYMD() const;

      /** \brief Converts the full timestamp in to custom formatted string
       *
       * Uses `strftime' for generating the formatted string. The result may not be longer
       * than 100 bytes including terminating zero.
       *
       * The specification of the format string can be found [here](http://www.cplusplus.com/reference/ctime/strftime/)
       *
       * \returns a string representation of the timestamp in a custom format
       */
      string getFormattedString(const string& fmt) const;

      /** \brief Sets the time information of the timestamp to a new value.
       *
       * \returns `false` if the user submitted invalid parameters and `true` on success
       */
      bool setTime(
          int hour,   ///< the hours value of the new timestamp
          int min,    ///< the minutes value of the new timestamp
          int sec     ///< the seconds value of the new timestamp
          );

      /** \brief Retrieves a 3-tuple of year, month and day of the stored timestamp
       *
       * \returns a 3-tuple of <year, month, day> of the stored timestamp
       */
      tuple<unsigned short, unsigned short, unsigned short> getYearMonthDay() const;

      /** \brief Checks whether a tuple of year, month and day forms a valid date
       *
       * This method constructs a temporary `boost::gregorian::date` object from the
       * provided parameters. If this works without exception and yields an object
       * that contains a regular date (no special value), the provided tuple is
       * deemed valid.
       *
       * This approach takes leap years etc. into account.
       *
       * \returns `true` if the provided values for year, month and day represent a valid date; `false` otherwise
       */
      static bool isValidDate(
          int year,   ///< the year value of the date to check
          int month,  ///< the month value of the date to check
          int day     ///< the day value of date to check
          );

      /** \brief Checks wheter a tuple of hours, minutes and seconds forms a valid time in the 24h-format
       *
       * This is basically a simple range check of the provided parameters.
       *
       * \returns `true` if the provided values for hours, minutes and seconds represent a valid time in 24h format; `false` otherwise
       */
      static bool isValidTime(
          int hour,   ///< the hours value to check
          int min,    ///< the minutes value to check
          int sec     ///< the seconds value to check
          );

      /** \brief Checks whether a given year is a leap year or not
       *
       * This is a wrapper for `boost::gregorian::gregorian_calendar::is_leap_year()`
       *
       * \returns `true` if the given year is a leap year; `false` otherwise
       */
      static bool isLeapYear(int year);

      /** \brief Retrieves a copy of the internal `ptime` object
       *
       * \returns the current timestamp as Boost's `ptime`
       */
      boost::posix_time::ptime getRawPtime() const
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

      /** \brief Shifts the timestamp by a given number of seconds.
       *
       * Positive paramters shift the timestamp's value forward (==> later / future), negative
       * values shift the timestamp's value backwards (==> earlier / past)
       */
      void applyOffset(long secs)
      {
        raw += boost::posix_time::seconds(secs);
      }

    protected:
      boost::posix_time::ptime raw;   ///< the internal representation of the timestamp as Boost's `ptime`
    };

    // forward; will be refined later in this file.
    class UTCTimestamp;

    /** \brief An extension of `CommonTimestamp` with a timestamp stored in local time.
     */
    class LocalTimestamp : public CommonTimestamp
    {
    public:
      /** \brief Ctor from a 6-tuple of year, month, day, hour, minute and second
       *
       * \throws std::out_of_range if not 1400 <= year <= 9999, because that's a Boost limitation
       * \throws std::invalid_argument if the values if year, month, day do not form a valid date
       * \throws std::invalid_argument if the values if hour, minute, second do not form a valid time in 24h format
       * \throws std::invalid_argument if no time zone pointer has been provided (=> `nullptr`)
       * \throws std::invalid_argument if the combination of date, time and timezone does not yield a valid, unambiguous timestamp
       * \throws (more) all the exceptions that Boost's ctors for `gregorian::date` and `posix_time::time_duration' can throw
       */
      LocalTimestamp(
          int year,   ///< the year of the timestamp; shall be >= 1400 and <= 9999
          int month,  ///< the month of the timestamp
          int day,    ///< the day of the timestamp
          int hour,   ///< hours
          int min,    ///< minutes
          int sec,     ///< seconds
          boost::local_time::time_zone_ptr tzp   ///< a pointer to a time_zone instance representing the local time zone used for the timestamp
          );

      /** \brief Ctor from a UTC timestamp in epoch-seconds and a time zone
       *
       * \throws std::invalid_argument if no time zone pointer has been provided (=> `nullptr`)
       * \throws (more) all the exceptions that Boost's ctors for `gregorian::date` and `posix_time::time_duration' can throw
       *
       */
      LocalTimestamp(
          time_t rawTimeInUTC,    ///< the timestamp in UTC as seconds since the epoch
          boost::local_time::time_zone_ptr tzp   ///< a pointer to a time_zone object that defines the local time zone
          );

      /** Ctor for the current date/time ("now"), stored as values for a given local time zone
       *
       * \throws std::invalid_argument if no time zone pointer has been provided (=> `nullptr`)
       */
      LocalTimestamp(
          boost::local_time::time_zone_ptr tzp   ///< a pointer to a time_zone object that defines the local time zone
          );

      /** \brief Retrieves an UTC representation of the local timestamp
       *
       * \returns an `UTCTimestamp` that contains the same time point as the local timestamp
       */
      UTCTimestamp toUTC() const;

      /** \brief Factory function for creating a `LocalTimestamp' from an ISO date string, a time zone and a time
       *
       * \returns `nullptr` if one or more paramters was invalid
       * \returns a new LocalTimestamp object wrapped in a `unique_otr` if all paramters were valid
       */
      static unique_ptr<LocalTimestamp> fromISODate(const string& isoDate, boost::local_time::time_zone_ptr tzp, int hour=12, int min=0, int sec=0);

      /** \brief Retrieves an UTC representation (seconds since epoch) of the local timestamp
       *
       * \returns the number of seconds since the epoch that represent the same time point as the local timestamp
       */
      time_t getRawTime() const override;

    protected:
      boost::posix_time::ptime utc;   ///< the UTC representation of the local timestamp
    };

    /** \brief An extension of `CommonTimestamp` with a timestamp stored in UTC.
     */
    class UTCTimestamp : public CommonTimestamp
    {
    public:
      /** \brief Ctor from a 6-tuple of year, month, day, hour, minute and second
       *
       * \throws std::out_of_range if not 1400 <= year <= 9999, because that's a Boost limitation
       * \throws std::invalid_argument if the values if year, month, day do not form a valid date
       * \throws std::invalid_argument if the values if hour, minute, second do not form a valid time in 24h format
       */
      UTCTimestamp(
          int year,   ///< the year of the timestamp; shall be >= 1400 and <= 9999
          int month,  ///< the month of the timestamp
          int day,    ///< the day of the timestamp
          int hour,   ///< hours
          int min,    ///< minutes
          int sec     ///< seconds
          );

      /** \brief Ctor from a numeric date (e.g., 20180224 = "2018-02-24") and integer values for hour, minute, second
       *
       * Construction is delegated to the (int, int, int, int, int, int)-ctor and thus they both throw the same exceptions
       */
      UTCTimestamp(
          int ymd,      ///< the integer-date for the timestamp
          int hour=12,  ///< hours
          int min=0,    ///< minutes
          int sec=0     ///< seconds
          );

      /** \brief Ctor from a UTC timestamp in epoch-seconds
       */
      UTCTimestamp(time_t rawTimeInUTC);

      /** \brief Ctor from a UTC timestamp stored in a Boost `ptime` object
       */
      UTCTimestamp(boost::posix_time::ptime utcTime);

      /** \brief Ctor for "now"
       */
      UTCTimestamp();

      /** \brief Retrieves a `LocalTimestamp` representation of the UTC timestamp
       *
       * \throws std::invalid_argument if the pointer to the time_zone object is null
       */
      LocalTimestamp toLocalTime (
          boost::local_time::time_zone_ptr tzp   ///< a pointer to a `time_zone` instance representing the local time zone used for the conversion
          ) const;
    };

    typedef unique_ptr<LocalTimestamp> upLocalTimestamp;
    typedef unique_ptr<UTCTimestamp> upUTCTimestamp;

    /** \brief A generic class for periods / ranges of any type.
     *
     * The class can be used with types that:
     *   - can be copy-constructed; and
     *   - support all relation operators (>, <, >=, <=, ==, !=)
     *
     * A special feature is that the period can be "open ended". That
     * means that the range has a start value but no end value.
     */
    template <class T>
    class GenericPeriod
    {
    public:
      /** \brief An enumeration of relations between a period and a single time point / sample
       *
       * For open ended ranges, a sample can never be "after" and is always "in" if the sample
       * is on or beyond the start.
       */
      enum class Relation
      {
        isBefore,    ///< the sample is before the period's start (sample < start)
        isIn,        ///< start <= sample <= end
        isAfter,     ///< the sample is after the period's end (sample > end)
        _undefined   ///< undefind
      };

      /** \brief Ctor for a period with defined start and end.
       *
       * The values for start and end are stored as a copy.
       *
       * \throws std::invalid_argument if the end is before the start
       */
      GenericPeriod(
          const T& _start,  ///< the start value of the period
          const T& _end     ///< the end value of the period
          )
        :start{_start}, end{_end}
      {
        static_assert(is_copy_constructible<T>(), "GenericPeriod works only with copy-constructible types!");
        if (*end < start)
        {
          throw invalid_argument("GenericPeriod ctor: 'end'' may not be before start!");
        }
      }

      /** \brief Ctor for a period with defined start and open end.
       *
       * The start value is stored as a copy.
       */
      GenericPeriod(
          const T& _start  ///< the start value of the period
          )
        :start(_start), end{}
      {
      }

      /** \returns `true` if the period is open-ended
       */
      inline bool hasOpenEnd() const
      {
        return (!end.has_value());
      }

      /** \returns `true` if a sample value is within the period
       */
      bool isInPeriod(
          const T& sample   ///< the sample value to check
          )
      {
        if (end.has_value())
        {
          return ((sample >= start) && (sample <= *end));
        }
        return (sample >= start);
      }

      /** \brief Determines the relation of a sample value with the period
       *
       * \returns an enum-value of type `Relation` that indicates how the sample relates to the period
       */
      Relation determineRelationToPeriod(
          const T& sample   ///< the sample value to check
          ) const
      {
        if (sample < start) return Relation::isBefore;

        if (end.has_value())
        {
          return (sample > *end) ? Relation::isAfter : Relation::isIn;
        }

        return Relation::isIn;
      }

      /** \brief Sets a new start value for the period
       *
       * The new start must be before the end. If this condition is not met,
       * the current start value is not modified.
       *
       * \returns `false` if the new start is after the current end; `true` otherwise.
       */
      inline bool setStart(
          const T& newStart    ///< the new start value
          )
      {
        if (end.has_value() && (newStart > *end)) return false;
        start = newStart;
        return true;
      }


      /** \brief Sets a new end value for the period
       *
       * The new end must be on or after the start. If this condition is not met,
       * the current end value is not modified.
       *
       * \returns `false` if the new end is before the current start; `true` otherwise.
       */
      inline bool setEnd(
          const T& newEnd    ///< the new end value
          )
      {
        if (newEnd < start) return false;
        end = newEnd;
        return true;
      }

      /** \returns a copy of the current start value
       */
      inline T getStart() const
      {
        return start;
      }

      /** \returns a copy of the current end value (which is an std::optional<T>)
       */
      inline optional<T> getEnd() const
      {
        return end;
      }

      /** \returns `true` if this period starts earlier than another period
       */
      inline bool startsEarlierThan (
          const GenericPeriod<T>& other    ///< the period used for the comparison
          ) const
      {
        return (start < other.start);
      }

      /** \returns `true` if this period starts later than another period
       */
      inline bool startsLaterThan (
          const GenericPeriod<T>& other    ///< the period used for the comparison
          ) const
      {
        return (start > other.start);
      }

    protected:
      T start;   ///< the period's start value
      optional<T> end;   ///< the period's end value, potentially empty
    };

    /** \brief A class for a time period that is defined by two `UTCTimestamp` values
     */
    class TimePeriod : public GenericPeriod<UTCTimestamp>
    {
    public:
      /** \brief Ctor for an open ended TimePeriod
       */
      TimePeriod(
          const UTCTimestamp& _start   ///< the start timestamp for the TimePeriod
          )
        :GenericPeriod<UTCTimestamp>(_start){}

      /** \brief Ctor for a closed TimePeriod with defined start and end
       *
       * \throws std::invalid_argument if the end is before the start
       */
      TimePeriod(
          const UTCTimestamp& _start,   ///< the start timestamp for the TimePeriod
          const UTCTimestamp& _end      ///< the end timestamp for the TimePeriod
          )
        :GenericPeriod<UTCTimestamp>(_start, _end){}

      /** \brief Retrieves a copy of the period as a `boost::posix_time::time_period`
       *
       * Since Boost does not support time periods with open ends, a `time_period` with
       * invalid duration is returned in case of open ends.
       *
       * \returns a Boost `time_period` that represents this TimePeriod (with "invalid duration" in case of open ends)
       */
      inline boost::posix_time::time_period toBoost() const
      {
        boost::posix_time::ptime pStart = start.getRawPtime();

        if (end.has_value())
        {
          boost::posix_time::ptime pEnd = end->getRawPtime();
          pEnd += boost::posix_time::time_duration(0, 0, 0, 1);
          return boost::posix_time::time_period(pStart, pEnd);
        }

        // return an invalid duration
        return boost::posix_time::time_period(pStart, pStart);
      }

      /** \returns the length of the time period in seconds or `-1` in case of open periods
       */
      long getLength_Sec() const;

      /** \returns the length of the time period in minutes (incl. digits) or `-1` in case of open periods
       */
      double getLength_Minutes() const;

      /** \returns the length of the time period in hours (incl. digits) or `-1` in case of open periods
       */
      double getLength_Hours() const;

      /** \returns the length of the time period in days (incl. digits) or `-1` in case of open periods
       */
      double getLength_Days() const;

      /** \returns the length of the time period in weeks (incl. digits) or `-1` in case of open periods
       */
      double getLength_Weeks() const;

      /** \brief Applies an offset to the period's start
       *
       * Positive arguments shift the start forward (later / future), negative
       * arguments shift the start backwards (earlier / past).
       *
       * If the new start would be after the current end, the start remains untouched.
       *
       * \returns `true` if the start has been updated or `false` if start+offset would be after the end
       */
      bool applyOffsetToStart(long secs);

      /** \brief Applies an offset to the period's end
       *
       * Positive arguments shift the end forward (later / future), negative
       * arguments shift the end backwards (earlier / past).
       *
       * If the new end would be after the current start, the end remains untouched.
       *
       * \returns `true` if the end has been updated or `false` if end+offset would be after the end or
       * the period has an open end
       */
      bool applyOffsetToEnd(long secs);
    };

    /** \brief A class for a date period that is defined by two `boost::gregorian::date` values
     *
     * \note Start day and end day are fully included in the period. Hence, a
     * `DatePeriod` that starts and ends on the same day has a length of one (1) day.
     */
    class DatePeriod : public GenericPeriod<boost::gregorian::date>
    {
    public:
      /** \brief Ctor for a closed date period
       *
       * \note Start day and end day are fully included in the period. Hence, a
       * `DatePeriod` that starts and ends on the same day has a length of one (1) day.
       *
       * \throws std::invalid_argument if the end is before the start
       */
      DatePeriod(
          const boost::gregorian::date& _start,   ///< the first day of the DatePeriod
          const boost::gregorian::date& _end      ///< the last day of the DatePeriod
          )
        :GenericPeriod<boost::gregorian::date>(_start, _end) {}

      /** \brief Ctor for an open ended date period
       */
      DatePeriod(
          const boost::gregorian::date& _start   ///< the first day of the DatePeriod
          )
        :GenericPeriod<boost::gregorian::date>(_start) {}

      /** \brief Retrieves a copy of the period as a `boost::gregorian::date_period`
       *
       * Since Boost does not support date periods with open ends, a `date_period` with
       * invalid duration is returned in case of open ends.
       *
       * \returns a Boost `date_period` that represents this DatePeriod (with "invalid duration" in case of open ends)
       */
      boost::gregorian::date_period toBoost() const
      {
        if (end.has_value())
        {
          boost::gregorian::date oneDayAfterEnd = (*end) + boost::gregorian::date_duration(1);
          return boost::gregorian::date_period(start, oneDayAfterEnd);
        }

        // return an invalid duration
        return boost::gregorian::date_period(start, start);
      }

      /** \returns the length of the date period in days or `-1` in case of open periods
       */
      long getLength_Days() const
      {
        if (!(end.has_value())) return -1;

        auto dp = toBoost();
        return dp.length().days();
      }

      /** \returns the length of the date period in weeks (incl. digits) or `-1` in case of open periods
       */
      inline double getLength__Weeks() const
      {
        return (!(end.has_value())) ? -1 : getLength_Days() / 7.0;
      }
    };
  }
}

#endif /* DATEANDTIME_H */
