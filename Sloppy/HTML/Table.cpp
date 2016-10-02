#include "Table.h"

namespace Sloppy
{
  namespace HTML
  {

    Table::Table(const StringList& headers)
      :StyledElement("table"), colCount{headers.size()}
    {
      // create table headers
      StyledElement* thead = createContentChild("thead");
      StyledElement* tr = thead->createContentChild("tr");
      for (const string& h : headers)
      {
        StyledElement* hdrElem = createContentChild("th");
        hdrElem->setPlainTextContent(h);
        headerElems.push_back(hdrElem);
      }

      // create the body element
      body = createContentChild("tbody");
    }

    //----------------------------------------------------------------------------

    void Table::appendRow(int cnt)
    {
      for (int i = 0; i < cnt; ++i)
      {
        // create the row element
        StyledElement* tr = body->createContentChild("tr");

        // add empty columns
        vector<StyledElement*> newColumnCells;
        for (int c = 0; c < colCount; ++c)
        {
          StyledElement* td = tr->createContentChild("td");
          newColumnCells.push_back(td);
        }

        // append the new row to the list of rows
        cells.push_back(newColumnCells);
      }
    }

    //----------------------------------------------------------------------------

    StyledElement* Table::getCell(int r, int c, bool createRowIfNotExisting)
    {
      if ((r < 0) || (c < 0)) return nullptr;
      if (c >= colCount) return nullptr;

      if (r >= cells.size())
      {
        if (!createRowIfNotExisting) return nullptr;

        int missingRowCount = r - cells.size() + 1;

        appendRow(missingRowCount);
      }

      auto& row = cells[r];

      return row[c];
    }

    //----------------------------------------------------------------------------

    StyledElement* Table::getHeader(int c) const
    {
      if ((c < 0) || (c >= colCount)) return nullptr;

      return headerElems[c];
    }

    //----------------------------------------------------------------------------

    bool Table::setCell(int r, int c, const string& plainText, bool createRowIfNotExisting)
    {
      StyledElement* cell = getCell(r, c, createRowIfNotExisting);
      if (cell == nullptr) return false;

      cell->setPlainTextContent(plainText);
      return true;
    }

    //----------------------------------------------------------------------------

    bool Table::setCell(int r, int c, StyledElement* elem, bool createRowIfNotExisting)
    {
      StyledElement* cell = getCell(r, c, createRowIfNotExisting);
      if (cell == nullptr) return false;

      cell->addContentElement(elem);
      return true;
    }


    //----------------------------------------------------------------------------


  }
}