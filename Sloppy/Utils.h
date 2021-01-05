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


#include <stddef.h>                 // for size_t
#include <initializer_list>         // for initializer_list
#include <iomanip>                  // for operator<<, setfill, setw
#include <sstream>                  // for ostringstream, basic_ostream, ope...
#include <string>                   // for string, allocator
#include <type_traits>              // for is_integral, is_signed
#include <utility>                  // for pair
#include <vector>                   // for vector
#include "ManagedFileDescriptor.h"  // for ManagedFileDescriptor
#include "String.h"                 // for StringList
#include "json.hpp"                 // for json

// we include some special file functions for
// non-Windows builds only
#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace std
{
  /** \brief A helper function in namespace `std` provided by libSloppy for easy conversion
   * of string literals to `string` objects; useful in template functions because we can call
   * 'to_string()` on a broader range of types.
   *
   * \returns a `std::string` that contains a copy of the provided C string
   */
  std::string to_string(const char* cString);

  /** \brief A helper function in namespace `std` provided by libSloppy for
   * usage in template functions because we can call
   * 'to_string()` on a broader range of types.
   *
   * \returns the provided parameter
   */
  const std::string& to_string(const std::string& s);
}

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

  /** \brief Takes a list of values and converts them to a comma-separated list.
   *
   * A `to_string()` function for the used data type has to be available.
   *
   * \returns a string with a comma-separated list of values
   */
  template<typename T>
  std::string commaSepStringFromValues(
      std::vector<T> vals,   ///< the list of values that shall be converted
      const std::string& delim = ","   ///< the delimiter between two items
      )
  {
    std::string result;
    for (const T& v : vals)
    {
      if (!result.empty()) result += delim;
      result += std::to_string(v);
    }

    return result;
  }

  /** \brief Takes a list of values and converts them to a comma-separated list.
   *
   * A `to_string()` function for the used data type has to be available.
   *
   * \returns a string with a comma-separated list of values
   */
  template<typename T>
  std::string commaSepStringFromValues(
      std::initializer_list<T> vals,   ///< the list of values that shall be converted
      const std::string& delim = ","   ///< the delimiter between two items
      )
  {
    // repeat the code from the `vector` implementation above.
    //
    // we could also do a simple
    //
    //    return commaSepStringFromValues<T>(vector<T>(vals), delim)
    //
    // but that would imply a copy of all values from the list to the vector
    // which is probably a bad idea


    std::string result;
    for (const T& v : vals)
    {
      if (!result.empty()) result += delim;
      result += std::to_string(v);
    }

    return result;
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
          const std::string& email   ///< a string containing the (potential) email address
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
          const std::vector<ElemType>& vec,   ///< the vector to search in
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
          std::vector<T>& vec,   ///< the vector to erase the values from
          const T& val   ///< the value to erase from the vector
          )
  {
    size_t oldSize = vec.size();
    vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
    size_t newSize = vec.size();

    return oldSize - newSize;
  }

  /** \brief Trims a std-string in place by removing all whitespace on the left
   */
  void trimLeft(std::string& s);

  /** \brief Trims a std-string in place by removing all whitespace on the right
   */
  void trimRight(std::string& s);

  /** \brief Trims both sides of a string in place
   */
  inline void trim(std::string& s) {
    trimLeft(s);
    trimRight(s);
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
          std::string& s,   ///< the string to be trimmed and checked
          size_t maxLen = 0   ///< an upper length limit to check; set to 0 to disable checking a max length
          );

  /** \brief Retrieves all file names in a directory and its sub-directories
   *
   * \returns a vector of strings containing the file names
   */
  StringList getAllFilesInDirTree(
          const std::string& baseDir,   ///< the root of the recursive search, either as absolute path or relative to the current work dir
          bool includeDirNameInList=false   ///< set to `true` to include directory names in the result list; if `false`, only files will be returned
          );

  // we include some special file functions for
  // non-Windows builds only
#ifndef WIN32

  /** \brief Retrieves the current work directory
   *
   * \note This is basically a wrapper for `getcwd()` and is hence not available on Windows.
   *
   * \returns the current work dir as a string
   */
  std::string getCurrentWorkDir();

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
          const std::string& fName   ///< string containing the file path to check (abs. or rel. path)
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
          const std::string& dirName   ///< string containing the directory path to check (abs. or rel. path)
          );
#endif

  /** \brief Returns a zero-padded string for an integer value
   *
   * A possible minus-sign is NOT included in the padding count!
   *
   * \returns a zero-padded `string` representation of the number
   */
  template<typename T>
  std::string zeroPaddedNumber(T v, int width)
  {
    static_assert (std::is_integral<T>::value, "Sloppy::zeroPaddedNumber() must be called with int-like argument");

    if (width < 1) return std::to_string(v);

    std::ostringstream oss;
    if (std::is_signed<T>::value)
    {
      if (v < 0)
      {
        oss << '-';
        v *= -1;
      }
    }

    oss << std::setfill('0') << std::setw(width) << v;

    return oss.str();
  }

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
  std::string json2String(
          const nlohmann::json& jv,   ///< the JSON value to convert
          int numDigits = 6   ///< an optional value that defines the number of digits for the conversion of floating point numbers
          );

  /** \brief Checks whether a json instance is an object that contains a given key
   * with a given value type.
   *
   * \returns `true` only if all three conditions are satisfied (is an object,
   * contains the key and the key's value has the right type)
   */
  bool jsonObjectHasKey(
      const nlohmann::json& js,   ///< the JSON object instance to check
      const std::string& key,   ///< the key to search for in the object
      nlohmann::json::value_t requiredValueType   ///< the required type of the value associated with the key
      );

  /** \brief Checks whether a json instance is an object that contains a given key
   *
   * \returns `true` only if all two conditions are satisfied (is an object,
   * and contains the key)
   */
  bool jsonObjectHasKey(
      const nlohmann::json& js,   ///< the JSON object instance to check
      const std::string& key   ///< the key to search for in the object
      );

  //----------------------------------------------------------------------------

#ifndef WIN32

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
        const std::string& data   ///< a string containing the data to write
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

  /** \brief Creates a bi-directional pipe that allows for two-way communication
   * between to peers.
   *
   * This is essentially a set of two normal pipes, one for each direction.
   *
   * \returns a pair of two BiDirPipeEnd objects, one for each pipe endpoint.
   */
  std::pair<BiDirPipeEnd, BiDirPipeEnd> createBirectionalPipe();

  /** \brief Creates a simple, one-directional pipe
   *
   * Basically calls `pipe()` and wraps the result into two
   * `ManagedFileDescriptor` instances.
   *
   * \returns a pair of <readFileDescr, writeFileDescr>
   */
  std::pair<ManagedFileDescriptor, ManagedFileDescriptor> createSimplePipe();

#endif

}

