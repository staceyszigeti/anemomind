/*
 * SplineUtilsTest.cpp
 *
 *  Created on: 28 Oct 2016
 *      Author: jonas
 */

#include <server/math/SplineUtils.h>
#include <gtest/gtest.h>
#include <fstream>
#include <server/common/LineKM.h>
#include <server/common/MDArray.h>

using namespace sail;


TEST(SplineUtilsTest, TemporalTest) {
  auto offset = TimeStamp::UTC(2016, 5, 4, 3, 3, 4);

  Array<TimeStamp> src{
    offset + 0.0_s,
    offset + 1.0_s,
    offset + 2.0_s,
    offset + 3.0_s,
    offset + 4.3_s
  };
  Span<TimeStamp> span(offset, offset + 5.0_s);

  Arrayd dst{9, 7, 4, 17, -3.4};


  TemporalSplineCurve c(span, 1.0_s, src, dst);

  EXPECT_NEAR(c.evaluate(offset + 0.0_s), 9.0, 1.0e-4);
  EXPECT_NEAR(c.evaluate(offset + 1.0_s), 7.0, 1.0e-4);
  EXPECT_NEAR(c.evaluate(offset + 2.0_s), 4.0, 1.0e-4);
  EXPECT_NEAR(c.evaluate(offset + 3.0_s), 17.0, 1.0e-4);
  EXPECT_NEAR(c.evaluate(offset + 4.3_s), -3.4, 1.0e-4);

  /*const int n = 300;
  MDArray2d v(n, 2);
  auto km = LineKM(0, n, -4.0, 4.0);
  for (int i = 0; i < n; i++) {
    auto x = km(i);
    auto y = c.evaluate(offset + x*1.0_s);
    v(i, 0) = x;
    v(i, 1) = y;
  }
  GnuplotExtra plot;
  plot.set_style("lines");
  plot.plot(v);
  plot.showonscreen();*/
}