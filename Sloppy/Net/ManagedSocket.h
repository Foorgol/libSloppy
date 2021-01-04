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

#ifndef SLOPPY__MANAGED_SOCKET_H
#define SLOPPY__MANAGED_SOCKET_H

#include "../ManagedFileDescriptor.h"
#include "Net.h"

namespace Sloppy
{
  namespace Net
  {
    /** \brief An enum for selecting between TCP and UDP sockets
     */
    enum class SocketType
    {
      TCP,   ///< TCP socket
      UDP    ///< UDP socket
    };

    /** \brief A class that manages a UDP or TCP socket
     *
     * The socket will be closed when the instance is deleted.
     */
    class ManagedSocket : public ManagedFileDescriptor
    {
    public:

      /** \brief Default ctor that creates an invalid, unusable socket
       * by setting the file descriptor to -1
       */
      ManagedSocket() :ManagedFileDescriptor{} {}

      /** \brief Ctor that creates a new, bare UDP or TCP socket
       */
      explicit ManagedSocket(
          SocketType t   ///< set to UDP or TCP, depending on the desired socket type
          )
        :ManagedFileDescriptor{socket(AF_INET, (t == SocketType::UDP) ? SOCK_DGRAM : SOCK_STREAM, 0)}{}

      /** \brief Ctor that takes over an already existing socket that has been created elsewhere
       */
      explicit ManagedSocket(
          int _fd   ///< the file descriptor of the existing socket
          )
        :ManagedFileDescriptor(_fd) {}

      /** \brief Disabled copy ctor; a socket can only belong to one instance
       */
      ManagedSocket(const ManagedSocket& other) = delete;

      /** \brief Disabled copy assignment; a socket can only belong to one instance
       */
      ManagedSocket& operator=(const ManagedSocket& other) = delete;

      /** \brief Move ctor that transfers the ownership
       * of the socket (==> the file descriptor).
       *
       * The source is left with the FD "-1" and the state "Closed".
       */
      ManagedSocket(ManagedSocket&& other)
        :ManagedFileDescriptor{std::move(other)} {}

      /** \brief Move assignment that transfers the ownership
       * of the socket (==> the file descriptor).
       *
       * The source is left with the FD "-1" and the state "Closed".
       */
      ManagedSocket& operator=(ManagedSocket&& other)
      {
        return static_cast<ManagedSocket&>(ManagedFileDescriptor::operator =(std::move(other)));
      }

      /** \brief Assigns a name to a socket
       *
       * \throws IOError if binding wasn't successful
       *
       * \throws std::invalid_argument if the port number is outside the permitted range (1..65535)
       */
      void bind(
          const std::string& bindName,   ///< the name to bind to (e.g., "localhost")
          int port   ///< the port to bind to
          );

      /** \brief Sets a socket to `listen` state
       *
       * \throws std::invalid_argument if the number of permitted connections is less than 1
       *
       * \throws IOError if a I/O error occurred when calling `listen()' on the socket
       */
      void listen(size_t maxConnectionCount = 1);

      /** \brief Waits for the next incoming connection on a listening socket
       *
       * \returns a pair of <file descriptor of new connection, client address>
       * with the file descriptor being set to -1 if a timeout occurred.
       */
      std::pair<int, sockaddr_in> acceptNext(
          size_t timeout_ms = 0   ///< the maximum time in milliseconds to wait for a new, incoming connection
          );

      /** \brief Connectes to another, listening socket
       *
       * \throws IOError if a I/O error occurred when calling `connect()' on the socket
       */
      void connect(
          const std::string& srvName,   ///< the name or IP address of the server to connect to
          int srvPort   ///< the port to connect to
          );
    };

  }
}

#endif
