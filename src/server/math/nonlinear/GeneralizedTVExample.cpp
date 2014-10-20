/*
 *  Created on: 2014-10-10
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <server/common/ArgMap.h>
#include <server/common/LineKM.h>
#include <server/math/nonlinear/GeneralizedTVAuto.h>
#include <server/common/Uniform.h>
#include <server/plot/extra.h>
#include <server/common/ScopedLog.h>
#include <server/common/string.h>

using namespace sail;

namespace {
  constexpr int fifth = 100;
  constexpr int count = 5*fifth;

  double testfun(double x) {
    int a = 2*fifth;
    int b = a + fifth;
    if (x < a) {
      return 0;
    } else if (x < b) {
      return LineKM(a, b, 0, 1)(x);
    }
    return 1;
  }

  Arrayd makeGT() {
    return Arrayd::fill(count, [](int i) {return testfun(i);});
  }

  Arrayd addNoise(Arrayd Y, double noise) {
    Uniform rng(-noise, noise);
    return Y.map<double>([&](double x) {return x + rng.gen();});
  }
}

int main(int argc, const char **argv) {
  ArgMap amap;
  int order = 2;
  double lambda = 60.0;
  double noise = 0.5;
  int iters = 30;
  int verbosity = 0;
  amap.registerOption("--order", "Set the order of the regularization term")
      .setArgCount(1).store(&order);
  amap.registerOption("--lambda", "Set the regularization weight")
    .setArgCount(1).store(&lambda);
  amap.registerOption("--noise", "Set the amplitude of the noise")
      .setArgCount(1).store(&noise);
  amap.registerOption("--iters", "Set the maximum number of iterations")
      .setArgCount(1).store(&iters);
  amap.registerOption("--auto", "Automatic tuning of regularization");
  amap.registerOption("--verbosity", "Set verbosity")
      .setArgCount(1).store(&verbosity);

  amap.setHelpInfo("This program illustrates generalized TV filtering\n"
                   "of a 1-D signal. The order option sets the order\n"
                   "of the differential operator. Standard TV denoising\n"
                   "has an order of 1. \n"
                   "\n"
                   "The default options should result in a filtered signal\n"
                   "of a signal that faithfully approximates the original\n"
                   "\n"
                   "An order of 1, however, can result in poor\n"
                   "recovery if the underlying signal is no piecewise\n"
                   "constant. Try for instance to call this program as:\n"
                   "\n"
                   "./math_nonlinear_GeneralizedTVExample --order 1 --lambda 4\n"
                   "\n"
                   "The recovered signal will have a staircase like shape.\n");
  if (amap.parseAndHelp(argc, argv)) {
    ScopedLog::setDepthLimit(verbosity);
    GeneralizedTV tv(iters);
    Arrayd Ygt = makeGT();
    Arrayd Ynoisy = addNoise(Ygt, noise);
    Arrayd X = GeneralizedTV::makeDefaultX(count);

    UniformSamples Yfiltered;
    if (amap.optionProvided("--auto")) {
      GeneralizedTVAuto autotv(tv);
      Yfiltered = autotv.filter(Ynoisy, order);
    } else {
      Yfiltered = tv.filter(Ynoisy, order, lambda);
    }


    GnuplotExtra plot;
    plot.set_style("lines");
    plot.plot_xy(X, Ygt, "Ground truth signal");
    plot.plot_xy(X, Ynoisy, "Noisy signal");
    plot.plot_xy(X, Yfiltered.interpolateLinear(X), "Filtered signal");
    plot.show();

    return 0;
  }
  return -1;
}



