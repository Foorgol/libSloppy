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
#include <boost/log/sources/severity_logger.hpp>

using namespace std;

namespace Sloppy
{
  namespace Logger
  {
    using namespace boost::log;

    /** \brief An enum class with severity levels for log messages
     */
    enum class SeverityLevel
    {
      trace,  ///< debug information
      normal,  ///< normal status messages
      warning,  ///< warnings, if something unusual has happened
      error,   ///< an error has occurred but operation can continue
      critical   ///< a severe error has occurred and we're doomed
    };

    /** \brief A wrapper class around Boost's logging functions.
     *
     * Provides a simple interface for logging to stdout through
     * Boost's logging implementation. Each `Logger` instance can use
     * an individual sender name so that multiple senders can be
     * distinguished on stdout.
     */
    class Logger
    {
    public:
      /** \brief The default ctor that creates a new logger without sender name
       */
      Logger();

      /** \brief Ctor that assigns an individual sender name to this
       * `Logger` instance.
       */
      explicit Logger (
          const string& senderName   ///< the sender's name that shall show up in the log
          );

      /** \brief Prints a log line to stdout.
       *
       * The the message's severity level is below the minimum level, printing
       * of the message will be suppressed.
       */
      void log(
          SeverityLevel lvl,   ///< the severity level of the message
          const string& msg   ///< the message itself
          );

      /** \brief Prints a log line with the implicit level "Warning"
       */
      inline void warn (
          const string& msg   ///< the log message
          )
      {
        log(SeverityLevel::warning, msg);
      }

      /** \brief Prints a log line with the implicit level "Error"
       */
      void error (
          const string& msg   ///< the log message
          )
      {
        log(SeverityLevel::error, msg);
      }

      /** \brief Prints a log line with the implicit level "Critical"
       */
      void critical (
          const string& msg   ///< the log message
          )
      {
        log(SeverityLevel::critical, msg);
      }

      /** \brief Prints a log line with the implicit level "Trace"
       */
      void trace (
          const string& msg   ///< the log message
          )
      {
        log(SeverityLevel::trace, msg);
      }

      /** \brief Prints a log line with the current default level
       */
      void log(
          const string& msg   ///< the log message
          )
      {
        log(defaultLvl, msg);
      }

      /** \brief Sets the new default level for messages
       */
      inline void setDefaultLevel(
          SeverityLevel newDefaultLvl   ///< the new default level for new messages
          )
      {
        defaultLvl = newDefaultLvl;
      }

      /** \brief Sets the minimum severity level that a message has to
       * achieve in order to be printed on `stdout`
       */
      inline void setMinLogLevel(
          SeverityLevel newMinLvl   ///< the new minimum severity level
          )
      {
        minLvl = newMinLvl;
      }

    protected:
      sources::severity_logger<SeverityLevel> lg;
      boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>> sink;
      string sender{};
      SeverityLevel defaultLvl = SeverityLevel::normal;
      SeverityLevel minLvl = SeverityLevel::normal;
    };
  }
}
#endif
