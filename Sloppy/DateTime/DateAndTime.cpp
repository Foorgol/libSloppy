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

#include <sstream>    // for basic_stringbuf<>::int_type, basic_stringbuf<>:...
#include <stdexcept>  // for out_of_range

#include "DateAndTime.h"

namespace Sloppy
{
  namespace DateTime
  {
    date::year_month_day ymdFromInt(int ymd)
    {
      if (ymd < 10101)
      {
        throw std::out_of_range("Invalid integer for conversion into year, month, day");
      }
      
      const unsigned short y = static_cast<unsigned short>(ymd / 10000);
      const unsigned short m = static_cast<unsigned short>((ymd % 10000) / 100);
      const unsigned short d = static_cast<unsigned short>(ymd % 100);
      
      return date::year{y} / m / d;   // uses special "/" operators defined in date.h
    }

    //----------------------------------------------------------------------------

    int intFromYmd(const date::year_month_day& d)
    {
      if (!d.ok()) {
        throw std::invalid_argument("year_month_day parameter invalid, can't convert to int");
      }

      const auto y = static_cast<int>(d.year());
      const auto m = static_cast<unsigned>(d.month());
      const auto day = static_cast<unsigned>(d.day());

      return y * 10000 + m * 100 + day;
    }

    //----------------------------------------------------------------------------

    bool isValidTime(int hour, int min, int sec)
    {
      return ((hour >= 0) && (hour < 24) && (min >=0) && (min < 60) && (sec >= 0) && (sec < 60));
    }

    //----------------------------------------------------------------------------

    bool isValidDate(int year, int month, int day) {
      const auto tmpDate = date::year{year} / month / day;
      return tmpDate.ok();
    }
    
    //----------------------------------------------------------------------------
    
    bool isLeapYear(int year) {
      const date::year y{year};
      
      return (y.ok() && y.is_leap());
    }
    
    //----------------------------------------------------------------------------
    
    std::optional<date::year_month_day> parseDateString(const std::string& in, const std::string& fmtString, bool strictChecking)
    {
      static const std::string DefaultFormat{"%F"};
      
      // determine the effective formatting string
      const std::string& effFormat = fmtString.empty() ? DefaultFormat : fmtString;
      
      // try parsing
      date::year_month_day result;
      std::istringstream inStream{in};
      inStream >> date::parse(effFormat, result);
      
      if (inStream.fail() || !result.ok()) return std::nullopt;   // the fail bit indicates that not the complete format string has been found / could be parsed
      
      if (!strictChecking) return result;
      
      // do a cross check by converting back to a string
      // and compare the strings
      std::ostringstream outStream;
      outStream << date::format(effFormat, result);
      
      return (outStream.str() == in) ? result : std::optional<date::year_month_day>{};
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

  }
}

