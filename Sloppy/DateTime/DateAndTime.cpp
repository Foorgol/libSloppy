#include <stdexcept>
#include <ctime>
#include <cstring>
#include <memory>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>

#include "DateAndTime.h"

using namespace std;

// a special hack to substitute the missing timegm() call
// under windows
//
// See the CMakeList.txt file for the definition of
// IS_WINDOWS_BUILD
#ifdef IS_WINDOWS_BUILD
#define timegm _mkgmtime
#endif

namespace Sloppy
{
  namespace DateTime
  {
    CommonTimestamp::CommonTimestamp(int year, int month, int day, int hour, int min, int sec)
    {
      using namespace boost;

      gregorian::date d{(short unsigned)year, (short unsigned)month, (short unsigned)day};
      posix_time::time_duration td{hour, min, sec, 0};

      raw = posix_time::ptime{d, td};
    }

    //----------------------------------------------------------------------------

    CommonTimestamp::CommonTimestamp(boost::posix_time::ptime rawTime)
      :raw{rawTime}
    {
    }

    //----------------------------------------------------------------------------

    time_t CommonTimestamp::getRawTime() const
    {
      return boost::posix_time::to_time_t(raw);
    }

    //----------------------------------------------------------------------------

    string CommonTimestamp::getISODate() const
    {
      return getFormattedString("%Y-%m-%d");
    }

    //----------------------------------------------------------------------------

    string CommonTimestamp::getTime() const
    {
      return getFormattedString("%H:%M:%S");
    }

    //----------------------------------------------------------------------------

    string CommonTimestamp::getTimestamp() const
    {
      return getFormattedString("%Y-%m-%d %H:%M:%S");
    }

    //----------------------------------------------------------------------------

    int CommonTimestamp::getDoW() const
    {
      return raw.date().day_of_week();
    }

    //----------------------------------------------------------------------------

    int CommonTimestamp::getYMD() const
    {
      using namespace boost;

      gregorian::date dat = raw.date();

      int y = dat.year();
      int m = dat.month();
      int d = dat.day();

      return (y) * 10000 + (m) * 100 + d;
    }

    //----------------------------------------------------------------------------

    bool CommonTimestamp::setTime(int hour, int min, int sec)
    {
      // check the time validity;
      boost::posix_time::time_duration td;
      try
      {
        td = boost::posix_time::time_duration{hour, min, sec};
      }
      catch (exception e)
      {
        return false;
      }

      // keep the date, assign a new time
      boost::gregorian::date d = raw.date();
      raw = boost::posix_time::ptime{d, td};

      return true;
    }

    //----------------------------------------------------------------------------

    tuple<int, int, int> CommonTimestamp::getYearMonthDay() const
    {
      using namespace boost;

      gregorian::date dat = raw.date();

      int y = dat.year();
      int m = dat.month();
      int d = dat.day();

      return make_tuple(y, m, d);
    }

    //----------------------------------------------------------------------------

    bool CommonTimestamp::isValidDate(int year, int month, int day)
    {
      try
      {
        boost::gregorian::date d{(short unsigned)year, (short unsigned)month, (short unsigned)day};
        return (!(d.is_special()));
      }
      catch (exception e)
      {
      }

      return false;
    }

    //----------------------------------------------------------------------------

    bool CommonTimestamp::isValidTime(int hour, int min, int sec)
    {
      return ((hour >= 0) && (hour < 24) && (min >=0) && (min < 60) && (sec >= 0) && (sec < 60));
    }

    //----------------------------------------------------------------------------

    bool CommonTimestamp::isLeapYear(int year)
    {
      return boost::gregorian::gregorian_calendar::is_leap_year(year);
    }

    //----------------------------------------------------------------------------

    string CommonTimestamp::getFormattedString(const string& fmt) const
    {
      tm timestamp = boost::posix_time::to_tm(raw);

      char buf[80];
      strftime(buf, 80, fmt.c_str(), &timestamp);
      string result = string(buf);

      return result;
    }

    //----------------------------------------------------------------------------

    UTCTimestamp::UTCTimestamp(int year, int month, int day, int hour, int min, int sec)
      : CommonTimestamp(year, month, day, hour, min, sec)
    {
      // nothing to do here, CommonTimestamp assumes UTC as default
    }

    //----------------------------------------------------------------------------

    UTCTimestamp::UTCTimestamp(int ymd, int hour, int min, int sec)
      : UTCTimestamp(ymd / 10000, (ymd % 10000) / 100, ymd % 100, hour, min, sec)
    {
    }

    //----------------------------------------------------------------------------

    UTCTimestamp::UTCTimestamp(time_t rawTimeInUTC)
      :CommonTimestamp(boost::posix_time::from_time_t(rawTimeInUTC))
    {
    }

    //----------------------------------------------------------------------------

    UTCTimestamp::UTCTimestamp(boost::posix_time::ptime utcTime)
      :CommonTimestamp{utcTime}
    {
    }

    //----------------------------------------------------------------------------

    UTCTimestamp::UTCTimestamp()
      :UTCTimestamp(time(0))
    {

    }

    //----------------------------------------------------------------------------

    LocalTimestamp UTCTimestamp::toLocalTime(boost::local_time::time_zone_ptr tzp) const
    {
      return LocalTimestamp(boost::posix_time::to_time_t(raw), tzp);
    }

    //----------------------------------------------------------------------------

    LocalTimestamp::LocalTimestamp(int year, int month, int day, int hour, int min, int sec, boost::local_time::time_zone_ptr tzp)
      : CommonTimestamp(year, month, day, hour, min, sec)
    {
      using namespace boost::gregorian;
      using namespace boost::posix_time;
      using namespace boost::local_time;

      // try to construct a date object
      date d;
      try
      {
        d = date{(short unsigned)year, (short unsigned)month, (short unsigned)day};
      }
      catch (exception e)
      {
        throw std::invalid_argument("Invalid date values!");
      }

      // try to construct a time duration
      time_duration td{0,0,0, 0};
      try
      {
        td = time_duration{hour, min, sec, 0};
      }
      catch (exception e)
      {
        throw std::invalid_argument("Invalid time values!");
      }

      // make sure the time zone pointer is valid
      if (tzp == nullptr)
      {
        throw std::invalid_argument("Time zone pointer is empty");
      }

      // construct a local_date_time object
      local_date_time ldt{d, td, tzp, local_date_time::NOT_DATE_TIME_ON_ERROR};
      if (ldt.is_not_a_date_time())
      {
        throw std::invalid_argument("Local time is invalid or ambiguous");
      }

      // convert to UTC and store the result
      raw = ldt.local_time();
      utc = ldt.utc_time();
    }

    //----------------------------------------------------------------------------

    LocalTimestamp::LocalTimestamp(time_t rawTimeInUTC, boost::local_time::time_zone_ptr tzp)
      :CommonTimestamp(2000, 01, 01, 12, 0, 0)  // dummy values, will be overwritten anyway
    {
      using namespace boost;

      // store the provided raw value
      utc = posix_time::from_time_t(rawTimeInUTC);


      // convert the UTC time to a local time and use this local time for
      // all other functionalities (e.g., conversion to strings etc.)
      local_time::local_date_time ldt{utc, tzp};
      raw = ldt.local_time();
    }

    //----------------------------------------------------------------------------

    LocalTimestamp::LocalTimestamp(boost::local_time::time_zone_ptr tzp)
      :LocalTimestamp(time(0), tzp)
    {

    }

    //----------------------------------------------------------------------------

    UTCTimestamp LocalTimestamp::toUTC() const
    {
      return UTCTimestamp(raw);
    }

    //----------------------------------------------------------------------------

    unique_ptr<LocalTimestamp> LocalTimestamp::fromISODate(const string& isoDate, boost::local_time::time_zone_ptr tzp, int hour, int min, int sec)
    {
      //
      // split the string into its components
      //

      // find the first "-"
      size_t posFirstDash = isoDate.find('-');
      if (posFirstDash != 4)    // the first dash must always be at position 4 (4-digit year!)
      {
        return nullptr;
      }
      string sYear = isoDate.substr(0, 4);

      // find the second "-"
      size_t posSecondDash = isoDate.find('-', 5);
      if (posSecondDash == string::npos)
      {
        return nullptr;
      }
      string sMonth = isoDate.substr(5, posSecondDash - 5 + 1);

      // get the day
      if (posSecondDash >= (isoDate.length() - 1))    // the dash is the last character ==> no day string
      {
        return nullptr;
      }
      string sDay = isoDate.substr(posSecondDash+1, string::npos);

      // try to convert the string into ints
      int year;
      int month;
      int day;
      try
      {
        year = stoi(sYear);
        month = stoi(sMonth);
        day = stoi(sDay);
      } catch (exception e) {
        return nullptr;
      }

      // try to construct a new LocalTimestamp from these ints
      LocalTimestamp* result;
      try
      {
        result = new LocalTimestamp(year, month, day, hour, min, sec, tzp);
      } catch (exception e) {
        return nullptr;   // invalid parameters
      }

      // return the result
      return upLocalTimestamp(result);
    }

    //----------------------------------------------------------------------------

    time_t LocalTimestamp::getRawTime() const
    {
      return boost::posix_time::to_time_t(utc);
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    TimePeriod::TimePeriod(const UTCTimestamp &_start)
      :start(_start), isOpenEnd(true)
    {

    }

    //----------------------------------------------------------------------------

    TimePeriod::TimePeriod(const UTCTimestamp &_start, const UTCTimestamp &_end)
      :start(_start), end(_end), isOpenEnd(false)
    {
      if (end < start)
      {
        throw invalid_argument("TimePeriod ctor: 'end'' may not be before start!");
      }
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::hasOpenEnd() const
    {
      return isOpenEnd;
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::isInPeriod(const UTCTimestamp &ts) const
    {
      return (determineRelationToPeriod(ts) == IS_IN_PERIOD);
    }

    //----------------------------------------------------------------------------

    int TimePeriod::determineRelationToPeriod(const UTCTimestamp &ts) const
    {
      if (ts < start) return IS_BEFORE_PERIOD;
      if (isOpenEnd) return IS_IN_PERIOD;

      if (ts > end) return IS_AFTER_PERIOD;
      return IS_IN_PERIOD;
    }

    //----------------------------------------------------------------------------

    long TimePeriod::getLength_Sec() const
    {
      if (isOpenEnd) return -1;

      return end.getRawTime() - start.getRawTime();
    }

    //----------------------------------------------------------------------------

    double TimePeriod::getLength_Minutes() const
    {
      long secs = getLength_Sec();

      return (secs < 0 ? -1 : secs / 60.0);
    }

    //----------------------------------------------------------------------------

    double TimePeriod::getLength_Hours() const
    {
      long secs = getLength_Sec();

      return (secs < 0 ? -1 : secs / (3600.0));
    }

    //----------------------------------------------------------------------------

    double TimePeriod::getLength_Days() const
    {
      long secs = getLength_Sec();

      return (secs < 0 ? -1 : secs / (3600.0 * 24.0));
    }

    //----------------------------------------------------------------------------

    double TimePeriod::getLength_Weeks() const
    {
      long secs = getLength_Sec();

      return (secs < 0 ? -1 : secs / (3600.0 * 24.0 * 7));
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::setStart(const UTCTimestamp &_start)
    {
      if (_start > end) return false;

      start = _start;
      return true;
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::setEnd(const UTCTimestamp &_end)
    {
      if (_end < start) return false;

      end = _end;
      isOpenEnd = false;

      return true;
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::applyOffsetToStart(long secs)
    {
      UTCTimestamp newStart{start.getRawTime() + secs};

      if (newStart > end) return false;

      start = newStart;
      return true;
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::applyOffsetToEnd(long secs)
    {
      if (isOpenEnd) return false;

      UTCTimestamp newEnd{end.getRawTime() + secs};

      if (newEnd < start) return false;

      end = newEnd;
      return true;
    }

    //----------------------------------------------------------------------------

    UTCTimestamp TimePeriod::getStartTime() const
    {
      return start;
    }

    //----------------------------------------------------------------------------

    upUTCTimestamp TimePeriod::getEndTime() const
    {
      if (isOpenEnd) return nullptr;

      return make_unique<UTCTimestamp>(end);
    }

    //----------------------------------------------------------------------------

    tuple<int, int, int> YearMonthDayFromInt(int ymd)
    {
      int year = ymd / 10000;
      int month = (ymd % 10000) / 100;
      int day = ymd % 100;

      return make_tuple(year, month, day);
    }

    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
