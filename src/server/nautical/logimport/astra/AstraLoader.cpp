/*
 * AstraLoader.cpp
 *
 *  Created on: 9 Mar 2018
 *      Author: jonas
 */

#include "AstraLoader.h"
#include <regex>
#include <map>
#include <iostream>
#include <server/common/string.h>
#include <server/common/logging.h>
#include <server/transducers/ParseTransducers.h>
#include <server/common/RegexUtils.h>
#include <set>
#include <fstream>
#include <server/common/Functional.h>

namespace sail {

Optional<AstraHeader> tryParseAstraHeader(const std::string& s) {
  static std::regex re("----* *(.*[^\\s-]) *-*----");
  std::smatch match;
  if (std::regex_search(s, match, re) && match.size() > 1) {
    return AstraHeader{match.str(1)};
  }
  return {};
}

namespace {

  std::string namedNumericParametersPattern() {
    using namespace Regex;
    auto notColon = "[^:]";
    auto colon = ":";
    auto wordChar = charNotInString(spaceChars + "\\:");
    auto name = captureGroup(wordChar/anyCount(notColon));
    auto value = captureGroup(basicNumber(digit));
    auto namedParameter = name/colon/anyCount(space)/value;
    return entireString(
        anyCount(space)
          /join1(atLeastOnce(space), namedParameter)
          /anyCount(space));
  }

}

Optional<std::map<std::string, Array<std::string>>>
  tryParseNamedNumericParameters(
    const std::string& s) {

  std::smatch matches;
  static std::regex re(namedNumericParametersPattern());
  if (std::regex_match(s, matches, re)) {
    auto pairs = transduce(
        matches,
        trDrop(1)
        |
        trFilter([](const std::smatch::value_type& sm) {
          return sm.matched;
        })
        |
        trMap([](const std::smatch::value_type& sm) {
          return sm.str();
        })
        |
        trPartition<std::string, 2>(),
        IntoArray<std::array<std::string, 2>>());
    std::map<std::string, Array<std::string>> dst;
    for (auto p: pairs) {
      dst[p[0]] = {p[1]};
    }
    return dst;
  } else {
    return {};
  }
}


namespace {
  std::pair<int, std::string> negativeLengthAndDataOf(const std::string& k) {
    return {-k.length(), k};
  }

}
// When we iterate over the possibilities
// in the parser, we want to start trying
// to match the long words (for instance we want
// to try 'DateTimeTs' before we try just 'Date'.
bool LongWordsFirst::operator()(
    const std::string& a, const std::string& b) const {
  return negativeLengthAndDataOf(a) < negativeLengthAndDataOf(b);
}

template <typename Q, typename FieldAccess>
AstraValueParser inUnit(Q unit, FieldAccess f) {
  return [f, unit](const std::string& s, AstraData* dst) {
    for (auto number: tryParse<double>(s)) {
      *(f(dst)) = number*unit;
      return true;
    }
    return false;
  };
}

template <typename FieldAccess>
AstraValueParser asString(FieldAccess f) {
  return [f](const std::string& s, AstraData* dst) {
    *(f(dst)) = s;
    return true;
  };
}

template <typename T, typename FieldAccess>
AstraValueParser asPrimitive(FieldAccess f) {
  return [f](const std::string& s, AstraData* dst) {
    for (auto x: tryParse<T>(s)) {
      *(f(dst)) = x;
      return true;
    }
    return false;
  };
}

bool ignoreAstraValue(const std::string&, AstraData*) {return true;}

Optional<Duration<double>> tryParseAstraTimeOfDay(const std::string& src) {
  return parseTimeOfDay("%T", src);
}

Optional<TimeStamp> tryParseAstraDate(
    const char* fmt, const std::string& s) {
  auto x = TimeStamp::parse(fmt, s);
  if (x.defined()) {
    return x;
  } else {
    return {};
  }
}

Optional<AstraData> tryMakeAstraData(
    const AstraColSpec& cols,
    const AstraTableRow& rawData,
    bool verbose) {
  if (cols.size() != rawData.size()) {
    if (verbose) {
      LOG(WARNING) << "Incompatible col-spec size and raw data size";
      LOG(INFO) << "Col size: " << cols.size();
      LOG(INFO) << "Raw size: " << rawData.size();
      for (int i = 0; i < cols.size(); i++) {
        LOG(INFO) << "  Col " << i+1 << ": " << cols[i].first;
      }
      for (int i = 0; i < rawData.size(); i++) {
        LOG(INFO) << "  Data " << i+1 << ": " << rawData[i];
      }
    }
    return {};
  }
  AstraData dst;
  for (int i = 0; i < cols.size(); i++) {
    const auto& kv = cols[i];
    const auto& rd = rawData[i];
    if (!bool(kv.second)) {
      //LOG(FATAL) << "No value parser registered for Astra value of type '"
      //    << kv.first << "'";
      continue;
    }
    if (!kv.second(rd, &dst)) {
      LOG(WARNING) << "Failed to parse col '" << kv.first << "' from value '"
          << rd << "'";
      return {};
    }
  }
  return dst;
}

template <typename FieldAccess>
AstraValueParser asDate(const char* fmt, FieldAccess f) {
  return [f, fmt](const std::string& src, AstraData* dst) {
    for (auto d: tryParseAstraDate(fmt, src)) {
      *(f(dst)) = d;
      return true;
    }
    return false;
  };
}

template <typename FieldAccess>
AstraValueParser asTimeOfDay(FieldAccess f) {
  return [f](const std::string& src, AstraData* dst) {
    for (auto p: tryParseAstraTimeOfDay(src)) {
      *(f(dst)) = p;
      return true;
    }
    return false;
  };
}

#define FIELD_ACCESS(NAME) ([](AstraData* src) {return &(src->NAME);})

/*
 *
 * WARNING: I am not at all sure about the units used
 * by Astra, e.g. degrees vs radians, m/s vs knots vs km/h.
 *
 * Also, different log files have different formats for a column with the
 * same name. For instance, in the file
 *
 * datasets/astradata/Regata/log1Hz20170708_1239.log
 *
 * the "Date" column has a different format than the "Date" column of
 *
 * datasets/astradata/Log from Dinghy/Device___15___2018-03-02.log
 *
 * For this reason, every log file needs its own map like below.
 */

auto fracDeg = 0.01_deg;

LogFileHeaderSpec regataLogFileSpec{
  // Current, and things like that?
  {"Set", AstraValueParser()},
  {"Drift", AstraValueParser()},
  {"DRIFT", &ignoreAstraValue},
  {"SET", &ignoreAstraValue},

  // Wind
  {"WindType", AstraValueParser()},
  {"AWA", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(AWA)))},
  {"AWS", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(AWS)))},
  {"TWA", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWA)))},
  {"TWS", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(TWS)))},// <-- See message2.txt
  {"TWD", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWD)))},// <-- See message2.txt
  {"GWS", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(GWS)))},// <-- See message2.txt
  {"GWD", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(GWD)))},// <-- See message2.txt

  // Not sure...
  {"FreezeWind", AstraValueParser()},
  {"FreezeTide", AstraValueParser()},

  // Time
  {"DateTimeTs", AstraValueParser()},
  {"Date", AstraValueParser(
      asDate("%d/%m/%y", FIELD_ACCESS(partialTimestamp)))},
  {"Time", AstraValueParser(asTimeOfDay(FIELD_ACCESS(timeOfDay)))},
  {"Ts", &ignoreAstraValue},

  // Meta
  {"DinghyID", AstraValueParser(asPrimitive<int>(FIELD_ACCESS(dinghyId)))},
  {"UserID", AstraValueParser(asString(FIELD_ACCESS(userId)))},

  // Boat motion
  {"BS", AstraValueParser()},



  // TODO: Because the log file typically contains some sort of date,
  // could we use that date to determine what unit to use for COG and SOG?
  // The date is also stored in the header at the top of the file. So
  // we can decide on the units before we have even started parsing the data.
  //
  // This value could be unreliable, See message2.txt:
  //   "'COG' in radians (but it will be in degrees like GWD and TWD);"
  {"COG", AstraValueParser(inUnit(1.0_rad, FIELD_ACCESS(COG)))},

  // This value could be unreliable, See message2.txt:
  //   "'SOG' in m/s (but it will be in knots);"
  {"SOG", AstraValueParser(inUnit(1.0_mps, FIELD_ACCESS(SOG)))},

  // Boat position

  // Should be some kind of fractional degrees here.
  {"Lat", AstraValueParser(geographicAngle(
      'N', 'S', FIELD_ACCESS(lat)))},
  {"Lon", AstraValueParser(geographicAngle(
      'E', 'W', FIELD_ACCESS(lon)))},
  {"Latitudine", AstraValueParser(geographicAngle(
      'N', 'S', FIELD_ACCESS(lat)))},
  {"Longitudine", AstraValueParser(geographicAngle(
      'E', 'W', FIELD_ACCESS(lon)))},

  // Should be unit degrees here? Not sure. In message2.txt, it says
  //
  // "  'LAT' and 'LON' is like google maps format, so decimals of degrees (but it will be as explained just before"
  //
  // But looking at the log file data, it seems that degrees is the unit
  // being used.
  {"LAT", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(lat)))},
  {"LON", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(lon)))},

  // Other boat orientation
  {"HDG", AstraValueParser()},
  {"Pitch", AstraValueParser(inUnit(1.0_rad, FIELD_ACCESS(pitch)))},
  {"Roll", AstraValueParser(inUnit(1.0_rad, FIELD_ACCESS(roll)))},

  {"AW_angle", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(AWA)))},
  {"AW_speed", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(AWS)))},
  {"BS_polar",AstraValueParser()},
  {"BS_target",AstraValueParser()},
  {"Boatspeed", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(waterSpeed)))},
  {"Ext_COG", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(COG)))},
  {"Ext_SOG", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(SOG)))},
  {"Heading", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(magHdg)))},
  {"LeewayAng",AstraValueParser()},
  {"LeewayMod",AstraValueParser()},
  {"Leeway_Ang",AstraValueParser()},
  {"Leeway_Mod",AstraValueParser()},
  {"TWA_target",AstraValueParser()},
  {"TW_Dir", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWD)))},
  {"TW_angle", AstraValueParser(inUnit(1.0_deg, FIELD_ACCESS(TWA)))},
  {"TW_speed", AstraValueParser(inUnit(1.0_kn, FIELD_ACCESS(TWS)))},
  {"Type_tgt",AstraValueParser()}
};

Optional<Array<std::pair<std::string, AstraValueParser>>>
  tryParseAstraColSpec(
      const Array<std::string>& tokens) {

  // Once we want to support multiple log file formats, we can
  // instead pass this as a parameter.
  const auto& spec = regataLogFileSpec;


  typedef std::map<std::string, AstraValueParser,
      LongWordsFirst>::const_iterator Iterator;

  if (tokens.empty()) {
    return {};
  }

  auto result = transduce(
      tokens,
      trMap([&](const std::string& token) {
        return spec.find(token);
      })
      |
      trTakeWhile([&](Iterator f){
        return f != spec.end();
      })
      |
      trMap([](Iterator f) {
        return *f;
      }),
      IntoArray<std::pair<std::string, AstraValueParser>>());
  // Only return a valid result if *all* tokens where recognized.
  return result.size() == tokens.size()?
      Optional<Array<std::pair<std::string, AstraValueParser>>>(result)
      : Optional<Array<std::pair<std::string, AstraValueParser>>>();
}

// Useful for explaining what other columns need to be considered.
void displayColHint(const AstraTableRow& rawData) {
  const auto& spec = regataLogFileSpec;

  std::set<std::string> recognized, notRecognized;
  for (auto x: rawData) {
    auto f = spec.find(x);
    if (f == spec.end()) {
      notRecognized.insert(x);
    } else {
      recognized.insert(x);
    }
  }
  LOG(INFO) << "If this is a col spec header, then these cols are not known:";
  for (auto x: notRecognized) {
    LOG(INFO) << "  * '" << x << "'";
  }
}

bool isDinghyLogHeader(const std::string& s) {
  using namespace Regex;
  static std::regex re(entireString("Device_.*"));
  return matches(s, re);
}

bool isProcessedCoachLogHeader(const std::string& s) {
  using namespace Regex;
  static std::regex re(entireString(".*_Charts"));
  return matches(s, re);
}



Array<std::string> tokenizeAstra(const std::string& s) {
  return transduce(s, trTokenize(&isBlank), IntoArray<std::string>());
}

auto astraParser = trStreamLines() // All the lines of the file
        |
        trFilter(complementFunction(&isBlankString))
        |
        trPreparseAstraLine() // Identify the type of line: header, column spec or data?
        |
        trMakeAstraData();

Array<AstraData> loadAstraFile(const std::string& filename) {
  return transduce(
      makeOptional(std::make_shared<std::ifstream>(filename)),
      astraParser, // Produce structs from table rows.
      IntoArray<AstraData>());
}

namespace {
  std::string stringOrQ(const Optional<std::string>& s) {
    return s.defined()? s.get() : "?";
  }
  std::string stringOrQ(const Optional<int>& s) {
    std::stringstream ss;
    if (s.defined()) {
      ss << s.get();
    } else {
      ss << "?";
    }
    return ss.str();
  }

  template <typename T>
  void copyIfDefined(
      const std::string& srcName, TimeStamp full,
      const Optional<T>& x, std::map<std::string,
        typename TimedSampleCollection<T>::TimedVector>* dst,
        const char* debug = "") {
    static bool verbose = true;
    if (!full.defined()) {
      LOG(WARNING) << "Missing timestamp for " << srcName;
    } else if (!x.defined()) {
      if (verbose) {
        verbose = false;
        LOG(WARNING) << "Missing value for " << srcName << ": " << debug;
      }
    } else {
      (*dst)[srcName].push_back(TimedValue<T>(full, x.get()));
    }
  }

  void accumulateRegatta(const AstraData& src, LogAccumulator* dst) {
    const std::string sourceName = "Astra regata";

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.COG, &(dst->_GPS_BEARINGsources), "COG");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.SOG, &(dst->_GPS_SPEEDsources), "SOG");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.geoPos(), &(dst->_GPS_POSsources), "pos");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.TWA, &(dst->_TWAsources), "TWA");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.TWD, &(dst->_TWDIRsources), "TWD");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.TWS, &(dst->_TWSsources), "TWS");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.AWA, &(dst->_AWAsources), "AWA");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.AWS, &(dst->_AWSsources), "AWS");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.magHdg, &(dst->_MAG_HEADINGsources), "magHdg");

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.waterSpeed, &(dst->_WAT_SPEEDsources), "watSpeed");
  }

  void accumulateDinghy(const AstraData& src, LogAccumulator* dst) {
    std::string sourceName = "astraDinghy_id"
        + stringOrQ(src.dinghyId)
        + "_user"
        + stringOrQ(src.userId);

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.COG, &(dst->_GPS_BEARINGsources));

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.SOG, &(dst->_GPS_SPEEDsources));

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.geoPos(), &(dst->_GPS_POSsources));

    // There is also Pitch, Roll
  }


  void accumulateCoach(const AstraData& src, LogAccumulator* dst) {
    std::string sourceName = "astraCoach";

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.TWD, &(dst->_TWDIRsources));

    copyIfDefined(sourceName, src.fullTimestamp(),
        src.TWS, &(dst->_TWSsources));
  }
}

/**
 *
 * When loading logs, we should ensure we don't load logs from unrelated coach
 * boats that are somewhere else.
 *
 *
 */
bool accumulateAstraLogs(const std::string& filename, LogAccumulator* dst) {
  Array<AstraData> data = loadAstraFile(filename);
  if (data.empty()) {
    return false;
  }

  for (const AstraData& x: data) {
    accumulateRegatta(x, dst);

    // TODO: Switch based on x.logType
  }
  return true;
}



} /* namespace sail */
