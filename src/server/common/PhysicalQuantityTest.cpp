/*
 *  Created on: 2014-02-13
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <cmath>
#include "PhysicalQuantity.h"
#include <gtest/gtest.h>

using namespace sail;

TEST(PhysQuantTest, CircumferenceTest) {
  Length<double> circumference = Length<double>::meters(4.0e7);
  double minutes = 60.0*Angle<double>::radians(2.0*M_PI).toDegrees();
  double oneNauticalMileMeters = circumference.toMeters()/minutes;
  EXPECT_NEAR(oneNauticalMileMeters, Length<double>::nauticalMiles(1.0).toMeters(), 30.0);
  EXPECT_NEAR(circumference.toNauticalMiles(), minutes, 40);
}

TEST(PhysQuantTest, TimeTest) {
  double n = 34.0;
  EXPECT_NEAR(Time<double>::seconds(n).toSeconds(), n, 1.0e-9);
  EXPECT_NEAR(Time<double>::seconds(60).toMinutes(), 1.0, 1.0e-9);
}

TEST(PhysQuantTest, HydroptereTest) {
  // « sustaining a speed of 52.86 knots (97.90 km/h; 60.83 mph) »
  Velocity<double> va = Velocity<double>::knots(52.86);
  Velocity<double> vb = Velocity<double>::kilometersPerHour(97.90);
  Velocity<double> vc = Velocity<double>::milesPerHour(60.83);
  EXPECT_NEAR(va.toMetersPerSecond(), vb.toMetersPerSecond(), 0.1);
  EXPECT_NEAR(va.toMetersPerSecond(), vc.toMetersPerSecond(), 0.1);
}

TEST(PhysQuantTest, WalkTest) {
  // When I am in a hurry, I walk
  Length<double> length = Length<double>::kilometers(1.0);
  // in
  Time<double> time = Time<double>::minutes(10);

  // Then my speed is
  Velocity<double> velA = Velocity<double>::kilometersPerHour(length.toKilometers()/time.toHours());
  Velocity<double> velB = Velocity<double>::metersPerSecond(length.toMeters()/time.toSeconds());

  EXPECT_NEAR(velA.toKnots(), velB.toKnots(), 1.0e-5);
}

TEST(PhysQuantTest, AngleTest) {
  Angle<double> a = Angle<double>::degrees(30);
  Angle<double> b = Angle<double>::degrees(60);
  Angle<double> a2 = Angle<double>::radians(M_PI/6.0);
  Angle<double> b2 = Angle<double>::radians(M_PI/3.0);
  EXPECT_NEAR(a.toDegrees(), a2.toDegrees(), 1.0e-5);
  EXPECT_NEAR(b.toDegrees(), b2.toDegrees(), 1.0e-5);
  EXPECT_NEAR(sin(a), 1.0/2.0, 1.0e-6);
  EXPECT_NEAR(sin(b), sqrt(3)/2.0, 1.0e-6);
  EXPECT_NEAR(sin(a)*sin(a) + cos(a)*cos(a), 1.0, 1.0e-6);
  EXPECT_NEAR(cos(a2)*cos(b2) - sin(a2)*sin(b2), cos(a.toRadians() + b.toRadians()), 1.0e-5);
}


TEST(PhysQuantTest, NewPQTest) {
  Mass<double> a = Mass<double>::kilograms(30.0);
  Mass<double> b = Mass<double>::kilograms(34.0);

  // The + operator is inherited
  Mass<double> c = a + b;

  EXPECT_NEAR(c.toKilograms(), 64.0, 1.0e-6);
}

TEST(PhysQuantTest, NewPQTest2) {
  Mass<double> sum = Mass<double>::lispund(0.0);
  for (int i = 0; i < 20; i++) {
    sum = sum + Mass<double>::lispund(1.0);
  }
  EXPECT_NEAR(sum.toSkeppund(), 1.0, 1.0e-6);
}
