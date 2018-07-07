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

#ifndef __LIBSLOPPY_MANAGEDFD_H
#define __LIBSLOPPY_MANAGEDFD_H

#include <string>
#include <cstring>
#include <optional>
#include <mutex>
#include <atomic>

#include "Memory.h"

using namespace std;

namespace Sloppy
{
  //
  // a few exceptions
  //
  class InvalidDescriptor{};

  class InvalidDataSize{};

  class OutOfMemory{};

  /** \brief A class that indicates a read timeout on a file descriptor
   * and is able to (optionally) return the partially read data.
   */
  class ReadTimeout
  {
  public:
    /** \brief Ctor from an array of data.
     *
     * Creates a deep copy of the MemView's contents.
     */
    explicit ReadTimeout(
        const MemView& incompleteData   ///< a view on an array that contains the data read so far
        )
      :data{incompleteData}, len{incompleteData.size()}{}

    /** \brief Ctor from just a numeric value that represents
     * the number of bytes read so far.
     */
    explicit ReadTimeout(
        const size_t& incompleteDataLen   ///< the number of bytes read so far
        )
      :data{}, len{incompleteDataLen}{}

    /** \brief Returns a view on the incomplete data that has been passed to the ctor
     *
     * The returned view is only valid as long as the
     * `ReadTimeout` object lives.
     *
     * \throws `InvalidDataSize` if no data has been provided to the ctor
     *
     * \returns a view on the data passed to the ctor
     */
    MemView getIncompleteData() const {
      if (!(data.has_value()))
      {
        throw InvalidDataSize();
      }
      return data->view();
    }

    /** \returns Returns the number of bytes read
     */
    size_t getNumBytesRead() const { return len; }

  private:
    optional<MemArray> data;
    size_t len;
  };

  /** \brief A wrapper class for the I/O error codes (`errno`)
   * of the standard library
   */
  class IOError
  {
  public:
    /** \brief Ctor that initializes the object with the
     * current I/O error as stored in `errno'
     */
    IOError()
      :e{errno}, eStr{strerror(errno)}{}

    /** \brief Ctor that uses an arbitrary error number
     * and error string for initialization.
     */
    IOError(
        int nError,   ///< the error number that shall be stored
        const string& errStr   ///< a description string for the error number
        )
      :e{nError}, eStr{errStr}{}

    /** \returns the stored error number
     */
    int getErrorNumber() const { return e; }

    /** \returns the stored error description string
     */
    string getErrString() const { return eStr; }

  private:
    int e;
    string eStr;
  };

  /** \brief Blocks / waits until a descriptor becomes ready for reading.
   *
   * The blocking time can be limited to an optional timeout.
   *
   * \throws IOError if an I/O error occurred while waiting for the descriptor
   *
   * \returns `true` if the descriptor is ready for reading or `false` if a timeout occurred.
   */
  bool waitForReadOnDescriptor(
      int fd,   ///< the descriptor to check
      size_t timeout_ms = 0   ///< the timeout value in milliseconds; set to 0 for infinite waiting
      );

  /** \brief A wrapper class that takes ownership of a file descriptor.
   *
   * The class provides convenience functions for reading and writing and
   * ensures that the file descriptor is closed when the object goes out
   * of scope or is deleted.
   *
   * Additionally the class offers state information (e.g, is a blocking
   * read is currently taking place) that can be checked from other threads.
   *
   * All read / write operations are guarded by a mutex and can thus
   * be accessed from different threads.
   */
  class ManagedFileDescriptor
  {
  public:
    /** \brief The default read buffer size
     */
    static constexpr size_t ReadChunkSize = 512000;

    /** \brief The default timeout for read operations in milliseconds
     */
    static constexpr int DefaultReadWait_ms = 10;

    /** \brief An enum class that indicated the current state of the
     * file descriptor.
     */
    enum class State
    {
      Idle,   ///< the descriptor is idle, no operations are currently taking place
      Reading,   ///< a blocking, synchronous read is currently running
      Writing,   ///< a blocking, synchronous write is currently running
      Closed   ///< the descriptor has been closed by the user
    };

    /** \brief Ctor that takes ownership of the provided file descriptor.
     *
     * The descriptor has to be open. It is the caller's responsibility
     * to ensure that the FD is open.
     *
     * \throws InvalidDescriptor if the file descriptor number is negative
     *
     * \throws OutOfMemory if the read buffer could not be allocated
     *
     * \note The descriptor will NOT be closed if the ctor throws an exception!
     */
    explicit ManagedFileDescriptor(
        int _fd,  ///< the descritpr number
        size_t readBufferSize = ReadChunkSize   ///< the size of the read buffer in bytes
        );

    /** \brief Dtor, closes the descriptor if it is still open
     */
    virtual ~ManagedFileDescriptor();

    /** \brief Executes a blocking write operation on the descriptor using `write()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the file descriptor.
     *
     * \throws IOError if an I/O error occurred during writing
     *
     * \returns `true` if the data has been fully written to the descriptor
     * or `false` otherwise (bytes written != bytes provided).
     */
    bool blockingWrite(
        const string& data   ///< a string containing the data to write
        );

    /** \brief Executes a blocking write operation on the descriptor using `write()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the file descriptor.
     *
     * \throws IOError if an I/O error occurred during writing
     *
     * \returns `true` if the data has been fully written to the descriptor
     * or `false` otherwise (bytes written != bytes provided).
     */
    bool blockingWrite(
        const MemView& data   ///< a MemView pointing at the data to write
        );

    /** \brief Executes a blocking write operation on the descriptor using `write()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the file descriptor.
     *
     * \throws IOError if an I/O error occurred during writing
     *
     * \returns `true` if the data has been fully written to the descriptor
     * or `false` otherwise (bytes written != bytes provided).
     */
    bool blockingWrite(
        const char* ptr,   ///< pointer to a memory section with data
        size_t len   ///< length of memory section
        );

    /** \brief Executes a blocking read operation on the descriptor using `read()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the file descriptor.
     *
     * The maximum read time can be limited by providing a timeout value.
     *
     * The call can be configured to read at least `minLen` bytes but not
     * more than `maxLen` bytes from the descriptor. If at least `minLen` bytes
     * have been read, the call will return immediately. Even if the
     * timeout has not yet been reached we will not make any additional
     * calls to `read()` in order to receive more than bytes.
     *
     * \throws InvalidDataSize if `maxLen` < `minLen`
     *
     * \throws IOError if an I/O error occurred during reading
     *
     * \throws ReadTimeout if the requested minimum amount of data could not
     * be read within the provided time range
     *
     * \returns a heap allocated buffer that contains the received data
     */
    MemArray blockingRead(
        size_t minLen,   ///< the minimal number of bytes to read; if zero, we'll wait for at least one byte
        size_t maxLen = 0,   ///< the maximum of bytes to read from the descriptor; if zero, we read as much as possible until we have at least `minLen`
        size_t timeout_ms = 0   ///< the timeout
        );

    /** \brief Executes a blocking read operation on the descriptor using `read()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the file descriptor.
     *
     * The maximum read time can be limited by providing a timeout value.
     *
     * The call will read exactly `expectedLen` bytes from the descriptor.
     *
     * \throws InvalidDataSize if `expectedLen` is zero
     *
     * \throws IOError if an I/O error occurred during reading
     *
     * \throws ReadTimeout if the requested minimum amount of data could not
     * be read within the provided time range
     *
     * \returns a heap allocated buffer that contains the received data
     */
    MemArray blockingRead_FixedSize(size_t expectedLen, size_t timeout_ms = 0);

    /** \brief Closes the descriptor by calling `close()`
     *
     * \throws IOError if an I/O error occurred during closing
     */
    void close();

    /** \returns the current state of the file descriptor
     */
    State getState();

    /** \returns the file descriptor "as is" and stops managing it
     */
    int releaseDescriptor();

    // disable the copy constructor; a FD can only belong to one owner
    ManagedFileDescriptor(const ManagedFileDescriptor& other) = delete;

    // disable the copy assignment operator; a FD can only belong to one owner
    ManagedFileDescriptor& operator=(const ManagedFileDescriptor& other) = delete;

    /** \brief Move constructor.
     *
     * Waits until the mutexes of source and target become available
     * and then closes the FD in the target (if any), transfers the FD
     * to the target and sets the source FD to an invalid value.
     *
     * \note Do not use the source object anymore after calling the move ctor!
     */
    ManagedFileDescriptor(ManagedFileDescriptor&& other);

    /** \brief Move assignment.
     *
     * Waits until the mutexes of source and target become available
     * and then closes the FD in the target (if any), transfers the FD
     * to the target and sets the source FD to an invalid value.
     *
     * \note Do not use the source object anymore after using the move assignment!
     */
    ManagedFileDescriptor& operator=(ManagedFileDescriptor&& other);

  protected:
    int fd;
    mutex fdMutex;
    atomic<State> st;
    MemArray readBuf;

  };
}

#endif
