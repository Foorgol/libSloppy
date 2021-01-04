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

#ifndef SLOPPY__CONSTRAINT_CHECKER_H
#define SLOPPY__CONSTRAINT_CHECKER_H

#include <optional>

#include "../String.h"

namespace Sloppy
{
  /** \brief An enum that defines a list of constraints that
   * an string (e.g., an entry in a config file) can be checked against.
   */
  enum class ValueConstraint
  {
    Exist,      ///< the value must exist but may be empty (e.g., is an empty string)
    NotEmpty,   ///< the value must exist and must contain data (e.g., is NOT an empty string)
    Alnum,      ///< the value is not empty and contains only alphanumeric characters
    Alpha,      ///< the value is not empty and contains only alphabetic characters
    Digit,      ///< the value is not empty and contains only digits (not including a minus sign!)
    Numeric,    ///< the value must be numeric (int or float), including a possible minus sign
    Integer,    ///< the value must be an integer (not a float), including a possible minus sign
    Bool,       ///< the value is either "0", "1", "o"n, "off", "true" or "false" (case insensitive)
    File,       ///< the value must point to an existing file (not a directory); it is not checked whether the file is accessible for reading or writing
    Directory,  ///< the value must point to an existing directory (not a file); it is not checked whether the directory is accessible for reading or writing
    StandardTimezone,   ///< the value must refer to one of the compiled-in timezone definitions (see LocalTime or `Zonespec.cpp`)
    IsoDate,    ///< the value is a valid ISO date (YYYY-MM-DD)
  };

  //----------------------------------------------------------------------------

  /** \brief Checks whether a string (e.g., from a config file) satisfies a given constraint.
   *
   * Optionally, this method generated a human-readable error message for
   * displaying it to the user, e.g., on the console. The pointed-to string
   * will only be modified if the requested constraint is not met.
   *
   * \returns `true` if value satisfies the requested constraint.
   */
  bool checkConstraint(
      const std::optional<estring>& val,    ///< the value that is to be checked
      ValueConstraint c,     ///< the constraint to check
      std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
      );

  //----------------------------------------------------------------------------

  /** \brief Checks whether a string (e.g., from a config file) satisfies a given constraint.
   *
   * Optionally, this method generated a human-readable error message for
   * displaying it to the user, e.g., on the console. The pointed-to string
   * will only be modified if the requested constraint is not met.
   *
   * \returns `true` if value satisfies the requested constraint.
   */
  bool checkConstraint(const estring& val,    ///< the value that is to be checked
      ValueConstraint c,     ///< the constraint to check
      std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
      );

  //----------------------------------------------------------------------------

  /** \brief Checks whether a string contains an integer in a given value range.
   *
   * \throws std::range_error if both min and max are provided and max is less than min.
   *
   * \returns `true` if the value contains an integer and the value is within the given min/max range
   */
  bool checkConstraint_IntRange(
      const std::optional<estring>& val,    ///< the value that is to be checked
      const std::optional<int>& minVal,     ///< if provided, the value will be checked against this min value
      const std::optional<int>& maxVal,     ///< if provided, the value will be checked against this max value
      std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
      );

  //----------------------------------------------------------------------------

  /** \brief Checks whether a string contains an integer in a given value range.
   *
   * \throws std::range_error if both min and max are provided and max is less than min.
   *
   * \returns `true` if the value contains an integer and the value is within the given min/max range
   */
  bool checkConstraint_IntRange(
      const estring& val,    ///< the value that is to be checked
      const std::optional<int>& minVal,     ///< if provided, the value will be checked against this min value
      const std::optional<int>& maxVal,     ///< if provided, the value will be checked against this max value
      std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
      );

  //----------------------------------------------------------------------------

  /** \brief Checks whether a string has a length in a given range.
   *
   * \note A min/max length of 0 is ignored because we have an implicit
   * "not empty" criterion for the string.
   *
   * \throws std::range_error if both min and max are provided and max is less than min.
   *
   * \returns `true` if the string has a length is within the given min/max range
   */
  bool checkConstraint_StrLen(
      const std::optional<estring>& val,    ///< the value that is to be checked
      const std::optional<size_t>& minLen,     ///< if provided, the string will be checked against this min length
      const std::optional<size_t>& maxLen,     ///< if provided, the string will be checked against this max length
      std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
      );

  //----------------------------------------------------------------------------

  /** \brief Checks whether a string has a length in a given range.
   *
   * \note A min/max length of 0 is ignored because we have an implicit
   * "not empty" criterion for the string.
   *
   * \throws std::range_error if both min and max are provided and max is less than min.
   *
   * \returns `true` if the string has a length is within the given min/max range
   */
  bool checkConstraint_StrLen(
      const estring& val,    ///< the value that is to be checked
      const std::optional<size_t>& minLen,     ///< if provided, the string will be checked against this min length
      const std::optional<size_t>& maxLen,     ///< if provided, the string will be checked against this max length
      std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
      );
}
#endif
