/*
 * SmoothGPSFilter.h
 *
 *  Created on: May 12, 2016
 *      Author: jonas
 */

#ifndef SERVER_NAUTICAL_FILTERS_SMOOTHGPSFILTER_H_
#define SERVER_NAUTICAL_FILTERS_SMOOTHGPSFILTER_H_

#include <server/math/Curve2dFilter.h>
#include <server/nautical/GeographicReference.h>
#include <server/nautical/NavDataset.h>
#include <server/nautical/GeographicReference.h>

namespace sail {

namespace DOM {struct Node;}

Curve2dFilter::Settings makeDefaultOptSettings();

struct GpsFilterSettings {
  Curve2dFilter::Settings curveFilterSettings = makeDefaultOptSettings();
  Duration<double> samplingPeriod = Duration<double>::seconds(1.0);
  Duration<double> subProblemThreshold = Duration<double>::minutes(3.0);
  Duration<double> subProblemLength = Duration<double>::hours(4.0);
  int medianWindowLength = 5;
  Length<double> positionSupportThreshold = 100.0_m;
};

struct LocalGpsFilterResults {
  typedef Curve2dFilter::Results::Curve Curve;
  GeographicReference geoRef;
  Curve2dFilter::Results filterResults;
  Duration<double> computationTime;
  Array<TimedValue<Curve2dFilter::Vec2<Length<double>>>> rawLocalPositions;
  Array<Curve> curves;

  bool empty() const {return curves.empty();}

  Duration<double> computationTimePerSample() const {
    return (1.0/filterResults.timeMapper.sampleCount())*computationTime;
  }

  Array<TimedValue<GeographicPosition<double>>> samplePositions() const;
  Array<TimedValue<HorizontalMotion<double>>> sampleMotions() const;
};

void outputLocalResults(
    const LocalGpsFilterResults& r,
    DOM::Node *dst);

LocalGpsFilterResults solveGpsSubproblem(
    const TimeMapper& mapper,
    const Array<TimedValue<GeographicPosition<double>>> rawPositions,
    const Array<TimedValue<HorizontalMotion<double>>> &motions,
    const GpsFilterSettings &settings,
    DOM::Node *dst);

struct GpsFilterResults {
  bool empty() const {return positions.empty();}
  TimedSampleCollection<GeographicPosition<double> >::TimedVector positions;
  TimedSampleCollection<HorizontalMotion<double> >::TimedVector motions;
};

Array<TimeStamp> listSplittingTimeStamps(const Array<TimeStamp> &timeStamps,
    Duration<double> threshold);

GpsFilterResults filterGpsData(
    const NavDataset &ds,
    DOM::Node *dst,
    const GpsFilterSettings &settings = GpsFilterSettings());

template <typename T>
Array<Array<T> > applySplits(const Array<T> &src,
    const Array<TimeStamp> &splits);
}

#endif /* SERVER_NAUTICAL_FILTERS_SMOOTHGPSFILTER_H_ */
