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

#include <algorithm>  // for all_of, find_if_not, for_each, max, count
#include <cctype>     // for isdigit, isspace, tolower, toupper
#include <stdexcept>  // for invalid_argument

#include "String.h"

using namespace std;

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
    return (n >= length()) ? *this : estring{substr(length() - n, n)};
  }

  //----------------------------------------------------------------------------

  estring estring::left(estring::size_type n) const {
    if (n == 0) return estring{};
    return (n >= length()) ? *this : estring{substr(0, n)};
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
    if (empty()) return false;
    if (key.empty()) return false;

    //
    // the following implementation is faster than
    // iteratively calling replaceFirst. The reason is
    // that here we traverse the string only once while
    // we would always start searching from the beginning
    // if we would call replaceFirst again and again...
    //

    size_type idxFirst = find(key);
    if (idxFirst == string::npos) return false;

    while (idxFirst != string::npos)
    {
      // replace key with value
      replace(idxFirst, key.length(), value);

      // the next starting point for the search
      // is idxFirst + length of the inserted string
      idxFirst += value.length();

      // search for the next occurence
      idxFirst = find(key, idxFirst);
    }

    return true;
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

  //----------------------------------------------------------------------------

  void estring::toUpper()
  {
    // normal uppercase conversion
    for_each(begin(), end(), [](char& c) {
      c = toupper(static_cast<unsigned char>(c));
    });

    // additional conversions for multibyte UTF-8 characters
    for (const pair<string, string>& umlaut : umlautTranslationTable)
    {
      replaceAll(umlaut.first, umlaut.second);
    }
  }

  //----------------------------------------------------------------------------

  void estring::toLower()
  {
    // normal lowercase conversion
    for_each(begin(), end(), [](char& c) {
      c = tolower(static_cast<unsigned char>(c));
    });

    // additional conversions for multibyte UTF-8 characters
    for (const pair<string, string>& umlaut : umlautTranslationTable)
    {
      replaceAll(umlaut.second, umlaut.first);
    }
  }

  //----------------------------------------------------------------------------

  void estring::arg(const string& s)
  {
    // get all tags in the string
    auto [allTags, lowestArgNum] = findAllArgTags();
    if ((lowestArgNum == TagData::NotFound) || (allTags.empty()))
    {
      return;   // string doesn't contain any tags
    }

    //
    // We could now delete unnecessary tags elements from
    // the vector or sort the vector by arg num.
    //
    // But all this would require data movements and/or
    // memory allocation / de-allocation so I think
    // we're better of by just looping over all tags
    // and skip the tags we don't need.
    //

    // calculate the size of the target string
    int sizeDiff{0};
    for (const auto& tag : allTags)
    {
      if (tag.val != lowestArgNum) continue;

      // we remove the tag and insert the substitution string
      //
      // the tag len can vary, because "%0001" and "%1" are
      // technically equal
      sizeDiff = sizeDiff - tag.len + s.size();
    }

    // prepare a temporary string that is sufficiently long
    // to store the complete target string
    estring tmp;
    tmp.reserve(size() + sizeDiff);

    // iterative concatenation of source string fragments
    // and the replacement string
    size_t srcPos{0};
    for (const auto& tag : allTags)
    {
      if (tag.val != lowestArgNum) continue;

      // append everything before the tag
      if (srcPos < static_cast<size_t>(tag.idxStart))
      {
        tmp.append(slice(srcPos, tag.idxStart - 1));
      }

      // append the replacement string
      tmp.append(s);

      // fast-forward to one character after the tag
      srcPos = tag.idxEnd + 1;
    }

    // append everything after the last tag
    //
    // the call does no harm if srcPos points after the string's
    // end (---> the string ended with a tag)
    tmp.append(substr(srcPos));

    // replace our data with the temp string
    operator=(std::move(tmp));


    /*
     * Old, slow implementation based on regex
     */

    /*
    // determine the lowest argument index
    regex re{R"(%(\d+))"};
    sregex_iterator begin{cbegin(), cend(), re};
    for (auto it = begin; it != sregex_iterator{}; ++it)
    {
      int argIdx = stoi((*it)[1]);  // will never throw because we matched only "%<digit>"
      if (argIdx < 0) continue;
      if (argIdx < minArg) minArg = argIdx;
    }

    // search / replace the argument with the lowest index
    if (minArg != NotFound)
    {
      string key = "%" + to_string(minArg);
      replaceAll(key, s);
    }
    */
  }

  //----------------------------------------------------------------------------

  void estring::arg(const double& d, int numDigits, char fillChar)
  {
    string fmt = "%";
    if (numDigits >=0) fmt += "." + to_string(numDigits);
    fmt += "f";

    arg2<double>(d, fmt);
  }

  //----------------------------------------------------------------------------

  bool estring::isInt() const
  {
    if (empty()) return false;

    auto start = cbegin();

    // check for signed numbers
    if (operator [](0) == '-')
    {
      ++start;
      if (size() < 2) return false;  // we need more than just '-'
    }

    // make sure that all characters are numbers
    return all_of(start, cend(), [](const char& c) {
      return isdigit(static_cast<unsigned int>(c));
    });
  }

  //----------------------------------------------------------------------------

  bool estring::isDouble() const
  {
    if (empty()) return false;

    auto start = cbegin();

    // check for signed numbers
    if (operator [](0) == '-')
    {
      ++start;
      if (size() < 2) return false;  // we need more than just '-'
    }

    // some special cases
    if (*this == ".") return false;
    if (*this == "-.") return false;
    if (*this == ".-") return false;

    // we may have exactly zero or one decimal points
    auto nDots = count(cbegin(), cend(), '.');
    if (nDots > 1) return false;

    // make sure that all characters are numbers or dot
    return all_of(start, cend(), [](const char& c) {
      return (isdigit(static_cast<unsigned int>(c)) || (c == '.'));
    });
  }

  //----------------------------------------------------------------------------

  vector<estring> estring::split(const string& delim, bool keepEmptyParts, bool trimParts) const
  {
    vector<estring> result;
    if (empty()) return result;

    if (delim.empty())
    {
      throw std::invalid_argument("estring::split: called with empty delimiter string!");
    }

    // iterate over the string segments, delimiter by delimiter
    size_t nextStartPos = 0;
    while (nextStartPos < length())
    {
      size_t nextDelimPos = find(delim, nextStartPos);
      if (nextDelimPos == string::npos) break;

      estring s;
      if (nextDelimPos > nextStartPos)
      {
        // there is content between two delimiters; extract
        // the content
        s = slice(nextStartPos, nextDelimPos - 1);
        if (trimParts) s.trim();
      }
      // else: we have two directly subsequent delimiters (e.g., ",,")
      // and thus we simply use an empty string

      nextStartPos = nextDelimPos + delim.length();

      if (s.empty() && !keepEmptyParts) continue;

      result.push_back(s);
    }

    // the last section between the last delimiter and the string end
    if (nextStartPos < length())
    {
      estring s = substr(nextStartPos);
      if (trimParts) s.trim();
      if (keepEmptyParts || s.notEmpty()) result.push_back(s);
    }

    // if the string terminated with a delimiter
    // push an empty string
    if ((nextStartPos == length()) && keepEmptyParts)
    {
      result.push_back(estring{});
    }

    return result;
  }

  //----------------------------------------------------------------------------

  std::tuple<std::vector<estring::TagData>, int> estring::findAllArgTags() const
  {
    // reserve space for 20 tags; that should fit to most
    // use cases and avoids frequent re-allocations
    vector<TagData> allTags;
    allTags.reserve(20);
    int lowestArg{TagData::NotFound};

    // helper variables for the iteration of all characters
    TagData curTag;

    // a helper function that closes the current tag data element
    // at a given index position
    auto closeCurrentTag = [&](size_t idxEnd)
    {
      curTag.idxEnd = idxEnd;
      curTag.len = idxEnd - curTag.idxStart + 1;

      const string sVal = this->substr(curTag.idxStart + 1, curTag.len - 1);
      curTag.val = stoi(sVal);
      allTags.push_back(curTag);

      // keep track of the lowest arg number
      if (curTag.val < lowestArg) lowestArg = curTag.val;
    };

    // determine the lowest argument index
    for (size_t idx=0; idx < size(); ++idx)
    {
      bool isInTag = (curTag.idxStart >= 0);

      const auto& c = at(idx);

      // deal with one or more '%' in a row
      if (isInTag && (c == '%') && (idx == (curTag.idxStart + 1)))
      {
        curTag.idxStart = idx;
        continue;
      }

      // deal with a '%' that isn't followed by a number
      if (isInTag && (idx == (curTag.idxStart + 1)) && ((c < '0') || (c > '9')))
      {
        curTag.idxStart = -1;  // cancel the current tag
        continue;
      }

      // end of the current tag
      if (isInTag && ((c < '0') || (c > '9')))
      {
        closeCurrentTag(idx - 1);

        // reset the current tag
        curTag = TagData{};  // reset

        //
        // DON'T break here by calling `continue`!!
        //
        // We need to further evaluate the character
        // in order to deal with two subsequent tags
        // like in "%1%2".
        //
        isInTag = false;
      }

      // tag continues (we're on the tag number
      if (isInTag) continue;

      // normal string parts, not tags
      if (!isInTag && (c != '%')) continue;

      // start of a new tag
      if (!isInTag && (c == '%'))
      {
        curTag.idxStart = idx;
      }
    }

    // if the string ended with a tag, close the
    // current tag
    if (curTag.idxStart >= 0)
    {
      closeCurrentTag(size() - 1);
    }

    return std::tuple{std::move(allTags), lowestArg};
  }
}

//----------------------------------------------------------------------------


