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

#ifndef SLOPPY__CONFIG_FILE_PARSER_H
#define SLOPPY__CONFIG_FILE_PARSER_H

#include <memory>
#include <istream>
#include <vector>
#include <unordered_map>
#include <optional>

#include "../String.h"

namespace Sloppy
{
  static constexpr const char* defaultSectionName = "__DEFAULT__";

  using KeyValueMap = std::unordered_map<string, string>;

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
    explicit Parser(istream& inStream);

    /** \brief Ctor reading the ini-data from a file
     *
     * \throws std::invalid_argument if the file doesn't exist or is empty
     */
    explicit Parser(const string& fName);

    /** \returns true if the config file contains a given section
     */
    bool hasSection(
        const string& secName   ///< the section name to look for
        ) const;

    /** \returns true if a given section contains a given key
     */
    bool hasKey(
        const string& secName,   ///< the name of section to look for
        const string& keyName    ///< the name of the key in that section
        ) const;

    /** \returns true if the default section contains a given key
     */
    bool hasKey(
        const string& keyName   ///< the name of the key to look for
        ) const;

    /** \brief tries to retrieve the value of a given key from a given section.
     *
     * Uses optional<estring>; the optional value is empty if the section/key-pair does not exist.
     *
     * \returns an optional<estring> that either contains the requested value or is empty
     */
    optional<estring> getValue(
        const string& secName,   ///< the name of the section containing the key
        const string& keyName    ///< the name of key in that section
        ) const;

    /** \brief tries to retrieve the value of a given key from the default section.
     *
     * Uses optional<estring>; the optional value is empty if the key does not exist.
     *
     * \returns an optional<estring> that either contains the requested value or is empty
     */
    optional<estring> getValue(
        const string& keyName   ///< the name of the key
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
    optional<bool> getValueAsBool(
        const string& secName,   ///< the name of the section containing the key
        const string& keyName    ///< the name of key in that section
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
    optional<bool> getValueAsBool(
        const string& keyName   ///< the name of the key
        ) const;

    /** \brief tries to retrieve the value of a given key from a given section as an integer.
     *
     * Uses optional<int>; the optional value is empty if the section/key-pair does not exist
     * or if it is not parseable into an integer.
     *
     * \returns an optional<int> that contains the requested value; for invalid keys or values, the return value is empty.
     */
    optional<int> getValueAsInt(const string& secName, const string& keyName) const;

    /** \brief tries to retrieve the value of a given key from a the default section as an integer.
     *
     * Uses optional<int>; the optional value is empty if the section/key-pair does not exist
     * or if it is not parseable into an integer.
     *
     * \returns an optional<int> that contains the requested value; for invalid keys or values, the return value is empty.
     */
    optional<int> getValueAsInt(const string& keyName) const;

  protected:
    /** \brief Does the actual parsing job and is called from the various ctors
     */
    void fillFromStream(istream& inStream);

    std::unordered_map<string, KeyValueMap> content{};   ///< a map of key-value pairs for each section; this is the actual content of the ini file

    /** \brief Creates a new, empty key/value-map for a given section in the overall `content` container or returns a reference the section map if the section already exists.
     *
     * The section name is processed as it is. No trimming will be applied.
     *
     * \throws std::invalid_argument if the section name is empty.
     *
     * \returns A reference to the (possibly empty) key/value map for a given section
     */
    KeyValueMap& getOrCreateSection(
        const string& secName   ///< the name of the section to get the key/value map for
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
        const string& secName,    ///< the section name for the key/value pair
        const string& keyName,    ///< the key name
        const string& val         ///< the key's value
        );

    /** \brief Stores a value for a given key in a given key/value map
     *
     * The entry in the section's key/value map is either updated or newly created.
     *
     * Key name and value are processed as they are. No trimming will be applied.
     */
    void insertOrOverwriteValue(
        KeyValueMap& sec,        ///< the key/value map for storing the value
        const string& keyName,   ///< the key name
        const string& val        ///< the key's value
        );
  };

}
#endif
