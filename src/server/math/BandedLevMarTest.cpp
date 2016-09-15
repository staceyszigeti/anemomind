/*
 * BandedLevMarTest.cpp
 *
 *  Created on: 15 Sep 2016
 *      Author: jonas
 */

#include <server/math/BandedLevMar.h>
#include <gtest/gtest.h>
#include <random>
#include <server/common/LineKM.h>

using namespace sail;
using namespace sail::BandedLevMar;

namespace {
  struct DataCost{
    double data;

    int inputCount() const {return 1;}
    int outputCount() const {return 1;}

    template <typename T>
    bool evaluate(const T *X, T *y) const {
      y[0] = X[0] - data;
      return true;
    }
  };

  struct RegCost {
    double weight;
    int inputCount() const {return 3;}
    int outputCount() const {return 1;}

    template <typename T>
    bool evaluate(const T *X, T *y) const {
      y[0] = weight*(X[0] - 2.0*X[1] + X[2]);
      return true;
    }
  };

}

TEST(BandedLevmarTest, BasicLineFit) {
  std::default_random_engine rng(0);

  std::uniform_real_distribution<double> distrib(0.0, 4.0);

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

}



