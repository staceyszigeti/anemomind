/*
 *  Created on: 2015
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <server/nautical/synthtest/NavalSimulationPrecomp.h>
#include <server/nautical/synthtest/NavalSimulationJson.h>
#include <server/common/Env.h>
#include <server/common/PathBuilder.h>
#include <server/common/JsonIO.h>
#include <Poco/File.h>
#include <server/common/logging.h>

namespace sail {

    NavalSimulation loadOrMake(std::function<NavalSimulation()> srcFunction, std::string filename) {

      Poco::Path p = PathBuilder::makeDirectory(Env::SOURCE_DIR)
        .pushDirectory("datasets").pushDirectory("synth").makeFile(filename).get();
      Poco::File file(p);
      if (file.exists()) {
        NavalSimulation dst;
        if (json::deserialize(json::load(p.toString()), &dst)) {
          return dst;
        }
        LOG(WARNING) << "Failed to deserialize data from " << p.toString();
      }
      LOG(INFO) << "No precomputed results available from " << p.toString() << ", generate...";
      auto results =  srcFunction();
      LOG(INFO) << "Done generating them. Save them to " << p.toString();
      json::save(p.toString(), json::serialize(results));
      return results;
    }



NavalSimulation getNavSimFractalWindOriented() {
  return loadOrMake(&makeNavSimFractalWindOriented,
      "navSimFractalWindOriented.json");
}

} /* namespace mmm */
