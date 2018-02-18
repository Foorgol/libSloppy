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
#include <cctype>
#include <algorithm>

#include "String.h"

namespace Sloppy
{

  estring estring::slice(estring::size_type idxFirst, estring::size_type idxLast) const
  {
    if (idxLast < idxFirst)
    {
      throw std::invalid_argument("estring::slice: inconsistent indices");
    }

    if (idxFirst >= size()) return estring{};

    return (idxLast == string::npos) ? substr(idxFirst) : substr(idxFirst, idxLast - idxFirst + 1);
  }

  //----------------------------------------------------------------------------

  estring estring::right(estring::size_type n) const {
    if (n == 0) return estring{};
    return (n >= length()) ? *this : estring{std::move(substr(length() - n, n))};
  }

  //----------------------------------------------------------------------------

  estring estring::left(estring::size_type n) const {
    if (n == 0) return estring{};
    return (n >= length()) ? *this : estring{std::move(substr(0, n))};
  }

  //----------------------------------------------------------------------------

  estring& estring::chopRight(estring::size_type n)
  {
    if (n == 0) return *this;

    if (n >= size())
    {
      clear();
    } else {
      erase(length() - n, n);
    }

    return *this;
  }

  //----------------------------------------------------------------------------

  estring& estring::chopLeft(estring::size_type n)
  {
    if (n == 0) return *this;

    if (n >= size())
    {
      clear();
    } else {
      erase(0, n);
    }

    return *this;
  }

  //----------------------------------------------------------------------------

  estring estring::chopRight_copy(estring::size_type n) const
  {
    if (n == 0) return *this;

    if (n >= size()) return estring{};

    return substr(0, size() - n);
  }

  //----------------------------------------------------------------------------

  estring estring::chopLeft_copy(estring::size_type n) const
  {
    if (n == 0) return *this;

    if (n >= size()) return estring{};

    return substr(n);
  }

  //----------------------------------------------------------------------------

  bool estring::startsWith(const string& ref) const
  {
    return (compare(0, ref.length(), ref) == 0);
  }

  //----------------------------------------------------------------------------

  bool estring::endsWith(const string& ref) const
  {
    if (ref.size() > size()) return false;

    return (compare(size() - ref.size(), ref.length(), ref) == 0);
  }

  //----------------------------------------------------------------------------

  estring& estring::trimLeft()
  {
    // find the first non-whitespace character
    auto it = std::find_if_not(cbegin(), cend(), [](const char& c) {
      return isspace(static_cast<unsigned char>(c));
    });

    if (it == cbegin()) return *this;  // nothing to trim
    if (it == cend())
    {
      // the string consists only of white spaces
      clear();
      return *this;
    }

    // keep everything starting from 'it'
    erase(begin(), it);
    return *this;
  }

  //----------------------------------------------------------------------------

  estring& estring::trimRight()
  {
    // find the first non-whitespace character
    auto it = std::find_if_not(crbegin(), crend(), [](const char& c) {
      return isspace(static_cast<unsigned char>(c));
    });

    if (it == crbegin()) return *this;  // nothing to trim
    if (it == crend())
    {
      // the string consists only of white spaces
      clear();
      return *this;
    }

    // keep everything starting from 'it'
    erase(it.base(), end());
    return *this;
  }

  //----------------------------------------------------------------------------

  estring& estring::trim()
  {
    trimLeft();
    trimRight();
    return *this;
  }

  //----------------------------------------------------------------------------

  estring estring::trimLeft_copy() const
  {
    estring tmp{*this};
    tmp.trimLeft();
    return tmp;
  }

  //----------------------------------------------------------------------------

  estring estring::trimRight_copy() const
  {
    estring tmp{*this};
    tmp.trimRight();
    return tmp;
  }

  //----------------------------------------------------------------------------

  estring estring::trim_copy() const
  {
    estring tmp{*this};
    tmp.trim();
    return tmp;
  }

  //----------------------------------------------------------------------------

  string estring::toStdString() const
  {
    return string{*this};
  }

  //----------------------------------------------------------------------------

  estring::estring(const vector<estring>& parts, const string& delim)
    :estring()   // initialize as an empty string
  {
    for (const estring& p : parts)
    {
      if (!empty())
      {
        append(delim);
      }
      append(p);
    }
  }

  //----------------------------------------------------------------------------

  bool estring::contains(const string& ref) const
  {
    if (ref.empty()) return true;
    return (find(ref) != string::npos);
  }

  //----------------------------------------------------------------------------

  bool estring::replaceFirst(const string& key, const string& value)
  {
    if (empty()) return false;
    if (key.empty()) return false;

    // find first occurence of "key"
    size_type startPos = find(key);
    if (startPos == string::npos) return false;

    // replace this first occurence with the new value
    replace(startPos, key.length(), value);
    return true;
  }

  //----------------------------------------------------------------------------

  bool estring::replaceAll(const string& key, const string& value)
  {
    bool hadReplacement{false};

    while (replaceFirst(key, value))
    {
      hadReplacement = true;
    }

    return hadReplacement;
  }

  //----------------------------------------------------------------------------

  void estring::replaceSection(estring::size_type idxFirst, estring::size_type idxLast, const string& s)
  {
    if (idxLast < idxFirst)
    {
      throw std::invalid_argument("estring::replaceSection: inconsistent indices");
    }

    if (idxFirst >= size())
    {
      append(s);
      return;
    }

    if (idxLast >= size()) idxLast = size() - 1;

    // erase the requried section
    erase(idxFirst, idxLast - idxFirst + 1);

    // insert the new string at the required position
    insert(idxFirst, s);
  }





}
