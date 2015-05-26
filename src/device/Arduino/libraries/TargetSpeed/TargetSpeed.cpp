// (c) 2014 Julien Pilet <julien.pilet@gmail.com>

#include "TargetSpeed.h"

#include <math.h>

#ifdef ON_SERVER
#include <device/Arduino/libraries/ChunkFile/ChunkFile.h>
#include <fstream>
#include <sstream>

#endif

namespace sail {

namespace {

float degToRad(float deg) {
  return float(deg * (M_PI / 180.0));
}

FP8_8 interpolate(const FP8_8* array, FP8_8 x) {
  if (x <= FP8_8(0)) {
    return array[0];
  }
  int x_lower = (int) x;
  FP8_8 remaining = x - FP8_8(x_lower);

  const int lastIndex = TargetSpeedTable::NUM_ENTRIES - 1;
  if (x_lower >= lastIndex) {
    return array[lastIndex];
  }

  if (array[x_lower] < FP8_8(0) || array[x_lower + 1] < FP8_8(0)) {
    return -1;
  }

  return array[x_lower] * (FP8_8(1) - remaining) + remaining * array[x_lower + 1];
}

FP8_8 getSpeedUpWind(const TargetSpeedTable& table, FP8_8 tws) {
  return interpolate(table._upwind, tws);
}

float getSpeedDownWind(const TargetSpeedTable& table, FP8_8 tws) {
  return interpolate(table._downwind, tws);
}

} // namespace

Velocity<> getVmgTarget(const TargetSpeedTable& table,
                        Angle<> twa, Velocity<> tws) {
  if (cos(twa) > 0) {
    // upwind
    return Velocity<>::knots(getSpeedUpWind(table, tws.knots()));
  } else {
    // downwind
    return Velocity<>::knots(getSpeedDownWind(table, tws.knots()));
  }
}

float getVmgSpeedRatio(const TargetSpeedTable& table,
                       short twa, FP8_8 tws, FP8_8 gpsSpeed) {
  float vmgGps = cos(degToRad(twa)) * static_cast<float>(gpsSpeed);
  float refSpeed;
  if (vmgGps > 0) {
    refSpeed = getSpeedUpWind(table, tws);
  } else {
    vmgGps = -vmgGps;
    refSpeed = getSpeedDownWind(table, tws);
  }

  if (refSpeed < .1f)
    return 0.0f;

  return vmgGps / refSpeed;
}

void invalidateSpeedTable(TargetSpeedTable *table) {
  for (int i = 0; i < TargetSpeedTable::NUM_ENTRIES; ++i) {
    table->_upwind[i] = table->_downwind[i] = FP8_8(-1);
  }
}

#ifdef ON_SERVER
bool loadTargetSpeedTable(const char *filename, TargetSpeedTable *table) {
  ChunkTarget targets[] = {
    makeChunkTarget(table)
  };
  ChunkLoader loader(targets, sizeof(targets) / sizeof(targets[0]));
  std::ifstream input(filename, std::ios::in | std::ios::binary);
  while (input.good()) {
    loader.addByte(input.get());
  }
  if (targets[0].success) {
    return true;
  } else {
    invalidateSpeedTable(table);
    return false;
  }
}

}  // namespace sail

#endif
