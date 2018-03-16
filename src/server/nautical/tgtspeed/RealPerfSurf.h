/*
 * RealPerfSurf.h
 *
 *  Created on: 2 Mar 2018
 *      Author: jonas
 *
 * This is glue code needed to perform a full polar computation
 * on a dataset.
 *
 */

#ifndef SERVER_NAUTICAL_TGTSPEED_REALPERFSURF_H_
#define SERVER_NAUTICAL_TGTSPEED_REALPERFSURF_H_

#include <server/transducers/Transducer.h>
#include <iostream>
#include <server/nautical/NavDataset.h>
#include <server/common/Functional.h>

namespace sail {

struct RealPerfSurfSettings {
  Duration<double> timedTupleThreshold = 4.0_seconds;
  Duration<double> sessionGap = 1.0_minutes;
  std::function<bool(TimeStamp)> timeFilter = constantly(true);
};

struct RealPerfSurfResults {
  int finalSampleCount = 0;
};

RealPerfSurfResults optimizeRealPerfSurf(
    const NavDataset& src,
    const RealPerfSurfSettings& settings
      = RealPerfSurfSettings());

void outputPolars(
    const std::string& filename,
    const NavDataset& src);

} /* namespace sail */

#endif /* SERVER_NAUTICAL_TGTSPEED_REALPERFSURF_H_ */