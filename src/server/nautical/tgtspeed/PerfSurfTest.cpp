/*
 * PerfSurfTest.cpp
 *
 *  Created on: 12 Nov 2017
 *      Author: jonas
 */

#include <server/nautical/tgtspeed/PerfSurf.h>
#include <random>
#include <server/common/math.h>
#include <gtest/gtest.h>
#include <server/plot/CairoUtils.h>
#include <stdlib.h>

using namespace sail;

auto unit = 1.0_kn;

// A speed function that given wind speed returns the maximum boat
// speed. Just hypothetical, for testing.
Velocity<double> trueMaxSpeed(const Velocity<double>& src) {
  return unit*10.0*(1 - exp(double(-src/13.0_kn)));
}

std::default_random_engine rng(0);

struct SmoothGen {
  SmoothGen(double maxVal, double maxChange)
    : _distrib(-maxChange, maxChange),
      _maxVal(maxVal) {}

  double operator()(double x) {
    return clamp<double>(x + _distrib(rng), 0, _maxVal);
  }
private:
  std::uniform_real_distribution<double> _distrib;
  double _maxVal;
};

Velocity<double> resolution = 0.5_mps;

// Represent the wind as a linear combination of some "vertices"
Array<WeightedIndex> encodeWindSpeed(const Velocity<double>& v) {
  double k = v/resolution;
  double fi = floor(k);
  int i = int(fi);
  double lambda = k - fi;
  return {{i, 1.0 - lambda}, {i+1, lambda}};
}

Velocity<double> decodeWindSpeed(const Array<WeightedIndex>& w) {
  Velocity<double> sum = 0.0_kn;
  for (auto x: w) {
    sum += x.weight*x.index*resolution;
  }
  return sum;
}

TEST(PerfSurfTest, WindCoding) {
  auto y = 12.34_mps;
  EXPECT_NEAR(decodeWindSpeed(encodeWindSpeed(y)).knots(), y.knots(), 1.0e-5);
}

std::ostream& operator<<(std::ostream& s, const PerfSurfPt& pt) {
  s << "perf=" << pt.performance << " time=" << pt.time.toIso8601String() <<
      " boat-speed=" << pt.boatSpeed.knots() << "kn wind-speed"
      << decodeWindSpeed(pt.windVertexWeights).knots() << "kn";
  return s;
}

Array<PerfSurfPt> makeData(int n) {
  double perf = 0;
  Velocity<double> maxWind = 15.0_mps;
  Velocity<double> wind = 0.0_kn;
  TimeStamp offset = TimeStamp::UTC(2017, 11, 12, 15, 1, 0);
  Duration<double> stepSize = 0.1_s;
  Array<PerfSurfPt> pts(n);
  SmoothGen perfGen(1.0, 0.05);
  SmoothGen windGen(16.0_mps/unit, 0.1_mps/unit);
  for (int i = 0; i < n; i++) {
    TimeStamp time = offset + double(i)*stepSize;

    auto maxSpeed = trueMaxSpeed(wind);

    PerfSurfPt pt;
    pt.time = time;
    pt.performance = perf;
    pt.boatSpeed = perf*maxSpeed;
    pt.windVertexWeights = encodeWindSpeed(wind);

    pts[i] = pt;

    perf = perfGen(perf);
    wind = windGen(wind/unit)*unit;
  }
  return pts;
}

Array<Eigen::Vector2d> dataToPlotPoints(
    const Array<PerfSurfPt>& pts) {
  int n = pts.size();
  Array<Eigen::Vector2d> dst(n);
  for (int i = 0; i < n; i++) {
    auto pt = pts[i];
    dst[i] = Eigen::Vector2d(
        decodeWindSpeed(pt.windVertexWeights)/unit,
        pt.boatSpeed/unit);
  }
  return dst;
}

TEST(PerfSurfTest, TestIt) {
  int dataSize = 6000;
  auto data = makeData(dataSize);

  if (getenv("ANEMOPLOT")) {
    PlotUtils::Settings2d settings;
    auto p = Cairo::Setup::svg(
        "input_data.svg",
        settings.width, settings.height);
    Cairo::renderPlot(settings, [&](cairo_t* cr) {
      Cairo::plotDots(
          cr, dataToPlotPoints(data), 1.0);
    }, "Wind speed", "Boat speed", p.cr.get());
  }

}
