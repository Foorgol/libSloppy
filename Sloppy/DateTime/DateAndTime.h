#ifndef __LIBSLOPPY_DATEANDTIME_H
#define __LIBSLOPPY_DATEANDTIME_H

#include <string>
#include <memory>
#include <ctime>
#include <cstring>
#include <tuple>

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace std;


//
// Extend boost's gregorian date to deal with
// integer date representations, e.g. "20160801" = 2016-08-01
//
namespace boost
{
  namespace gregorian
  {
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

    // a time period
    class TimePeriod
    {
    public:
      static constexpr int IS_BEFORE_PERIOD = -1;
      static constexpr int IS_IN_PERIOD = 0;
      static constexpr int IS_AFTER_PERIOD = 1;

      TimePeriod(const UTCTimestamp& _start);
      TimePeriod(const UTCTimestamp& _start, const UTCTimestamp& _end);
      bool hasOpenEnd() const;
      bool isInPeriod(const UTCTimestamp& ts) const;
      int determineRelationToPeriod(const UTCTimestamp& ts) const;
      long getLength_Sec() const;
      double getLength_Minutes() const;
      double getLength_Hours() const;
      double getLength_Days() const;
      double getLength_Weeks() const;
      virtual bool setStart(const UTCTimestamp& _start);
      virtual bool setEnd(const UTCTimestamp& _end);

      virtual bool applyOffsetToStart(long secs);
      virtual bool applyOffsetToEnd(long secs);

      UTCTimestamp getStartTime() const;
      upUTCTimestamp getEndTime() const;

      inline bool startsEarlierThan (const TimePeriod& other) const
      {
        return (start < other.start);
      }
      inline bool startsLaterThan (const TimePeriod& other) const
      {
        return (start > other.start);
      }

    protected:
      UTCTimestamp start;
      UTCTimestamp end;
      bool isOpenEnd;
    };

    tuple<int, int, int> YearMonthDayFromInt(int ymd);
  }
}

#endif /* DATEANDTIME_H */
