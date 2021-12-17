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

#pragma once

#include <variant>

namespace Sloppy
{
  /** \brief Wrapper around a std::variant to return either a result
   *  object or an error code from a function
   */
  template<typename ResultType, typename ErrorEnum>
  class ResultOrError {
  public:
    /** \brief Ctor for a error result */
    ResultOrError(ErrorEnum err)
      : data{err} {}

    /** \brief Ctor for a positive result, copy constructed
     */
    ResultOrError(const ResultType& result)
      : data{result} {}

    /** \brief Ctor for a positive result, move constructed
     */
    ResultOrError(ResultType&& result)
      : data{std::forward<ResultType>(result)} {}

    ResultOrError() = default;
    ResultOrError(const ResultOrError& other) = default;
    ResultOrError(ResultOrError&& other) = default;
    ResultOrError& operator=(const ResultOrError& other) = default;
    ResultOrError& operator=(ResultOrError&& other) = default;
    ~ResultOrError() = default;

    bool isErr() const {
      return std::holds_alternative<ErrorEnum>(data);
    }

    bool isOk() const {
      return std::holds_alternative<ResultType>(data);
    }

    /** \brief Access to the result content; may not be used
     *  if the object contains an error enum
     */
    const ResultType* operator->() const {
      const auto& tmp = std::get<ResultType>(data);
      return &tmp;
    }

    /** \brief Access to the result content; may not be used
     *  if the object contains an error enum
     */
    const ResultType& operator*() const {
      return std::get<ResultType>(data);
    }

    operator bool() const {
      return std::holds_alternative<ResultType>(data);
    }

    /** \brief Access to the storred error code; may not
     *  be used if the object contains a data result
     */
    ErrorEnum err() const {
      return std::get<ErrorEnum>(data);
    }

  private:
    std::variant<ResultType, ErrorEnum> data;
  };
}
