/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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

#include <string>
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>

#include <boost/filesystem.hpp>

#include "ConstraintChecker.h"
#include "../String.h"
#include "../DateTime/DateAndTime.h"

namespace bfs = boost::filesystem;

namespace Sloppy
{

  bool checkConstraint(const optional<estring>& val, Sloppy::KeyValueConstraint c, string* errMsg)
  {
    // regexs are expensive to create and thus we keep a few
    // of them in static local variables
    static regex reAlnum{"[[:alnum:]]+"};
    static regex reAlpha{"[[:alpha:]]+"};
    static regex reDigit{"[[:digit:]]+"};
    static regex reIsoDate{"(\\d{4})-(\\d{1,2})-(\\d{1,2})"};

    // is the value non-empty?
    if (val->empty())
    {
      if (errMsg != nullptr)
      {
        *errMsg = "is empty!";
      }

      return false;
    }

    // the checks so far where enough to satisfy KeyValueConstraint::NotEmpty
    if (c == KeyValueConstraint::NotEmpty) return true;

    // check the AlNum constraint with a regex
    if (c == KeyValueConstraint::Alnum)
    {
      bool isOkay = regex_match(val.value(), reAlnum);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "is not purely alphanumeric!";
      }

      return isOkay;
    }

    // check the Alpha constraint with a regex
    if (c == KeyValueConstraint::Alpha)
    {
      bool isOkay = regex_match(val.value(), reAlpha);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "is not purely alphabetic!";
      }

      return isOkay;
    }

    // check the Digit constraint with a regex
    if (c == KeyValueConstraint::Digit)
    {
      bool isOkay = regex_match(val.value(), reDigit);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "contains non-digit characters!";
      }

      return isOkay;
    }

    // check the Numeric constraint by checking against
    // a double value. isDouble() is also true for pure integers
    if (c == KeyValueConstraint::Numeric)
    {
      bool isOkay = val->isDouble();
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "contains a non-numeric value!";
      }

      return isOkay;
    }

    // check the integer constraint
    if (c == KeyValueConstraint::Integer)
    {
      bool isOkay = val->isInt();
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "is not a valid integer!";
      }

      return isOkay;
    }

    // check the boolean constraint
    if (c == KeyValueConstraint::Bool)
    {
      estring tmp{val.value()};
      tmp.toLower();

      bool isOkay{false};
      for (const string& permitted : {"1", "true", "on", "yes"})
      {
        if (tmp == permitted)
        {
          isOkay = true;
          break;
        }
      }
      if (!isOkay)
      {
        for (const string& permitted : {"0", "false", "off", "no"})
        {
          if (tmp == permitted)
          {
            isOkay = true;
            break;
          }
        }
      }

      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not contain valid boolean data ('1', '0', 'on', 'off', 'true' or 'false')!";
      }

      return isOkay;
    }

    // check the file constraint using Boost's file system implementation
    if (c == KeyValueConstraint::File)
    {
      bfs::path p{val.value()};
      bool isOkay = bfs::is_regular_file(p);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not point to an existing, regular file!";
      }

      return isOkay;
    }

    // check the directory constraint using Boost's file system implementation
    if (c == KeyValueConstraint::Directory)
    {
      bfs::path p{val.value()};
      bool isOkay = bfs::is_directory(p);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not point to an existing directory!";
      }

      return isOkay;
    }

    // check against one of the compiled-in timezone specs
    if (c == KeyValueConstraint::StandardTimezone)
    {
      auto db = DateTime::getPopulatedTzDatabase();
      auto tz = db.time_zone_from_region(val.value());

      bool isOkay =  (tz != nullptr);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not contain a known timezone name!";
      }

      return isOkay;
    }

    // check ISO dates
    if (c == KeyValueConstraint::IsoDate)
    {
      smatch sm;
      bool isOkay = regex_match(val.value(), sm, reIsoDate);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not match for ISO date format YYYY-MM-DD!";
      }
      if (!isOkay) return false;

      int y = stoi(sm[1]);
      int m = stoi(sm[2]);
      int d = stoi(sm[3]);

      isOkay = DateTime::CommonTimestamp::isValidDate(y, m, d);
      if (!isOkay && (errMsg != nullptr))
      {
       *errMsg = "does not contain a valid date!";
      }

      return isOkay;
    }

    //
    // we should never reach this point
    //
    if (errMsg != nullptr)
    {
      *errMsg = "Unknown error when checking constraints on '" + val.value() + "'";
    }
    return false;
  }

  //----------------------------------------------------------------------------

  bool checkConstraint_IntRange(optional<estring> val, optional<int> minVal, optional<int> maxVal, string* errMsg)
  {
    bool hasMin = minVal.has_value();
    bool hasMax = maxVal.has_value();

    if (hasMin && hasMax)
    {
      if (*maxVal < *minVal)
      {
        throw std::range_error("Constraint Checker: parameters for integer range check are inconsistent.");
      }
    }

    bool isOkay = checkConstraint(val, KeyValueConstraint::Integer, errMsg);
    if (!isOkay) return false;

    // after the previous check, the following should always succeed
    int v = stoi(*val);

    if (hasMin)
    {
      isOkay = (v >= *minVal);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = "shall have a min value of at least %1";
        e.arg(*minVal);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    if (hasMax)
    {
      isOkay = (v <= *maxVal);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = "shall have a max value of not more than %1";
        e.arg(*maxVal);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------

  bool checkConstraint_StrLen(optional<estring> val, optional<size_t> minLen, optional<size_t> maxLen, string* errMsg)
  {
    bool hasMin = minLen.has_value();
    bool hasMax = maxLen.has_value();

    if (hasMin && hasMax)
    {
      if (*maxLen < *minLen)
      {
        throw std::range_error("Config file parser: parameters for string length check are inconsistent.");
      }
    }

    bool isOkay = checkConstraint(val, KeyValueConstraint::NotEmpty, errMsg);
    if (!isOkay) return false;

    const estring& v = val.value();

    if (hasMin && (*minLen > 0))
    {
      isOkay = (v.length() >= *minLen);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = "shall have a min length of at least %1 characters!";
        e.arg(*minLen);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    if (hasMax && (*maxLen > 0))
    {
      isOkay = (v.length() <= *maxLen);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = "shall have a max length of not more than %1 characters!";
        e.arg(*maxLen);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    return true;
  }
}
