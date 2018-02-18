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

#ifndef __LIBSLOPPY_STRING_H
#define __LIBSLOPPY_STRING_H

#include <string>
#include <vector>

using namespace std;

namespace Sloppy
{
  /** \brief An extend string class with some convenience functions
   *
   * This class is derived from std::string and can be used like std::string
   * but additionally offers some methods similar to QString (but not as complex)
   */
  class estring : public std::string
  {
  public:
    /// default ctor, creates an empty estring
    estring() : string() {}

    /// ctor from an C-style string
    estring(const char* s) : string(s) {}

    /// copy constructor from an std::string
    estring(const string& other) : string(other) {}

    /// copy constructor from an estring
    estring(const estring& other) = default;

    /// copy assignment from an estring
    estring& operator =(const estring& other) = default;

    /// copy assignment from an std::string
    estring& operator =(const string& other) { string::operator =(other); return *this; }

    /// move constructor from an estring
    estring(estring&& other) = default;

    /// move assignment from an estring
    estring& operator =(estring&& other) = default;

    /// move assignment of std-string
    estring& operator =(string&& other) { string::operator =(std::move(other)); return *this; }

    /// move constructor from std-string
    estring(string&& other) noexcept : string(std::move(other)) {}

    //----------------------------------------------------------------------------

    /** \brief Returns a substring that is defined by the index of its first and last character
     *
     * If the index of the first character is outside the string, an empty string is returned.
     *
     * The source string will not be modified.
     *
     * \throws std::invalid_argument if idxLast < idxFirst
     *
     * \returns a new estring instance that contains the substring defined by idxFirst and idxLast; or
     * \returns an empty estring if idxFirst was beyond the last character in the string
     */
    estring slice(size_type idxFirst,                ///< index of the first character
                  size_type idxLast = string::npos   ///< index of the last character
        ) const;

    //----------------------------------------------------------------------------

    /** \brief Returns the n rightmost characters of the string
     *
     *  If n >= size(), the full string is returned.
     *
     * The source string will not be modified.
     *
     * \returns the n rightmost characters for 0 < n; or
     * \returns an empty string for n == 0
     */
    estring right(size_type n    ///< the number of character to return from the right
                  ) const;

    //----------------------------------------------------------------------------

    /** \brief Returns the n leftmost characters of the string
     *
     *  If n >= size(), the full string is returned.
     *
     * The source string will not be modified.
     *
     * \returns the n leftmost characters for 0 < n; or
     * \returns an empty string for n == 0
     */
    estring left(size_type n    ///< the number of character to return from the left
                 ) const;

    //----------------------------------------------------------------------------

    /** \brief Removes the n last characters from the string; the string is modified in place
     *
     *  If n >= size(), the string is cleared.
     *
     * \returns *this
     */
    estring& chopRight(size_type n    ///< the number of character to erase on the right
                       );

    //----------------------------------------------------------------------------

    /** \brief Removes the n first characters from the string; the string is modified in place
     *
     *  If n >= size(), the string is cleared.
     *
     * \returns *this
     */
    estring& chopLeft(size_type n    ///< the number of character to erase on the left
                       );

    //----------------------------------------------------------------------------

    /** \brief Returns a string with the last n characters removed
     *
     *  If n >= size(), an empty string is returned
     *
     * \returns *this
     */
    estring chopRight_copy(size_type n    ///< the number of character to erase on the right
                       ) const;


    //----------------------------------------------------------------------------

    /** \brief Returns a string with the first n characters removed
     *
     *  If n >= size(), an empty string is returned
     *
     * \returns *this
     */
    estring chopLeft_copy(size_type n    ///< the number of character to erase on the left
                       ) const;

    //----------------------------------------------------------------------------

    /** \brief Checks whether a string starts with a certain character sequence
     *
     *  Using an empty reference string always returns 'true'
     *
     *  \note We're using string& for the comparison (not estring&) for compatibility
     *  with other string sources
     *
     * \returns `true` if the string start with the reference string; `false` otherwise
     */
    bool startsWith(const string& ref    ///< the reference string used for the comparision
                       ) const;

    //----------------------------------------------------------------------------

    /** \brief Checks whether a string ends with a certain character sequence
     *
     *  Using an empty reference string always returns 'true'
     *
     *  \note We're using string& for the comparison (not estring&) for compatibility
     *  with other string sources
     *
     * \returns `true` if the string ends with the reference string; `false` otherwise
     */
    bool endsWith(const string& ref    ///< the reference string used for the comparision
                       ) const;

    //----------------------------------------------------------------------------

    /** \brief Removes all whitespace characters on the left; the string is modified in place
     *
     *  Uses std::isspace() for whitespace detection
     *
     * \returns *this
     */
    estring& trimLeft();

    //----------------------------------------------------------------------------

    /** \brief Removes all whitespace characters on the right; the string is modified in place
     *
     *  Uses std::isspace() for whitespace detection
     *
     * \returns *this
     */
    estring& trimRight();

    //----------------------------------------------------------------------------

    /** \brief Removes all whitespace characters on both ends of the string; the string is modified in place
     *
     *  Uses std::isspace() for whitespace detection
     *
     * \returns *this
     */
    estring& trim();

    //----------------------------------------------------------------------------

    /** \brief Returns a string with all whitespace characters on the left removed
     *
     *  Uses std::isspace() for whitespace detection
     *
     * \returns a copy of the string without whitespaces on the left
     */
    estring trimLeft_copy() const;

    //----------------------------------------------------------------------------

    /** \brief Returns a string with all whitespace characters on the right removed
     *
     *  Uses std::isspace() for whitespace detection
     *
     * \returns a copy of the string without whitespaces on the right
     */
    estring trimRight_copy() const;

    //----------------------------------------------------------------------------

    /** \brief Returns a string with all whitespaces removed on both ends
     *
     *  Uses std::isspace() for whitespace detection
     *
     * \returns a copy of the string without whitespaces on the left and right
     */
    estring trim_copy() const;

    //----------------------------------------------------------------------------

    /** \brief Converts an estring into a std::string
     *
     *  Creates a copy of the string
     *
     * \returns a copy of the string as std::string
     */
    string toStdString() const;

    //----------------------------------------------------------------------------

    /** \brief A special constructor that creates an estring from a vector of estrings and a delimiter string
     *
     *  The elements in the vector are concatenated using the delimiter string. The delimiter as well as the
     *  vector may be empty.
     *
     *  The delimiter is of type string& so that we can use std:string as well as estring for this parameter.
     */
    estring(const vector<estring>& parts,  ///< a vector containing the string parts
            const string& delim            ///< the delimiter string used for the concatenation of the parts
            );

    //----------------------------------------------------------------------------

    /** \brief Checks whether a certain substring is contained in the string
     *
     *  Using an empty reference string always returns 'true'
     *
     *  \note We're using string& for the comparison (not estring&) for compatibility
     *  with other string sources
     *
     * \returns `true` if the string contains the reference string; `false` otherwise
     */
    bool contains(const string& ref    ///< the reference string used for the search
                       ) const;

    //----------------------------------------------------------------------------

    /** \brief Replaces the first occurence of `key` with `value`; the string is modified in place
     *
     *  \note We're using string& for `key` and `value` for compatibility
     *  with other string sources
     *
     * \returns `true` if an replacement occurred, `false` otherwise;
     * \returns `false` if the `key` was empty
     */
    bool replaceFirst(const string& key,   ///< the string to search for
                      const string& value  ///< the string to replace `key` with
                      );

    //----------------------------------------------------------------------------

    /** \brief Replaces all occurences of `key` with `value`; the string is modified in place
     *
     *  \note We're using string& for `key` and `value` for compatibility
     *  with other string sources
     *
     * \returns `true` if an replacement occurred, `false` otherwise;
     * \returns `false` if the `key` was empty
     */
    bool replaceAll(const string& key,   ///< the string to search for
                    const string& value  ///< the string to replace `key` with
                    );

    //----------------------------------------------------------------------------

    /** \brief Replaces a section of a string with a new string; the string is modified in place
     *
     *  The new string is appended if idxFirst >= size(). If idxLast is beyond the string's end,
     *  everything starting at idxFirst replaced.
     *
     *  \note We're using string& for `s` for compatibility
     *  with other string sources
     *
     * \throws std::invalid_argument if idxLast < idxFirst
     *
     * \returns nothing
     */
    void replaceSection(size_type idxFirst,  ///< index of the first character to replace
                           size_type idxLast,   ///< index of the last character to replace
                           const string& s      ///< the string to insert
                           );
  };
}

#endif
