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

#ifndef BASICTESTCLASS_H
#define	BASICTESTCLASS_H

#include <string>
#include <memory>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "../Sloppy/Logger/Logger.h"

using namespace std;
namespace boostfs = boost::filesystem;

class EmptyFixture
{

};

class BasicTestFixture : public ::testing::Test
{
protected:
  static constexpr char DB_TEST_FILE_NAME[] = "SqliteTestDB.db";

  virtual void SetUp ();
  virtual void TearDown ();

  string getTestDir () const;
  string genTestFilePath(string fName) const;
  boostfs::path tstDirPath;
  unique_ptr<Sloppy::Logger::Logger> log;
  void printStartMsg(string _tcName);
  void printEndMsg();

private:
  string tcName;
};

#endif /* BASICTESTCLASS_H */
