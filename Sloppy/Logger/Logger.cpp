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
