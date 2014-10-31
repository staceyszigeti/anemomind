/*
 *  Created on: 2014-
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <server/math/ADFunction.h>
#include <server/nautical/CalibratedNavData.h>
#include <server/math/nonlinear/Levmar.h>
#include <server/math/armaadolc.h>
#include <server/common/ScopedLog.h>
#include <server/common/string.h>
#include <adolc/taping.h>
#include <server/common/Span.h>
#include <server/common/ProportionateSampler.h>
#include <algorithm>
#include <server/math/BandMat.h>


namespace sail {

CalibratedNavData::Settings::Settings() :
    costType(L2_COST),
    weightType(DIRECT),
    order(1) {
    levmar.maxiter = 30;
}

const char *CalibratedNavData::Settings::costTypeString(CostType costType) {
  const char *str[COST_TYPE_COUNT] = {"L2_COST", "L1_COST"};
  return str[int(costType)];
}

const char *CalibratedNavData::Settings::weightTypeString(WeightType weightType) {
  const char *str[WEIGHT_TYPE_COUNT] = {"DIRECT", "SQRT_ABS", "UNIFORM"};
  return str[int(weightType)];
}


namespace {

  /*
   * A cost function that looks like the square root of absolute value,
   * except that it is smooth close to 0, so that we
   * can use it with a smooth optimization algorithm.
   *
   * The width parameter controls how close to 0 it should
   * be smooth.
   *
   * This function will tend to encourage sparse solutions,
   * in contrast to the square function.
   *
   * The sqrt function is applied, so that we can use it
   * with the Levenberg-Marquardt framework.
   */
  adouble sqrtRoundedAbs(adouble x0, double width) {
    adouble x = x0/width;
    double xd = ToDouble(x);
    if (std::abs(xd) < 1.0) {
      constexpr double a = 0.5;
      constexpr double b = 0.5;
      return sqrt(a*x*x + b);
    } else {
      return sqrt(fabs(x));
    }
  }

  adouble applyCostFun(adouble x, double w) {
    return x;
    //return sqrtRoundedAbs(x, w);
  }

  Velocity<double> makePositive(Velocity<double> x) {
    return Velocity<double>::knots(std::max(0.00001, x.knots()));
  }

  class BaseObjf {
   public:
    static constexpr int eqsPerComparison = 4;

    BaseObjf(FilteredNavData data_, CorrectorSet<adouble>::Ptr corr_, Arrayd times_,
        const CalibratedNavData::Settings &settings);

    FilteredNavData data;
    CorrectorSet<adouble>::Ptr corr;
    Arrayd times, weights;
    LineKM sampling;
    CalibratedNavData::Settings settings;

    adouble balance(adouble *parameters) {
      return parameters[corr->paramCount()];
    }
    CalibratedValues<adouble> computeAtIndex(int time, adouble *parameters);
    CalibratedValues<adouble> computeAtTime(double time, adouble *parameters);

    void evalDeriv(double time, adouble *params, HorizontalMotion<adouble> *windder,
          HorizontalMotion<adouble> *currentder);

    double getWeight(int index) const;
    adouble applyCostFun(adouble x) const;
    double low, high;

    Arrayd difCoefs;
  };



  CalibratedValues<adouble> BaseObjf::computeAtIndex(int index, adouble *parameters) {

    HorizontalMotion<adouble> gpsMotion =
        HorizontalMotion<adouble>::polar(data.gpsSpeed().get(index).cast<adouble>(),
            data.gpsBearing().get(index).cast<adouble>());

    return CalibratedValues<adouble>(*corr,
                  parameters,
                  gpsMotion,
                  Angle<adouble>::degrees(data.magHdg().get(index).degrees()),
                  makePositive(data.watSpeed().get(index)),
                  Angle<adouble>::degrees(data.awa().get(index).degrees()),
                  makePositive(data.aws().get(index)));
  }

  CalibratedValues<adouble> BaseObjf::computeAtTime(double time, adouble *parameters) {

    HorizontalMotion<adouble> gpsMotion =
        HorizontalMotion<adouble>::polar(data.gpsSpeed().get(time).cast<adouble>(),
            data.gpsBearing().interpolateLinear(time).cast<adouble>());

    return CalibratedValues<adouble>(*corr,
                  parameters,
                  gpsMotion,
                  Angle<adouble>::degrees(data.magHdg().interpolateLinear(time).degrees()),
                  makePositive(data.watSpeed().interpolateLinear(time)),
                  Angle<adouble>::degrees(data.awa().interpolateLinear(time).degrees()),
                  makePositive(data.aws().interpolateLinear(time)));
  }

  void BaseObjf::evalDeriv(double time, adouble *params,
        HorizontalMotion<adouble> *windder,
        HorizontalMotion<adouble> *currentder) {
    int index = int(floor(sampling.inv(time)));
    CalibratedValues<adouble> a = computeAtIndex(index + 0, params);
    CalibratedValues<adouble> b = computeAtIndex(index + 1, params);
    *windder = b.trueWind - a.trueWind;
    *currentder = b.trueCurrent - a.trueCurrent;
  }

  double BaseObjf::getWeight(int index) const {
    double w = weights[index];
    switch (settings.weightType) {
      case CalibratedNavData::Settings::DIRECT:
        return w;
      case CalibratedNavData::Settings::SQRT_ABS:
        return sqrt(std::abs(w));
      case CalibratedNavData::Settings::UNIFORM:
        return 1;
      default:
        return w;
    };
    return w;
  }

  adouble BaseObjf::applyCostFun(adouble x) const {
    switch (settings.costType) {
      case CalibratedNavData::Settings::L1_COST:
        return sqrtRoundedAbs(x, 0.01);
      default:
        return x;
    }
  }


  const double marg2 = 1.0e-6;

  BaseObjf::BaseObjf(FilteredNavData data_, CorrectorSet<adouble>::Ptr corr_,
    Arrayd times_, const CalibratedNavData::Settings &settings_) :
      data(data_), corr(corr_),
      times(times_),
      weights(data_.magHdg().interpolateLinearDerivative(times_).map<double>([&](Angle<double> x) {return x.degrees();})),
      sampling(data_.sampling()), settings(settings_),
      low(data_.low() + marg2), high(data_.high() - marg2),
      difCoefs(BandMatInternal::makeCoefs(settings_.order)) {}



  /*
   * Add a parameter that controls the balance
   * between the current and wind fitness.
   */
  Arrayd addBalanceParam(Arrayd calibParams) {
    int count = calibParams.size();
    Arrayd dst(count + 1);
    dst.last() = 0.5;
    calibParams.copyToSafe(dst.sliceBut(1));
    return dst;
  }



  /*
   * This objective function
   * tries to minimize the derivatives of true wind and current
   * weighted by the derivative of the magnetic heading.
   */
  class Objf1 : public Function {
   public:
    Objf1(FilteredNavData data, CorrectorSet<adouble>::Ptr corr, Arrayd times,
        const CalibratedNavData::Settings &settings);

    int inDims() {
      return _base.corr->paramCount() + 1;
    }

    int outDims() {
      return BaseObjf::eqsPerComparison*_base.times.size();
    }

   private:
    BaseObjf _base;


    void evalDif(double w, CalibratedValues<adouble> a,
        CalibratedValues<adouble> b, adouble balance, adouble *dst);
    void evalDifAnyOrder(double weight, double time, adouble *parameters, adouble *dst);
    void eval(double *Xin, double *Fout, double *Jout);
  };

  Objf1::Objf1(FilteredNavData data, CorrectorSet<adouble>::Ptr corr, Arrayd times,
      const CalibratedNavData::Settings &settings) :
    _base(data, corr, times, settings) {}


  void Objf1::eval(double *Xin, double *Fout, double *Jout) {
    ENTERSCOPE("Evaluating calibration objf");
    assert(_base.times.size() == _base.weights.size());


    Arrayad adX(inDims());
    Arrayad adF(4);

    int rows = outDims();
    int cols = inDims();
    MDArray2d Jmat;
    if (Jout != nullptr) {
      Jmat = MDArray2d(rows, cols, Jout);
    }

    /*
     * Here we differentiate each
     * iteration separately. It turns
     * out that this results in a
     * significant speed-up. Not doing
     * so is painfully slow, for some
     * reason.
     */
    short int tape = 0;
    for (int i = 0; i < _base.times.size(); i++) {
      if (Jout != nullptr) {
        trace_on(tape);
      }

      adolcInput(inDims(), adX.getData(), Xin);
      if (i % 10000 == 0) {
        SCOPEDMESSAGE(INFO, stringFormat("Iteration %d/%d", i, _base.times.size()));
      }
      int index = int(floor(_base.sampling.inv(_base.times[i])));

      double weight = _base.getWeight(i);
      if (_base.settings.order == 1) {
        CalibratedValues<adouble> from = _base.computeAtIndex(index, adX.ptr());
        CalibratedValues<adouble> to = _base.computeAtIndex(index + 1, adX.ptr());
        evalDif(weight, from, to, _base.balance(adX.ptr()), adF.getData());
      } else {
        evalDifAnyOrder(weight, _base.times[i], adX.ptr(), adF.getData());
      }


      int at = i*_base.eqsPerComparison;
      adolcOutput(_base.eqsPerComparison, adF.getData(), Fout + at);

      if (Jout != nullptr) {
        trace_off();
        outputJacobianColMajor(tape, Xin, Jmat.getPtrAt(at, 0), rows);
      }
    }
  }

  void Objf1::evalDif(double w, CalibratedValues<adouble> a,
      CalibratedValues<adouble> b, adouble balance, adouble *dst) {

    // Skip the balancing.
    balance = 0.5;

    dst[0] = _base.applyCostFun(balance*w*(a.trueWind[0].knots()
        - b.trueWind[0].knots()));
    dst[1] = _base.applyCostFun(balance*w*(a.trueWind[1].knots()
        - b.trueWind[1].knots()));
    dst[2] = _base.applyCostFun((1.0 - balance)*w*(a.trueCurrent[0].knots()
        - b.trueCurrent[0].knots()));
    dst[3] = _base.applyCostFun((1.0 - balance)*w*(a.trueCurrent[1].knots()
        - b.trueCurrent[1].knots()));
  }

  void Objf1::evalDifAnyOrder(double weight, double time, adouble *parameters, adouble *dst) {
    int count = _base.difCoefs.size();
    double half = 0.5*_base.settings.order;
    LineKM times(0, count-1, time - half, time + half);
    if (_base.low < times(0) && times(_base.settings.order) < _base.high) {
      for (int j = 0; j < 4; j++) {
        dst[j] = 0.0;
      }
      adouble balance = _base.balance(parameters);
      for (int i = 0; i < count; i++) {
        CalibratedValues<adouble> values = _base.computeAtTime(time, parameters);
        adouble windWeight = weight*balance*_base.difCoefs[i];
        adouble currentWeight = weight*(1.0 - balance)*_base.difCoefs[i];
        for (int j = 0; j < 2; j++) {
          dst[0 + j] += windWeight*values.trueWind[i];
        }
      }


    } else {
      for (int i = 0; i < 4; i++) {
        dst[i] = 0.0;
      }
    }
  }


  class TimeAndWeight {
   public:
    TimeAndWeight() : time(NAN), weight(NAN) {}
    TimeAndWeight(double t, double w) : time(t), weight(w) {}

    double time, weight;
    bool operator< (const TimeAndWeight &other) {
      return weight < other.weight;
    };
  };

  Array<Spani> makeSortedChunks(Array<TimeAndWeight> data, int chunkSize) {
    std::random_shuffle(data.begin(), data.end());
    int chunkCount = div1(data.size(), chunkSize);
    Array<Spani> spans(chunkCount);
    for (int i = 0; i < chunkCount; i++) {
      int from = i*chunkSize;
      int to = std::min(from + chunkSize, data.size());
      spans[i] = Spani(from, to);
    }
    return spans;
  }

  class Objf2 : public Function {
   public:
    Objf2(FilteredNavData data, CorrectorSet<adouble>::Ptr corr, Arrayd times,
        const CalibratedNavData::Settings &settings);
    int inDims() {
      return 1 + _base.corr->paramCount();
    }

    int outDims() {
      return 2*_spans.size();
    }

    void eval(double *Xin, double *Fout, double *Jout);
   private:
    void calcMeans(Array<TimeAndWeight> data, adouble *params, adouble *out);
    BaseObjf _base;
    Array<TimeAndWeight> _timeAndWeight;
    Array<Spani> _spans;
    int _chunkSize;
  };

  Objf2::Objf2(FilteredNavData data, CorrectorSet<adouble>::Ptr corr, Arrayd times,
      const CalibratedNavData::Settings &settings) :
    _base(data, corr, times, settings) {
    int count = _base.times.size();
    assert(count == _base.weights.size());
    _timeAndWeight.create(count);
    for (int i = 0; i < count; i++) {
      _timeAndWeight[i] = TimeAndWeight(_base.times[i], _base.weights[i]);
    }
    _chunkSize = 50;

    // Multiple chunks as some sort of cross validation, to avoid overfitting.
    _spans = makeSortedChunks(_timeAndWeight, _chunkSize);
  }

  void Objf2::eval(double *Xin, double *Fout, double *Jout) {
    Arrayad adX(inDims());
    Arrayad adF(2);

    int rows = outDims();
    int cols = inDims();
    MDArray2d Jmat;
    if (Jout != nullptr) {
      Jmat = MDArray2d(rows, cols, Jout);
    }

    short int tape = 0;
    for (int i = 0; i < _spans.size(); i++) {
      if (Jout != nullptr) {
        trace_on(tape);
      }
      Spani span = _spans[i];
      int middle = span.middle();

      adolcInput(inDims(), adX.getData(), Xin);
      adouble lowmeans[2], highmeans[2];
      calcMeans(_timeAndWeight.slice(span.minv(), middle), adX.ptr(), lowmeans);
      calcMeans(_timeAndWeight.slice(middle, span.maxv()), adX.ptr(), highmeans);

      adouble adF[2] = {lowmeans[0] - highmeans[0], lowmeans[1] - highmeans[1]};

      adolcOutput(2, adF, Fout + 2*i);

      if (Jout != nullptr) {
        trace_off();
        outputJacobianColMajor(tape, Xin, Jmat.getPtrAt(2*i, 0), rows);
      }
    }
  }

  void Objf2::calcMeans(Array<TimeAndWeight> data, adouble *params, adouble *out) {
    int count = data.size();
    double factor = 1.0/count;
    assert(factor > 0);
    assert(count > 0);
    assert(!std::isnan(factor));
    out[0] = 0;
    out[1] = 0;
    for (int i = 0; i < count; i++) {
      HorizontalMotion<adouble> wind, current;
      _base.evalDeriv(data[i].time, params, &wind, &current);
      out[0] += factor*wind.norm().knots();
      out[1] += factor*current.norm().knots();
      assert(!std::isnan(out[0].getValue()));
      assert(!std::isnan(out[1].getValue()));
    }
  }

}

namespace {

  CorrectorSet<adouble>::Ptr makeDefaultCorr(CorrectorSet<adouble>::Ptr correctorSet) {
    return (bool(correctorSet)? correctorSet : CorrectorSet<adouble>::Ptr(new DefaultCorrectorSet<adouble>()));
  }

}

CalibratedNavData::CalibratedNavData(FilteredNavData filteredData,
      Arrayd times, CorrectorSet<adouble>::Ptr correctorSet,
      Settings settings, Arrayd initialization) : _filteredRawData(filteredData) {
  ENTERSCOPE("CalibratedNavData");
  _correctorSet = makeDefaultCorr(correctorSet);
  if (times.empty()) {
    times = filteredData.makeCenteredX();
  }

  _correctorSetd = _correctorSet->toDouble();

  Objf1 objf(filteredData, _correctorSet, times, settings);


  SCOPEDMESSAGE(INFO, stringFormat("Objf dimensions: %d", objf.outDims()));
  if (initialization.empty()) {
    initialization = _correctorSet->makeInitialParams(0.0);
  }
  initialization = addBalanceParam(initialization);
  _initialCalibrationParameters = initialization.dup();

  constexpr bool checkJacobian = true;
  if (checkJacobian) {
    double maxjdif = objf.maxNumJacDif(initialization.ptr());
    assert(maxjdif < 1.0e-3);
    std::cout << EXPR_AND_VAL_AS_STRING(maxjdif) << std::endl;
  }

  assert(initialization.size() == objf.inDims());
  LevmarState state(initialization);
  state.minimize(settings.levmar, objf);
  _optimalCalibrationParameters = state.getXArray(true);

  _initCost = objf.calcSquaredNorm(_initialCalibrationParameters.ptr());
  _cost = objf.calcSquaredNorm(_optimalCalibrationParameters.ptr());
  _costType = settings.costType;
  _weightType = settings.weightType;
}

Arrayd CalibratedNavData::sampleTimes(FilteredNavData navdata, int count) {
  Arrayd times = navdata.makeCenteredX();
  if (times.size() < count) {
    return times;
  }
  Arrayd maghdg = navdata.magHdg().interpolateLinear(times).map<double>([&](Angle<double> x) {
    return std::abs(x.degrees());
  });

  ProportionateSampler prop(maghdg);
  double marg = 1.0e-6;
  Arrayd selected(count);
  LineKM line(0, count-1, marg, 1.0-marg);
  for (int i = 0; i < count; i++) {
    selected[i] = times[prop.getAndRemove(line(i))];
  }
  return selected;
}

CalibratedNavData CalibratedNavData::bestOfInits(Array<Arrayd> initializations,
    FilteredNavData fdata, Arrayd times,
    CorrectorSet<adouble>::Ptr correctorSet,
             Settings settings) {
  correctorSet = makeDefaultCorr(correctorSet);
  ENTERSCOPE("Calibration starting from different initializations");
  assert(initializations.hasData());
  CalibratedNavData best;
  for (int i = 0; i < initializations.size(); i++) {
    SCOPEDMESSAGE(INFO, stringFormat("Calibrating from starting point %d/%d",
        i+1, initializations.size()));
    CalibratedNavData candidate(fdata, times, correctorSet, settings,
        initializations[i]);
    SCOPEDMESSAGE(INFO, stringFormat("Cost: %.8g   Initial cost: %.8g (best cost: %.8g)", candidate.cost(),
        candidate.initCost(), best.cost()));
    if (candidate < best) {
      best = candidate;
      SCOPEDMESSAGE(INFO, "  Improvement :-)");
    }
  }
  return best;
}

CalibratedNavData CalibratedNavData::bestOfInits(int initCount,
    FilteredNavData fdata, Arrayd times,
    CorrectorSet<adouble>::Ptr correctorSet,
             Settings settings) {
  correctorSet = makeDefaultCorr(correctorSet);
  assert(initCount > 0);
  Array<Arrayd> inits(initCount);

  // Gradually increase the randomness...
  LineKM randomness(0, std::max(1, initCount-1), 0.0, 0.2);

  for (int i = 0; i < initCount; i++) {
    inits[i] = correctorSet->makeInitialParams(randomness(i));
  }
  return bestOfInits(inits, fdata, times, correctorSet, settings);
}

void CalibratedNavData::outputGeneralInfo(std::ostream *dst) const {
  *dst << "OPTIMIZATION INFO" << std::endl;
  *dst << "  Cost type:   " << Settings::costTypeString(_costType) << std::endl;
  *dst << "  Weight type: " << Settings::weightTypeString(_weightType) << std::endl;
}

CalibratedValues<double> CalibratedNavData::calibratedValues(double localTime) const {
  return CalibratedValues<double>(*_correctorSetd,
      _optimalCalibrationParameters.ptr(),
      _filteredRawData.gpsMotion(localTime),
      _filteredRawData.magHdg().interpolateLinear(localTime),
      makePositive(_filteredRawData.watSpeed().interpolateLinear(localTime)),
      _filteredRawData.awa().interpolateLinear(localTime),
      makePositive(_filteredRawData.aws().interpolateLinear(localTime)));
}


}
