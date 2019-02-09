/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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

#include "Memory.h"

// support of memory mapped files if for non-Windows builds only
#ifndef WIN32

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace Sloppy
{

  MemFile::MemFile(const string& fname)
  {
    if (fname.empty())
    {
      throw std::invalid_argument("MemFile: could not open the file for reading");
    }

    // try to open the file for reading
    fd = ::open(fname.c_str(), O_RDONLY);
    if (fd < 0)
    {
      throw std::invalid_argument("MemFile: could not open the file for reading");
    }

    // determine the file size
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
      ::close(fd);
      fd = -1;
      throw std::invalid_argument("MemFile: could not determine the file size");
    }
    fSize = sb.st_size;

    // memory-map the file
    mapAddr = mmap(nullptr, fSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapAddr == MAP_FAILED)
    {
      ::close(fd);
      fd = -1;
      throw std::invalid_argument("MemFile: creation of the memory map failed");
    }
  }

  //----------------------------------------------------------------------------

  MemFile::~MemFile()
  {
    // release the memory map
    if ((mapAddr != nullptr) && (mapAddr != MAP_FAILED))
    {
      munmap(mapAddr, fSize);
    }

    // close the file
    if (fd >= 0)
    {
      ::close(fd);
      fd = -1;
      fSize = -1;
    }
  }

  //----------------------------------------------------------------------------

  MemFile::MemFile(MemFile&& other)
  {
    operator=(std::move(other));
  }

  //----------------------------------------------------------------------------

  MemFile& MemFile::operator=(MemFile&& other)
  {
    mapAddr = other.mapAddr;
    other.mapAddr = nullptr;

    fSize = other.fSize;
    other.fSize = -1;

    fd = other.fd;
    other.fd = -1;

    return *this;
  }

  //----------------------------------------------------------------------------

  string MemFile::getString(size_t idxStart) const
  {
    assertIndex(idxStart, 1);

    size_t len{0};
    size_t lMax = static_cast<size_t>(fSize) - idxStart;

    char* startPtr = static_cast<char*>(mapAddr) + idxStart;
    char* sPtr = startPtr;

    while ((len < lMax) && (*sPtr != 0)) ++len;

    // idStart pointed directly at a zero-byte
    if (len == 0) return string{};

    // we found a zero-terminator
    if (*sPtr == 0) return string{startPtr, len-1};  // do not include the zero-terminator in the result

    // we hit the end of the file without finding a zero-terminator
    return string{startPtr, len};
  }

  //----------------------------------------------------------------------------

  string MemFile::getString(size_t idxStart, int len) const
  {
    if (len < 0)
    {
      throw std::invalid_argument("MemFile: request for negative length string");
    }

    if (len == 0) return string{};

    assertIndex(idxStart, len);
    char* startPtr = static_cast<char*>(mapAddr) + idxStart;
    return string{startPtr, static_cast<size_t>(len)};
  }

  //----------------------------------------------------------------------------


}

#endif
