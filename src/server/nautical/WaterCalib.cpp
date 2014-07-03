/*
 *  Created on: 2014-07-03
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "WaterCalib.h"
#include <server/math/armaadolc.h>
#include <server/math/ADFunction.h>
#include <server/math/GemanMcClure.h>
#include <server/math/nonlinear/Levmar.h>
#include <server/common/SharedPtrUtils.h>
#include <server/math/nonlinear/LevmarSettings.h>

namespace sail {

namespace {
  Array<Nav> getDownwindNavs(Array<Nav> navs) {
    return navs.slice([&](const Nav &n) {
      return cos(n.externalTwa()) < 0;
    });
  }
}

WaterCalib::WaterCalib(const HorizontalMotionParam &param, Velocity<double> sigma, Velocity<double> initR) :
    _param(param), _sigma(sigma), _initR(initR) {}

void WaterCalib::initialize(double *outParams) const {
  wcK(outParams) = SpeedCalib<double>::initK();
  wcM(outParams) = 0;
  wcC(outParams) = 0;
  wcAlpha(outParams) = 0;
  magOffset(outParams) = 0;
  _param.initialize(outParams + 5);
}

Arrayd WaterCalib::makeInitialParams() const {
  Arrayd dst(paramCount());
  initialize(dst.ptr());
  return dst;
}

namespace {
  class WaterCalibObjf : public AutoDiffFunction {
   public:
    WaterCalibObjf(const WaterCalib &calib, Array<Nav> navs) : _calib(calib), _navs(navs) {}
    int inDims() {
      return _calib.paramCount();
    }

    int outDims() {
      return 2*_navs.size();
    }

    void evalAD(adouble *Xin, adouble *Fout);
   private:
    Array<Nav> _navs;
    const WaterCalib &_calib;
  };

  void WaterCalibObjf::evalAD(adouble *Xin, adouble *Fout) {
    //Arrayad X(inDims(), Xin);
    SpeedCalib<adouble> sc = _calib.makeSpeedCalib<adouble>(Xin);
  }

}

Arrayd WaterCalib::optimize(Array<Nav> allnavs) const {
  Array<Nav> navs = getDownwindNavs(allnavs);

  WaterCalibObjf rawObjf(*this, navs);
  GemanMcClureFunction robustObjf(unwrap(_sigma), unwrap(_initR), 2,
          makeSharedPtrToStack(rawObjf));

  LevmarSettings settings;
  LevmarState state(makeInitialParams());
  state.minimize(settings, robustObjf);
  return state.getXArray();
}


} /* namespace mmm */
