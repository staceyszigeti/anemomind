/*
 * SineFitTest.cpp
 *
 *  Created on: 14 Nov 2016
 *      Author: jonas
 */

#include <server/math/SineFit.h>
#include <gtest/gtest.h>

using namespace sail;

void testFor(const Sine &gt) {

  Array<Angle<double>> angles{
    -13.0_deg, 4.7_deg, 83.0_deg, 97.12_deg,
    917.0_deg
  };

  Array<std::pair<Angle<double>, double>> data(angles.size());
  for (int i = 0; i < angles.size(); i++) {
    auto alpha = angles[i];
    data[i] = std::pair<Angle<double>, double>(
        alpha, gt(alpha));
  }

  auto x0 = fit(1.0, data);
  EXPECT_TRUE(x0.defined());
  auto x = x0.get();

  for (auto angle: angles) {
    EXPECT_NEAR(x(angle), gt(angle), 1.0e-6);
  }

  std::cout << "Compare C: " << gt.C() << " and "
      << x.C() << std::endl;
  std::cout << "Compare D: " << gt.D() << " and "
      << x.D() << std::endl;
  std::cout << "Compared cos(phi): " << cos(gt.phi())
      << " and " << cos(x.phi()) << std::endl;
  std::cout << "Compare sin(phi): " << sin(gt.phi())
      << " and " << sin(x.phi()) << std::endl;
}

TEST(SineFit, FittingTests) {
  testFor(Sine(1.0, 1.0, 0.0_deg, 0.0));
  testFor(Sine(4.0, 1.0, 0.0_deg, 0.0));
  testFor(Sine(1.0, 9.0, 0.0_deg, 0.0));
  //testFor(Sine(1.0, 9.0, 34.0_deg, 0.0));
  //testFor(Sine(1.0, 1.0, 0.0_deg, 17.0));
}


