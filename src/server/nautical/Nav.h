/*
 * Nav.h
 *
 *  Created on: 16 janv. 2014
 *      Author: jonas
 */

#ifndef NAV_H_
#define NAV_H_

#include <string>
#include <server/common/MDArray.h>
#include <server/common/math.h>
#include <server/common/PhysicalQuantity.h>
#include <server/nautical/GeographicPosition.h>

namespace sail {


// Represents a single recording of data from the devices onboard.
class Nav {
 public:
  Nav();
  Nav(MDArray2d row);
  virtual ~Nav();

  // For sorting
  bool operator< (const Nav &other) const {
    return _timeSince1970 < other._timeSince1970;
  }

  Duration<double> time() const {return _timeSince1970;}
  const GeographicPosition<double> &geographicPosition() const {return _pos;}
  Angle<double> awa() const {return _awa;}
  Velocity<double> aws() const {return _aws;}
  Angle<double> magHdg() const {return _magHdg;}
  Angle<double> gpsBearing() const {return _gpsBearing;}
  Velocity<double> gpsSpeed() const {return _gpsSpeed;}
  Velocity<double> watSpeed() const {return _watSpeed;}

  // This is just temporary. We should
  // replace it with CMake-generated paths in the future.
  static const char AllNavsPath[];
 private:

  Velocity<double> _gpsSpeed;
  Angle<double> _awa;
  Velocity<double> _aws;

  // Can we trust these estimates of the true wind? Don't think so. We'd better reconstruct them
  // with a good model.
  Angle<double> _twaFromFile;
  Velocity<double> _twsFromFile;

  Angle<double> _magHdg;
  Velocity<double> _watSpeed;
  Angle<double> _gpsBearing;

  GeographicPosition<double> _pos;

  // What does cwd and wd stand for? I forgot...
  double _cwd;
  double _wd;



  // TIME RELATED
  Duration<double> _timeSince1970;
};

Array<Nav> loadNavsFromText(std::string filename, bool sort = true);
bool areSortedNavs(Array<Nav> navs);
void plotNavTimeVsIndex(Array<Nav> navs);
void dispNavTimeIntervals(Array<Nav> navs);
Array<Array<Nav> > splitNavsByDuration(Array<Nav> navs, double durSeconds);
MDArray2d calcNavsEcefTrajectory(Array<Nav> navs);
Array<MDArray2d> calcNavsEcefTrajectories(Array<Array<Nav> > navs);
void plotNavsEcefTrajectory(Array<Nav> navs);
void plotNavsEcefTrajectories(Array<Array<Nav> > navs);
int countNavs(Array<Array<Nav> > navs);


} /* namespace sail */

#endif /* NAV_H_ */
