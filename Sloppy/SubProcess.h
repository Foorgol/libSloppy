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
#include <vector>
#include "String.h"

namespace Sloppy
{
  /** \brief Contains the complete output of an external program that has been called with `execExternalCmd`
   */
  struct CmdReturnData
  {
    int rc{-1};   ///< the program's exit code
    StringList out;  ///< the program's stdout, already split up in lines using `newline` as the separator
    StringList err;  ///< the program's stderr, already split up in lines using `newline` as the separator

    /** \returns `true` if the exit code is zero, meaning that the program has finished successfully
     */
    constexpr operator bool() const noexcept
    {
      return (rc == 0);
    }
  };

  /** \brief Runs an external program in a forked sub-process,
   * waits for its completion (blocking) and returns its output
   *
   * \note If the child did not exit normally, the `rc` in `CmdReturnData` will be set to -1.
   *
   * \throws std::runtime_error if the the call to `execv()` failed or
   * if the pipes for the data exchange with the child process could not
   * be created
   *
   * \returns the program's stdout, stderr and exit code
   */
  CmdReturnData execCmd(
      const std::vector<std::string>& cmdAndArgs  ///< the command's full path at index 0 and all parameters at index 1...
      );

  /** \brief Runs a program via SSH on a remote machine,
   * waits for its completion (blocking) and returns its output
   *
   * This is basically a call to `execCmd` but parameter list will be prepended
   * with `/usr/bin/ssh' and the provided hostname.
   *
   * \note Password authentication IS NOT SUPPORTED. Passwordless login based on
   * public keys is mandatory for this function to work correctly
   *
   * \note SSH returns with the exit value of the remote command or 255 in case of erros. Thus,
   * the `rc` field in the returned struct can be used to evaluate the remote command's exit value.
   *
   * \throws std::runtime_error if the the call to `execv()` failed or
   * if the pipes for the data exchange with the child process could not
   * be created
   *
   * \throws std::invalid_argument if the provided hostname was empty
   *
   * \returns the program's stdout, stderr and exit code
   */
  CmdReturnData execRemoteCmd(
      const std::vector<std::string>& cmdAndArgs,  ///< the command's full path at index 0 and all parameters at index 1...
      const std::string& hostname   ///< the host on which to run the command
      );
}
