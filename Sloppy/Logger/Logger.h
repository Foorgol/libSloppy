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

#ifndef __LIBSLOPPY_LOGGER_H
#define __LIBSLOPPY_LOGGER_H

#include <string>

#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>

using namespace std;

namespace Sloppy
{
  namespace Logger
  {
    using namespace boost::log;

    enum class SeverityLevel
    {
      trace,
      normal,
      warning,
      error,
      critical
    };

    class Logger
    {
    public:
      Logger (const string& senderName="");

      void log(SeverityLevel lvl, const string& msg);

      inline void warn (const string& msg)
      {
        log(SeverityLevel::warning, msg);
      }
      void error (const string& msg)
      {
        log(SeverityLevel::error, msg);
      }
      void critical (const string& msg)
      {
        log(SeverityLevel::critical, msg);
      }
      void trace (const string& msg)
      {
        log(SeverityLevel::trace, msg);
      }
      void log(const string& msg)
      {
        log(defaultLvl, msg);
      }

      inline void setDefaultLevel(SeverityLevel newDefaultLvl)
      {
        defaultLvl = newDefaultLvl;
      }

      inline void setMinLogLevel(SeverityLevel newMinLvl)
      {
        minLvl = newMinLvl;
      }

    protected:
      sources::severity_logger<SeverityLevel> lg;
      boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>> sink;
      string sender;
      SeverityLevel defaultLvl = SeverityLevel::normal;
      SeverityLevel minLvl = SeverityLevel::normal;
    };
  }
}
#endif
