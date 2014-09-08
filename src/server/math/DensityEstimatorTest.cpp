/*
 *  Created on: 2014-09-08
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <gtest/gtest.h>
#include <server/math/DensityEstimator.h>
#include <cmath>

using namespace sail;

TEST(DensityEstimatorTest, NaiveTest1d) {
  // A single training sample for the distribution.
  double c = 3.9;

  NaiveDensityEstimator est(1.0, Array<Arrayd>::args(Arrayd::args(c)));

  for (int i = 0; i < 30; i++) {
    double a = sin(i*123.324342);
    double b = sin(i*857.374522);
    EXPECT_EQ(std::abs(a - c) < std::abs(b - c),
              est.density(Arrayd::args(a)) > est.density(Arrayd::args(b)));
  }
}

TEST(DensityEstimatorTest, NaiveTest1dWithTwoPoints) {
  // A single training sample for the distribution.
  double c = 3.9;
  double d = 34.0;

  NaiveDensityEstimator est(1.0, Array<Arrayd>::args(Arrayd::args(c),
                                                     Arrayd::args(d)));

  for (int i = 0; i < 30; i++) {
    double a = sin(i*123.324342);
    double b = sin(i*857.374522);
    double amin = std::min(std::abs(a - c), std::abs(a - d));
    double bmin = std::min(std::abs(b - c), std::abs(b - d));
    EXPECT_EQ(amin < bmin,
              est.density(Arrayd::args(a)) > est.density(Arrayd::args(b)));
  }

  double middle = 0.5*(c + d);
  double middleDensity = est.density(Arrayd::args(middle));

  for (int i = 0; i < 30; i++) {
    double x = double(i)/29;
    double y = c + x*(d - c);
    EXPECT_LE(middleDensity, est.density(Arrayd::args(y)));
  }
}

TEST(DensityEstimatorTest, BandWidth) {
  Arrayd a = Arrayd::args(3.4);
  Arrayd b = Arrayd::args(9.89);

  NaiveDensityEstimator deWide(30, Array<Arrayd>::args(a));
  NaiveDensityEstimator deNarrow(0.89, Array<Arrayd>::args(a));
  EXPECT_LE(deNarrow.density(b)/deNarrow.density(a),
            deWide.density(b)/deWide.density(a));
}

TEST(DensityEstimatorTest, TwoDims) {
  Arrayd a = Arrayd::args(0, 0);
  Arrayd b = Arrayd::args(1, 1);
  Arrayd c = Arrayd::args(-0.5, 20);

  NaiveDensityEstimator est(1, Array<Arrayd>::args(a));
  EXPECT_LE(est.density(c), est.density(b));
  EXPECT_LE(est.density(b), est.density(a));
}


