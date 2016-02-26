/*
 *  Created on: 2014-03-28
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "NavNmeaScan.h"
#include "NavNmea.h"
#include <Poco/File.h>
#include <server/common/filesystem.h>
#include <server/common/logging.h>
#include <server/nautical/NavLoader.h>
#include <algorithm>
#include <server/common/Functional.h>

namespace sail {

using namespace NavCompat;

NavDataset scanNmeaFolderWithSimulator(Poco::Path p, Nav::Id boatId,
                          ScreenRecordingSimulator *simulator, ParsedNavs::FieldMask mask) {
  { // Initial checks.
    Poco::File file(p);
    if (!file.exists()) {
      return NavDataset();
    }

    if (!file.isDirectory()) {
      return NavDataset();
    }

    if (boatId.empty()) {
      return NavDataset();
    }
  }

  Array<std::string> nmeaExtensions = Array<std::string>{"txt", "log", "csv"};
  Array<Poco::Path> files = listFilesRecursivelyByExtension(p, nmeaExtensions);
  int count = files.size();
  Array<ParsedNavs> parsedNavs(count);
  for (int i = 0; i < count; i++) {
    parsedNavs[i] = loadNavsFromFile(files[i].toString(), boatId);

    LOG(INFO) << "Parsed " << files[i].toString();

    if (simulator) {
      simulator->simulate(files[i].toString());
    }
  }
  Array<Nav> result = makeArray(flattenAndSort(parsedNavs, mask));

  if (simulator) {
    for (Nav& nav : result) {
      ScreenInfo info;
      if (simulator->screenAt(nav.time(), &info)) {
        auto delta = fabs(nav.time() - info.time);
        if (delta > Duration<>::seconds(4)) {
          LOG(WARNING) << "Time problem while matching simulated data to nav: "
            << delta.str() << " of time difference, at "
            << nav.time().toString();
        } else {
          nav.setDeviceScreen(info);
        }
      }
    }
  }

  return fromNavs(result);
}

NavDataset scanNmeaFolder(Poco::Path p, Nav::Id boatId,
                          ParsedNavs::FieldMask mask) {
  return scanNmeaFolderWithSimulator(p, boatId, nullptr, mask);
}

NavDataset scanNmeaFolders(Array<Poco::Path> p, Nav::Id boatId,
    ParsedNavs::FieldMask mask) {
  auto scanResults = toArray(map(p, [=](const Poco::Path &p) {
    return makeArray(scanNmeaFolder(p, boatId, mask));
  }));
  auto cat = concat(scanResults);
  std::sort(cat.begin(), cat.end());
  return fromNavs(cat);
}



} /* namespace sail */
