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
#ifdef WIN32
#define timegm _mkgmtime
#endif

namespace Sloppy
{
  namespace DateTime
  {
    CommonTimestamp::CommonTimestamp(int year, int month, int day, int hour, int min, int sec)
      :raw{}
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
      : CommonTimestamp(year, month, day, hour, min, sec), utc{}
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
      :CommonTimestamp(2000, 01, 01, 12, 0, 0),  // dummy values, will be overwritten anyway
       utc{}
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

    long TimePeriod::getLength_Sec() const
    {
      if (hasOpenEnd()) return -1;

      return end->getRawTime() - start.getRawTime();
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

    bool TimePeriod::applyOffsetToStart(long secs)
    {
      UTCTimestamp newStart{start.getRawTime() + secs};

      return setStart(newStart);
    }

    //----------------------------------------------------------------------------

    bool TimePeriod::applyOffsetToEnd(long secs)
    {
      if (hasOpenEnd()) return false;

      UTCTimestamp newEnd{end->getRawTime() + secs};
      return setEnd(newEnd);
    }

    //----------------------------------------------------------------------------

    bool parseDateString(const string& in, boost::gregorian::date& out, const string& fmtString, bool strictChecking)
    {
      // create a date input facet that represents the
      // requested format or use extended ISO (yyyy-mm-dd) instead
      boost::gregorian::date_input_facet* dif = new boost::gregorian::date_input_facet{};
      if (fmtString.empty())
      {
        dif->set_iso_extended_format();
      } else {
        dif->format(fmtString.c_str());
      }

      // add this facet to a locale; the locale will own the facet object
      // that has been instanciated with "new"
      std::locale myLocale_in{std::locale::classic(), dif};

      // try to parse the input string
      try
      {
        istringstream is(in);
        is.imbue(myLocale_in);

        // the following tmp-date is initialized to "not_a_date_time"
        boost::gregorian::date tmp;

        // try to parse the stream into tmp
        is >> tmp;

        // if tmp now holds a valid date and not a special value
        // such as "not_a_date_time", the conversion was successfull
        if (!(tmp.is_special()))
        {
          string check{in};

          if (strictChecking)
          {
            ostringstream os;
            boost::gregorian::date_facet* df = new boost::gregorian::date_facet();
            if (fmtString.empty())
            {
              df->set_iso_extended_format();
            } else {
              df->format(fmtString.c_str());
            }
            std::locale myLocale_out{std::locale::classic(), df};
            os.imbue(myLocale_out);
            os << tmp;
            check = os.str();
          }

          if (check == in)
          {
            out = tmp;
            return true;
          }
        }
      }
      catch (...)
      {
      }

      out = boost::gregorian::date{};   // set output to "not_a_date_time"
      return false;
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


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
