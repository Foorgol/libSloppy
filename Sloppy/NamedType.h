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

#ifndef __LIBSLOPPY_NAMEDTYPE_H
#define __LIBSLOPPY_NAMEDTYPE_H

#include <type_traits>
#include <utility>

/** The following code is taken from
 *  https://github.com/joboccara/NamedType
 *  https://www.fluentcpp.com/2017/03/06/passing-strong-types-reference-revisited/
 */
namespace Sloppy
{
  template<typename T>
  using IsNotReference = typename std::enable_if<!std::is_reference<T>::value, void>::type;

  template <typename T, typename Parameter>
  class NamedType
  {
  public:
      using UnderlyingType = T;

      // constructor
      explicit constexpr NamedType(T const& value) : value_(value) {}
      template<typename T_ = T, typename = IsNotReference<T_>>
      explicit constexpr NamedType(T&& value) : value_(std::move(value)) {}

      // get
      constexpr T& get() { return value_; }
      constexpr std::remove_reference_t<T> const& get() const {return value_; }

      // comparison
      bool operator<(T const& other) const  { return value_ < other; }
      bool operator>(T const& other) const  { return other < value_; }
      bool operator<=(T const& other) const { return !(*this > other); }
      bool operator>=(T const& other) const { return !(*this < other); }
      bool operator==(T const& other) const { return !(*this < other || *this > other); }
      bool operator!=(T const& other) const { return !(*this == other); }

      // conversions
      using ref = NamedType<T&, Parameter>;
      operator ref ()
      {
          return ref(value_);
      }

  private:
      T value_;
  };
}

#endif
