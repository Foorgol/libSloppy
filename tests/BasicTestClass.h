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
