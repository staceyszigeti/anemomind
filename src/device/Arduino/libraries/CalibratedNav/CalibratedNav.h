/*
 *  Created on: 2014-11-06
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#ifndef CALIBRATEDNAV_H_
#define CALIBRATEDNAV_H_

#include "../PhysicalQuantity/PhysicalQuantity.h"
#include <cassert>

namespace sail {

// More powerful and safe than using nan
// to indicate whether a value is defined or not.
template <typename T>
class DefinedValue {
 public:
  DefinedValue() : _defined(false) {}
  DefinedValue(T x) : _defined(true), _value(x) {}

  T get() const {
    assert(_defined); // <-- only active in debug mode.
    return _value;
  }

  T get(T defaultValue) const {
    return (_defined? _value : defaultValue);
  }

  void set(T x) {
    _defined = true;
    _value = x;
  }

  void setOnce(T x) {
    assert(!_defined);
    set(x);
  }

  bool defined() const {return _defined;}
  bool undefined() const {return !_defined;}

  // TODO:
  // Would it make sense to make
  // the assignment operator private
  // for this class, so that we are obliged
  // to use the set/get methods?
 private:
  bool _defined;
  T _value;
};


/*
 * The purpose of this class
 * is to hold the results of a
 * calibration procedure.
 *
 * It is initialized from an InstrumentAbstraction
 * (such as a Nav) and the idea is then that some
 * calibration procedure (not implemented here) populates
 * it with calibrated values.
 *
 * Note that this class does not exhibit the
 * the InstrumentAbstraction type of methods (awa(), aws(), etc...)
 * because for each value, there is a raw and calibrated one. However,
 * its constructors accepts an argument that follows the InstrumentAbstracation
 * convention.
 *
 *
 * How to use this class:
 *  1. Construct an instance from an object exhibiting the InstrumentAbstraction interface.
 *  2. Populate the calibrated instance variables any way you like.
 *  3. Call the fill() method to compute true wind, current, etc, using the calibrated values.
 */
template <typename T>
class CalibratedNav {
 public:
  CalibratedNav() {}

  typedef DefinedValue<Angle<T> > DefinedAngle;
  typedef DefinedValue<Velocity<T> > DefinedVelocity;
  typedef DefinedValue<HorizontalMotion<T> > DefinedMotion;

  // InstrumentAbstraction can for instance be a Nav.
  template <typename InstrumentAbstraction>
  CalibratedNav(const InstrumentAbstraction &x) :
    rawAwa(x.awa()), rawMagHdg(x.magHdg()),
    rawAws(x.aws()), rawWatSpeed(x.rawWatSpeed()),
    gpsMotion(HorizontalMotion<T>::polar(x.gpsSpeed(), x.gpsBearing())),
    driftAngle(Angle<T>::degrees(0)) {}

  /*
   * Since all instance variables are encapsulated
   * in the type DefinedValue, I think we can make
   * them public instead of having them private.
   */
  // Values that are populated by the constructor.
  DefinedMotion gpsMotion;
  DefinedAngle rawAwa, rawMagHdg;
  DefinedVelocity rawAws, rawWatSpeed;

  // Values that need to be calibrated externally.
  DefinedAngle calibAwa, boatOrientation;
  DefinedVelocity calibAws, calibWatSpeed;
  DefinedAngle driftAngle; // <-- Optional to calibrate.

  // Values that are populated using the fill() method.
  // Depend on the calibrated values.
  DefinedAngle  apparentWindAngleWrtEarth;
  DefinedMotion apparentWind;
  DefinedMotion trueWind;
  DefinedMotion trueCurrent;
  DefinedMotion boatMotionThroughWater;

  // Call this method once calibrated values have been provided.
  void fill() {
    // Compute the true wind
    apparentWindAngleWrtEarth.set(calibAwa.get() + boatOrientation.get()
        + Angle<T>::degrees(T(180)));
    apparentWind.set(HorizontalMotion<T>::polar(calibAws,
        apparentWindAngleWrtEarth));
    trueWind.set(apparentWind + gpsMotion);

    // Compute the true current
    boatMotionThroughWater.set(HorizontalMotion<T>::polar(
        calibWatSpeed.get(), driftAngle.get() + boatOrientation.get()));
    trueCurrent.set(gpsMotion.get() - boatMotionThroughWater.get());
  }

  // TODO: Extra conveniency methods, such as twdir.
};

}


#endif /* CALIBRATEDNAV_H_ */
