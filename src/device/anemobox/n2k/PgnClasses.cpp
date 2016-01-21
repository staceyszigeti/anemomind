/** Generated on Thu Jan 21 2016 18:18:34 GMT+0100 (CET) using 
 *
 *     /opt/local/bin/node /Users/leto/Documents/anemomind/anemomind/src/device/anemobox/n2k/codegen/index.js /Users/leto/Documents/anemomind/canboat/analyzer/pgns.xml
 *
 *  WARNING: Modifications to this file will be overwritten when it is re-generated
 */
#include "PgnClasses.h"

#include <device/anemobox/n2k/BitStream.h>

namespace PgnClasses {

  VesselHeading::VesselHeading() {
    reset();
  }

  VesselHeading::VesselHeading(const uint8_t *data, int lengthBytes) {
    BitStream src(data, lengthBytes);
    if (58 <= src.remainingBits()) {
      _sid = src.getUnsigned(8);
      {auto x = src.getUnsigned(16); if (BitStream::isAvailable(x, 16)) {_heading = double(0.0001*x)*sail::Angle<double>::radians(1.0);}}
      {auto x = src.getSigned(16); if (BitStream::isAvailable(x, 16)) {_deviation = double(0.0001*x)*sail::Angle<double>::radians(1.0);}}
      {auto x = src.getSigned(16); if (BitStream::isAvailable(x, 16)) {_variation = double(0.0001*x)*sail::Angle<double>::radians(1.0);}}
      _reference = src.getUnsigned(2);

      _valid = true;
    } else {
      reset();
    }
  }

  void VesselHeading::reset() {
    _valid = false;
  }

  Attitude::Attitude() {
    reset();
  }

  Attitude::Attitude(const uint8_t *data, int lengthBytes) {
    BitStream src(data, lengthBytes);
    if (56 <= src.remainingBits()) {
      _sid = src.getUnsigned(8);
      // Skipping yaw
      advanceBits(16);
      // Skipping pitch
      advanceBits(16);
      // Skipping roll
      advanceBits(16);

      _valid = true;
    } else {
      reset();
    }
  }

  void Attitude::reset() {
    _valid = false;
  }

  Speed::Speed() {
    reset();
  }

  Speed::Speed(const uint8_t *data, int lengthBytes) {
    BitStream src(data, lengthBytes);
    if (44 <= src.remainingBits()) {
      _sid = src.getUnsigned(8);
      {auto x = src.getUnsigned(16); if (BitStream::isAvailable(x, 16)) {_speedWaterReferenced = double(0.01*x)*sail::Velocity<double>::metersPerSecond(1.0);}}
      {auto x = src.getUnsigned(16); if (BitStream::isAvailable(x, 16)) {_speedGroundReferenced = double(0.01*x)*sail::Velocity<double>::metersPerSecond(1.0);}}
      _speedWaterReferencedType = src.getUnsigned(4);

      _valid = true;
    } else {
      reset();
    }
  }

  void Speed::reset() {
    _valid = false;
  }

  WindData::WindData() {
    reset();
  }

  WindData::WindData(const uint8_t *data, int lengthBytes) {
    BitStream src(data, lengthBytes);
    if (43 <= src.remainingBits()) {
      _sid = src.getUnsigned(8);
      {auto x = src.getUnsigned(16); if (BitStream::isAvailable(x, 16)) {_windSpeed = double(0.01*x)*sail::Velocity<double>::metersPerSecond(1.0);}}
      {auto x = src.getUnsigned(16); if (BitStream::isAvailable(x, 16)) {_windAngle = double(0.0001*x)*sail::Angle<double>::radians(1.0);}}
      _reference = src.getUnsigned(3);

      _valid = true;
    } else {
      reset();
    }
  }

  void WindData::reset() {
    _valid = false;
  }
bool PgnVisitor::visit(int pgn, const uint8_t *data, int length) {
  switch(pgn) {
    case 127250: return apply(VesselHeading(data, length));
    case 127257: return apply(Attitude(data, length));
    case 128259: return apply(Speed(data, length));
    case 130306: return apply(WindData(data, length));
  }  // closes switch
  return false;
}

}