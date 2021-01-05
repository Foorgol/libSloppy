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

#include <iostream>                                         // for basic_ost...

#include "Logger.h"
#include "../DateTime/DateAndTime.h"                        // for WallClock...
#include "../String.h"                                      // for estring
#include "../DateTime/tz.h"  // for locate_zone

using namespace std;

namespace Sloppy
{
  namespace Logger
  {
    Logger::Logger()
    {
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

      estring outText{"%1%2%3: "};
      if (useTimestamps)
      {
        DateTime::WallClockTimepoint_secs now{tzPtr};
        if (tzPtr != nullptr)
        {
          outText.arg(now.timestampString() + " ");
        } else {
          outText.arg(now.timestampString() + "UTC ");
        }
      } else {
        outText.arg("");
      }

      if (sender.empty())
      {
        outText.arg("");
      } else {
        outText.arg(sender + " ");
      }

      switch (lvl) {
      case SeverityLevel::trace:
        outText.arg("Info");
        break;
      case SeverityLevel::normal:
        outText.arg("");
        break;
      case SeverityLevel::warning:
        outText.arg("WARN");
        break;
      case SeverityLevel::error:
        outText.arg("ERROR");
        break;
      case SeverityLevel::critical:
        outText.arg("CRITICAL");
        break;
      }

      cerr << outText << msg << endl;

    }

    //----------------------------------------------------------------------------

    void Logger::enableTimestamp(bool isEnabled)
    {
      useTimestamps = isEnabled;
    }

    //----------------------------------------------------------------------------

    bool Logger::setTimezone(const string& tzName)
    {
      try {
        const auto tmpPtr = date::locate_zone(tzName);
        if (tmpPtr != nullptr) tzPtr = tmpPtr;
        return (tmpPtr != nullptr);
      }
      catch (...) {
        return false;
      }
    }

    //----------------------------------------------------------------------------

    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
