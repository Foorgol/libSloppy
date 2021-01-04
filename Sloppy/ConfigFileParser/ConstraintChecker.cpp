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

#include <string>
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>

#include <filesystem>

#include "ConstraintChecker.h"
#include "../String.h"
#include "../DateTime/tz.h"
#include "../DateTime/date.h"

namespace Sloppy
{

  bool checkConstraint(const std::optional<estring>& val, ValueConstraint c, std::string* errMsg)
  {
    // first basic check: does the value exist at all?
    if (!(val.has_value()))
    {
      if (errMsg != nullptr)
      {
        *errMsg = "does not exist!";
      }

      return false;
    }

    // the checks so far where enough to satisfy KeyValueConstraint::Exist
    if (c == ValueConstraint::Exist) return true;

    // in all other cases, continue with the value-based evaluations
    return checkConstraint(val.value(), c, errMsg);
  }

  //----------------------------------------------------------------------------

  bool checkConstraint(const estring& val, ValueConstraint c, std::string* errMsg)
  {
    // regexs are expensive to create and thus we keep a few
    // of them in static local variables
    static std::regex reAlnum{"[[:alnum:]]+"};
    static std::regex reAlpha{"[[:alpha:]]+"};
    static std::regex reDigit{"[[:digit:]]+"};
    static std::regex reIsoDate{"(\\d{4})-(\\d{1,2})-(\\d{1,2})"};

    // is the value non-empty?
    if (val.empty())
    {
      if (errMsg != nullptr)
      {
        *errMsg = "is empty!";
      }

      return false;
    }

    // the checks so far where enough to satisfy KeyValueConstraint::NotEmpty
    if (c == ValueConstraint::NotEmpty) return true;

    // check the AlNum constraint with a regex
    if (c == ValueConstraint::Alnum)
    {
      bool isOkay = regex_match(val, reAlnum);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "is not purely alphanumeric!";
      }

      return isOkay;
    }

    // check the Alpha constraint with a regex
    if (c == ValueConstraint::Alpha)
    {
      bool isOkay = regex_match(val, reAlpha);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "is not purely alphabetic!";
      }

      return isOkay;
    }

    // check the Digit constraint with a regex
    if (c == ValueConstraint::Digit)
    {
      bool isOkay = regex_match(val, reDigit);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "contains non-digit characters!";
      }

      return isOkay;
    }

    // check the Numeric constraint by checking against
    // a double value. isDouble() is also true for pure integers
    if (c == ValueConstraint::Numeric)
    {
      bool isOkay = val.isDouble();
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "contains a non-numeric value!";
      }

      return isOkay;
    }

    // check the integer constraint
    if (c == ValueConstraint::Integer)
    {
      bool isOkay = val.isInt();
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "is not a valid integer!";
      }

      return isOkay;
    }

    // check the boolean constraint
    if (c == ValueConstraint::Bool)
    {
      estring tmp{val};
      tmp.toLower();

      bool isOkay{false};
      for (const std::string& permitted : {"1", "true", "on", "yes"})
      {
        if (tmp == permitted)
        {
          isOkay = true;
          break;
        }
      }
      if (!isOkay)
      {
        for (const std::string& permitted : {"0", "false", "off", "no"})
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

    // check the file constraint using
    if (c == ValueConstraint::File)
    {
      const std::string& s{val};
      std::filesystem::path p{s};
      bool isOkay = std::filesystem::is_regular_file(p);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not point to an existing, regular file!";
      }

      return isOkay;
    }

    // check the directory constraint
    if (c == ValueConstraint::Directory)
    {
      const std::string& s{val};
      std::filesystem::path p{s};
      bool isOkay = std::filesystem::is_directory(p);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not point to an existing directory!";
      }

      return isOkay;
    }

    // check against one of the compiled-in timezone specs
    if (c == ValueConstraint::StandardTimezone)
    {
      try {
        const auto tzPtr = date::locate_zone(val);
        return (tzPtr != nullptr);
      }
      catch (...) {
        return false;
      }
    }

    // check ISO dates
    if (c == ValueConstraint::IsoDate)
    {
      std::smatch sm;
      bool isOkay = regex_match(val, sm, reIsoDate);
      if (!isOkay && (errMsg != nullptr))
      {
        *errMsg = "does not match for ISO date format YYYY-MM-DD!";
      }
      if (!isOkay) return false;

      int y = stoi(sm[1]);
      int m = stoi(sm[2]);
      int d = stoi(sm[3]);

      isOkay = (date::year{y} / m / d).ok();
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
      *errMsg = "Unknown error when checking constraints on '" + val + "'";
    }
    return false;
  }

  //----------------------------------------------------------------------------

  bool checkConstraint_IntRange(const std::optional<estring>& val, const std::optional<int>& minVal, const std::optional<int>& maxVal, std::string* errMsg)
  {
    if (!val.has_value())
    {
      if (errMsg != nullptr)
      {
        *errMsg = "does not exist!";
      }

      return false;
    }

    return checkConstraint_IntRange(val.value(), minVal, maxVal, errMsg);
  }

  //----------------------------------------------------------------------------

  bool checkConstraint_IntRange(const estring& val, const std::optional<int>& minVal, const std::optional<int>& maxVal, std::string* errMsg)
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

    bool isOkay = checkConstraint(val, ValueConstraint::Integer, errMsg);
    if (!isOkay) return false;

    // after the previous check, the following should always succeed
    int v = stoi(val);

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

  bool checkConstraint_StrLen(const std::optional<estring>& val, const std::optional<size_t>& minLen, const std::optional<size_t>& maxLen, std::string* errMsg)
  {
    if (!val.has_value())
    {
      if (errMsg != nullptr)
      {
        *errMsg = "does not exist!";
      }

      return false;
    }

    return checkConstraint_StrLen(val.value(), minLen, maxLen, errMsg);
  }

  //----------------------------------------------------------------------------

  bool checkConstraint_StrLen(const estring& val, const std::optional<size_t>& minLen, const std::optional<size_t>& maxLen, std::string* errMsg)
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

    if (val.empty())
    {
      if (errMsg != nullptr)
      {
        *errMsg = "is empty!";
      }

      return false;
    }

    if (hasMin && (*minLen > 0))
    {
      bool isOkay = (val.length() >= *minLen);
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
      bool isOkay = (val.length() <= *maxLen);
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
