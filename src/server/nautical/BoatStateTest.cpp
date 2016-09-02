/*
 * BoatStateTest.cpp
 *
 *  Created on: 26 Aug 2016
 *      Author: jonas
 */

#include <server/nautical/BoatState.h>
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <device/Arduino/libraries/PhysicalQuantity/PhysicalQuantity.h>
#include <ceres/jet.h>
#include <Eigen/Geometry>
#include <random>

using namespace sail;

typedef BoatState<double> BS;
typedef ceres::Jet<double, 4> ADType;

namespace {
  bool eq(
      const HorizontalMotion<double> &a,
      const HorizontalMotion<double> &b) {
    return std::abs(a[0].knots() - b[0].knots()) < 1.0e-6
        && std::abs(a[1].knots() - b[1].knots()) < 1.0e-6;
  }

  HorizontalMotion<double> hm(
      const Velocity<double> &x,
      const Velocity<double> &y) {
    return HorizontalMotion<double>(x, y);
  }
}


TEST(BoatStateTest, OrientationOrthonormality) {
  std::default_random_engine rng;
  std::uniform_real_distribution<double>
    distrib(0.0, 2.0*M_PI);
  auto randomAngle = [&]() {
    auto value = Angle<double>::radians(distrib(rng));
    return value;
  };

  for (int i = 0; i < 30; i++) {
    AbsoluteOrientation o{
      randomAngle(), randomAngle(), randomAngle()
    };

    Eigen::Matrix<double, 3, 3> R = orientationToMatrix(o);
    Eigen::Matrix3d RtR = R.transpose()*R;

    EXPECT_TRUE(
        (isZero<double, 3, 3>(
            RtR - Eigen::Matrix3d::Identity(),
            1.0e-5)));
  }
}

TEST(BoatStateTest, ElementaryOrientations) {
  double k = 1.0/sqrt(2.0);
  { // heading
    AbsoluteOrientation o{45.0_deg, 0.0_deg, 0.0_deg};
    auto R = orientationToMatrix(o);
    EXPECT_LT(
        (R*Eigen::Vector3d(1, 0, 0)
           - Eigen::Vector3d(k, -k, 0.0)).norm(), 1.0e-6);
    EXPECT_LT(
        (R*Eigen::Vector3d(0, 1, 0)
           - Eigen::Vector3d(k, k, 0.0)).norm(), 1.0e-6);
    EXPECT_LT(
        (R*Eigen::Vector3d(0, 0, 1)
           - Eigen::Vector3d(0, 0, 1)).norm(), 1.0e-6);
  }{ // roll
    AbsoluteOrientation o{0.0_deg, 45.0_deg, 0.0_deg};
    auto R = orientationToMatrix(o);
    EXPECT_LT(
        (R*Eigen::Vector3d(1, 0, 0)
           - Eigen::Vector3d(k, 0, -k)).norm(), 1.0e-6);
    EXPECT_LT(
        (R*Eigen::Vector3d(0, 1, 0)
           - Eigen::Vector3d(0, 1, 0)).norm(), 1.0e-6);
    EXPECT_LT(
        (R*Eigen::Vector3d(0, 0, 1)
           - Eigen::Vector3d(k, 0, k)).norm(), 1.0e-6);
  }{ // pitch
    AbsoluteOrientation o{0.0_deg, 0.0_deg, 45.0_deg};
    auto R = orientationToMatrix(o);
    EXPECT_LT(
        (R*Eigen::Vector3d(1, 0, 0)
           - Eigen::Vector3d(1, 0, 0)).norm(), 1.0e-6);
    EXPECT_LT(
        (R*Eigen::Vector3d(0, 1, 0)
           - Eigen::Vector3d(0, k, k)).norm(), 1.0e-6);
    EXPECT_LT(
        (R*Eigen::Vector3d(0, 0, 1)
           - Eigen::Vector3d(0, -k, k)).norm(), 1.0e-6);
  }
}

TEST(BoatStateTest, VariousProperties) {
  GeographicPosition<double> pos(45.0_deg, 45.0_deg);

  auto gps = hm(0.0_kn, 0.0_kn);
  auto wind = hm(0.0_kn, 0.0_kn);
  auto current = hm(0.0_kn, 0.0_kn);
  auto angle = 0.0_deg;

  BS bs0(pos, gps, wind,
      current, AbsoluteOrientation::onlyHeading(angle));

  EXPECT_EQ(bs0, bs0);

  EXPECT_TRUE(eq(bs0.windOverGround(), hm(0.0_kn, 0.0_kn)));
  EXPECT_TRUE(eq(bs0.currentOverGround(), hm(0.0_kn, 0.0_kn)));
  EXPECT_TRUE(eq(bs0.boatOverGround(), hm(0.0_kn, 0.0_kn)));

  EXPECT_TRUE(bs0.valid());

  auto angle2 = 45.0_deg;
  {
    BS bs2(pos, gps, wind, current,
        AbsoluteOrientation::onlyHeading(angle2));
    /*Eigen::Vector3d wh = bs2.worldHeadingVector();
    EXPECT_NEAR(wh(0), -1.0/sqrt(2.0), 1.0e-6);
    EXPECT_NEAR(wh(1), 1.0/sqrt(2.0), 1.0e-6);
    EXPECT_NEAR(wh(2), 0.0, 1.0e-6);*/
    EXPECT_NEAR(bs2.heading().degrees(), 45.0, 1.0e-6);
  }
}

TEST(BoatStateTest, LeewayTestNoDrift) {
  GeographicPosition<double> pos(45.0_deg, 45.0_deg);
  HorizontalMotion<double> gps(1.0_kn, 1.0_kn);
  HorizontalMotion<double> wind(0.0_kn, -1.0_kn);
  HorizontalMotion<double> current(0.0_kn, 0.0_kn);
  auto angle = 45.0_deg;

  BS bs(pos, gps, wind, current,
      AbsoluteOrientation::onlyHeading(angle));


  EXPECT_TRUE(eq(hm(-1.0_kn, -2.0_kn), bs.apparentWind()));
  EXPECT_NEAR(bs.computeLeewayError(0.0_deg).knots(), 0.0, 1.0e-6);
  auto expectedAWA = (45.0_deg) - Angle<double>::radians(atan(2.0));
  EXPECT_NEAR(bs.AWA().radians(),
      expectedAWA.radians(), 1.0e-6);
  EXPECT_LT(bs.AWA().radians(), 0.0);
  EXPECT_NEAR(bs.computeAWAError(expectedAWA).knots(), 0.0, 1.0e-6);
}

TEST(BoatStateTest, InterpolationTest) {
  BS a, b;
  {
    GeographicPosition<double> pos(45.0_deg, 45.0_deg);
    HorizontalMotion<double> gps(1.0_kn, 1.0_kn);
    HorizontalMotion<double> wind(0.0_kn, -1.0_kn);
    HorizontalMotion<double> current(0.0_kn, 0.0_kn);
    auto angle = 45.0_deg;
    a = BS(pos, gps, wind, current,
        AbsoluteOrientation::onlyHeading(angle));
  }{
    GeographicPosition<double> pos(-45.0_deg, 135.0_deg);
    HorizontalMotion<double> gps(4.0_kn, 5.0_kn);
    HorizontalMotion<double> wind(-12.0_kn, -9.0_kn);
    HorizontalMotion<double> current(3.0_kn, 4.0_kn);
    auto angle = 70.0_deg;
    b = BS(pos, gps, wind, current,
        AbsoluteOrientation::onlyHeading(angle));
  }
  EXPECT_EQ(a, interpolate(0.0, a, b));
  EXPECT_EQ(b, interpolate(1.0, a, b));
}
