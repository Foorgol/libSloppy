/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

#include <sys/wait.h>
#include <unistd.h>


#include "SubProcess.h"
#include "Memory.h"

namespace Sloppy
{

  CmdReturnData execCmd(const std::vector<std::string>& cmdAndArgs)
  {
    // convert the vector of strings into an arry of
    // pointers to 0-terminated strings
    ManagedArray<const char *> argv{cmdAndArgs.size() + 1};
    for (size_t i = 0; i < cmdAndArgs.size(); ++i)
    {
      argv[i] = cmdAndArgs[i].c_str();
    }
    argv[cmdAndArgs.size()] = nullptr;

    // prepare pipes to transfer stderr and stdout between
    // sub-command and parent
    int outPipe[2];
    int rc = pipe(outPipe);
    if (rc != 0)
    {
      throw std::runtime_error("Sloppy::execExternalCmd(): could not create pipes for data exchance with the child process");
    }
    int outPipe_readEnd = outPipe[0];
    int outPipe_writeEnd = outPipe[1];

    int errPipe[2];
    rc = pipe(errPipe);
    if (rc != 0)
    {
      close(outPipe_readEnd);
      close(outPipe_writeEnd);
      throw std::runtime_error("Sloppy::execExternalCmd(): could not create pipes for data exchance with the child process");
    }
    int errPipe_readEnd = errPipe[0];
    int errPipe_writeEnd = errPipe[1];

    int pid  = fork();

    //
    // The following part is only executed by the child
    //
    if (pid == 0)
    {
      // hook stdout to the outPipe
      dup2(outPipe_writeEnd, STDOUT_FILENO);
      close(outPipe_readEnd);  // not needed in child

      // hook stderr to the errPipe
      dup2(errPipe_writeEnd, STDERR_FILENO);
      close(errPipe_readEnd);  // not needed in child

      // execute the external command; the combination
      // of argv[0] and argv[] as parameters is by convention
      //
      // see man 3 exec
      execv(argv[0], static_cast<char * const*>(argv.to_voidPtr()));

      // this code will only be reached if the exec fails
      close(outPipe_writeEnd);
      close(errPipe_writeEnd);
      throw std::runtime_error("Sloppy::execExternalCmd(): call to execv failed");
    }

    //
    // The following part is only executed by the parent
    //
    if (pid > 0)
    {
      // not needed in parent process
      close(outPipe_writeEnd);
      close(errPipe_writeEnd);

      // wait for the child to exit
      int status;
      waitpid(pid, &status, 0);

      // helper for reading from a pipe until it's empty
      auto readAllFromPipe = [](int pipeFd)
      {
        static constexpr int ReadBufSize{1000};
        char buf[ReadBufSize];
        int64_t cnt;
        estring data;
        while ((cnt = read(pipeFd, buf, ReadBufSize)) > 0)
        {
          data.append(buf, static_cast<size_t>(cnt));
        }
        return data;
      };

      // read the buffered stdout / stderr data from
      // the pipe
      CmdReturnData result;
      result.out = readAllFromPipe(outPipe_readEnd).split("\n", true, false);
      close(outPipe_readEnd);
      result.err = readAllFromPipe(errPipe_readEnd).split("\n", true, false);
      close(errPipe_readEnd);

      // remove the last empty output line, if any
      for (std::vector<estring>* data : {&result.out, &result.err})
      {
        if (!data->empty())
        {
          if (data->operator[](data->size() - 1).empty())
          {
            data->pop_back();
          }
        }
      }

      // check if the child returned normally
      // and try to extract the result coded
      if (WIFEXITED(status))
      {
        result.rc = WEXITSTATUS(status);
      } else {
        result.rc = -1;
      }

      return result;
    }

    // this part should never be reached
    throw std::runtime_error("Sloppy::execExternalCmd(): logic error, access to unreachable code");
  }

  //----------------------------------------------------------------------------

  CmdReturnData execRemoteCmd(const std::vector<std::string>& cmdAndArgs, const std::string& hostname)
  {
    if (hostname.empty())
    {
      throw std::invalid_argument("Sloppy::execRemoteCmd(): empty hostname");
    }

    std::vector<std::string> cmd{"/usr/bin/ssh", hostname};
    std::copy(cmdAndArgs.begin(), cmdAndArgs.end(), std::back_inserter(cmd));

    return execCmd(cmd);
  }


}
