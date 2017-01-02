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

#include <iostream>

#include <boost/log/sources/record_ostream.hpp>

#include "Logger.h"

namespace Sloppy
{
  namespace Logger
  {
    Logger::Logger(const string& senderName)
      :sender{senderName}
    {
      add_common_attributes();

      sink = add_console_log(cout);
      sink->locked_backend()->auto_flush(true);
    }

    //----------------------------------------------------------------------------

    void Logger::log(SeverityLevel lvl, const string& msg)
    {
      if (lvl < minLvl) return;

      record rec = lg.open_record(keywords::severity = lvl);
      if (rec)
      {
        record_ostream strm{rec};
        if (!(sender.empty()))
        {
          strm << sender << ": ";
        }
        strm << msg;
        strm.flush();
        lg.push_record(boost::move(rec));
      }
    }

    //----------------------------------------------------------------------------

    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
