/*
 * Reconstructor.h
 *
 *  Created on: 25 Aug 2016
 *      Author: jonas
 *
 * Performs joint calibration and filtering.
 */

#ifndef SERVER_NAUTICAL_RECONSTRUCTOR_H_
#define SERVER_NAUTICAL_RECONSTRUCTOR_H_

#include <device/anemobox/Dispatcher.h>
#include <ceres/ceres.h>
#include <server/nautical/BoatState.h>
#include <unordered_map>
#include <server/nautical/calib/BoatStateReconstructor.h>
#include <server/nautical/calib/SensorSet.h>

namespace sail {
namespace Reconstructor {

struct Settings {
  Duration<double> windowSize = Duration<double>::minutes(1.0);
};

class ChannelRef {
  DataCode code;
  std::string sourceName;
};

struct Results {
  SensorNoiseSet<double> sensorNoise;
  SensorDistortionSet<double> sensorDistortion;

  void outputSummary(std::ostream *dst) const;
};

// Reconstructs the noise and distortion for a
// chunks of data.
Results reconstruct(
    const Array<CalibDataChunk> &chunks,
    const Settings &settings);

}
}

#endif /* SERVER_NAUTICAL_RECONSTRUCTOR_H_ */