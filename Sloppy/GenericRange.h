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

#pragma once

#include <optional>
#include <stdexcept>

namespace Sloppy {
  
  /** \brief An enumeration of relations between a range and a single time point / sample
   * 
   * For open ended ranges, a sample can never be "after" and is always "in" if the sample
   * is on or beyond the start.
   */
  enum class RelationToRange
  {
    isBefore,    ///< the sample is before the range's start (sample < start)
    isIn,        ///< start <= sample <= end
    isAfter,     ///< the sample is after the range's end (sample > end)
    _undefined   ///< undefind
  };
  
  /** \brief A generic class for ranges of arbitrary type.
   * 
   * The class can be used with types that:
   *   - can be copy-constructed; and
   *   - support all relation operators (>, <, >=, <=, ==, !=)
   *
   * A special feature is that the period can be "open ended". That
   * means that the range has a start value but no end value.
   */
  template <class T>
  class GenericRange
  {
  public:
    /** \brief Ctor for a range with defined start and end.
     * 
     * The values for start and end are stored as a copy.
     * 
     * Start and end value are included in the range.
     *
     * \throws std::invalid_argument if the end is before the start
     */
    explicit GenericRange(
      const T& _start,  ///< the start value of the range; the start value is part of the range
      const T& _end     ///< the end value of the period; the end value is part of the range
    )
    :start{_start}, end{_end}
    {
      static_assert(std::is_copy_constructible<T>(), "GenericPeriod works only with copy-constructible types!");
      if (*end < start)
      {
        throw std::invalid_argument("GenericPeriod ctor: 'end'' may not be before start!");
      }
    }
    
    /** \brief Ctor for a range with defined start and open end.
     * 
     * The start value is stored as a copy.
     */
    explicit GenericRange(
      const T& _start  ///< the start value of the range
    )
    :start(_start), end{}
    {
    }
    
    GenericRange(const GenericRange<T>& other) = default;
    GenericRange<T>& operator=(const GenericRange<T>& other) = default;
    GenericRange(GenericRange<T>&& other) = default;
    GenericRange<T>& operator=(GenericRange<T>&& other) = default;
    ~GenericRange() = default;
    
    /** \returns `true` if the range is open-ended
     */
    inline bool hasOpenEnd() const
    {
      return (!end.has_value());
    }
    
    /** \returns `true` if a sample value is within the range
     */
    bool isInRange(
      const T& sample   ///< the sample value to check
    )
    {
      if (end.has_value())
      {
        return ((sample >= start) && (sample <= *end));
      }
      return (sample >= start);
    }
    
    /** \brief Determines the relation of a sample value with the range
     * 
     * \returns an enum-value of type `Relation` that indicates how the sample relates to the period
     */
    RelationToRange determineRelationToRange(
      const T& sample   ///< the sample value to check
    ) const
    {
      if (sample < start) return RelationToRange::isBefore;
      
      if (end.has_value())
      {
        return (sample > *end) ? RelationToRange::isAfter : RelationToRange::isIn;
      }
      
      return RelationToRange::isIn;
    }
    
    /** \brief Sets a new start value for the period
     * 
     * The new start must be before the end. If this condition is not met,
     * the current start value is not modified.
     *
     * \returns `false` if the new start is after the current end; `true` otherwise.
     */
    inline bool setStart(
      const T& newStart    ///< the new start value
    )
    {
      if (end.has_value() && (newStart > *end)) return false;
      start = newStart;
      return true;
    }
    
    
    /** \brief Sets a new end value for the period
     * 
     * The new end must be on or after the start. If this condition is not met,
     * the current end value is not modified.
     *
     * \returns `false` if the new end is before the current start; `true` otherwise.
     */
    inline bool setEnd(
      const T& newEnd    ///< the new end value
    )
    {
      if (newEnd < start) return false;
      end = newEnd;
      return true;
    }
    
    /** \returns a copy of the current start value
     */
    inline T getStart() const
    {
      return start;
    }
    
    /** \returns a copy of the current end value (which is an std::optional<T>)
     */
    inline std::optional<T> getEnd() const
    {
      return end;
    }
    
    /** \returns `true` if this range starts earlier than another range
     */
    inline bool startsEarlierThan (
      const GenericRange<T>& other    ///< the period used for the comparison
    ) const
    {
      return (start < other.start);
    }
    
    /** \returns `true` if this range starts later than another range
     */
    inline bool startsLaterThan (
      const GenericRange<T>& other    ///< the period used for the comparison
    ) const
    {
      return (start > other.start);
    }
    
    /** \returns `true` if this range starts earlier than a given sample
     * 
     * Essentially checks for "start < sample"
     */
    inline bool startsEarlierThan (
      const T& sample    ///< the sample used for the comparison
    ) const
    {
      return (start < sample);
    }
    
    /** \returns `true` if this range starts later than a given sample
     * 
     * Essentially checks for "start > sample"
     */
    inline bool startsLaterThan (
      const T& sample    ///< the sample used for the comparison
    ) const
    {
      return (start > sample);
    }
    
  protected:
    T start;   ///< the period's start value
    std::optional<T> end;   ///< the period's end value, potentially empty
  };

  using IntRange = Sloppy::GenericRange<int>;
  using DoubleRange = Sloppy::GenericRange<double>;
  
}
