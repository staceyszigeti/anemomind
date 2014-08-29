/*
 *  Created on: 2014-05-20
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 *
 *  Port of Matlab routines for target speed computation,
 *  see e-mail on 2014-05-08 with subject "What I used to produce Irene perts last year".
 */

#ifndef TARGETSPEED_H_
#define TARGETSPEED_H_

#include <ostream>
#include <server/nautical/Nav.h>
#include <server/common/Histogram.h>

namespace sail {




class RefImplTgtSpeed {
 public:
  static Arrayd makeDefaultQuantiles();
  RefImplTgtSpeed(bool isUpwind_, Array<Velocity<double> > tws, Array<Velocity<double> > vmg,
          Array<Velocity<double> > bounds, Arrayd quantiles_);

  Array<Velocity<double> > binCenters;
  Array<Array<Velocity<double> > > medianValues;
  bool isUpwind;
  Arrayd quantiles;

  void plot();
};

class TargetSpeedData {
 public:
  TargetSpeedData() {}
  TargetSpeedData(Array<Velocity<double> > windSpeeds,
      Array<Velocity<double> > vmg,
      int binCount, Velocity<double> minTws, Velocity<double> maxTws,
      Arrayd quantiles = RefImplTgtSpeed::makeDefaultQuantiles());
  void plot(std::string title = "Untitled target speed");

  Arrayd targetVmgForWindSpeed(Velocity<double> windSpeed) const;
 private:
  void init(Array<Velocity<double> > windSpeeds,
      Array<Velocity<double> > vmg,
      HistogramMap map,
      Arrayd quantiles);

  Velocity<double> wrap(double x) {return Velocity<double>::knots(x);}
  double unwrap(Velocity<double> x) {return x.knots();}
  std::function<double(Velocity<double>)> makeUnwrapper() {return [=](Velocity<double> x) {return unwrap(x);};}

  // All velocities are internally stored as [knots]
  // Such a convention is reasonable, since HistogramMap only
  // works with doubles.
  Arrayd _quantiles;
  HistogramMap _hist;
  Array<Arrayd> _medianValues;
};

// Function used by getUpwindVmg and getDownwindVmg.
Array<Velocity<double> > calcVmg(Array<Nav> navs, bool isUpwind);

// Using the TWA from file
Array<Velocity<double> > calcExternalVmg(Array<Nav> navs, bool isUpwind);

// All navs are collected from upwind legs
Array<Velocity<double> > calcUpwindVmg(Array<Nav> navs);

// All navs are collected from downwind legs
Array<Velocity<double> > calcDownwindVmg(Array<Nav> navs);

// Tws
Array<Velocity<double> > estimateTws(Array<Nav> navs);

// Tws from file, reliable.
Array<Velocity<double> > estimateExternalTws(Array<Nav> navs);


// Pack "upwind" and "downwind" tables into a TargetSpeedTable and
// save it to the given ostream, in binary/chunk format.
void saveTargetSpeedTableChunk(
    ostream *stream,
    const TargetSpeedData& upwind,
    const TargetSpeedData& downwind);

} /* namespace sail */

#endif /* TARGETSPEED_H_ */
