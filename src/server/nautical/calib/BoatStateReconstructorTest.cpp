/*
 * BoatStateReconstructorTest.cpp
 *
 *  Created on: 22 Sep 2016
 *      Author: jonas
 */

#include <server/nautical/calib/BoatStateReconstructor.h>
#include <gtest/gtest.h>

using namespace sail;

namespace {
  TimeStamp offsetTime = TimeStamp::UTC(2016, 9, 22, 15, 17, 0);

  TimeStamp t(double s) {
    return offsetTime + Duration<double>::seconds(s);
  }

  TimeStampToIndexMapper mapper0{
    offsetTime, 2.0_s, 4};
}

TEST(BoatStateReconstructor, TimeMapper) {
  EXPECT_EQ(mapper0.offset, offsetTime);
  EXPECT_EQ(mapper0.period, 2.0_s);
  EXPECT_EQ(mapper0.sampleCount, 4);
  EXPECT_EQ(mapper0.map(offsetTime - 1.0_s), -1);
  EXPECT_EQ(mapper0.map(offsetTime - 9.0_s), -1);
  EXPECT_EQ(mapper0.map(offsetTime + 1.0_s), 0);
  EXPECT_EQ(mapper0.map(offsetTime + 3.0_s), 1);
  EXPECT_EQ(mapper0.map(offsetTime + 9.0_s), -1);

  struct Call {
    int index;
    Spani span;
  };
  std::vector<Call> calls;

  Array<TimedValue<Velocity<double> > > values{
    {t(-11.3), 3.4_kn},
    {t(0.1), 3.4_kn},
    {t(1.0), 4.4_kn},
    {t(1.5), 4.5_kn},
    {t(5), 9.0_kn},
    {t(11), 9.4_kn}
  };
  foreachSpan<Velocity<double> >(mapper0, values,
      [&](int index, Spani span) {
    calls.push_back(Call{index, span});
  });
  EXPECT_EQ(calls.size(), 2);
  auto c0 = calls[0];
  auto c1 = calls[1];
  EXPECT_EQ(c0.index, 0);
  EXPECT_EQ(c0.span.minv(), 1);
  EXPECT_EQ(c0.span.maxv(), 4);
  EXPECT_EQ(c1.index, 2);
  EXPECT_EQ(c1.span.minv(), 4);
  EXPECT_EQ(c1.span.maxv(), 5);
}

TEST(BoatStateReconstructor, ValueAccumulator) {
  TimeStampToIndexMapper mapper{
    offsetTime, 1.0_s, 30};

  std::map<std::string, Array<TimedValue<Velocity<double>>>> src{
    {"NMEA2000", {
        {t(0.4), 3.4_kn},
        {t(4.56), 2.0_kn},
        {t(9.6), 5.5_kn},
        {t(9.7), 5.4_kn}
    }},
    {"Anemobox", {
        {t(4.5), 9.9_kn},
        {t(4.6), 11.4_kn}
    }}
  };

  std::vector<std::string> sensorNames;
  std::vector<int> assignedIndices;

  ValueAccumulator<Velocity<double> > acc(mapper, src);
  EXPECT_EQ(acc.sensorIndices.size(), 2);
  for (auto kv: acc.sensorIndices) {
    sensorNames.push_back(kv.first);
    assignedIndices.push_back(kv.second);
  }
  std::sort(sensorNames.begin(), sensorNames.end());
  EXPECT_EQ(sensorNames, (std::vector<std::string>{"Anemobox", "NMEA2000"}));
  std::sort(assignedIndices.begin(), assignedIndices.end());
  EXPECT_EQ(assignedIndices, (std::vector<int>{0, 1}));
  EXPECT_EQ(acc.values.size(), 4 + 2);
  EXPECT_EQ(acc.valuesPerIndex.size(), 30);


}
