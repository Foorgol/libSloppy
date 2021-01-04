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

#pragma once

#include <string>

#include "../DateTime/tz.h"

namespace Sloppy
{
  namespace Logger
  {
    /** \brief An enum class with severity levels for log messages
     */
    enum class SeverityLevel
    {
      trace = 0,  ///< debug information
      normal = 10,  ///< normal status messages
      warning = 20,  ///< warnings, if something unusual has happened
      error = 30,   ///< an error has occurred but operation can continue
      critical = 40   ///< a severe error has occurred and we're doomed
    };

    /** \brief A wrapper class for easy logging
     *
     * Provides a simple interface for logging to stderr.
     * Each `Logger` instance can use
     * an individual sender name so that multiple senders can be
     * distinguished on stderr.
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
          const std::string& senderName   ///< the sender's name that shall show up in the log
          );

      /** \brief Prints a log line to stdout.
       *
       * The the message's severity level is below the minimum level, printing
       * of the message will be suppressed.
       */
      void log(
          SeverityLevel lvl,   ///< the severity level of the message
          const std::string& msg   ///< the message itself
          );

      /** \brief Prints a log line with the implicit level "Warning"
       */
      inline void warn (
          const std::string& msg   ///< the log message
          )
      {
        log(SeverityLevel::warning, msg);
      }

      /** \brief Prints a log line with the implicit level "Error"
       */
      void error (
          const std::string& msg   ///< the log message
          )
      {
        log(SeverityLevel::error, msg);
      }

      /** \brief Prints a log line with the implicit level "Critical"
       */
      void critical (
          const std::string& msg   ///< the log message
          )
      {
        log(SeverityLevel::critical, msg);
      }

      /** \brief Prints a log line with the implicit level "Trace"
       */
      void trace (
          const std::string& msg   ///< the log message
          )
      {
        log(SeverityLevel::trace, msg);
      }

      /** \brief Prints a log line with the current default level
       */
      void log(
          const std::string& msg   ///< the log message
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
       * achieve in order to appear on `stderr`
       */
      inline void setMinLogLevel(
          SeverityLevel newMinLvl   ///< the new minimum severity level
          )
      {
        minLvl = newMinLvl;
      }

      /** \brief Enables or disables a timestamp for the log messages; the
       * default is `on`
       */
      void enableTimestamp(
          bool isEnabled   ///< set to `true` to enable timestamps or to `false` for disabling them
          );

      /** \brief Defines the timezone (e.g., "Europe/Berlin") for the console
       * timestamps.
       *
       * \returns `true` if the timezone was found and set successfully; `false` otherwise
       */
      bool setTimezone(const std::string& tzName);

    protected:
      bool useTimestamps{true};
      std::string sender{};
      SeverityLevel defaultLvl = SeverityLevel::normal;
      SeverityLevel minLvl = SeverityLevel::normal;
      const date::time_zone* tzPtr{nullptr};

    };
  }
}
