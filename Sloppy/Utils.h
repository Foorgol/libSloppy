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

#ifndef __LIBSLOPPY_UTILS_H
#define __LIBSLOPPY_UTILS_H

#include <vector>
#include <algorithm>
#include <string>

// we include some special file functions for
// non-Windows builds only
#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "String.h"
#include "ManagedFileDescriptor.h"
#include "../json_fwd.hpp"

using namespace std;

// a forward declaration
namespace boost { namespace filesystem { class path; }}

namespace Sloppy
{
  // another forward
  //class ManagedFileDescriptor;
  class MemArray;
  class MemView;

  /** \brief Assigns a value to a pointed-to variable if the pointer is not null
   *
   * Useful if a function uses an optional pointer for returning an error code.
   *
   * The template parameter is the type of the pointed-to variable, e.g., an int
   * or an enum.
   */
  template<typename T>
  inline void assignIfNotNull(
          T* ptr,   ///< the pointer to use for the assignment
          const T& val   ///< the value to assign if the pointer is not null
          )
  {
    if (ptr != nullptr) *ptr = val;
  }

  /** \brief Checks whether a string is a valid email adress.
   *
   * Something weird occurred during implementation.
   * Using the case-insensitive mode
   * of a regex doesn't work as expected. Example: the
   * simple pattern ^[A-Z]+ used with regex::icase does match
   * "ABC", "aaa" but not "abc". I would expect to match "abc"
   * as well. Hmmm....
   *
   * So for the purpose of "simple" regex, I only use the
   * character class A-Z and convert the email address to
   * uppercase first. I hoped the uppercase conversion would
   * not be necessary, but the simple test explained above
   * proved me wrong.
   *
   * The regex is taken from
   * http://www.regular-expressions.info/email.html
   *
   * \returns `true` if the provided string contains a valid email address (read: if it matches the regex)
   */
  bool isValidEmailAddress(
          const string& email   ///< a string containing the (potential) email address
          );


  /** \brief Checks whether an element is in a vector.
   *
   * The template parameter is the content type of the vector.
   *
   * \returns `true` if the provided element occurs at least once in the vector,
   * `false` otherwise.
   */
  template<typename ElemType>
  bool isInVector(
          const vector<ElemType>& vec,   ///< the vector to search in
          const ElemType& el   ///< the value to search for in the vector
          )
  {
    return (find(vec.begin(), vec.end(), el) != vec.end());
  }

  /** \brief Erases all occurences of a value from a vector.
   *
   * The template parameter is the content type of the vector.
   * The vector is modified in place.
   *
   * \returns the number of removed elements
   */
  template<class T>
  size_t eraseAllOccurencesFromVector(
          vector<T>& vec,   ///< the vector to erase the values from
          const T& val   ///< the value to erase from the vector
          )
  {
    size_t oldSize = vec.size();
    vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
    size_t newSize = vec.size();

    return oldSize - newSize;
  }

  /** \brief Trim a string and checks that it's not too long or empty.
   *
   * The string is trimmed on both ends and it is modified in place.
   *
   * Trimming happens ALWAYS, regardless of the result of the length check.
   *
   * \returns `false` if the trimmed string exceeds a certain length OR is empty;
   * `true` if it is not empty AND not exceeding an upper length limit (if provided)
   */
  bool trimAndCheckString(
          string& s,   ///< the string to be trimmed and checked
          size_t maxLen = 0   ///< an upper length limit to check; set to 0 to disable checking a max length
          );

  /** \brief Retrieves all file names in a directory and its sub-directories
   *
   * \returns a vector of strings containing the file names
   */
  StringList getAllFilesInDirTree(
          const string& baseDir,   ///< the root of the recursive search, either as absolute path or relative to the current work dir
          bool includeDirNameInList=false   ///< set to `true` to include directory names in the result list; if `false`, only files will be returned
          );

  /** \brief Helper function for `getAllFilesInDirTree`; not to be called directly
   */
  void getAllFilesInDirTree_Recursion(const boost::filesystem::path& basePath, StringList& resultList, bool includeDirNameInList);

  // we include some special file functions for
  // non-Windows builds only
#ifndef WIN32

  /** \brief Retrieves the current work directory
   *
   * \note This is basically a wrapper for `getcwd()` and is hence not available on Windows.
   *
   * \returns the current work dir as a string
   */
  string getCurrentWorkDir();

  /** \brief Checks whether a string points to an existing regular file
   *
   * Special files will be returned as `false`.
   *
   * \note This is basically a wrapper for `stat()` and `S_ISREG` and is hence not available on Windows.
   *
   * \note This call does not check whether the file is accessible or not!
   *
   * \returns `true` if the string parameter contains a path to an existing file (not directory); `false`
   * if the target doesn't exist or is not a file.
   */
  bool isFile(
          const string& fName   ///< string containing the file path to check (abs. or rel. path)
          );

  /** \brief Checks whether a string points to an existing directory
   *
   * \note This is basically a wrapper for `stat()` and `S_ISDIR` and is hence not available on Windows.
   *
   * \note This call does not check whether the directory is accessible or not!
   *
   * \returns `true` if the string parameter contains a path to an existing directory; `false`
   * if the target doesn't exist or is not a directory.
   */
  bool isDirectory(
          const string& dirName   ///< string containing the directory path to check (abs. or rel. path)
          );
#endif

  /** \brief Converts a JSON-value to string, regardless of the actual inner JSON type
   *
   * A boolean `true` will be returned as "1" and `false` as "0".
   *
   * This call is designed for single JSON values, not arrays etc. This is not a replacement
   * for serializing via dump().
   *
   * \note Empty JSON objects (type == `null`) return "" and are thus indistinguishable from
   * JSON objects with strings (type == `string`) whose contained string is empty.
   *
   * \throws std::invalid_argument if the JSON value was not empty, bool, numeric or string
   *
   * \returns a string representation of the contained value as returned by the library call `to_string()`
   */
  string json2String(
          const nlohmann::json& jv,   ///< the JSON value to convert
          int numDigits = 6   ///< an optional value that defines the number of digits for the conversion of floating point numbers
          );

  //----------------------------------------------------------------------------

  /** \brief A class that represents the end point of a bi-directional pipe.
   *
   * Is internally represented by two ManagedFileDescriptor instances, one
   * for the pipe to read from and one for the pipe to write to.
   *
   * All read operations are mapped to the read FD and all write operations
   * are mapped to the write FD.
   */
  class BiDirPipeEnd
  {
  public:
    /** \brief Deleted default ctor because this object has no useable defaults
     */
    BiDirPipeEnd() = delete;

    /** \brief Ctor that takes the raw file descriptors for the read pipe
     * and the write pipe.
     *
     * \note The FDs belong to two pipes, they are not the two ends of a
     * single pipe! You have to call `pipe()` twice in order to get all
     * necessary file descriptors!
     */
    BiDirPipeEnd(int _fdRead, int _fdWrite);

    /** \brief Disabled copy construction because we cannot share our ressources
     */
    BiDirPipeEnd(const BiDirPipeEnd& other) = delete;

    /** \brief Disabled copy assignment because we cannot share our ressources
     */
    BiDirPipeEnd& operator=(const BiDirPipeEnd& other) = delete;

    /** \brief Move assignment, shifts the file descriptors to the new object
     * and leaves the source object with empty / invalid FDs.
     *
     * \note If the target object already contained valid descriptors then
     * these descriptors will be closed by this operation.
     */
    BiDirPipeEnd& operator=(BiDirPipeEnd&& other) noexcept;

    /** \brief Move ctor, shifts the file descriptors to the new object
     * and leaves the source object with empty / invalid FDs.
     */
    BiDirPipeEnd(BiDirPipeEnd&& other) noexcept;

    /** \brief Default dtor that closes the underlying file descriptors
     */
    ~BiDirPipeEnd() = default;


    /** \brief Executes a blocking write operation on the write pipe using `write()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the underlying file descriptor.
     *
     * \throws IOError if an I/O error occurred during writing
     *
     * \returns `true` if the data has been fully written to the pipe
     * or `false` otherwise (bytes written != bytes provided).
     */
    bool blockingWrite(
        const string& data   ///< a string containing the data to write
        );

    /** \brief Executes a blocking write operation on the write pipe using `write()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the underlying file descriptor.
     *
     * \throws IOError if an I/O error occurred during writing
     *
     * \returns `true` if the data has been fully written to the descriptor
     * or `false` otherwise (bytes written != bytes provided).
     */
    bool blockingWrite(
        const MemView& data   ///< a MemView pointing at the data to write
        );

    /** \brief Executes a blocking write operation on the write pipe using `write()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the underlying file descriptor.
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

    /** \brief Executes a blocking read operation on the read pipe using `read()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the underlying file descriptor.
     *
     * The maximum read time can be limited by providing a timeout value.
     *
     * The call can be configured to read at least `minLen` bytes but not
     * more than `maxLen` bytes from the pipe. If at least `minLen` bytes
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

    /** \brief Executes a blocking read operation on the read pipe using `read()'
     *
     * When used in a multi-thread environment, this call blocks until we
     * can acquire the access mutex for the underlying file descriptor.
     *
     * The maximum read time can be limited by providing a timeout value.
     *
     * The call will read exactly `expectedLen` bytes from the pipe.
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

    /** \brief Closes the underlying descriptors by calling `close()`
     *
     * \throws IOError if an I/O error occurred during closing
     */
    void close();

  private:
    ManagedFileDescriptor fdRead;
    ManagedFileDescriptor fdWrite;
  };

  pair<BiDirPipeEnd, BiDirPipeEnd> createBirectionalPipe();

}

#endif
