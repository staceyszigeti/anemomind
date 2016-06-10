/*
 *  Created on: 2015
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#ifndef SERVER_NAUTICAL_GPSFILTER_H_
#define SERVER_NAUTICAL_GPSFILTER_H_

#include <server/nautical/NavCompatibility.h>
#include <server/math/nonlinear/DataFit.h>
#include <server/nautical/GeographicReference.h>

namespace sail {

struct GpsFilterSettings {
  GpsFilterSettings();

  double motionWeight;
  Duration<double> samplingPeriod;
  double regWeight;
  Length<double> inlierThreshold;
  irls::Settings irlsSettings;
};

struct GpsFilter {
  // Before filtering
  NavDataset rawNavs;
  Array<Observation<2> > positionObservations;

  // Related to the optimization
  Sampling sampling;
  MDArray2d Xmeters;
  TimeStamp timeRef;
  GeographicReference geoRef;
  Spani reliableSampleRange;

  Arrayb inlierMask();
  NavDataset filteredNavs() const;
  Sampling::Weights calcWeights(TimeStamp t) const;
  HorizontalMotion<double> calcMotion(const Sampling::Weights &w) const;
  GeographicPosition<double> calcPosition(const Sampling::Weights &w) const;

  // Please check this first before consuming anything else.
  bool defined() const;

  static GpsFilter filter(NavDataset navs, GpsFilterSettings settings);
};


}  // namespace sail

#endif /* SERVER_NAUTICAL_GPSFILTER_H_ */
