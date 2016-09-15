/*
 * BandedLevMarTest.cpp
 *
 *  Created on: 15 Sep 2016
 *      Author: jonas
 */

#include <server/common/ArrayIO.h>
#include <server/math/BandedLevMar.h>
#include <gtest/gtest.h>
#include <random>
#include <server/common/LineKM.h>

using namespace sail;
using namespace sail::BandedLevMar;

namespace {
  struct DataCost{
    double data;

    static const int inputCount = 1;
    static const int outputCount = 1;

    template <typename T>
    bool evaluate(const T *X, T *y) const {
      y[0] = X[0] - data;
      return true;
    }
  };

  struct RegCost {
    double weight;
    static const int inputCount = 3;
    static const int outputCount = 1;

    template <typename T>
    bool evaluate(const T *X, T *y) const {
      y[0] = weight*(X[0] - 2.0*X[1] + X[2]);
      return true;
    }
  };

}

TEST(BandedLevmarTest, BasicLineFit) {
  std::default_random_engine rng(0);

  std::uniform_real_distribution<double> distrib(0.0, 0.01);

  const int n = 30;

  LineKM gtLine(0, n, 3.4, 9.5);

  Problem<double> problem;
  for (int i = 0; i < n; i++) {
    problem.addCostFunction(Spani(i, i+1),
        new DataCost{gtLine(i) + distrib(rng)});
  }

  EXPECT_EQ(0, problem.bandWidth());
  EXPECT_EQ(n, problem.paramCount());
  EXPECT_EQ(n, problem.residualCount());

  for (int i = 1; i < n-1; i++) {
    problem.addCostFunction(Spani(i-1, i+2), new RegCost{100});
  }
  EXPECT_EQ(2, problem.bandWidth());
  EXPECT_EQ(n, problem.paramCount());
  EXPECT_EQ(n + n-2, problem.residualCount());

  double X[n];
  for (int i = 0; i < n; i++) {
    X[i] = 0.0;
  }

  {
    BandMatrix<double> JtJ;
    MDArray2d minusJtF;

    EXPECT_TRUE(problem.fillNormalEquations(X, &JtJ, &minusJtF));

    BandedLU::forwardEliminate(&JtJ, &minusJtF, 1);

    auto JtJ0 = JtJ.makeDense();
    auto minusJtF0 = minusJtF.dup();
    Eigen::MatrixXd eJtJ = Eigen::Map<Eigen::MatrixXd>(JtJ0.ptr(), n, n);
    Eigen::MatrixXd eMinusJtF = Eigen::Map<Eigen::MatrixXd>(minusJtF0.ptr(), n, 1);
    Eigen::MatrixXd Y = eJtJ.lu().solve(eMinusJtF);

    std::cout << "The final JtJ is now \n" << JtJ.makeDense() << std::endl;
    for (int i = 0; i < n; i++) {
      EXPECT_NEAR(Y(i, 0), gtLine(i), 0.1);
      //EXPECT_NEAR(minusJtF(i, 0), gtLine(i), 0.1);
    }


  }
}



