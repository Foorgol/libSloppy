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

#include <sys/types.h>        // for time_t
#include <chrono>             // for seconds, hours, minutes, operator+, dur...
#include <optional>           // for optional, nullopt
#include <ratio>              // for ratio
#include <string>             // for string, basic_string, allocator
#include <tuple>              // for tuple

#include "../GenericRange.h"  // for GenericRange
#include "date.h"             // for year_month_day, days, sys_days, format
#include "tz.h"               // for locate_zone, make_zoned, zoned_time


namespace Sloppy
{
  namespace DateTime {
    
    using MinuteDurationDouble = std::chrono::duration<double, std::ratio<60>>;
    using HourDurationDouble = std::chrono::duration<double, std::ratio<3600>>;
    using DayDurationInt = std::chrono::duration<int, std::ratio<86400>>;
    using DayDurationDouble = std::chrono::duration<double, std::ratio<86400>>;
    using WeekDurationInt = std::chrono::duration<int, std::ratio<604800>>;
    using WeekDurationDouble = std::chrono::duration<double, std::ratio<604800>>;
    
    /** \brief Converts a single integer into a 3-tuple of <year, month, day>,
     * represented as date::year_month_day
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
    date::year_month_day ymdFromInt(
        int ymd  ///< the integer-date to convert
        );

    /** \brief Converts a string with custom formatting into a data::year_month_day
     *
     *  The format of the conversion string can be found
     *  [here](https://howardhinnant.github.io/date/date.html#from_stream_formatting).
     *
     *  If strict checking is requested, the resulting date object will be converted back into a string
     *  and this string will be compared to the input string. The conversion is deemed successful if
     *  both strings are equal.
     * 
     * \note It is guaranteed that for the resulting date a call to .ok() will yield `true`.
     * We never return `year_month_day` objects for that `.ok()` would be false.
     *
     *  \returns the parsed date, empty if the strict check failed
     *
     */
    std::optional<date::year_month_day> parseDateString(
        const std::string& in,            ///< the string to parse
        const std::string& fmtString="",  ///< the format of the string; if empty, "yyyy-mm-dd" will be used
        bool strictChecking=true     ///< enable or disable strict checking
        );

    /** \brief Checks whether a tuple of year, month and day forms a valid date
     * 
     * This method constructs a temporary `date::year_month_day` object from the
     * provided parameters. If this works without exception and yields an object
     * that contains a regular date (`.ok() = true`), the provided tuple is
     * deemed valid.
     *
     * This approach takes leap years etc. into account.
     *
     * \returns `true` if the provided values for year, month and day represent a valid date; `false` otherwise
     */
    bool isValidDate(
      int year,   ///< the year value of the date to check
      int month,  ///< the month value of the date to check
      int day     ///< the day value of date to check
    );
    
    /** \brief Checks whether a given year is a leap year or not
     * 
     * This is a wrapper for `date::year::is_leap()`
     *
     * \returns `true` if the given year is a leap year; `false` otherwise
     */
    bool isLeapYear(int year);
    
    template<typename Duration = typename std::chrono::system_clock::duration>
    class WallClockTimepoint
    {
    public:
      using clock = std::chrono::system_clock;
      using duration = Duration;
      using TpType = std::chrono::time_point<std::chrono::system_clock, Duration>;
      
      /** \brief Ctor for "now". If a timezone name is provided, all
       * output operations (e.g., conversion to strings) are in local time
       */
      explicit WallClockTimepoint(
        const std::optional<std::string> tzName = std::nullopt   ///< optional name of the timezone that should be used for output and conversion
      )
      : tp{std::chrono::time_point_cast<duration>(clock::now())}
      , tzPtr{tzName ? date::locate_zone(*tzName) : nullptr} {}
      
      /** \brief Ctor for "now". If a timezone pointer is provided, all
       * output operations (e.g., conversion to strings) are in local time
       */
      explicit WallClockTimepoint(
        const date::time_zone* ptr   ///< pointer to a previously looked up timezone definition 
      )
      : tp{std::chrono::time_point_cast<duration>(clock::now())}
      , tzPtr{ptr} {}
      
      // ctor from an existing time_t
      explicit WallClockTimepoint(const time_t& utc)
      :tp{std::chrono::time_point_cast<duration>(clock::from_time_t(utc))}, tzPtr{nullptr} {}
      
      explicit WallClockTimepoint(
        const time_t& utc,   ///< existing timepoint in UTC
        const date::time_zone* zonePtr  ///< timezone for output operations
      )
      :tp{std::chrono::time_point_cast<duration>(clock::from_time_t(utc))}, tzPtr{zonePtr} {}
      
      explicit WallClockTimepoint(
        const time_t& utc,   ///< existing timepoint in UTC
        const std::string& tzName   ///< timezone for output operations
      )
      :tp{std::chrono::time_point_cast<duration>(clock::from_time_t(utc))}, tzPtr{date::locate_zone(tzName)} {}
      
      /** \brief Ctor for an existing UTC timepoint along with an optional
       * timezone for subsequent output operations
       */
      explicit WallClockTimepoint(
        const std::chrono::system_clock::time_point& utcTp,   ///< the represented timepoint in UTC
        const date::time_zone* zonePtr = nullptr   ///< optional timezone for output operations
      )
      : tp{std::chrono::time_point_cast<duration>(utcTp)}
      , tzPtr{zonePtr} {}
      
      /** \brief Ctor for a given point in time; all date and time values are
       * interpreted in UTC
       */
      explicit WallClockTimepoint(
        const date::year_month_day& ymd,   ///< the date part of the time point
        const std::chrono::hours& h = std::chrono::hours{0},   ///< hours past midnight of that date
        const std::chrono::minutes& m = std::chrono::minutes{0},   ///< minutes past midnight of that date
        const std::chrono::seconds& s = std::chrono::seconds{0}   ///< seconds since midnight of that date
      )
      :WallClockTimepoint(ymd, h, m, s, nullptr) {}
      
      /** \brief Ctor for a given point in time; all date and time values are
       * interpreted as local values
       * 
       * \note Hours, minutes and seconds are arbitrary durations. It is completely fine to
       * use larger values than 24 / 60 / 60 for these parameters. Or even negative values. In such
       * cases the constructed time point is not on the same day as given by `ymd` since we simply
       * apply the durations as offsets to midnight on the `ymd` day.
       * 
       * \throws std::runtime_error if the provided time zone name could not be found in the database
       */
      explicit WallClockTimepoint(
        const date::year_month_day& ymd,   ///< the date part of the time point
        const std::chrono::hours& h,  ///< hours past midnight of that date
        const std::chrono::minutes& m,    ///< minutes past midnight of that date
        const std::chrono::seconds& s,   ///< seconds since midnight of that date
        const std::string& tzName   ///< name of the time zone for the provided date and time (e.g. "Europe/Berlin"); if empty, everything will be interpreted as UTC (exactly: as unix time which is UTC without leap seconds)
      )
      :WallClockTimepoint(ymd, h, m, s, date::locate_zone(tzName)) {}
      
      
      /** \brief Ctor for a given point in time; all date and time values are
       * interpreted as local values
       * 
       * \note Hours, minutes and seconds are arbitrary durations. It is completely fine to
       * use larger values than 24 / 60 / 60 for these parameters. Or even negative values. In such
       * cases the constructed time point is not on the same day as given by `ymd` since we simply
       * apply the durations as offsets to midnight on the `ymd` day.
       * 
       */
      explicit WallClockTimepoint(
        const date::year_month_day& ymd,   ///< the date part of the time point
        const std::chrono::hours& h,   ///< hours past midnight of that date
        const std::chrono::minutes& m,   ///< minutes past midnight of that date
        const std::chrono::seconds& s,   ///< seconds since midnight of that date
        const date::time_zone* zonePtr   ///< pointer to the timezone definition of the local time
      )
      :tzPtr{zonePtr}
      {
        // translate the "local midnight" of the provided data
        // to UTC
        if (tzPtr != nullptr) {
          const auto localMidnight = date::make_zoned(tzPtr, date::local_days{ymd});
          tp = localMidnight.get_sys_time();
        } else {
          tp = date::sys_days{ymd};
        }
        
        tp += h + m + s;
      }
      
      WallClockTimepoint(const WallClockTimepoint& other) = default;
      WallClockTimepoint& operator=(const WallClockTimepoint& other) = default;
      WallClockTimepoint(WallClockTimepoint&& other) = default;
      WallClockTimepoint& operator=(WallClockTimepoint&& other) = default;
      
      /** \brief Gets the date-part of the timestamp as string in "yyyy-mm-dd" format
       * 
       * \returns a string in "yyyy-mm-dd" format, representing the date-part of the timestamp
       */
      std::string isoDateString() const {
        return formattedString("%F");
      }
      
      /** \brief Gets the time-part of the timestamp as string in "hh:mm:ss" format
       * 
       * \returns a string in "hh:mm:ss" format, representing the time-part of the timestamp
       */
      std::string timeString() const {
        return formattedString("%T");
      }
      
      /** \brief Gets a string representation of the timestamp in "yyyy-mm-dd hh:mm:ss" format
       * 
       * \returns a string in "yyyy-mm-dd hh:mm:ss" format representing timestamp
       */
      std::string timestampString() const {
        return formattedString("%F %T");
      }
      
      /** \brief Converts the time point in to custom formatted string
       * 
       * The specification of the format string can be found [here](https://howardhinnant.github.io/date/date.html#from_stream_formatting)
       *
       * \returns a string representation of the timestamp in a custom format
       */
      std::string formattedString(const std::string& fmt) const {
        if (tzPtr != nullptr) {
          const auto localTime = date::make_zoned(tzPtr, tp).get_local_time();
          return date::format(fmt, localTime);
        }
        
        return date::format(fmt, tp);
      }
      
      /** \returns true if this object outputs local date or time */
      bool usesLocalTime() const { return (tzPtr != nullptr); }
      
      /** \returns true if this object outputs Unix date or time, which is
       * almost identical to UTC except for leap seconds
       */
      bool usesUnixTime() const { return (tzPtr == nullptr); }
      
      /** \return the time that has passed since midnight; if we have been
       * constructed with a timezone name, the local midnight is used
       */
      duration sinceMidnight() const {
        if (tzPtr != nullptr) {
          const auto localTime = date::make_zoned(tzPtr, tp).get_local_time();
          const auto localMidnight = date::floor<date::days>(localTime);
          
          return (localTime - localMidnight);
        }
        
        const auto utcMidnight = date::floor<date::days>(tp);
        return (tp - utcMidnight);
      }
      
      /** \returns a tuple with hours, minutes and seconds since (local) midnight
       */
      std::tuple<std::chrono::hours, std::chrono::minutes, std::chrono::seconds> hms() const {
        const auto sm = sinceMidnight();
        const std::chrono::hours h = std::chrono::floor<std::chrono::hours>(sm);
        const std::chrono::minutes m = std::chrono::floor<std::chrono::minutes>(sm - h);
        const std::chrono::seconds s = std::chrono::floor<std::chrono::seconds>(sm - h - m);
        
        return std::tuple{h, m, s};
      }
      
      /** \brief Adds the provided duration to the time point.
       */
      void applyOffset(const duration& d) {
        tp += d;
      }
      
      /** \brief Rewinds the internal time point to (local) midnight and
       * applies hours, minutes and seconds as an offset.
       */
      void setTimeSinceMidnight(
        const std::chrono::hours& h = std::chrono::hours{0},   ///< new hours past midnight of the current date
        const std::chrono::minutes& m = std::chrono::minutes{0},   ///< new minutes past midnight of the current date
        const std::chrono::seconds& s = std::chrono::seconds{0}   ///< new seconds since midnight of the current date
      )
      {
        tp += -sinceMidnight() + h + m + s;
      }
      
      /** \returns a year, month, day tuple for the time point.
       * 
       * The returned date is local date if a timezone was specified during construction.
       */
      date::year_month_day ymd() const {
        if (tzPtr != nullptr) {
          const auto localTime = date::make_zoned(tzPtr, tp).get_local_time();
          return date::year_month_day{date::floor<date::days>(localTime)};
        }
        
        return date::year_month_day{date::floor<date::days>(tp)};
      }
        
      /** \brief Gets the timestamp's date as a single integer
       * 
       * The returned date is local date if a timezone was specified during construction.
       * 
       * \returns an integer representing the timestamp's date (e.g., 2018-02-24 = 20180224)
       */
      int ymdInt() const {
        const auto _ymd = ymd();
        
        // convert to plain  numbers using "operator unsigned()" type
        // conversions that are defined for day, month etc.
        const auto d = static_cast<unsigned>(_ymd.day());
        const auto m = static_cast<unsigned>(_ymd.month());
        const auto y = static_cast<int>(_ymd.year());
        
        return d + m * 100 + y * 10000;
      }
      
      /** \brief Gets the day-of-week for the timestamp as an int
       * 
       * \returns an integer representing the day-of-week (0 = Sunday, 1 = Monday, ...)
       */
      int dow() const {
        const auto ymwd = date::year_month_weekday{ymd()};
        return ymwd.weekday().c_encoding();
      }
      
      /** \returns the timepoint in UTC
       */
      std::chrono::time_point<clock, Duration> utc() const { return tp; }
      
      /** \returns the timepoint as `time_t`
       */
      time_t to_time_t() const { return clock::to_time_t(tp); }
      
      template<typename Dur2>
      bool operator>(const WallClockTimepoint<Dur2>& rhs) const {
        return (tp > rhs.tp);
      }
      
      template<typename Dur2>
      bool operator<(const WallClockTimepoint<Dur2>& rhs) const {
        return (tp < rhs.tp);
      }
      
      template<typename Dur2>
      bool operator>=(const WallClockTimepoint<Dur2>& rhs) const {
        return (tp >= rhs.tp);
      }
      
      template<typename Dur2>
      bool operator<=(const WallClockTimepoint<Dur2>& rhs) const {
        return (tp <= rhs.tp);
      }
      
      template<typename Dur2>
      bool operator==(const WallClockTimepoint<Dur2>& rhs) const {
        return (tp == rhs.tp);
      }
      
      template<typename Dur2>
      bool operator!=(const WallClockTimepoint<Dur2>& rhs) const {
        return (tp != rhs.tp);
      }
      
      template<typename Dur2>
      WallClockTimepoint<Duration>& operator+=(const Dur2& d) {
        tp += std::chrono::duration_cast<Duration>(d);
        return *this;
      }
      
      template<typename Dur2>
      WallClockTimepoint<Duration>& operator-=(const Dur2& d) {
        tp -= std::chrono::duration_cast<Duration>(d);
        return *this;
      }
      
    private:
      TpType tp;  // stores ALWAYS UTC time, never local time
      const date::time_zone* tzPtr{nullptr};
    };
    
    using WallClockTimepoint_secs = WallClockTimepoint<std::chrono::seconds>;
    using WallClockTimepoint_ms = WallClockTimepoint<std::chrono::milliseconds>;
    using WallClockTimepoint_us = WallClockTimepoint<std::chrono::microseconds>;
    
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
    
    
    /** \brief A class for a time period that is defined by two system_clock timepoints
     */
    template<class Duration>
    class TimeRange : public GenericRange<WallClockTimepoint<Duration>>
    {
    public:
      using TpType = typename WallClockTimepoint<Duration>::TpType;
      using ValueType = WallClockTimepoint<Duration>;
      
      /** \brief Ctor for an open ended TimePeriod
       */
      TimeRange(
        const ValueType& _start   ///< the start timestamp for the TimePeriod
      )
      :GenericRange<ValueType>(_start){}
      
      /** \brief Ctor for a closed TimePeriod with defined start and end
       *
       * \throws std::invalid_argument if the end is before the start
       */
      TimeRange(
        const ValueType& _start,   ///< the start timestamp for the TimePeriod
        const ValueType& _end      ///< the end timestamp for the TimePeriod
      )
      :GenericRange<ValueType>(_start, _end){}
      
      TimeRange(const TimeRange<Duration>& other) = default;
      TimeRange(TimeRange<Duration>&& other) = default;
      TimeRange<Duration>& operator=(const TimeRange<Duration>& other) = default;
      TimeRange<Duration>& operator=(TimeRange<Duration>&& other) = default;
      ~TimeRange() = default;
      
      /** \returns the length of the time period; empty in case of open periods
       */
      template<class Dur2>
      std::optional<Dur2> length() const {
        if (this->hasOpenEnd()) return std::nullopt;
        
        return std::chrono::duration_cast<Dur2>(this->end.value().utc() - this->start.utc());
      }
      
      /** \returns the length of the time period in seconds; empty in case of open periods
        */
      inline std::optional<std::chrono::seconds> length_secs() const
      {
        return length<std::chrono::seconds>();
      }
            
      /** \returns the length of the time period in minutes (incl. digits) or empty in case of open periods
        */
      inline std::optional<MinuteDurationDouble> length_minutes() const {
        return length<MinuteDurationDouble>();
      }
      
      /** \returns the length of the time period in hours (incl. digits) or empty in case of open periods
       */
      inline std::optional<HourDurationDouble> length_hours() const {
        return length<HourDurationDouble>();
      }
            
      /** \returns the length of the time period in days (incl. digits) or empty in case of open periods
       */
      inline std::optional<DayDurationDouble> length_days() const {
        return length<DayDurationDouble>();
      }
      
      /** \returns the length of the time period in weeks (incl. digits) or empty in case of open periods
       */
      inline std::optional<WeekDurationDouble> length_weeks() const {
        return length<WeekDurationDouble>();
      }
      
      /** \brief Applies an offset to the period's start
       * 
       * Positive arguments shift the start forward (later / future), negative
       * arguments shift the start backwards (earlier / past).
       *
       * If the new start would be after the current end, the start remains untouched.
       *
       * \returns `true` if the start has been updated or `false` if start+offset would be after the end
       */
      template<class Dur2>
      bool applyOffsetToStart(const Dur2& offset) {
        WallClockTimepoint<Duration> newStart{this->start};  // copy the old start
        newStart += std::chrono::duration_cast<Duration>(offset);
        
        if (this->end.has_value() && (newStart > this->end.value())) return false;
        
        this->start = newStart;
        return true;        
      }
      
      /** \brief Applies an offset to the period's end
       * 
       * Positive arguments shift the end forward (later / future), negative
       * arguments shift the end backwards (earlier / past).
       *
       * If the new end would be before the current start, the end remains untouched.
       *
       * \returns `true` if the end has been updated or `false` if end+offset would be after the end or
       * the period has an open end
       */
      template<class Dur2>
      bool applyOffsetToEnd(const Dur2& offset) {
        if (this->hasOpenEnd()) return false;
        
        WallClockTimepoint<Duration> newEnd{this->end.value()};  // copy the old end
        newEnd += std::chrono::duration_cast<Duration>(offset);;
        
        if (newEnd < this->start) return false;
        
        this->end = newEnd;
        return true;
      }
  };
  using TimeRange_secs = TimeRange<std::chrono::seconds>;
  using TimeRange_ms = TimeRange<std::chrono::milliseconds>;
  using TimeRange_us = TimeRange<std::chrono::microseconds>;
  
  /** \brief A class for a date period that is defined by two `date::year_month_day` values
   * 
   * \note Start day and end day are fully included in the period. Hence, a
   * `DatePeriod` that starts and ends on the same day has a length of one (1) day.
   */
  class DateRange : public Sloppy::GenericRange<date::year_month_day>
  {
  public:
    /** \brief Ctor for a closed date period
     * 
     * \note Start day and end day are fully included in the period. Hence, a
     * `DatePeriod` that starts and ends on the same day has a length of one (1) day.
     *
     * \throws std::invalid_argument if the end is before the start
     */
    DateRange(
      const date::year_month_day& _start,   ///< the first day of the DatePeriod
      const date::year_month_day& _end      ///< the last day of the DatePeriod
    )
    :GenericRange<date::year_month_day>(_start, _end) {}
    
    /** \brief Ctor for an open ended date period
     */
    DateRange(
      const date::year_month_day& _start   ///< the first day of the DatePeriod
    )
    :GenericRange<date::year_month_day>(_start) {}
    
    template<class Dur2>
    std::optional<Dur2> length() const {
      if (this->hasOpenEnd()) return std::nullopt;
      
      // convert start and end to UTC timestamps
      const auto tpStart = date::sys_days{this->start};
      const auto tpEnd = date::sys_days{this->end.value()};
      
      return std::chrono::duration_cast<Dur2>(tpEnd - tpStart + DayDurationInt{1});  // "+1" because "start == end" means "one day"      
    }
    
    /** \returns the length of the date range in days (or empty in case of open periods)
     */
    std::optional<DayDurationInt> length_days() const
    {
      return length<DayDurationInt>();
    }
    
    /** \returns the length of the date period in weeks (incl. digits) or `-1` in case of open periods
     */
    std::optional<WeekDurationDouble> length_weeks() const
    {
      return length<WeekDurationDouble>();
    }
  };
  
  }
}
