/*
 * TimedValueUtilsTest.cpp
 *
 *  Created on: 10 Nov 2016
 *      Author: jonas
 */

#include <server/common/TimedValueUtils.h>
#include <gtest/gtest.h>

using namespace sail;

namespace {
  auto offset = TimeStamp::UTC(2016, 11, 10, 17, 50, 0);

  TimeStamp t(double s) {
    return offset + s*1.0_s;
  }
}

TEST(TimedValueUtils, TestIt) {
  auto bds = listAllBounds({t(0), t(1), t(2), t(3), t(9)}, 2.0_s);
  EXPECT_EQ((Array<int>{0, 4, 5}), bds);
  EXPECT_EQ((Array<int>{0}), (listAllBounds({}, 2.0_s)));

  {
    auto spans = listTimeSpans({t(0), t(1), t(4), t(5)}, 2.0_s, true);
    EXPECT_EQ(spans, (Array<Span<TimeStamp>>{
      Span<TimeStamp>(t(0), t(1)),
      Span<TimeStamp>(t(4), t(5))
    }));
  }{
    auto spans = listTimeSpans({t(0), t(1), t(4)}, 2.0_s, false);
    EXPECT_EQ(spans, (Array<Span<TimeStamp>>{
      Span<TimeStamp>(t(0), t(1))
    }));
  }
}

TEST(TimedValueUtils, AssignSpan) {
  auto spans = Array<Span<TimeStamp>>{
    Span<TimeStamp>{offset, offset + 2.0_s},
    Span<TimeStamp>{offset + 2.0_s, offset + 4.0_s}
  };

  auto times = Array<TimeStamp>{
    offset - 1.0_s,
    offset + 1.0_s,
    offset + 3.0_s,
    offset + 5.0_s
  };

  auto inds = getTimeSpanPerTimeStamp(spans, times);
  EXPECT_EQ(inds, (Array<int>{-1, 0, 1, -1}));
}

TEST(TimedValueUtils, FindNearest) {
  auto a = Array<TimeStamp>{
    offset - 1.0_s,
    offset + 1.0_s,
    offset + 3.0_s,
    offset + 9.0_s
  };

  auto b = Array<TimeStamp>{
    offset + 1.5_s,
    offset + 4.0_s
  };

  auto inds = findNearestTimePerTime(a, b);
  EXPECT_EQ(inds, (Array<int>{0, 0, 1, 1}));
}

TEST(TimedValueUtils, WindowIndexer1) {
  TimeWindowIndexer indexer(offset, 1.0_minutes);
  {
    auto span = indexer.getWindowIndexSpan(offset + 1.0_s);
    EXPECT_EQ(span.minv(), -1);
    EXPECT_EQ(span.maxv(), 1);
  }{
    auto span = indexer.getWindowIndexSpan(offset + 29.0_s);
    EXPECT_EQ(span.minv(), -1);
    EXPECT_EQ(span.maxv(), 1);
  }{
    auto span = indexer.getWindowIndexSpan(offset + 31.0_s);
    EXPECT_EQ(span.minv(), 0);
    EXPECT_EQ(span.maxv(), 2);
  }
}

TEST(TimedValueUtils, WindowIndexer2) {
  TimeWindowIndexer indexer(offset, 1.0_minutes, 0.75);
  {
    auto span = indexer.getWindowIndexSpan(offset + 1.0_s);
    EXPECT_EQ(span.minv(), -3);
    EXPECT_EQ(span.maxv(), 1);
  }{
    auto span = indexer.getWindowIndexSpan(offset + 16.0_s);
    EXPECT_EQ(span.minv(), -2);
    EXPECT_EQ(span.maxv(), 2);
  }
}

TEST(TimedValueUtils, IndexedWindows) {
  IndexedWindows win(Span<TimeStamp>(offset, offset + 1.0_s),
      1.0_minutes);
  EXPECT_EQ(win.size(), 2);
  auto sp = win.getWindowIndexSpan(offset);
  EXPECT_EQ(sp.minv(), 0);
  EXPECT_EQ(sp.maxv(), 2);
}