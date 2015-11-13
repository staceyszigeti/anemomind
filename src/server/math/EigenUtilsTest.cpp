/*
 *  Created on: 2015
 *      Author: Jonas Östlund <jonas@anemomind.com>
 */

#include <gtest/gtest.h>
#include <server/math/EigenUtils.h>

using namespace sail;
using namespace EigenUtils;

namespace {
  auto rng = makeRngForTests();

  double evalNormRatio(Eigen::MatrixXd A, Eigen::MatrixXd B, Eigen::VectorXd X) {
    auto a = A*X;
    auto b = B*X;
    return a.squaredNorm()/b.squaredNorm();
  }

  double evalNormRatio(Eigen::MatrixXd A, Eigen::VectorXd B,
                          Eigen::MatrixXd C, Eigen::VectorXd D, Eigen::VectorXd X) {
    auto a = A*X + B;
    auto b = C*X + D;
    return a.squaredNorm()/b.squaredNorm();
  }
}

TEST(LinearCalibrationTest, MinimizeNormFraction) {
  auto A = makeRandomMatrix(9, 3, &rng, 1.0);
  auto B = makeRandomMatrix(4, 3, &rng, 1.0);
  auto X = minimizeNormRatio(A, B);
  EXPECT_EQ(X.rows(), 3);
  EXPECT_EQ(X.cols(), 1);
  auto val = evalNormRatio(A, B, X);
  for (int i = 0; i < 12; i++) {
    EXPECT_LE(val, evalNormRatio(A, B, X + makeRandomMatrix(3, 1, &rng, 0.01)));
  }
}


TEST(LinearCalibrationTest, MinimizeNormFraction2) {
  auto A = makeRandomMatrix(9, 3, &rng, 1.0);
  auto B = makeRandomMatrix(9, 1, &rng, 1.0);
  auto C = makeRandomMatrix(5, 3, &rng, 1.0);
  auto D = makeRandomMatrix(5, 1, &rng, 1.0);
  auto X = minimizeNormRatio(A, B, C, D);
  EXPECT_EQ(X.rows(), 3);
  EXPECT_EQ(X.cols(), 1);
  auto val = evalNormRatio(A, B, C, D, X);
  for (int i = 0; i < 12; i++) {
    EXPECT_LE(val, evalNormRatio(A, B, C, D, X + makeRandomMatrix(3, 1, &rng, 0.01)));
  }
}
