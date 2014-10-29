/*
 *  Created on: 2014-
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "GeneralizedTVAuto.h"
#include <cmath>
#include <server/common/DataSplits.h>
#include <cassert>
#include <server/common/math.h>
#include <server/math/nonlinear/StepMinimizer.h>
#include <server/common/ScopedLog.h>
#include <server/common/string.h>

namespace sail {

GeneralizedTVAuto::GeneralizedTVAuto(std::default_random_engine &engine,
    const GeneralizedTV &tv,
  double initX, double step, int maxIter) :
  _tv(tv), _initX(initX), _step(step), _maxIter(maxIter),
  _factor(log(2)), _engine(engine) {}



namespace {
  double calcFitnessScore(UniformSamplesd fitted, Arrayd X, Arrayd Y,
    Arrayb split) {
    double score = 0.0;
    int count = X.size();
    assert(count == Y.size());
    assert(count == split.size());
    for (int i = 0; i < count; i++) {
      if (!split[i]) {
        score += sqr(fitted.interpolateLinear(X[i]) - Y[i]);
      }
    }
    return score;
  }

  double evaluateCrossValidation(const GeneralizedTV &tv,
      UniformSamplesd initialSignal,
    Arrayd X, Arrayd Y, int order,
    double regularization, Arrayb split) {
      ENTERSCOPE("Evaluated TV cross validation");
      int count = X.size();
      assert(count == Y.size());
      assert(count == split.size());
      UniformSamplesd fitted = tv.filter(initialSignal, X.slice(split), Y.slice(split),
          order, regularization);
      return calcFitnessScore(fitted, X, Y, split);
  }

  double evaluateCrossValidation(const GeneralizedTV &tv,
      UniformSamplesd initialSignal,
    Arrayd X, Arrayd Y, int order,
    double regularization, Array<Arrayb> splits) {
    double score = 0.0;
    for (int i = 0; i < splits.size(); i++) {
      score += evaluateCrossValidation(tv, initialSignal,
          X, Y, order, regularization, splits[i]);
    }
    return score;
  }

  Array<Arrayb> makeDefaultSplits(Array<Arrayb> splits, int len,
    std::default_random_engine &engine) {
    if (splits.empty()) {
      return makeRandomSplits(4, len, engine);
    } else {
      for (int i = 0; i < splits.size(); i++) {
        if (splits[i].size() != len) {
          return makeRandomSplits(splits.size(), len, engine);
        }
      }
      return splits;
    }
  }
}

double GeneralizedTVAuto::optimizeRegWeight(UniformSamplesd initialSignal,
                Arrayd X, Arrayd Y,
                int order,
                Array<Arrayb> initSplits) const {
  ENTER_FUNCTION_SCOPE;
  assert(X.size() == Y.size());
  Array<Arrayb> splits = makeDefaultSplits(initSplits, X.size(), _engine);
  std::function<double(double)> objf = [=] (double x) {
    return evaluateCrossValidation(_tv, initialSignal,
      X, Y, order, exp(x), splits);
  };
  StepMinimizer minimizer(_maxIter);
  StepMinimizerState state(_initX, _step, objf(_initX));
  StepMinimizerState optstate = minimizer.minimize(state, objf);
  double w = exp(optstate.getX());
  SCOPEDMESSAGE(INFO, stringFormat("Tuned regweight to %.3g", w));
  return w;
}

UniformSamplesd GeneralizedTVAuto::filter(UniformSamplesd initialSignal,
                Arrayd X, Arrayd Y,
                int order,
                Array<Arrayb> initSplits) const {
  return _tv.filter(initialSignal, X, Y, order,
      optimizeRegWeight(initialSignal, X, Y, order, initSplits));
}

UniformSamplesd GeneralizedTVAuto::filter(Arrayd Y, int order, Array<Arrayb> splits) const {
  return filter(_tv.makeInitialSignal(Y),
        _tv.makeDefaultX(Y.size()), Y, order, splits);
}



}
