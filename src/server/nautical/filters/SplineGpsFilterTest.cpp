/*
 * SplineGpsFilterTest.cpp
 *
 *  Created on: 31 Oct 2016
 *      Author: jonas
 */


#include <server/nautical/filters/SplineGpsFilter.h>
#include <gtest/gtest.h>
#include <server/common/ArrayBuilder.h>


using namespace sail;

auto offset = TimeStamp::UTC(2016, 11, 1, 8, 17, 0.0);

TEST(SplineTest, Test1) {
  ArrayBuilder<TimedValue<GeographicPosition<double>>> positions0;
  int m = 30;
  for (int i = 0; i < m; i++) {
    auto t = offset + double(i)*1.0_s;
    auto p = GeographicPosition<double>(34.0_deg + (0.0001*i)*1.0_deg,
        44.0_deg, 0.0_m);
    TimedValue<GeographicPosition<double>> tv(t, p);
    positions0.add(tv);
  }

  auto positions = positions0.get();
  TimeMapper mapper{offset, 2.0_s, 15};

  auto curves = SplineGpsFilter::filter(positions,
      Array<TimedValue<HorizontalMotion<double>>>(),
      Array<TimeMapper>{mapper}, SplineGpsFilter::Settings());

  EXPECT_EQ(curves.size(), 1);
  for (int i = 0; i < m; i++) {
    auto pos = curves[0].evaluateGeographicPosition(
        offset + double(i)*1.0_s);

    EXPECT_NEAR(pos.lon().degrees(), 34.0 + (0.0001*i), 1.0e-6);
    EXPECT_NEAR(pos.lat().degrees(), 44.0, 1.0e-6);
    EXPECT_NEAR(pos.alt().meters(), 0.0, 1.0e-3);
  }

  auto firstPos = ECEF::convert(positions.first().value);
  auto lastPos = ECEF::convert(positions.last().value);
  auto timeDif = positions.last().time - positions.first().time;
  double distMeters = 0.0;
  for (int i = 0; i < 3; i++) {
    distMeters += sqr(firstPos.xyz[i].meters() - lastPos.xyz[i].meters());
  }
  distMeters = sqrt(distMeters);
  double speed = (distMeters/timeDif.seconds());

  EXPECT_NEAR(speed, 8.0, 2.0);

  auto motion = curves[0].evaluateHorizontalMotion(offset + 15.0_s);
  EXPECT_NEAR(motion[0].metersPerSecond(), speed, 1.0e-3);
  EXPECT_NEAR(motion[1].metersPerSecond(), 0.0, 1.0e-3);
}

TEST(SplineTest, Test2) {
  ArrayBuilder<TimedValue<GeographicPosition<double>>> positions0;
  int m = 30;
  for (int i = 0; i < m; i++) {
    auto t = offset + double(i)*1.0_s;
    auto lon = 34.0_deg + (0.0001*i)*1.0_deg;
    auto lat = 44.0_deg;

    if (i == 8) {
      lon = 39.0_deg;
      lat = -27.0_deg;
    }

    auto p = GeographicPosition<double>(lon, lat, 0.0_m);

    TimedValue<GeographicPosition<double>> tv(t, p);
    positions0.add(tv);
  }

  auto positions = positions0.get();
  TimeMapper mapper{offset, 2.0_s, 15};

  SplineGpsFilter::Settings settings;

  auto curves = SplineGpsFilter::filter(positions,
      Array<TimedValue<HorizontalMotion<double>>>(),
      Array<TimeMapper>{mapper}, settings);

  auto tol = 0.001;

  EXPECT_EQ(curves.size(), 1);
  for (int i = 0; i < m; i++) {
    auto pos = curves[0].evaluateGeographicPosition(
        offset + double(i)*1.0_s);

    EXPECT_NEAR(pos.lon().degrees(), 34.0 + (0.0001*i), tol);
    EXPECT_NEAR(pos.lat().degrees(), 44.0, tol);
    EXPECT_NEAR(pos.alt().meters(), 0.0, tol);
  }
}

namespace {
TimedValue<HorizontalMotion<double>> tv(double s,
      const HorizontalMotion<double> &m) {
    return TimedValue<HorizontalMotion<double>>(offset + s*1.0_s, m);
  }
}

TEST(SplineTest, Test3) {
  auto pos0 = GeographicPosition<double>(34.0_deg, 9.0_deg, 0.0_m);
  Array<TimedValue<GeographicPosition<double>>> positions{
    TimedValue<GeographicPosition<double>>{offset, pos0}
  };

  auto m = HorizontalMotion<double>(-1.0_mps, 0.0_mps);

  ArrayBuilder<TimedValue<HorizontalMotion<double>>> motions;
  for (int i = 0; i < 30; i++) {
    motions.add(tv(i, m));
  }

  TimeMapper mapper{offset, 2.0_s, 15};

  SplineGpsFilter::Settings settings;
  settings.lmSettings.verbosity = 2;

  auto curves = SplineGpsFilter::filter(positions, motions.get(),
      Array<TimeMapper>{mapper}, settings);

  EXPECT_EQ(curves.size(), 1);
  auto curve = curves[0];

  {
    auto pos = curve.evaluateGeographicPosition(offset);
    EXPECT_NEAR(pos.lon().degrees(), 34.0, 1.0e-6);
    EXPECT_NEAR(pos.lat().degrees(), 9.0, 1.0e-6);
    EXPECT_NEAR(pos.alt().meters(), 0.0, 1.0e-6);
  }

  {
    auto m = curve.evaluateHorizontalMotion(offset);
    EXPECT_NEAR(m[0].metersPerSecond(), -1.0, 1.0e-2);
    EXPECT_NEAR(m[1].metersPerSecond(), .0, 1.0e-2);
  }

  /*{
    auto m = curve.evaluateHorizontalMotion(offset + 9.0_s);
    EXPECT_NEAR(m[0].metersPerSecond(), -1.0, 1.0e-2);
    EXPECT_NEAR(m[1].metersPerSecond(), .0, 1.0e-2);
  }*/
}

