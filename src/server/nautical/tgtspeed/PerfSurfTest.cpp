/*
 * PerfSurfTest.cpp
 *
 *  Created on: 12 Nov 2017
 *      Author: jonas
 */

#include <server/nautical/tgtspeed/PerfSurf.h>
#include <server/common/ArrayBuilder.h>
#include <random>
#include <server/common/math.h>
#include <gtest/gtest.h>
#include <server/plot/CairoUtils.h>
#include <stdlib.h>
#include <server/common/logging.h>
#include <server/common/LineKM.h>
#include <server/common/DOMUtils.h>
#include <server/common/string.h>

using namespace sail;

auto unit = 1.0_kn;

// A speed function that given wind speed returns the maximum boat
// speed. Just hypothetical, for testing.
Velocity<double> trueMaxSpeed(const Velocity<double>& src) {
  return unit*10.0*(1 - exp(double(-src/13.0_kn)));
}

std::default_random_engine rng(0);

// Used to produce a gradually varying signal, like a random walk.
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

TEST(PerfSurfTest, ConstraintedSolve) {
  Eigen::MatrixXd A(1, 2);
  A << 1, 0;
  Eigen::MatrixXd AtA = A.transpose()*A;

  Eigen::VectorXd X = solveConstrained(AtA, SystemConstraintType::Norm1);
  if (X(1) < 0) {
    X = -X;
  }
  EXPECT_NEAR(X(0), 0.0, 1.0e-6);
  EXPECT_NEAR(X(1), 1.0, 1.0e-6);

  Eigen::VectorXd Y = solveConstrained(AtA, SystemConstraintType::Sum1);
  std::cout << "Y = \n" << Y << std::endl;
  EXPECT_NEAR(Y(0), 0.0, 1.0e-6);
  EXPECT_NEAR(Y(1), 1.0, 1.0e-6);
}

Array<PerfSurfPt> makeData(int n) {
  double perf = 0;
  Velocity<double> maxWind = 15.0_mps;
  Velocity<double> wind = 0.0_kn;
  TimeStamp offset = TimeStamp::UTC(2017, 11, 12, 15, 1, 0);
  Duration<double> stepSize = 0.1_s;
  Array<PerfSurfPt> pts(n);
  SmoothGen perfGen(1.0, 0.025);
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

// Convert the performance surface points
// to something convenient that we can plot.
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

int getRequiredVertexCount(const Array<PerfSurfPt>& pts) {
  int n = 0;
  for (const auto& pt: pts) {
    for (const auto& w: pt.windVertexWeights) {
      n = std::max(n, w.index+1);
    }
  }
  return n;
}

Array<Velocity<double>> initializeVertices(int n) {
  Array<Velocity<double>> dst(n);
  for (int i = 0; i < n; i++) {
    dst[i] = double(i)*resolution;
  }
  return dst;
}


Array<Eigen::Vector2d> solutionToCoords(
    const Array<Velocity<double>>& src) {
  int n = src.size();
  Array<Eigen::Vector2d> dst(n);
  for (int i = 0; i < n; i++) {
    dst[i] = Eigen::Vector2d(
        double(double(i)*resolution/unit),
        double(src[i]/unit));
  }
  return dst;
}

void displaySolution(
    const Array<PerfSurfPt>& data,
    const Array<Array<Velocity<double>>>& optimized) {
  int solutionCount = optimized.size();

  auto hue = LineKM(0, solutionCount-1, 240.0, 360.0);

  PlotUtils::Settings2d plotSettings;
  auto p = Cairo::Setup::svg(
      "input_data.svg",
      plotSettings.width, plotSettings.height);
  Cairo::renderPlot(plotSettings, [&](cairo_t* cr) {
    for (int i = 0; i < solutionCount; i++) {
      LOG(INFO) << "Plot solution";
      Cairo::setSourceColor(cr, PlotUtils::HSV::fromHue(hue(i)*1.0_deg));
      Cairo::plotLineStrip(cr, solutionToCoords(optimized[i]));
    }
    Cairo::setSourceColor(cr, PlotUtils::RGB::black());
    Cairo::plotDots(
        cr, dataToPlotPoints(data), 1.0);
  }, "Wind speed", "Boat speed", p.cr.get());
}

Velocity<double> referenceSpeed(const PerfSurfPt& pt) {
  return decodeWindSpeed(pt.windVertexWeights);
}

Optional<Eigen::Vector2d> toNormed(const PerfSurfPt& x, const PerfSurfSettings& s) {
  double normed = double(x.boatSpeed/s.refSpeed(x));
  if (std::isfinite(normed) && 0 <= normed && normed < s.maxFactor) {
    return Eigen::Vector2d(
        decodeWindSpeed(x.windVertexWeights).knots(),
        normed);
  }
  return {};
}

Array<Eigen::Vector2d> normalizedDataToPlotPoints(
    const Array<PerfSurfPt>& src,
    const PerfSurfSettings& s) {
  int n = src.size();
  ArrayBuilder<Eigen::Vector2d> dst(n);
  for (int i = 0; i < n; i++) {
    auto x = toNormed(src[i], s);
    if (x.defined()) {
      dst.add(x.get());
    }
  }
  return dst.get();
}

TEST(PerfSurfTest, SpanTest) {
  auto spans = generatePairs({{1, 6}}, 4);
  EXPECT_EQ(spans.size(), 1);
  auto sp = spans[0];
  EXPECT_EQ(sp.first, 1);
  EXPECT_EQ(sp.second, 5);
}


Array<Eigen::Vector2d> levelsToCoords(
    const Eigen::VectorXd& X) {
  int n = X.size();
  Array<Eigen::Vector2d> dst(n);
  for (int i = 0; i < n; i++) {
    dst[i] = Eigen::Vector2d(
        double(double(i)*resolution/unit),
        double(1.0/X(i)));
  }
  return dst;
}

std::ostream& operator<<(std::ostream& s, const WeightedIndex& wi) {
  s << "(i=" << wi.index << ", w=" << wi.weight << ")";
  return s;
}

TEST(PerfSurfTest, SumConstraintTest) {
  int n = 4;
  double expectedSum = 7.0;

  SumConstraint cst(n, expectedSum);

  std::vector<double> coeffs{9.34, 199.3444, 78.345};
  EXPECT_EQ(coeffs.size(), cst.coeffCount());

  double sum = 0.0;

  for (int i = 0; i < n; i++) {
    auto x = cst.get(i);
    auto ptrs = x.pointers(coeffs.data());
    auto value = x.eval(*ptrs.first, *ptrs.second);
    sum += value;
  }

  EXPECT_NEAR(sum, expectedSum, 1.0e-4);
}

TEST(PerfSurfTest, TestIt2) {
  int dataSize = 6000;
  auto data = makeData(dataSize);
  int vertexCount = getRequiredVertexCount(data);

  PerfSurfSettings settings;
  settings.regWeight = 10;
  settings.refSpeed = &referenceSpeed;

  auto page = DOM::makeBasicHtmlPage("Perf test", "", "results");
  DOM::addSubTextNode(&page, "h1", "Perf surf");

  PlotUtils::Settings2d ps;
  ps.orthonormal = false;

  auto results = optimizePerfSurf(
      data,
      generateSurfaceNeighbors1d(vertexCount),
      settings);

  std::cout << "Number of vertices: " <<
      results.rawNormalizedVertices.size() << std::endl;

  for (auto v: results.rawNormalizedVertices) {
    std::cout << " " << v;
  }
  std::cout << std::endl;
  std::cout << "DONE" << std::endl;
}


/*TEST(PerfSurfTest, TestIt1) {
  int dataSize = 6000;
  auto data = makeData(dataSize);
  int vc = getRequiredVertexCount(data);
  auto vertices = initializeVertices(vc);

  PerfSurfSettings settings;
  settings.refSpeed = &referenceSpeed;

  auto page = DOM::makeBasicHtmlPage("Perf test", "", "results");
  DOM::addSubTextNode(&page, "h1", "Perf surf");

  PlotUtils::Settings2d ps;
  ps.orthonormal = false;

  auto pairs = generatePairs({{0, data.size()}}, 5);

  if (false) {
    DOM::addSubTextNode(&page, "h2", "Input data");
    auto im = DOM::makeGeneratedImageNode(&page, ".svg");
    auto p = Cairo::Setup::svg(
        im.toString(),
        ps.width,
        ps.height);
    Cairo::renderPlot(ps, [&](cairo_t* cr) {
      Cairo::plotDots(cr, dataToPlotPoints(data), 1);
    }, "Wind speed", "Boat speed", p.cr.get());
  }

  auto reg = makeOneDimensionalReg(vc, 2);

  auto results0 = optimizeLevels(
      data,
      pairs,
      reg,
      settings);

  if (false) {
    DOM::addSubTextNode(&page, "h2",
        "Divide all the boatspeeds by the reference speed");
    auto im = DOM::makeGeneratedImageNode(&page, ".svg");
    auto p = Cairo::Setup::svg(
        im.toString(),
        ps.width,
        ps.height);
    Cairo::renderPlot(ps, [&](cairo_t* cr) {
      auto pts = normalizedDataToPlotPoints(data, settings);
      Cairo::plotDots(cr, pts, 1);

      Cairo::setSourceColor(cr, PlotUtils::HSV::fromHue(240.0_deg));
      cairo_set_line_width(cr, 0.2);
      for (auto p: pairs) {
        auto a = toNormed(data[p.first], settings);
        auto b = toNormed(data[p.second], settings);
        if (a.defined() && b.defined()) {
          Cairo::plotLineStrip(cr, {a.get(), b.get()});
        }
      }
    }, "Wind speed", "Boat speed", p.cr.get());
  }
  double quantile = 0.9;

  auto results = results0.normalize(quantile);

  double perf = computePerfAtQuantile(results, quantile);
  DOM::addSubTextNode(&page, "p", stringFormat("Performance at %.3g is %.3g",
      quantile, perf));

  LOG(INFO) << "Perfs: " << results.final().transpose();
  int ln = results.levels.size();
  LineKM hueDeg(0, ln-1, 240.0, 360.0);
  if (true) {
      DOM::addSubTextNode(&page, "h2",
          "Optimization results");
      auto im = DOM::makeGeneratedImageNode(&page, ".svg");
      auto p = Cairo::Setup::svg(
          im.toString(),
          ps.width,
          ps.height);
      Cairo::renderPlot(ps, [&](cairo_t* cr) {
        auto pts = normalizedDataToPlotPoints(data, settings);
        Cairo::plotDots(cr, pts, 1);

        cairo_set_line_width(cr, 0.2);
        for (int i = 0; i < ln; i++) {
          auto lev = results.levels[i];
          Cairo::setSourceColor(cr, PlotUtils::HSV::fromHue(hueDeg(i)*1.0_deg));
          Cairo::plotLineStrip(cr, levelsToCoords(lev));
        }
      }, "Wind speed", "Boat speed", p.cr.get());
    }
}
*/