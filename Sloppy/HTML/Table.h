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

#ifndef SLOPPY__HTML__TABLE_H
#define SLOPPY__HTML__TABLE_H

#include <stddef.h>         // for size_t
#include <string>           // for string
#include <utility>          // for forward
#include <vector>           // for vector

#include "StyledElement.h"  // for StyledElement

namespace Sloppy
{
  namespace HTML
  {
    /** \brief A HTML table with headers and an arbitrary number of columns.
     *
     * The number of columns is determined during construction and
     * fix afterwards.
     *
     * The content of table cells will always be wrapped into `&lt;td&gt;` elements automatically.
     * The user only has to set the content that shall appear between the `&lt;td&gt;` tags.
     */
    class Table : public StyledElement
    {
    public:
      /** \brief Ctor for a new `&lt;table&gt;` element.
       *
       * \throws std::invalid_argument if the list of headers is empty
       */
      explicit Table(
          const std::vector<std::string>& headers   ///< list of strings containing the column headers
          );

      virtual ~Table(){}

      /** \brief Appends one or more empty rows to the table
       */
      void appendRow(
          int cnt=1   ///< the number of rows to append
          );

      /** \returns a pointer to the `&lt;td&gt;`-element in a specific content cell or `nullptr` if the cell coordinates were invalid
       *
       * \note The pointer is owned by the table element and shall not be deleted by the user!
       */
      StyledElement* getCell(
          int r,   ///< the zero-based index of the cell's row
          int c,   ///< the zero-based index of the cell's column
          bool createRowIfNotExisting=false   ///< if set to `true`, the necessary row(s) are created automatically
          );

      /** \returns a pointer to the `&lt;th&gt;`-element of a specific column or `nullptr` if the column index was invalid
       *
       * \note The pointer is owned by the table element and shall not be deleted by the user!
       */
      StyledElement* getHeader(
          int c   ///< the zero-based index of the column
          ) const;

      /** \brief Sets the content of a specific cell to the provided plain text data
       *
       * This call overwrites all previously set contents of the cell.
       *
       * \returns `false` if the provided cell coordinates were invalid or if the necessary could not be created; `true` otherwise
       */
      bool setCell(
          int r,   ///< the zero-based index of the cell's row
          int c,   ///< the zero-based index of the cell's column
          const std::string& plainText,   ///< the plain text to assign to the cell
          bool createRowIfNotExisting=false   ///< if set to `true`, the necessary row(s) are created automatically
          );

      /** \brief Sets the content of a specific cell to the provided element (should be derived from StyledElement)
       *
       * \note We take OWNERSHIP of the provided pointer!
       *
       * This call overwrites all previously set contents of the cell.
       *
       * \returns `false` if the provided cell coordinates were invalid or if the necessary could not be created; `true` otherwise
       */
      bool setCell(int r, int c, StyledElement* elem, bool createRowIfNotExisting=false);   // TAKES OWNERSHIP!!

      /** \brief Heap-constructs a new child element of a custom type; the type should be derived from StyledElement.
       *
       * \note We take OWNERSHIP of the newly created element
       *
       * This call overwrites all previously set contents of the cell.
       *
       * \returns a pointer to the newly created element or `nullptr` on error (e.g., invalid cell coordinates)
       */
      template<typename ElemType, typename... Args>
      ElemType* createCustomElemInCell(int r, int c, Args&&... args)
      {
        StyledElement* cell = getCell(r, c, true);
        if (cell == nullptr) return nullptr;

        cell->deleteAllContent();

        return cell->createCustomChild<ElemType>(std::forward<Args>(args)...);
      }

    private:
      const size_t colCount;   ///< the number of columns in the table
      StyledElement* body;     ///< the `&lt;tbody&gt;`-element of the table
      std::vector<StyledElement*> headerElems;   ///< the `&lt;th&gt;`-elements of the table
      std::vector<std::vector<StyledElement*>> cells;   ///< a row,column-matrix of the `&lt;td&gt;`-elements with the content data
    };

  }
}

#endif
