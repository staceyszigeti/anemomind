/*
 * DebugPlot.h
 *
 *  Created on: 1 Oct 2016
 *      Author: jonas
 *
 *  Just for debuggin
 */

#ifndef SERVER_NAUTICAL_CALIB_DEBUGPLOT_H_
#define SERVER_NAUTICAL_CALIB_DEBUGPLOT_H_

#include <server/common/TimeStamp.h>
#include <string>
#include <iosfwd>
#include <random>
#include <sstream>
#include <server/common/LineKM.h>
#include <server/common/Span.h>
#include <server/common/TimedValue.h>

namespace sail {

template <typename T>
struct ValuesPerPixel {
  static double get() {
    return 1.0;
  }
};

template <>
struct ValuesPerPixel<Velocity<double>> {
  static Velocity<double> get() {
    return (1.0/200.0)*20.0_kn;
  }
};

template <>
struct ValuesPerPixel<Angle<double>> {
  static Angle<double> get() {
    return (1.0/200.0)*360.0_deg;
  }
};

template <>
struct ValuesPerPixel<Duration<double>> {
  static Duration<double> get() {
    return (1.0/200.0)*1.0_min;
  }
};

enum class StrokeType {Line, Dot};

struct AxisMapping {
  Span<double> sourceRange;
  double targetWidth;
  double margin;
};

enum class AxisMode {XY, IJ};

HtmlNode::Ptr makeSvg(
    HtmlNode::Ptr parent,
    double pixelWidth, double pixelHeight);

LineKM makeAxisFun(bool forward,
    const AxisMapping &m);

HtmlNode::Ptr makePlotSpace(
    const HtmlNode::Ptr &parent,
    const LineKM &xmap,
    const LineKM &ymap);
HtmlNode::Ptr makePlotSpace(HtmlNode::Ptr parent,
                            AxisMode mode,
                            const AxisMapping &x,
                            const AxisMapping &y);

template <typename T>
class TemporalSignalPlot {
public:

  struct SignalToPlot {
    StrokeType type;
    std::string color;
    Array<TimedValue<T>> values;
  };

  void add(
      StrokeType type,
      const Array<TimedValue<T>> &values,
      std::string color = "") {
    if (color.empty()) {
      color = generateColor();
    }
    for (auto value: values) {
      CHECK(value.time.defined());
      timeSpan.extend(value.time);
      CHECK(timeSpan.initialized());
      valueSpan.extend(value.value);
      CHECK(valueSpan.initialized());
    }
    _data.push_back(SignalToPlot{
      type, color, values
    });
    std::cout << "   Added "
        << values.size() << " values to a plot\n";
  }

  std::string generateColor() {
    if (!colors.empty()) {
      auto c = colors.back();
      colors.pop_back();
      return c;
    } else {
      std::uniform_int_distribution<int> distrib(0, 359);
      std::stringstream code;
      code << "hsl(" << distrib(_rng) << ", 100%, 50%)";
      return code.str();
    }
  }

  void renderTo(HtmlNode::Ptr dst) const {
    if (!(valueSpan.initialized() && timeSpan.initialized())) {
      HtmlTag::tagWithData(dst, "p", "Nothing to plot");
      return;
    }
    auto vpy = ValuesPerPixel<T>::get();
    auto vpx = ValuesPerPixel<Duration<double>>::get();
    auto dur = duration();
    double pixelHeight = valueSpan.width()/vpy;
    HtmlTag::tagWithData(dst, "p",
        stringFormat("Duration: %s",
            dur.str().c_str()));
    double pixelWidth = dur/vpx;

    auto svg = makeSvg(dst, pixelWidth + 2*_margin,
        pixelHeight + 2*_margin);
    auto xmap = makeAxisFun(true, AxisMapping{
      Spand(0.0, pixelWidth), pixelWidth, _margin
    });
    auto ymap = makeAxisFun(false, AxisMapping{
      Spand(valueSpan.minv()/vpy, valueSpan.maxv()/vpy),
      pixelHeight,
      _margin
    });

    auto canvas = makePlotSpace(
        svg, xmap, ymap);

    for (const auto &curve : _data) {
      std::cout << "  Render curve with " << curve.values.size()
              << " values"<< std::endl;
      if (curve.type == StrokeType::Line) {
        auto &stream = canvas->stream();
        stream << "<polyline points=\"";
        for (auto pt: curve.values) {
          stream << (pt.time - timeSpan.minv())/vpx
                 << "," << (pt.value/vpy) << " ";
        }
        stream << "\" style=\"fill: none; stroke:" << curve.color << "; stroke-width: 2\" />";
      } else {
        auto circleGroup = HtmlTag::make(canvas, "g", {
            {"stroke-width", 0},
            {"fill", curve.color},
        });
        auto &cstream = circleGroup->stream();
        for (auto pt: curve.values) {
          cstream << "<circle cx='"
              << (pt.time - timeSpan.minv())/vpx
              << "' cy='" << (pt.value/vpy) <<
              "' r='2' />";
        }
      }
    }
  }

  Duration<double> duration() const {
    return timeSpan.maxv() - timeSpan.minv();
  }
private:
  double _margin = 30.0;
  std::default_random_engine _rng;
  std::vector<std::string> colors{
    "red", "green", "blue",
  };
  Span<TimeStamp> timeSpan;
  Span<T> valueSpan;
  std::vector<SignalToPlot> _data;
};

} /* namespace sail */

#endif /* SERVER_NAUTICAL_CALIB_DEBUGPLOT_H_ */
