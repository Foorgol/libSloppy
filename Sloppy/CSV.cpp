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

#include <regex>
#include <algorithm>
#include <iostream>

#include <Sloppy/ConfigFileParser/ConstraintChecker.h>

#include "CSV.h"

using namespace std;


namespace Sloppy
{

  string escapeStringForCSV(const string& rawInput, bool addQuotes)
  {
    // replace all backslashes with a double backslash
    static const regex reBackslash{"\\\\"};  // matches a single backslash
    string result = regex_replace(rawInput, reBackslash, "\\\\");  // insert double backslash

    // replace " with \"
    static const regex reQuote{"\""}; // matches a single "
    result = regex_replace(result, reQuote, "\\\"");  // inserts \"

    // replace , with \,
    static const regex reComma{","}; // matches a single ','
    result = regex_replace(result, reComma, "\\,");  // inserts \,

    // replace newlines with a literal "\n"
    static const regex reNewline{"\n"}; // matches a single \n
    result = regex_replace(result, reNewline, "\\n");  // inserts a literal "\n"

    return (addQuotes ? "\"" + result + "\"" : result);
  }

  //----------------------------------------------------------------------------

  string unescapeStringForCSV(const string& escapedInput)
  {
    if (escapedInput.empty()) return string{};

    Sloppy::estring s{escapedInput};

    /*
    // remove literal, un-escaped quotation marks
    if (s[0] == '"') s = s.substr(1);
    if (s.empty()) return string{};
    if (s.size() >= 2)
    {
      auto lastIdx = s.size() - 1;
      if ((s[lastIdx] == '"') && (s[lastIdx-1] != '\\'))
      {
        s.chopRight(1);
      }
    }
    */

    // make sure there no un-escaped commas and backslashes in the string
    for (char c : {',', '\\'})
    {
      size_t idx = s.find(c);
      while (idx != string::npos)
      {
        if ((idx == 0) && (c != '\\')) throw std::invalid_argument("unescapeStringForCSV(): invalid input string");
        if ((c != '\\') && (idx > 0) && (s[idx - 1] != '\\')) throw std::invalid_argument("unescapeStringForCSV(): invalid input string");

        idx = s.find(c, idx+1);
      }
    }

    // unescape newlines
    static const regex reNewline{"\\\\n"};
    s = regex_replace(s, reNewline, "\n");

    // unescape commas
    static const regex reComma{"\\\\,"};
    s = regex_replace(s, reComma, ",");

    // unescape quotes
    static const regex reQuote{"\\\\\""};
    s = regex_replace(s, reQuote, "\"");

    // unescape backslashes
    static const regex reBackslash{"\\\\\\\\"};
    s = regex_replace(s, reBackslash, "\\");

    return s;
  }

  //----------------------------------------------------------------------------

  string CSV_Value::asString(CSV_StringRepresentation rep) const
  {
    const auto valType = valueType();

    const bool usesEscaping = ((rep == CSV_StringRepresentation::Escaped) || (rep == CSV_StringRepresentation::QuotedAndEscaped));
    const bool usesQuotes = ((rep == CSV_StringRepresentation::Quoted) || (rep == CSV_StringRepresentation::QuotedAndEscaped));

    // if strings shall not be quoted, we refuse to
    // deal with NULL values
    if ((valType == Type::Null) && !usesQuotes)
    {
      throw std::bad_optional_access();
    }

    switch (valType) {
    case Type::Null:
      return string{};

    case Type::Long:
      return to_string(std::get<long>(value()));

    case Type::Double:
      return to_string(std::get<double>(value()));
      break;

    case Type::String:
      string s = std::get<std::string>(value());
      if (usesEscaping) s = escapeStringForCSV(s, false);
      if (usesQuotes) s = "\"" + s + "\"";
      return s;
    }

    // we should never reach this point
    throw std::runtime_error("CSV_Value::asString(): conversion logic error!");
  }

  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------

  CSV_Row::CSV_Row(const Sloppy::estring& rowData, CSV_StringRepresentation rep)
  {
    const bool usesEscaping = ((rep == CSV_StringRepresentation::Escaped) || (rep == CSV_StringRepresentation::QuotedAndEscaped));
    const bool usesQuotes = ((rep == CSV_StringRepresentation::Quoted) || (rep == CSV_StringRepresentation::QuotedAndEscaped));

    const auto optStringChunks = splitInputInChunks(rowData, rep);

    for (const auto& optChunk : optStringChunks)
    {
      if (!optChunk.has_value())
      {
        cols.push_back(CSV_Value{});
        continue;
      }

      const estring& trimmed = optChunk->trim_copy();

      if (Sloppy::checkConstraint(trimmed, Sloppy::ValueConstraint::Integer))
      {
        long l = stol(trimmed);
        cols.push_back(CSV_Value{l});
        continue;
      }

      if (Sloppy::checkConstraint(trimmed, Sloppy::ValueConstraint::Numeric))
      {
        double d = stod(trimmed);
        cols.push_back(CSV_Value{d});
        continue;
      }

      //
      // so we have a string as last option.
      //

      // in a quoted string, the first and last
      // character must be pure, unescaped quotation marks
      const estring& s = *optChunk;
      if (usesQuotes && (s.size() < 2))
      {
        throw std::invalid_argument("CSV_Row::ctor(): received invalid input string (missing quotation marks)");
      }
      if (usesQuotes && ((s.front() != '"') || (s.back() != '"')))
      {
        throw std::invalid_argument("CSV_Row::ctor(): received invalid input string (missing quotation marks)");
      }
      if ((rep == CSV_StringRepresentation::QuotedAndEscaped) && (s.size() > 2) && (s[s.size() - 2] == '\\'))
      {
        throw std::invalid_argument("CSV_Row::ctor(): received invalid input string (escaped quotation mark instead of raw quotation mark)");
      }

      Sloppy::estring raw = usesEscaping ? Sloppy::estring{unescapeStringForCSV(s)} : s;

      if (usesQuotes)
      {
        raw = raw.substr(1);
        raw.chopRight(1);
      }

      cols.push_back(CSV_Value{raw});
    }
  }

  CSV_Row::ColumnConstRef CSV_Row::operator[](CSV_Row::IndexType idx) const
  {
    return cols.at(idx);
  }

  //----------------------------------------------------------------------------

  CSV_Row::ColumnConstRef CSV_Row::get(CSV_Row::IndexType idx) const
  {
    return cols.at(idx);
  }

  //----------------------------------------------------------------------------

  string CSV_Row::asString(CSV_StringRepresentation rep) const
  {
    if (cols.empty()) return std::string{};

    auto it = cols.cbegin();
    std::string result = it->asString(rep);
    ++it;
    for (; it != cols.cend(); ++it)
    {
      result += ',';
      result += it->asString(rep);
    }

    return result;
  }

  //----------------------------------------------------------------------------

  std::vector<std::optional<estring> > CSV_Row::splitInputInChunks(const estring& ctorInput, CSV_StringRepresentation rep)
  {
    const bool usesEscaping = ((rep == CSV_StringRepresentation::Escaped) || (rep == CSV_StringRepresentation::QuotedAndEscaped));
    const bool usesQuotes = ((rep == CSV_StringRepresentation::Quoted) || (rep == CSV_StringRepresentation::QuotedAndEscaped));

    std::vector<std::optional<estring> > result;
    if (ctorInput.empty()) return result;

    // find all valid comma separator positions
    vector<size_t> commaPos;
    char prevChar{0};
    int quoteCount{0};
    for (size_t idx=0; idx < ctorInput.size(); ++idx)
    {
      const char c = ctorInput[idx];

      // track if we're inside a quoted string section or not
      if (usesQuotes && (c == '"'))
      {
        // is this an "effective" quotation marks or is it an
        // escaped, literal quotation mark that doesn't have any effect?
        if ((usesEscaping && (prevChar != '\\')) || !usesEscaping)
        {
          ++quoteCount;

          if (quoteCount > 2)
          {
            throw std::invalid_argument("Inconsistent number of quotation marks in CSV input string");
          }
        }
      }

      if ((c == ',') && (quoteCount != 1))
      {
        if (!usesEscaping || (usesEscaping && (prevChar != '\\')))
        {
          // valid, field separating comma
          commaPos.push_back(idx);
          quoteCount = 0;
        }
      }

      prevChar = c;
    }

    // initialize column values from comma-delimited data
    size_t idxStart = 0;
    for (size_t idx : commaPos)
    {
      // row starts with a NULL value or
      // intermediate NULL value like "xxxx,,xxxx"
      if (idx == idxStart)
      {
        result.push_back(std::optional<estring>{});
      } else {
        // regular content field
        Sloppy::estring sVal = ctorInput.slice(idxStart, idx - 1);
        if (usesQuotes) sVal.trim();
        result.push_back(sVal.empty() ? std::optional<estring>{} : sVal);
      }

      // start position for next slice is one behind the comma
      idxStart = idx + 1;

    }

    // don't forget everything after the last comma
    if (idxStart < ctorInput.size())
    {
      Sloppy::estring sVal = ctorInput.slice(idxStart, ctorInput.size() - 1);
      if (usesQuotes) sVal.trim();
      result.push_back(sVal.empty() ? std::optional<estring>{} : sVal);
    }

    // if the row ended with a comma, we have to push
    // another NULL value
    if (idxStart == ctorInput.size())
    {
      result.push_back(std::optional<estring>{});
    }

    return result;
  }

  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------

  CSV_Table::CSV_Table()
  {}

  //----------------------------------------------------------------------------

  CSV_Table::CSV_Table(const estring& tableData, bool firstRowContainsHeaders, CSV_StringRepresentation rep)
  {
    if (tableData.empty()) return;

    auto lines = tableData.split("\n", false, false);

    for (auto& line : lines)
    {
      // remove trailing "\r" characters
      if (line.back() == '\r') line.chopRight(1);

      // don't catch exceptions here; leave it to
      // the caller to deal with invalid row data
      CSV_Row r{line, rep};

      // is this the header row?
      if (firstRowContainsHeaders && (colCount == 0))
      {
        bool isOkay = setHeader(r);
        if (!isOkay)
        {
          throw std::invalid_argument("CSV_Table ctor: invalid header data");
        }

        continue;
      }

      // try to append the row
      bool isOkay = append(std::move(r));
      if (!isOkay)
      {
        throw std::invalid_argument("CSV_Table ctor: inconsistent column count across data rows");
      }
    }
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::append(const CSV_Row& row)
  {
    // is this the first row in an empty table?
    if (colCount == 0)
    {
      colCount = row.size();
    }

    // is the number of columns correct?
    if (row.size() != colCount) return false;

    rows.push_back(row);

    return true;
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::append(CSV_Row&& row)
  {
    // is this the first row in an empty table?
    if (colCount == 0)
    {
      colCount = row.size();
    }

    // is the number of columns correct?
    if (row.size() != colCount) return false;

    rows.push_back(std::move(row));

    return true;
  }

  //----------------------------------------------------------------------------

  string CSV_Table::getHeader(CSV_Table::ColumnIndexType colIdx) const
  {
    auto it = findHeaderForColumnIndex_const(colIdx);

    if (it == header2columnIndex.cend())
    {
      throw std::out_of_range("CSV_Table::getHeader(): invalid column index");
    }

    return it->first;
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::setHeader(CSV_Table::ColumnIndexType colIdx, const estring& newHeader)
  {
    auto s = newHeader.trim_copy();

    // check header conditions
    if (!isValidHeader(s)) return false;

    // find the map entry for the column
    auto it = findHeaderForColumnIndex(colIdx);
    if (it == end(header2columnIndex)) return false;  // invalid index

    // update the entry
    header2columnIndex.erase(it);
    header2columnIndex.emplace(std::move(s), colIdx);

    return true;
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::setHeader(const std::vector<string>& headers)
  {
    std::vector<std::string> v;
    for (const auto& h : headers)
    {
      estring e{h};
      e.trim();
      v.push_back(e);
    }

    return setHeader_trimmed(v);
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::setHeader(const std::vector<estring>& headers)
  {
    std::vector<std::string> v;
    for (const auto& h : headers) v.push_back(h.trim_copy());

    return setHeader_trimmed(v);
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::setHeader(const CSV_Row& headers)
  {
    std::vector<std::string> v;

    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
    {
      if (it->valueType() == CSV_Value::Type::Null) return false;
      estring s = it->asString(CSV_StringRepresentation::Plain);
      s.trim();
      v.push_back(s);
    }

    return setHeader_trimmed(v);
  }

  //----------------------------------------------------------------------------

  CSV_Table::ElementConstRef CSV_Table::get(CSV_Table::RowIndexType rowIdx, CSV_Table::ColumnIndexType colIdx) const
  {
    return rows.at(rowIdx).get(colIdx);
  }

  //----------------------------------------------------------------------------

  CSV_Table::ElementConstRef CSV_Table::get(CSV_Table::RowIndexType rowIdx, const string& colName) const
  {
    auto colIdx = header2columnIndex.at(colName);
    return get(rowIdx, colIdx);
  }

  //----------------------------------------------------------------------------

  CSV_Table::RowConstRef CSV_Table::get(CSV_Table::RowIndexType rowIdx) const
  {
    return rows.at(rowIdx);
  }

  //----------------------------------------------------------------------------

  string CSV_Table::asString(bool includeHeaders, CSV_StringRepresentation rep) const
  {
    const bool usesEscaping = ((rep == CSV_StringRepresentation::Escaped) || (rep == CSV_StringRepresentation::QuotedAndEscaped));
    const bool usesQuotes = ((rep == CSV_StringRepresentation::Quoted) || (rep == CSV_StringRepresentation::QuotedAndEscaped));

    string result;

    if (includeHeaders && !header2columnIndex.empty())
    {
      for (ColumnIndexType colIdx = 0; colIdx < header2columnIndex.size(); ++colIdx)
      {
        string hdr = getHeader(colIdx);
        if (usesEscaping) hdr = escapeStringForCSV(hdr, usesQuotes);

        if (!result.empty()) result += ",";
        result += hdr;
      }

      result += "\n";
    }

    for (const auto& row : rows)
    {
      result += row.asString(rep);
      result += "\n";
    }

    return result;
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::eraseColumn(CSV_Table::ColumnIndexType colIdx)
  {
    if (colCount == 0) return false;
    if (colIdx >= colCount) return false;

    // iterate over the rows, delete the requested
    // column in each row
    for (auto& row : rows)
    {
      auto it = row.begin();
      std::advance(it, colIdx);
      row.erase(it);
    }

    // if we have headers, delete the header as well
    if (!header2columnIndex.empty())
    {
      auto it = findHeaderForColumnIndex(colIdx);
      header2columnIndex.erase(it);

      // shift all following header indices by one to the left
      for (ColumnIndexType idx = colIdx + 1; idx < colCount; ++idx)
      {
        auto it2 = findHeaderForColumnIndex(idx);
        header2columnIndex[it2->first] = idx - 1;
      }
    }

    --colCount;

    return true;
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::eraseColumn(const string& colName)
  {
    if (header2columnIndex.empty()) return false;

    ColumnIndexType colIdx{0};
    try
    {
      colIdx = header2columnIndex.at(colName);
    }
    catch (std::out_of_range&)
    {
      return false;
    }

    return eraseColumn(colIdx);
  }

  //----------------------------------------------------------------------------

  CSV_Table::HeaderMap::iterator CSV_Table::findHeaderForColumnIndex(CSV_Table::ColumnIndexType colIdx)
  {
    return std::find_if(
          begin(header2columnIndex), end(header2columnIndex),
          [&](std::unordered_map<std::string, ColumnIndexType>::const_reference p)
    {
        return (p.second == colIdx);
    });
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::isValidHeader(const string& s) const
  {
    // headers shall be non-empty
    if (s.empty()) return false;

    // headers may not contain commas or quotation marks
    const string invalidChars{",\""};
    auto it = std::find_first_of(
          s.cbegin(), s.cend(),
          invalidChars.cbegin(), invalidChars.cend()
          );
    if (it != s.cend()) return false;

    // headers must be unique
    auto it2 = header2columnIndex.find(s);
    if (it2 != header2columnIndex.cend()) return false;

    return true;
  }

  //----------------------------------------------------------------------------

  bool CSV_Table::setHeader_trimmed(const std::vector<string>& headers)
  {
    if ((colCount > 0) && (headers.size() != colCount))
    {
      return false;
    }

    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
    {
      const string& hdr = *it;

      // all headers must be non-empty
      if (hdr.empty()) return false;

      // all headers must be unique
      // ==> since we already iterate over the headers it means
      // that the same header may not occur AFTER the
      // the current element
      auto itOther = std::find(it + 1, headers.cend(), hdr);
      if (itOther != headers.cend()) return false;

      // headers may not contain baaad characters
      const string invalidChars{",\""};
      auto itDummy = std::find_first_of(
            hdr.cbegin(), hdr.cend(),
            invalidChars.cbegin(), invalidChars.cend()
            );
      if (itDummy != hdr.cend()) return false;
    }

    // everything is okay. copy the headers over
    header2columnIndex.clear();
    for (ColumnIndexType idx=0; idx < headers.size(); ++idx)
    {
      header2columnIndex.emplace(headers.at(idx), idx);
    }

    if (colCount == 0) colCount = headers.size();

    return true;
  }

  //----------------------------------------------------------------------------

  CSV_Table::HeaderMap::const_iterator CSV_Table::findHeaderForColumnIndex_const(ColumnIndexType colIdx) const
  {
    return std::find_if(
          header2columnIndex.cbegin(), header2columnIndex.cend(),
          [&](std::unordered_map<std::string, ColumnIndexType>::const_reference p)
    {
        return (p.second == colIdx);
    });
  }

  //----------------------------------------------------------------------------



  //----------------------------------------------------------------------------

}
