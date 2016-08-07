#include <iostream>

#include <gtest/gtest.h>

#include <boost/date_time/local_time/local_time.hpp>

#include "../Sloppy/DateTime/DateAndTime.h"
#include "BasicTestClass.h"

using namespace boost::local_time;
using namespace Sloppy::DateTime;

TEST(Timestamps, testTimeConversion)
{
  // create a fake localtime object
  time_zone_ptr testZone{new posix_time_zone("TST-01:00:00")};
  LocalTimestamp lt(2000, 1, 1, 12, 0, 0, testZone);

  // convert to raw time
  time_t raw1 = lt.getRawTime();

  // construct UTC from this raw time
  UTCTimestamp utc(raw1);

  // convert to raw again
  time_t raw2 = utc.getRawTime();

  // both raws should be identical
  ASSERT_EQ(raw1, raw2);

  // construct a new LocalTimestamp, this time from raw
  LocalTimestamp lt2(raw2, testZone);

  // conversion back to raw should give identical values
  ASSERT_EQ(raw2, lt2.getRawTime());
}

//----------------------------------------------------------------------------

TEST(Timestamps, testEpoch)
{
  tz_database tzdb;
  tzdb.load_from_file("../tests/date_time_zonespec.csv");
  time_zone_ptr tzCEST = tzdb.time_zone_from_region("Europe/Berlin");
  ASSERT_TRUE(tzCEST != nullptr);

  // create a fake localtime object (CEST)
  LocalTimestamp lt(2015, 6, 27, 12, 0, 0, tzCEST);

  // 2015-06-27 is in summer time, so CEST applies
  //
  // CEST is 2 hours ahead of UTC / GMT, so the
  // equivalent UTC time is 2015-06-27, 10:00:00
  UTCTimestamp utc(2015, 6, 27, 10, 0, 0);

  // the epoch value for this UTC date is, according
  // to an internet converter:
  time_t expectedEpochVal = 1435399200;

  // convert local time to raw
  time_t raw1 = lt.getRawTime();
  ASSERT_EQ(expectedEpochVal, raw1);

  // convert UTC time to raw
  time_t raw2 = utc.getRawTime();
  ASSERT_EQ(expectedEpochVal, raw2);

  // create a timestamp from the epoch value
  lt = LocalTimestamp(expectedEpochVal, tzCEST);
  ASSERT_EQ("2015-06-27 12:00:00", lt.getTimestamp());

  // repeat the test with a timestamp
  // in CET (winter time)

  // create a fake localtime object (CET)
  lt = LocalTimestamp(2015, 1, 27, 11, 0, 0, tzCEST);

  // 2015-01-27 is in winter time, to CET applies
  //
  // CET is 1 hour ahead of UTC / GMT, so the
  // equivalent UTC time is 2015-01-27, 10:00:00
  utc = UTCTimestamp(2015, 1, 27, 10, 0, 0);

  // the epoch value for this UTC date is, according
  // to an internet converter:
  expectedEpochVal = 1422352800;

  // convert local time to raw
  raw1 = lt.getRawTime();
  ASSERT_EQ(expectedEpochVal, raw1);

  // convert UTC time to raw
  raw2 = utc.getRawTime();
  ASSERT_EQ(expectedEpochVal, raw2);

  // create a timestamp from the epoch value
  lt = LocalTimestamp(expectedEpochVal, tzCEST);
  ASSERT_EQ("2015-01-27 11:00:00", lt.getTimestamp());
}

//----------------------------------------------------------------------------

TEST(Timestamps, testGetters)
{
  // create a fake localtime object
  time_zone_ptr testZone{new posix_time_zone("TST-01:00:00")};
  LocalTimestamp lt(2000, 1, 1, 8, 3, 2, testZone);

  ASSERT_EQ("2000-01-01", lt.getISODate());
  ASSERT_EQ("08:03:02", lt.getTime());
  ASSERT_EQ("2000-01-01 08:03:02", lt.getTimestamp());
}

//----------------------------------------------------------------------------

TEST(Timestamps, testLocalTimestampFromISODate)
{
  time_zone_ptr testZone{new posix_time_zone("TST-01:00:00")};

  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("skjfh", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("2000", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("20000", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("2000-", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("2000-03", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("2000-05-", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("2000-05-sdfd", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("200-05-03", testZone));
  ASSERT_EQ(nullptr, LocalTimestamp::fromISODate("2000-15-03", testZone));

  upLocalTimestamp t = LocalTimestamp::fromISODate("2000-05-03", testZone);
  ASSERT_TRUE(t != nullptr);
  ASSERT_EQ("2000-05-03", t->getISODate());
  t = LocalTimestamp::fromISODate("2000-5-3", testZone);
  ASSERT_TRUE(t != nullptr);
  ASSERT_EQ("2000-05-03", t->getISODate());
}
