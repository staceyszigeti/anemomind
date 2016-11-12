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
#include <server/common/ArrayIO.h>

using namespace sail;

auto offset = TimeStamp::UTC(2016, 5, 4, 3, 3, 4);

TEST(SplineUtilsTest, TemporalTest) {

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
}

Eigen::Matrix<double, 1, 1> v1(double x) {
  Eigen::Matrix<double, 1, 1> dst;
  dst(0) = x;
  return dst;
}

TimeStamp t(double s) {
  return offset + s*1.0_s;
}

TEST(SplineUtilsTest, RobustSplineFitTest) {
  TimeMapper mapper(offset, 1.0_s, 9);

  auto settings = RobustSplineFit<1>::Settings();
  RobustSplineFit<1> fit(mapper, settings);
  int order = 0;

  fit.addObservation(t(0), order,
      v1(3), 1.0);

  fit.addObservation(t(1), order,
      v1(4), 1.0);

  auto coefs = fit.solve();
  auto basis = fit.basis();
  {
    auto x = evaluateSpline<1>(basis.build(0), coefs);
    EXPECT_NEAR(x(0), 3.0, 1.0e-6);
  }{
    auto x = evaluateSpline<1>(basis.build(1), coefs);
    EXPECT_NEAR(x(0), 4.0, 1.0e-6);
  }{
    auto x = evaluateSpline<1>(basis.build(6.0), coefs);
    EXPECT_NEAR(x(0), 9.0, 1.0e-6);
  }
}

TEST(SplineUtilsTest, RobustSplineFitTestWithOutlier) {
  TimeMapper mapper(offset, 1.0_s, 9);

  auto settings = RobustSplineFit<1>::Settings();
  RobustSplineFit<1> fit(mapper, settings);
  int order = 0;

  for (int i = 0; i < 9; i++) {
    auto y = i == 5? 300000000 : 4.0 + 0.5*i;
    std::cout << "Add observation at " << y << std::endl;
    fit.addObservation(t(i), order, v1(y), 1.0);
  }

  auto basis = fit.basis();
  auto coefs = fit.solve();
  for (int i = 0; i < 9; i++) {
    auto y = evaluateSpline<1>(basis.build(i), coefs);
    EXPECT_NEAR(y(0), 4.0 + 0.5*i, 1.0e-6);
  }
}
