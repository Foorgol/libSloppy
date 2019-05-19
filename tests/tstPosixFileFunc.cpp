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

#include <string>
#include <iostream>

#include <boost/filesystem.hpp>

#include <gtest/gtest.h>

#include "../Sloppy/Utils.h"

using namespace std;

namespace boostfs = boost::filesystem;

//-------------------------------------------------
//
// IGNORE THIS FILE if we're building under Windows
//
//-------------------------------------------------
#ifndef WIN32

TEST(PosixFileFunc, GetCWD)
{
  auto cwdPath = boostfs::current_path();
  string cwd = cwdPath.native();
  cerr << "Workdir from boost: " << cwd << endl;
  cerr << "Workdir from Sloppy: " << Sloppy::getCurrentWorkDir() << endl;

  ASSERT_EQ(cwd, Sloppy::getCurrentWorkDir());
}

//----------------------------------------------------------------------------

TEST(PosixFileFunc, isFile)
{
  // this file is certain to exist
  string f{"/usr/bin/bash"};
  ASSERT_TRUE(Sloppy::isFile(f));

  // this file is certain to NOT exist
  f = "/usr/sdfkjhskfhs";
  ASSERT_FALSE(Sloppy::isFile(f));

  // this is certain to be a directory
  f = "/usr/lib";
  ASSERT_FALSE(Sloppy::isFile(f));
  f = "/usr/lib/";
  ASSERT_FALSE(Sloppy::isFile(f));
}

//----------------------------------------------------------------------------

TEST(PosixFileFunc, isDir)
{
  // this directory is certain to exist
  string d{"/usr/lib"};
  ASSERT_TRUE(Sloppy::isDirectory(d));

  // do NOT accept trailing slashes
  d = "/ust/lib/";
  ASSERT_FALSE(Sloppy::isDirectory(d));

  // do not accept files
  d = "/usr/bin/bash";
  ASSERT_FALSE(Sloppy::isDirectory(d));

  // handle special directories
  d =".";
  ASSERT_TRUE(Sloppy::isDirectory(d));
  d ="..";
  ASSERT_TRUE(Sloppy::isDirectory(d));
  d ="./..";
  ASSERT_TRUE(Sloppy::isDirectory(d));
}

#endif
