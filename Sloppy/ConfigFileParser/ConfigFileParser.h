/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

#ifndef SLOPPY__CONFIG_FILE_PARSER_H
#define SLOPPY__CONFIG_FILE_PARSER_H

#include <memory>
#include <istream>
#include <vector>
#include <unordered_map>
#include <optional>

#include "ConstraintChecker.h"
#include "../String.h"

namespace Sloppy
{
  static constexpr const char* defaultSectionName = "__DEFAULT__";

  using KeyValueMap = std::unordered_map<std::string, std::string>;

  /** \brief A struct that takes section name, key name and constraint type for
   * a constraint check.
   *
   * This is for the easy creation of "bulk checks" based a list of these structs.
   */
  struct ConstraintCheckData
  {
    std::string secName;
    std::string keyName;
    ValueConstraint c;
  };

  /** \brief A class that parses ini-style text files for reading configuration data.
     *
     * Writing of ini-files is not supported.
     *
     * The file format is simple:
     *   - `[xxxx]` starts a new section
     *   - Each section holds any number of `<key> = <value>` pairs
     *   - Lines starting with `#` or `;` are ignored (==> comments)
     *   - Empty lines or lines containing only whitespaces are ignored
     *
     * Whitespaces around keys and values will be removed.
     *
     * If a key occurs multiple times within a section, the last value assigment "wins".
     *
     * Key and section names are case-sensitive.
     *
     * If the file provides key/values before any section has been opened, these key/value-pairs
     * are filed internally under a "default section" named "__DEFAULT__".
     */
  class Parser
  {
  public:

    /** \brief Default ctor creating an empty, valid but essentially unusable instance
     */
    Parser() = default;

    /** \brief Ctor reading the ini-data from an input stream
     *
     * \throws std::invalid_argument if the input stream is empty or invalid
     */
    explicit Parser(std::istream& inStream);

    /** \brief Ctor reading the ini-data from a file
     *
     * \throws std::invalid_argument if the file doesn't exist or is empty
     */
    explicit Parser(const std::string& fName);

    /** \returns true if the config file contains a given section
     */
    bool hasSection(
        const std::string& secName   ///< the section name to look for
        ) const;

    /** \returns true if a given section contains a given key
     */
    bool hasKey(
        const std::string& secName,   ///< the name of section to look for
        const std::string& keyName    ///< the name of the key in that section
        ) const;

    /** \returns true if the default section contains a given key
     */
    bool hasKey(
        const std::string& keyName   ///< the name of the key to look for
        ) const;

    /** \brief tries to retrieve the value of a given key from a given section.
     *
     * Uses optional<estring>; the optional value is empty if the section/key-pair does not exist.
     *
     * \returns an optional<estring> that either contains the requested value or is empty
     */
    std::optional<estring> getValue(
        const std::string& secName,   ///< the name of the section containing the key
        const std::string& keyName    ///< the name of key in that section
        ) const;

    /** \brief tries to retrieve the value of a given key from the default section.
     *
     * Uses optional<estring>; the optional value is empty if the key does not exist.
     *
     * \returns an optional<estring> that either contains the requested value or is empty
     */
    std::optional<estring> getValue(
        const std::string& keyName   ///< the name of the key
        ) const;

    /** \brief tries to retrieve the value of a given key from a given section as bool.
     *
     * Uses optional<bool>; the optional value is empty if the section/key-pair does not exist
     * or if it is not parseable into a bool.
     *
     * The values "1", "true", "on", "yes" are evaluated to `true`.
     *
     * The values "0", "false", "off", "no" are evaluated to `false`.
     *
     * Parsing of string literals is case-insensitive.
     *
     * \returns an optional<bool> that contains the requested value; for invalid keys or values, the return value is empty.
     */
    std::optional<bool> getValueAsBool(
        const std::string& secName,   ///< the name of the section containing the key
        const std::string& keyName    ///< the name of key in that section
        ) const;

    /** \brief tries to retrieve the value of a given key from the default section as bool.
     *
     * Uses optional<bool>; the optional value is empty if the section/key-pair does not exist
     * or if it is not parseable into a bool.
     *
     * The values "1", "true", "on", "yes" are evaluated to `true`.
     * The values "0", "false", "off", "no" are evaluated to `false`.
     *
     * Parsing of string literals is case-insensitive.
     *
     * \returns an optional<bool> that contains the requested value; for invalid keys or values, the return value is empty.
     */
    std::optional<bool> getValueAsBool(
        const std::string& keyName   ///< the name of the key
        ) const;

    /** \brief tries to retrieve the value of a given key from a given section as an integer.
     *
     * Uses optional<int>; the optional value is empty if the section/key-pair does not exist
     * or if it is not parseable into an integer.
     *
     * \returns an optional<int> that contains the requested value; for invalid keys or values, the return value is empty.
     */
    std::optional<int> getValueAsInt(const std::string& secName, const std::string& keyName) const;

    /** \brief tries to retrieve the value of a given key from a the default section as an integer.
     *
     * Uses optional<int>; the optional value is empty if the section/key-pair does not exist
     * or if it is not parseable into an integer.
     *
     * \returns an optional<int> that contains the requested value; for invalid keys or values, the return value is empty.
     */
    std::optional<int> getValueAsInt(const std::string& keyName) const;

    /** \brief Checks whether a key/value-pair satisfies a given constraint.
     *
     * Optionally, this method generated a human-readable error message for
     * displaying it to the user, e.g., on the console. The pointed-to string
     * will only be modified if the requested constraint is not met.
     *
     * \throws std::invalid_argument if the provided key name is empty
     *
     * \returns `true` if the key and its value satisfy the requested constraint.
     */
    bool checkConstraint(
        const std::string& keyName,    ///< the name of the key
        ValueConstraint c,     ///< the constraint to check
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;

    /** \brief Checks whether a key/value-pair satisfies a given constraint.
     *
     * Optionally, this method generated a human-readable error message for
     * displaying it to the user, e.g., on the console. The pointed-to string
     * will only be modified if the requested constraint is not met.
     *
     * \throws std::invalid_argument if the provided key name or the provided section name is empty
     *
     * \returns `true` if the key and its value satisfy the requested constraint.
     */
    bool checkConstraint(
        const std::string& secName,    ///< the name of the section containing the key
        const std::string& keyName,    ///< the name of the key
        ValueConstraint c,     ///< the constraint to check
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;

    /** \brief Checks whether a key/value-pair satisfies a given constraint.
     *
     * Optionally, this method generated a human-readable error message for
     * displaying it to the user, e.g., on the console. The pointed-to string
     * will only be modified if the requested constraint is not met.
     *
     * If the section name in the ConstraintCheckData instance is empty, the default
     * section name is used.
     *
     * \throws std::invalid_argument if the provided key name in the ConstraintCheckData instance is empty
     *
     * \returns `true` if the key and its value satisfy the requested constraint.
     */
    bool checkConstraint(
        const ConstraintCheckData& ccd,   ///< section name, key name and constraint for checking
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;

    /** \brief Bulk checks the config file agains a list of constraints
     *
     * \returns `true` if ALL of the provided constraints are met; `false` if ANY of the provided contraints failed.
     */
    bool bulkCheckConstraints(
        std::vector<ConstraintCheckData> constraintList,   ///< the list of contraints to check
        bool logErrorToConsole = true,   ///< if `true`, an error message for the frist unmet contraint is printed to `cerr`
        std::string* errMsg = nullptr   ///< an optional pointer to a string for returning a human-readable error message for the first unmet constraint
        ) const;

    /** \brief Checks whether a key contains an integer in a given value range.
     *
     * \throws std::invalid_argument if the provided key name or the provided section name is empty
     *
     * \throws std::range_error if both min and max are provided and max is less than min.
     *
     * \returns `true` if the key contains an integer and the value is within the given min/max range
     */
    bool checkConstraint_IntRange(
        const std::string& secName,    ///< the name of the section containing the key
        const std::string& keyName,    ///< the name of the key
        std::optional<int> minVal,     ///< if provided, the value will be checked against this min value
        std::optional<int> maxVal,     ///< if provided, the value will be checked against this max value
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;

    /** \brief Checks whether a key contains an integer in a given value range.
     *
     * \throws std::invalid_argument if the provided key name or the provided section name is empty
     *
     * \throws std::range_error if both min and max are provided and max is less than min.
     *
     * \returns `true` if the key contains an integer and the value is within the given min/max range
     */
    bool checkConstraint_IntRange(
        const std::string& keyName,    ///< the name of the key
        std::optional<int> minVal,     ///< if provided, the value will be checked against this min value
        std::optional<int> maxVal,     ///< if provided, the value will be checked against this max value
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;


    /** \brief Checks whether a key contains an string with a length in a given range.
     *
     * \note A min/max length of 0 is ignored because we have an implicit
     * "not empty" criterion for the string.
     *
     * \throws std::invalid_argument if the provided key name or the provided section name is empty
     *
     * \throws std::range_error if both min and max are provided and max is less than min.
     *
     * \returns `true` if the key contains a string and its length is within the given min/max range
     */
    bool checkConstraint_StrLen(
        const std::string& secName,    ///< the name of the section containing the key
        const std::string& keyName,    ///< the name of the key
        std::optional<size_t> minLen,     ///< if provided, the string will be checked against this min length
        std::optional<size_t> maxLen,     ///< if provided, the string will be checked against this max length
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;

    /** \brief Checks whether a key contains an string with a length in a given range.
     *
     * \note A min/max length of 0 is ignored because we have an implicit
     * "not empty" criterion for the string.
     *
     * \throws std::invalid_argument if the provided key name or the provided section name is empty
     *
     * \throws std::range_error if both min and max are provided and max is less than min.
     *
     * \returns `true` if the key contains a string and its length is within the given min/max range
     */
    bool checkConstraint_StrLen(
        const std::string& keyName,    ///< the name of the key
        std::optional<size_t> minLen,     ///< if provided, the string will be checked against this min length
        std::optional<size_t> maxLen,     ///< if provided, the string will be checked against this max length
        std::string* errMsg = nullptr  ///< an optional pointer to a string for returning a human-readable error message
        ) const;

    /** \returns a list of all sections including the default section
     */
    std::vector<std::string> allSections() const;

  protected:
    /** \brief Does the actual parsing job and is called from the various ctors
     */
    void fillFromStream(std::istream& inStream);

    std::unordered_map<std::string, KeyValueMap> content{};   ///< a map of key-value pairs for each section; this is the actual content of the ini file

    /** \brief Creates a new, empty key/value-map for a given section in the overall `content` container or returns a reference the section map if the section already exists.
     *
     * The section name is processed as it is. No trimming will be applied.
     *
     * \throws std::invalid_argument if the section name is empty.
     *
     * \returns A reference to the (possibly empty) key/value map for a given section
     */
    KeyValueMap& getOrCreateSection(
        const std::string& secName   ///< the name of the section to get the key/value map for
        );

    /** \brief Stores a value for a given key in a given section
     *
     * The entry in the section's key/value map is either updated or newly created.
     *
     * Section name, key name and value are processed as they are. No trimming will be applied.
     *
     * If no key/value map for the particular section exists, a new map is created.
     */
    void insertOrOverwriteValue(
        const std::string& secName,    ///< the section name for the key/value pair
        const std::string& keyName,    ///< the key name
        const std::string& val         ///< the key's value
        );

    /** \brief Stores a value for a given key in a given key/value map
     *
     * The entry in the section's key/value map is either updated or newly created.
     *
     * Key name and value are processed as they are. No trimming will be applied.
     */
    void insertOrOverwriteValue(
        KeyValueMap& sec,        ///< the key/value map for storing the value
        const std::string& keyName,   ///< the key name
        const std::string& val        ///< the key's value
        );
  };

}
#endif
