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

#ifndef SLOPPY__HTML__TABLE_H
#define SLOPPY__HTML__TABLE_H

#include <string>
#include <vector>
#include <unordered_map>

#include "StyledElement.h"
#include "../libSloppy.h"

using namespace std;

namespace Sloppy
{
  namespace HTML
  {
    class Table : public StyledElement
    {
    public:
      explicit Table(const StringList& headers);
      virtual ~Table(){}

      void appendRow(int cnt=1);

      StyledElement* getCell(int r, int c, bool createRowIfNotExisting=false);
      StyledElement* getHeader(int c) const;

      bool setCell(int r, int c, const string& plainText, bool createRowIfNotExisting=false);
      bool setCell(int r, int c, StyledElement* elem, bool createRowIfNotExisting=false);   // TAKES OWNERSHIP!!

      template<typename ElemType, typename... Args>
      ElemType* createCustomElemInCell(int r, int c, Args&&... args)
      {
        StyledElement* cell = getCell(r, c, true);
        if (cell == nullptr) return nullptr;

        return cell->createCustomContentChild<ElemType>(forward<Args>(args)...);
      }

    protected:
      const size_t colCount;
      StyledElement* body;
      vector<StyledElement*> headerElems;
      vector<vector<StyledElement*>> cells;
    };

  }
}

#endif
