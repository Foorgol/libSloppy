/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/expressions.hpp>

#include "Logger.h"

namespace Sloppy
{
  namespace Logger
  {
    bool Logger::isInitialized{false};

    Logger::Logger()
    {
      if (!isInitialized)
      {
        add_common_attributes();

        sink = add_console_log(cerr);
        sink->locked_backend()->auto_flush(true);
        enableTimestamp(true);

        isInitialized = true;
      }
    }

    //----------------------------------------------------------------------------

    Logger::Logger(const string& senderName)
      :Logger{}
    {
      sender = senderName;
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

    void Logger::enableTimestamp(bool isEnabled)
    {
      namespace expr = boost::log::expressions;
      namespace keywords = boost::log::keywords;
      if (isEnabled)
      {
        sink->set_formatter(
              expr::stream
              << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S ")
              << expr::smessage
              );
      } else {
        sink->set_formatter(
              expr::stream
              << expr::smessage
              );
      }
    }

    //----------------------------------------------------------------------------

    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
