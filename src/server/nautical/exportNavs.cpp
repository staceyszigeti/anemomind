/*
 *  Created on: 2015
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <server/common/ArgMap.h>
#include <server/nautical/logimport/LogLoader.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <server/common/TimeStamp.h>
#include <server/nautical/Calibrator.h>
#include <iostream>
#include <server/common/Functional.h>

using namespace sail;
using namespace sail::NavCompat;

enum Format  {CSV, MATLAB, JSON};

struct ExportSettings {
  Format format;
  std::string formatStr;
  bool simulatedTrueWindData;
  bool withHeader;
};

NavDataset loadNavsFromArgs(Array<ArgMap::Arg*> args) {
  LogLoader loader;
  for (auto arg: args) {
    auto p = arg->value();
    LOG(INFO) << "Load navs from " << p;
    loader.load(p);
  }
  return loader.makeNavDataset();
}

struct NavField {
  std::string columnHeader;

  // I guess that, at least floating point and integer
  // literals look the same no matter if it is matlab, csv or json.
  // I don't think we are going to export string literals or other things like that.
  std::function<std::string(Nav)> getLiteral;
};

std::string doubleToString(double x, const ExportSettings& settings) {
  if (std::isfinite(x) || settings.format != CSV) {
    stringstream ss;
    ss.precision(std::numeric_limits<double>::max_digits10);
    ss << x;
    return ss.str();
  } else {
    return "";
  }
}

std::string angleToLiteral(Angle<double> x, const ExportSettings& settings,
                           double maxValDegrees) {
  Angle<double> maxVal = Angle<double>::degrees(maxValDegrees);
  return doubleToString(
      x.moveToInterval(maxVal - Angle<double>::degrees(360), maxVal).degrees(),
      settings);
}

std::string velocityToLiteral(Velocity<double> x, const ExportSettings& settings) {
  return doubleToString(x.knots(), settings);
}

std::string timeToLiteral(TimeStamp t) {
  std::stringstream ss;
  ss << t.toMilliSecondsSince1970();
  return ss.str();
}

std::string timeToLiteralHumanReadable(TimeStamp t, const ExportSettings& settings) {
  if (settings.format == MATLAB) { // Don't export text, only numbers.
    return timeToLiteral(t);
  }
  return t.toString("%Y-%m-%d %T");
}

Array<NavField> getNavFields(const ExportSettings& format) {
  ArrayBuilder<NavField> result;
  result.add(Array<NavField>{
    NavField{"DATE/TIME (UTC)", [=](const Nav &x) {
      return timeToLiteralHumanReadable(x.time(), format);
    }},
    NavField{"AWA (degrees)", [=](const Nav &x) {
      return angleToLiteral(x.awa(), format, 180);
    }},
    NavField{"AWS (knots)", [=](const Nav &x) {
      return velocityToLiteral(x.aws(), format);
    }},
    NavField{"TWA NMEA (degrees)", [=](const Nav &x) {
      return angleToLiteral(x.externalTwa(), format, 180);
    }},
    NavField{"TWS NMEA (knots)", [=](const Nav &x) {
      return velocityToLiteral(x.externalTws(), format);
    }},
    NavField{"TWDIR NMEA (degrees)", [=](const Nav &x) {
      return angleToLiteral(x.externalTwa() + x.gpsBearing(), format, 360);
    }},
    NavField{"TWA Anemobox (degrees)", [=](const Nav &x) {
      auto angle = (x.hasDeviceTwa() ?  x.deviceTwa() : Angle<double>());
      return angleToLiteral(angle, format, 180);
    }},
    NavField{"TWS Anemobox (knots)", [=](const Nav &x) {
      auto speed = (x.hasDeviceTws() ?  x.deviceTws() : Velocity<double>());
      return velocityToLiteral(speed, format);
    }},
    NavField{"TWDIR Anemobox (degrees)", [=](const Nav &x) {
      auto twdir = (x.hasDeviceTwdir() ?  x.deviceTwdir() : Angle<double>());
      return angleToLiteral(twdir, format, 360);
    }},
    NavField{"MagHdg (degrees)", [=](const Nav &x) {
      return angleToLiteral(x.magHdg(), format, 360);
    }},
    NavField{"Wat speed (knots)", [=](const Nav &x) {
      return velocityToLiteral(x.watSpeed(), format);
    }},
    NavField{"Longitude (degrees)", [=](const Nav &x) {
      return angleToLiteral(x.geographicPosition().lon(), format, 180);
    }},
    NavField{"Latitude (degree)", [=](const Nav &x) {
      return angleToLiteral(x.geographicPosition().lat(), format, 180);
    }},
    NavField{"GPS speed (knots)", [=](const Nav &x) {
      return velocityToLiteral(x.gpsSpeed(), format);
    }},
    NavField{"GPS bearing (degrees)", [=](const Nav &x) {
      return angleToLiteral(x.gpsBearing(), format, 360);
    }}
  });

  if (format.simulatedTrueWindData) {
    result.add(Array<NavField>{
      NavField{"TWA Anemomind simulated (degrees)", [=](const Nav &x) {
        auto angle = (x.hasTrueWindOverGround() ?
            x.twaFromTrueWindOverGround()
            : Angle<double>());
        return angleToLiteral(angle, format, 180);
      }},
      NavField{"TWS Anemomind simulated (knots)", [=](const Nav &x) {
        auto speed = x.trueWindOverGround().norm();
        return velocityToLiteral(speed, format);
      }},
      NavField{"TWDIR Anemomind simulated (degrees)", [=](const Nav &x) {
        return angleToLiteral(
            x.twdir(), format, 360);
      }}
      });
  }
  return result.get();
}


std::string quote(const std::string &x) {
  return "\"" + x + "\"";
}

std::string makeHeader(Array<NavField> fields, bool quoted) {
  std::string result;
  int last = fields.size()-1;
  for (int i = 0; i < fields.size(); i++) {
    auto h = fields[i].columnHeader;
    result += (quoted? quote(h) : h) + (i == last? "" : ", ");
  }
  return result;
}

std::string makeLiteralString(const Array<NavField> &fields,
    const Nav &x, std::string sep) {
  std::string result;
  int last = fields.size() - 1;
  for (int i = 0; i < fields.size(); i++) {
    auto v = fields[i].getLiteral(x);
    result += v + (i == last? "" : sep);
  }
  return result;
}

int exportCsv(bool withHeader, Array<NavField> fields,
    Array<Nav> navs, std::ostream *dst) {
  if (withHeader) {
    *dst << makeHeader(fields, true) << "\n";
  }
  for (auto nav: navs) {
    *dst << makeLiteralString(fields, nav, ", ") << "\n";
  }
  return 0;
}

int exportJson(bool withHeader, Array<NavField> fields, Array<Nav> navs,
    std::ostream *dst) {
  *dst << "[";
  if (withHeader) {
    *dst << "[" << makeHeader(fields, true) << "],\n";
  }
  int last = navs.size() - 1;
  for (int i = 0; i < navs.size(); i++) {
    *dst << "[" << makeLiteralString(fields, navs[i], ", ") << "]" << (i == last? "" : ", ");
  }
  *dst << "]";
  return 0;
}

int exportMatlab(bool withHeader, Array<NavField> fields,
    Array<Nav> navs, std::ostream *dst) {
  if (withHeader) {
    *dst << "% Columns: " << makeHeader(fields, false) << "\n";
  }
  for (auto nav: navs) {
    *dst << makeLiteralString(fields, nav, " ") << "\n";
  }
  return 0;
}

void performCalibration(NavDataset navs0, Array<Nav> *navs) {
  WindOrientedGrammarSettings gs;
  WindOrientedGrammar grammar(gs);
  auto tree = grammar.parse(navs0);
  std::shared_ptr<Calibrator> calib(new Calibrator(grammar));
  calib->setVerbose();
  calib->calibrate(navs0, tree, Nav::debuggingBoatId());
  calib->simulate(navs);
}

int exportNavs(Array<ArgMap::Arg*> args, const ExportSettings& settings, std::string output) {
  auto navs0 = loadNavsFromArgs(args);
  Array<Nav> navs = makeArray(navs0);
  Array<NavField> fields = getNavFields(settings);
  std::sort(navs.begin(), navs.end());
  if (navs.empty()) {
    LOG(ERROR) << "No navs were loaded";
    return -1;
  }
  if (settings.simulatedTrueWindData) {
    performCalibration(navs0, &navs);
  }
  const std::string& format = settings.formatStr;
  LOG(INFO) << "Navs successfully loaded, export them to "
      << output << " with format " << format;
  std::ofstream file(output);
  if (format == "csv") {
    return exportCsv(settings.withHeader, fields, navs, &file);
  } else if (format == "json") {
    return exportJson(settings.withHeader, fields, navs, &file);
  } else if (format == "matlab") {
    return exportMatlab(settings.withHeader, fields, navs, &file);
  }
  LOG(ERROR) << ("Export format not recognized: " + format);
  return -1;
}

int main(int argc, const char **argv) {
  ExportSettings settings;
  settings.formatStr = "csv";
  std::string output = "/tmp/exported_navs.txt";

  ArgMap amap;
  amap.registerOption("--format", "What export format to use: (matlab, json, csv). Defaults to " + settings.formatStr)
      .store(&settings.formatStr);
  amap.registerOption("--output", "Where to put the exported data. Defaults to " + output)
    .store(&output);
  amap.registerOption("--no-header", "Omit header labels for data columns");
  amap.registerOption("--no-simulate", "Skip simulated true wind columns");
  amap.setHelpInfo(
      std::string("") +
      "Exports nav data to other formats. In addition to the named arguments,\n" +
      "it also accepts any number of free arguments pointing to directories\n" +
      "where navs are located. Example usage: \n" +
      "./nautical_exportNavs ~/sailsmart/datasets/Irene --output /tmp/irene_data.txt --format matlab\n");
  switch (amap.parse(argc, argv)) {
    case ArgMap::Continue:
      if (amap.freeArgs().empty()) {
        std::cout << "No folder provided.\n";
        amap.dispHelp(&std::cout);
        return 0;
      } else {
        settings.format = (settings.formatStr == "csv"?
                           CSV : (settings.formatStr == "json"? JSON : MATLAB));
        settings.withHeader = !amap.optionProvided("--no-header");
        settings.simulatedTrueWindData = !amap.optionProvided("--no-simulate");
        return exportNavs(amap.freeArgs(), settings, output);
      }
    case ArgMap::Done:
      return 0;
    case ArgMap::Error:
      return -1;
  }
}
