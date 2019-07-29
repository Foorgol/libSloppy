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

#ifndef __LIBSLOPPY_CSV_H
#define __LIBSLOPPY_CSV_H

#include <vector>
#include <variant>
#include <string>
#include <optional>
#include <unordered_map>

#include "String.h"

namespace Sloppy
{
  /** \brief Escape special characters (e.g., commas) in a raw string so that the raw
   * string can be savely used as a cell value in a CSV representation.
   *
   * \returns the escaped (and optionally quoted) input string
   */
  std::string escapeStringForCSV(
      const std::string& rawInput,   ///< the raw input string that shall be escaped
      bool addQuotes   ///< set to `true` to add verbatim quotation marks at the begin and end of the result string
      );

  //----------------------------------------------------------------------------

  /** \brief Un-escapes a previously escaped CSV string so that we're back to the original
   * raw string.
   *
   * \warning The input string is NOT trimmed before processing! Leading or trailing white spaces
   * will prevent the detection of to-be-removed quotation marks!
   *
   * \throws std::invalid_argument if there are any un-escaped commas, quotation marks or
   * backslashes in the input string.
   *
   * \returns the un-escaped input string
   */
  std::string unescapeStringForCSV(
      const std::string& escapedInput   ///< the escaped input string that shall be reverted
      );

  //----------------------------------------------------------------------------

  /** \brief An enum that defines how string data is represented
   * in CSV texts.
   */
  enum class CSV_StringRepresentation
  {
    Plain,   ///< unescaped, unquoted
    Quoted,   ///< unescaped, wrapped in quotation marks
    Escaped,   ///< escaped, no quotation marks
    QuotedAndEscaped   ///< escaped and wrapped in quotation marks
  };

  //----------------------------------------------------------------------------

  /** \brief A single value in a (CSV-)table that can hold either
   * a string, a long integer or a double; it can also be empty (NULL)
   */
  class CSV_Value : public std::optional<std::variant<long, double, std::string>>
  {
  public:
    using InnerType = std::variant<long, double, std::string>;

    /** \brief An enum that represents the index of the different
     * value types in the underlying std::variant
     */
    enum class Type
    {
      Long = 0,
      Double = 1,
      String = 2,

      Null = -1
    };

    /** \brief Ctor for an empty NULL value
     */
    CSV_Value() noexcept
      : std::optional<std::variant<long, double, std::string>>{} {}

    /** \brief Ctor for a long int value
     */
    explicit CSV_Value(const long& l) noexcept
      : std::optional<std::variant<long, double, std::string>>{l} {}

    /** \brief Ctor for a standard int value
     */
    explicit CSV_Value(const int& i) noexcept
      : std::optional<std::variant<long, double, std::string>>{static_cast<long>(i)} {}

    /** \brief Ctor for a double value
     */
    explicit CSV_Value(const double& d) noexcept
      : std::optional<std::variant<long, double, std::string>>{d} {}

    /** \brief Ctor for a string value (creates string copy)
     */
    explicit CSV_Value(
        const std::string& s   ///< the raw, unescaped, unquoted string (means: the actual user level data)
        ) noexcept
      : std::optional<std::variant<long, double, std::string>>{s} {}

    /** \brief Ctor for a string value (moves string content)
     */
    explicit CSV_Value(
        std::string&& s   ///< the raw, unescaped, unquoted string (means: the actual user level data)
        ) noexcept
      : std::optional<std::variant<long, double, std::string>>{std::move(s)} {}

    /** \brief Overrides the contained value with a new value
     */
    template<typename T>
    void set(const T& val)
    {
      if (has_value())
      {
        value().emplace<T>(val);
      } else {
        emplace(InnerType{val});
      }
    }

    /** \brief Overrides the contained value with a new value; convenience function
     * that implies a static_cast from int to long.
     */
    void set(const int& i)
    {
      set(static_cast<const long&>(i));
    }

    /** \brief Overrides the contained value with a new value; convenience function
     * that implies a static_cast from a raw C string pointer to a C++ string
     */
    void set(const char* s)
    {
      set(std::string{s});
    }

    /** \brief Overrides the contained value with a new value; moves string
     * instead of creating a copy
     */
    void set(
        std::string&& s   ///< the raw, unescaped, unquoted string (means: the actual user level data)
        )
    {
      if (has_value())
      {
        value().emplace<std::string>(std::move(s));
      } else {
        emplace(InnerType{std::move(s)});
      }
    }

    /** \brief Set the contained value to NULL
     */
    void set()
    {
      reset();
    }

    /** \brief Direct access to the underlying value without
     * dereferencing via "value()" first.
     *
     * \warning We do not perform any type conversion here! We only
     * return the exact value type (long, double or string) that has
     * been provided during construction or `set()`.
     *
     * \throws bad_optional_access if the contained value is empty (NULL)
     *
     * \throws bad_variant_access if the contained value is not of type T
     */
    template<typename T>
    const T& get() const
    {
      return std::get<T>(value());
    }

    /** \returns the currently stored value type
     */
    Type valueType() const
    {
      if (has_value())
      {
        return static_cast<Type>(value().index());
      }

      return Type::Null;
    }

    /** \return a string representation of the currently contained value
     *
     * If quoting is requested, a NULL value is returned as an empty string
     * while an empty string is returned as `""`.
     *
     * Numeric contents are never wrapped in quotation marks.
     *
     * \throws bad_optional_access if the contained value is NULL and
     * no string quoting is requested. Reason: in this case you can't differ between
     * NULL and empty strings.
     */
    std::string asString(CSV_StringRepresentation rep) const;

  private:
  };

  //----------------------------------------------------------------------------

  /** \brief A vector of CSV_Value elements, representing a row in a CSV table
   */
  class CSV_Row
  {
  public:
    using ContainerType = std::vector<CSV_Value>;
    using ColumnConstRef = ContainerType::const_reference;
    using IndexType = ContainerType::size_type;

    /** \brief Default ctor for an empty row
     */
    CSV_Row() noexcept {}

    /** \brief Constructs a row from a single string that consists of
     * comma-separated values.
     *
     * Subsequent commas (",,") will be treated as NULL value.
     *
     * If string data is quoted, all data chunks will be trimmed and empty
     * strings will be treated as NULL, too. Example: the input row "42,  ,66" will
     * be converted into NULL value for the second column if strings are quoted. It
     * will be converted into the string "   " for the second column if string
     * columns are not quoted.
     *
     * \throws std::invalid_argument if the input string was malformed (e.g.,
     * because of inconsistent use of quotation marks).
     *
     */
    explicit CSV_Row(
        const Sloppy::estring& rowData,
        CSV_StringRepresentation rep   ///< defines how string data is represented in the input string
        );

    /** \return a reference to the CSV_Value stored in a given column
     *
     * \throws std::out_of_range if the provided index is invalid
     */
    ColumnConstRef operator[](
        IndexType idx   ///< zero-based index of the requested column
        ) const;

    /** \returns a reference to the CSV_Value in at a given index
     */
    ColumnConstRef get(
        IndexType idx   ///< zero-based index of the requested column
        ) const;

    /** \returns the number of columns in the row
     */
    IndexType size() const { return cols.size(); }

    bool empty() const { return cols.empty(); }

    /** \brief Appends a new column to the row
     *
     * The parameter type must be compatible / convertible
     * to `long`, `double` or `string` because otherwise we
     * can't construct a new `CSV_Value` for the column.
     */
    template<typename T>
    void append(const T& v)
    {
      cols.push_back(CSV_Value{v});
    }

    /** \brief Appends a new column to the row
     */
    void append(std::string&& v)
    {
      cols.push_back(CSV_Value{std::move(v)});
    }

    /** \brief Appends a new column with a NULL value
     */
    void append()
    {
      cols.push_back(CSV_Value{});
    }

    /** \brief Converts the row to a CSV string
     *
     * \throws bad_optional_access if the contained value is NULL and
     * no string quoting is requested. Reason: in this case you can't differ between
     * NULL and empty strings.
     *
     * \return A CSV string representing the row data
     */
    std::string asString(
        CSV_StringRepresentation rep   ///< defines how string data is represented in the output string
        ) const;

    ContainerType::const_iterator cbegin() const { return cols.cbegin(); }
    ContainerType::const_iterator cend() const { return cols.cend(); }

    ContainerType::iterator begin() { return cols.begin(); }
    ContainerType::iterator end() { return cols.end(); }

    /** \brief Erases a data column; wrapper for the `erase()` method of the underlying data vector.
     */
    ContainerType::iterator erase(
        ContainerType::iterator it   ///< iterator pointing on the to-be-deleted row
        ) { return cols.erase(it); }

    /** \brief Erases a consecutive set of data columns; wrapper for the `erase()` method of the underlying data vector.
     */
    ContainerType::iterator erase(
        ContainerType::iterator first,   ///< first row to be deleted
        ContainerType::iterator last   ///< one-past-the-last-row to be deleted
        )
    { return cols.erase(first, last); }

  protected:
    /** \brief Used internally by the ctor to split the input string in
     * chunks of data
     *
     * Subsequent commas (",,") will be treated as NULL value.
     *
     * If string data is quoted, all data chunks will be trimmed and empty
     * strings will be treated as NULL, too. Example: the input row "42,  ,66" will
     * be converted into NULL value for the second column if strings are quoted. It
     * will be converted into the string "   " for the second column if string
     * columns are not quoted.
     *
     * \return a vector of optional strings; empty optionals represent NULL.
     */
    std::vector<std::optional<Sloppy::estring>> splitInputInChunks(
        const Sloppy::estring& ctorInput,
        CSV_StringRepresentation rep   ///< defines how string data is represented in the input string
        );

  private:
    std::vector<CSV_Value> cols;
  };

  //----------------------------------------------------------------------------

  /** \brief A vector of CSV_Rows, representing a CSV table.
   *
   * The class enforces that all rows have the same number of columns. It also
   * allows for accessing column by names and not only by indices.
   */
  class CSV_Table
  {
  public:
    using ContainerType = std::vector<CSV_Row>;
    using ElementConstRef = CSV_Row::ColumnConstRef;
    using RowIndexType = ContainerType::size_type;
    using ColumnIndexType = CSV_Row::IndexType;
    using RowConstRef = ContainerType::const_reference;
    using HeaderMap = std::unordered_map<std::string, ColumnIndexType>;

    /** \brief Default ctor, constructs an empty table without headers or data.
     *
     * The first appended row determins the number of columns for the table
     */
    CSV_Table();

    /** \brief Ctor that parses a given string as a CSV table
     *
     * Rows MUST be terminated by a single newline character "\n".
     * A potential trailing "\r" (from a "\r\n" line break) will be
     * removed before processing.
     *
     * Empty rows will be ignored. Line data is NOT trimmed before
     * CSV processing.
     *
     * For header names, NULL values are not permitted. Non-string data will be converted to
     * string using `std::to_string()`. All header names will be trimmed.
     * They have to be unique and non-empty and may not contain commas or
     * quotation marks.
     *
     * \throws std::invalid_argument if the input string was malformed (e.g.,
     * because of inconsistent use of quotation marks).
     *
     * \throws std::invalid_argument if the number of columns is inconsistent
     * between rows.
     *
     * \throws std::invalid_argument if any of the header naming conditions
     * is violated by the input data.
     */
    CSV_Table(
        const Sloppy::estring& tableData,   ///< the string that shall be parsed
        bool firstRowContainsHeaders,   ///< if true then the first row will be treated as column headers
        CSV_StringRepresentation rep   ///< defines how string data is represented in the input string
        );

    /** \brief Appends a new row to the table
     *
     * If you're appending a new row to an empty table, this first row
     * determines required the number of columns for all subsequent rows
     *
     * \returns `true` if the row was appended successfully, `false`
     * otherwise (i.e., because the number of columns didn't match).
     */
    bool append(
        const CSV_Row& row   ///< the row that shall be appended (copied) to the table
        );

    /** \brief Appends a new row to the table
     *
     * If you're appending a new row to an empty table, this first row
     * determines required the number of columns for all subsequent rows.
     *
     * The move operation is only executed if the number of columns in the
     * row is correct. If not, the provided row remains untouched.
     *
     * \returns `true` if the row was appended successfully, `false`
     * otherwise (i.e., because the number of columns didn't match).
     */
    bool append(
        CSV_Row&& row   ///< the row that shall be appended (moved) to the table
        );

    /** \returns the number of columns in the table (0 if the table
     * is still empty)
     */
    ColumnIndexType nCols() const { return colCount; }

    /** \returns the number of data rows in the table
     */
    RowIndexType size() const { return rows.size(); }

    /** \returns `true` if the table contains column headers
     */
    bool hasHeaders() const { return !header2columnIndex.empty(); }

    /** \returns `true' if the table does not contain any DATA rows (headers are
     * not relevant to this function's return value).
     */
    bool empty() const { return rows.empty(); }

    /** \returns the header for a given column index (empty if headers
     * are not defined).
     *
     * \throws std::out_of_range if the provided column index was invalid
     */
    std::string getHeader(
        ColumnIndexType colIdx   ///< the zero-based index of the column
        ) const;

    /** \brief Sets a new header name for a given column index.
     *
     * The header name will be trimmed and may not be empty. It must be
     * unique. The comparison is done case-sensitive.
     *
     * \returns 'true' if the header was updated successfully, `false`
     * otherwise (e.g., invalid index, empty header, header not unique, ...)
     */
    bool setHeader(
        ColumnIndexType colIdx,   ///< the zero-based index of the column
        const Sloppy::estring& newHeader   ///< the new column header
        );

    /** \brief Sets all headers at once.
     *
     * If the table is still empty, the size of the provided data vector
     * determines the number of columns for the table.
     *
     * If the table already contains data, the number of elements in
     * the provided vector must match the number of columns in the table.
     *
     * All header names will be trimmed. They have to be unique and non-empty
     * and may not contain commas or quotation marks.
     *
     * There's no partial setting of header names; we either consume the whole
     * input array or nothing.
     *
     * \returns `true` on success or `false` if any of the mentioned conditions
     * were not fulfilled.
     */
    bool setHeader(
        const std::vector<std::string>& headers   ///< the new column headers
        );

    /** \brief Sets all headers at once.
     *
     * If the table is still empty, the size of the provided data vector
     * determines the number of columns for the table.
     *
     * If the table already contains data, the number of elements in
     * the provided vector must match the number of columns in the table.
     *
     * All header names will be trimmed. They have to be unique and non-empty
     * and may not contain commas or quotation marks.
     *
     * There's no partial setting of header names; we either consume the whole
     * input array or nothing.
     *
     * \returns `true` on success or `false` if any of the mentioned conditions
     * were not fulfilled.
     */
    bool setHeader(
        const std::vector<Sloppy::estring>& headers   ///< the new column headers
        );

    /** \brief Sets all headers at once.
     *
     * If the table is still empty, the size of the provided data vector
     * determines the number of columns for the table.
     *
     * If the table already contains data, the number of elements in
     * the provided vector must match the number of columns in the table.
     *
     * NULL values are not permitted. Non-string data will be converted to
     * string using `std::to_string()`. All header names will be trimmed.
     * They have to be unique and non-empty and may not contain commas or
     * quotation marks.
     *
     * There's no partial setting of header names; we either consume the whole
     * input array or nothing.
     *
     * \returns `true` on success or `false` if any of the mentioned conditions
     * were not fulfilled.
     */
    bool setHeader(
        const CSV_Row& headers   ///< the new column headers
        );

    /** \returns a reference to a value in a given row and column
     *
     * \throws std::out_of_range if the column index or row index was invalid
     */
    ElementConstRef get(
        RowIndexType rowIdx,   ///< zero-based index of the row
        ColumnIndexType colIdx   ///< zero-based index of the column
        ) const;

    /** \returns a reference to a value in a given row and column
     *
     * \throws std::out_of_range if the row index was invalid
     *
     * \throws std::invalid_argument if the provided column name (header)
     * could not be found; this includes the case that no column headers
     * have been set at all.
     */
    ElementConstRef get(
        RowIndexType rowIdx,   ///< zero-based index of the row
        const std::string& colName   ///< name of the column (case-sensitive)
        ) const;

    /** \returns a reference to a full data row
     *
     * \throws std::out_of_range if the row index was invalid
     *
     */
    RowConstRef get(
        RowIndexType rowIdx   ///< zero-based index of the row
        ) const;

    ContainerType::const_iterator cbegin() const { return rows.cbegin(); }
    ContainerType::const_iterator cend() const { return rows.cend(); }

    /** \brief Erases a data row; wrapper for the `erase()` method of the underlying data vector.
     */
    ContainerType::iterator erase(
        ContainerType::iterator it   ///< iterator pointing on the to-be-deleted row
        ) { return rows.erase(it); }

    /** \brief Erases a consecutive set of data rows; wrapper for the `erase()` method of the underlying data vector.
     */
    ContainerType::iterator erase(
        ContainerType::iterator first,   ///< first row to be deleted
        ContainerType::iterator last   ///< one-past-the-last-row to be deleted
        )
    { return rows.erase(first, last); }

    /** \brief Converts the table to a CSV string
     *
     * \return A CSV string representing the table
     */
    std::string asString(
        bool includeHeaders,   ///< export the headers yes/no; ignored if we don't have headers
        CSV_StringRepresentation rep   ///< defines how string data is represented in the output string
        ) const;

    /** \brief Erases a complete column from all rows in the table, including the headers (if any)
     *
     * \warning This is not an atomic operation. We rather delete the given column one-by-one from
     * each row. Should one of the deletions fail, the table is left in an inconsistent state.
     *
     * \returns `true` if the deletion was successfull, `false` otherwise (most likely due to an invalid index)
     */
    bool eraseColumn(
        ColumnIndexType colIdx   ///< zero-based index of the column to be deleted
        );

    /** \brief Erases a complete column from all rows in the table, including the headers (if any)
     *
     * \warning This is not an atomic operation. We rather delete the given column one-by-one from
     * each row. Should one of the deletions fail, the table is left in an inconsistent state.
     *
     * \returns `true` if the deletion was successfull, `false` otherwise (most likely due to
     * an invalid column name)
     */
    bool eraseColumn(
        const std::string& colName   ///< name of the column to be deleted (case-sensitive)
        );

  protected:
    /** \brief Resolves a column index to an iterator for the
     * asscociated entry in the `header2columnIndex` map.
     *
     * If we don't have any headers defined or if the column index
     * is invalid, the end()-iterator is returned.
     *
     * \returns an iterator pointing to the map entry for the given
     * column index.
     */
    HeaderMap::const_iterator findHeaderForColumnIndex_const(ColumnIndexType colIdx) const;

    /** \brief Resolves a column index to an iterator for the
     * asscociated entry in the `header2columnIndex` map.
     *
     * If we don't have any headers defined or if the column index
     * is invalid, the end()-iterator is returned.
     *
     * \returns an iterator pointing to the map entry for the given
     * column index.
     */
    HeaderMap::iterator findHeaderForColumnIndex(ColumnIndexType colIdx);

    /** \returns 'true' if the provided parameter is a valid header name:
     * non-empty, unique, no comma, no quotation mark.
     */
    bool isValidHeader(const std::string& s) const;

    bool setHeader_trimmed(
        const std::vector<std::string>& headers   ///< the new column headers, already trimmed
        );

  private:
    CSV_Row::IndexType colCount{0};
    ContainerType rows;
    std::unordered_map<std::string, ColumnIndexType> header2columnIndex;
  };
}

#endif
