/*
 * StateReconstructor.h
 *
 *  Created on: 22 Sep 2016
 *      Author: jonas
 */

#ifndef SERVER_NAUTICAL_CALIB_BOATSTATERECONSTRUCTOR_H_
#define SERVER_NAUTICAL_CALIB_BOATSTATERECONSTRUCTOR_H_

#include <server/nautical/calib/Fitness.h>
#include <server/math/BandedLevMar.h>

namespace sail {

template <typename T, typename BoatStateSettings>
class BoatStateReconstructor {
public:
  BoatStateReconstructor(
      TimeStamp offsetTime,
      Duration<double> samplingPeriod,
      const Array<BoatState<T> > &base) :
        _offsetTime(offsetTime),
        _samplingPeriod(samplingPeriod),
        _base(base) {}

  template <DataCode code>
  void addObservation(
      TimedValue<typename TypeForCode<code>::type> &x) {
    double realIndex = (x.time - _offsetTime)/_samplingPeriod;
    int index = int(round(realIndex));
    const int inputCount = BoatStateParamCount<BoatStateSettings>::value;
    if (0 <= index && index < sampleCount()-1) {
      int from = index*inputCount;
      int to = from + 2*inputCount;
      _problem.addCost(
          Spani(from, to),
          new BoatStateFitness<code, BoatStateSettings>(
              realIndex, x.value,
              _base.ptr() + index));

    }
  }

  int sampleCount() const {
    return _base.size();
  }
private:
  TimeStamp _offsetTime;
  Duration<double> _samplingPeriod;
  Array<BoatState<double> > _base;
  BandedLevMar::Problem<T> _problem;
};


}



#endif /* SERVER_NAUTICAL_CALIB_BOATSTATERECONSTRUCTOR_H_ */