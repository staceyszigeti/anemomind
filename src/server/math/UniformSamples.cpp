/*
 *  Created on: 2014-10-10
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "UniformSamples.h"
#include <cassert>

namespace sail {

UniformSamples::UniformSamples(LineKM sampling_, Arrayd samples_) :
    _sampling(sampling_), _samples(samples_) {}

double UniformSamples::interpolateLinear(double x) const {
  int I[2];
  double W[2];
  _sampling.makeInterpolationWeights(x, I, W);
  return W[0]*_samples[I[0]] + W[1]*_samples[I[1]];
}

Arrayd UniformSamples::interpolateLinear(Arrayd X) const {
  return X.map<double>([&](double x) {return interpolateLinear(x);});
}

double UniformSamples::interpolateLinearDerivative(double x) const {
  int I[2];
  double W[2];
  _sampling.makeInterpolationWeights(x, I, W);
  return (_samples[I[1]] - _samples[I[0]])/_sampling.getK();
}

Arrayd UniformSamples::interpolateLinearDerivative(Arrayd X) const {
  return X.map<double>([&](double x) {return interpolateLinearDerivative(x);});
}

Arrayd UniformSamples::makeCentredX() {
  int sampleCount = _samples.size() - 1;
  LineKM map(0, 1, _sampling(0.5), _sampling(1.5));
  Arrayd dst(sampleCount);
  for (int i = 0; i < sampleCount; i++) {
    dst[i] = map(i);
  }
  return dst;
}


}
