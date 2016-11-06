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
      const int colCount;
      StyledElement* body;
      vector<StyledElement*> headerElems;
      vector<vector<StyledElement*>> cells;
    };

  }
}

#endif
