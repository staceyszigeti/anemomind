/*
 *  Created on: 2015
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#ifndef SERVER_MATH_NONLINEAR_SPARSECURVEFIT_H_
#define SERVER_MATH_NONLINEAR_SPARSECURVEFIT_H_

#include <server/math/SparsityConstrained.h>
#include <server/common/logging.h>
#include <server/math/nonlinear/SignalUtils.h>
#include <server/common/MDArray.h>
#include <Eigen/Sparse>
#include <vector>

namespace sail {
namespace SparseCurveFit {

typedef Eigen::Triplet<double> Triplet;

Array<Spani> makeReg(int order, int firstRowOffset, int firstColOffset,
    int dim, int count, std::vector<Triplet> *dst);

template <int Dim>
Array<Spani> makeDataFitness(int firstRowOffset, int firstColOffset,
    Array<Observation<Dim> > observations, int sampleCount,
    std::vector<Triplet> *dst, Eigen::VectorXd *rhs) {
  int count = observations.size();
  int totalDim = Dim*count;
  int sampleDim = Dim*sampleCount;

  for (int i = 0; i < count; i++) {
    Observation<Dim> x = observations[i];
    int colA = firstColOffset + Dim*x.weights.lowerIndex;
    int colB = firstColOffset + Dim*x.weights.upperIndex();
    int iDim = i*Dim;
    int rowOffset = firstRowOffset + iDim;
    for (int k = 0; k < Dim; k++) {
      int row = rowOffset + k;

      // Express a point on the curve as a linear combination of the samples.
      dst->push_back(Triplet(row, colA + k, x.weights.lowerWeight));
      dst->push_back(Triplet(row, colB + k, x.weights.upperWeight));

      (*rhs)(row) = x.data[k];
      int slackCol = firstColOffset + sampleDim + iDim + k;

      // On the same row as the observation,
      // there is also a slack variable. For inliers,
      // this slack variable will be zero. For outliers,
      // it can be anything that minimizes the datafit.
      // The algorithm will automatically select the best
      // choice of slack variables to be put to zero.
      dst->push_back(Triplet(row, slackCol, 1.0));

      // This row is part of the constraint, that
      // forces some slack variables to zero.
      dst->push_back(Triplet(row + totalDim, slackCol, 1.0));
    }
  }
  return Spani(0, count).map<Spani>([&](int index) {
    int offset = firstRowOffset + totalDim + Dim*index;
    return Spani(offset, offset + Dim);
  });
}



struct Settings {
  // These are the settings of the underlying algorithm used to solve the problem.
  SparsityConstrained::Settings spcstSettings;

  // Select this fraction of all observations to use as inliers, ignore the others.
  // An alternative to this would have been to have some inlier threshold, sigma, so that
  // if the distance between the observation and the fitted curve is less than sigma, then
  // the observation should be treated as an inlier. In that case, we should know more or less
  // the noise distribution, but we might not know it. The problem with choosing sigma is that
  // a value that is too low will make us overfit to a few samples, whereas a value which is too high
  // will also include outliers among the samples treated as inliers.
  // On the other hand, when picking
  // a fraction of all the measurements and use to perform the fit, we should be sure that the inlier
  // rate is not lower than that fraction, because then it is going to break. And we should not choose
  // the fraction too low, because then we are going to overfit to a few samples. In either case,
  // no matter if we choose fraction-based outlier rejection or threshold-based rejection, we will
  // have to tune a parameter because one size doesn't fit all. But in most cases, I believe that it
  // is fair to say that at least around the best 60% of the samples should be high quality
  // observations even for crappy gps devices.
  double inlierRate = 0.6;

  // For which derivative order we count the discontinuities.
  // A regOrder of 1 would mean a constant piecewise curve.
  // A regOrder of 2 means a continuous curve of piecewise straight line segments,
  //   or its first derivative being piecewise constant.
  // A regorder of 3 means that the second derivative of the curve is piecewise constant.
  int regOrder = 3;
};



struct Results {
  Arrayb inliers;
  MDArray2d samples; // One reconstructed sample per row.
};

Results assembleResults(int dim, int sampleCount, int inlierCount, const Eigen::VectorXd &solution);

/*
 *
 * The main function to solve the problem.
 *
 */
template <int Dim>
Results fit(int sampleCount, int discontinuityCount,
    Array<Observation<Dim> > observations, const Settings &settings) {
  CHECK(0 <= discontinuityCount);
  CHECK(observations.all([&](const Observation<Dim> &x) {
    return 0 <= x.weights.lowerIndex && x.weights.upperIndex() < sampleCount;
  }));
  int regCount = (sampleCount - settings.regOrder);
  int coefsPerReg = (settings.regOrder + 1);
  int regRowCount = Dim*regCount;
  int regElemCount = coefsPerReg*regRowCount;
  int dataElemCount = Dim*4*observations.size();
  int dataRowCount = observations.size()*Dim*2;
  int rowCount = dataRowCount + regRowCount;

  Eigen::VectorXd rhs = Eigen::VectorXd::Zero(rowCount);
  std::vector<Triplet> elements;
  elements.reserve(dataElemCount + regElemCount);
  Array<Spani> slackSpans = makeDataFitness(0, 0,
      observations, sampleCount,
      &elements, &rhs);
  CHECK(slackSpans.last().maxv() == dataRowCount);
  Array<Spani> regSpans = makeReg(settings.regOrder, dataRowCount, 0,
      Dim, regCount, &elements);
  CHECK(elements.back().col() <= Dim*sampleCount);
  std::cout << EXPR_AND_VAL_AS_STRING(dataRowCount) << std::endl;
  CHECK(regSpans.first().minv() == dataRowCount);
  CHECK(regSpans.last().maxv() == rowCount);

  int inlierCount = int(round(observations.size()*settings.inlierRate));
  CHECK(0 <= discontinuityCount);
  int activeRegCount = regCount - discontinuityCount;

  Array<SparsityConstrained::ConstraintGroup> groups{
    SparsityConstrained::ConstraintGroup{slackSpans, inlierCount},
    SparsityConstrained::ConstraintGroup{regSpans, activeRegCount}
  };

  int colCount = Dim*sampleCount + Dim*observations.size();
  Eigen::SparseMatrix<double> lhs(rowCount, colCount);
  lhs.setFromTriplets(elements.begin(), elements.end());
  Eigen::VectorXd solution =
      SparsityConstrained::solve(lhs, rhs, groups, settings.spcstSettings);
  CHECK(solution.size() == colCount);
  return assembleResults(Dim, sampleCount, inlierCount, solution);
}


}
}

#endif /* SERVER_MATH_NONLINEAR_SPARSECURVEFIT_H_ */
