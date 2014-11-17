/*
 *  Created on: 2014-
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "SynthBoatTrajectory.h"
#include <cmath>

namespace sail {


void SynthBoatTrajectory::WayPt::solveCircleTangentLine(const WayPt &a, bool posa,
      const WayPt &b, bool posb,
      Angle<double> *outAngleA, Angle<double> *outAngleB) {
  Vectorize<Length<double>, 2> dif = a.pos - b.pos;
  double C = sqrt(sqr(dif[0].meters()) + sqr(dif[1].meters()));
  double r = (posa? 1 : -1)*a.radius.meters();
  double t = (posb? 1 : -1)*b.radius.meters();
  double u = atan2(dif[0].meters(), dif[1].meters());
  double sol1 = asin((t - r)/C);
  double sol2 = M_PI - sol1;
  *outAngleA = Angle<double>::radians(sol1 - u);
  *outAngleB = Angle<double>::radians(sol2 - u);
}





} /* namespace mmm */
