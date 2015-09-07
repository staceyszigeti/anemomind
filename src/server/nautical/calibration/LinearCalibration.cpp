/*
 *  Created on: 2015
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <server/nautical/calibration/LinearCalibration.h>
#include <armadillo>
#include <server/math/BandMat.h>
#include <server/common/ArrayIO.h>
#include <server/common/ScopedLog.h>
#include <server/common/string.h>

namespace sail {
namespace LinearCalibration {

void initializeLinearParameters(bool withOffset, double *dst2or4) {
  dst2or4[0] = 1.0;
  int n = (withOffset? 4 : 2);
  for (int i = 1; i < n; i++) {
    dst2or4[i] = 0.0;
  }
}


typedef Eigen::Triplet<double> Triplet;

CommonCalibrationSettings::CommonCalibrationSettings() {
  spcst.initialWeight = 0.01;
}

CommonCalibrationSettings CommonCalibrationSettings::firstOrderSettings() {
  CommonCalibrationSettings s;
  s.regOrder = 1;
  s.nonZeroPeriod = Duration<double>::seconds(6.0);
  s.inlierFrac = 0.1;
  return s;
}

CommonResults calibrateSparse(FlowMatrices mats, Duration<double> totalDuration, CommonCalibrationSettings settings) {
  ENTER_FUNCTION_SCOPE;
  assert(mats.A.rows() == mats.B.rows());
  assert(mats.B.cols() == 1);
  int flowDim = mats.A.rows();
  int flowCount = flowDim/2;
  assert(2*flowCount == flowDim);
  int paramDim = mats.A.cols();
  int regCount = flowCount - settings.regOrder;
  int regDim = 2*regCount;
  int dstCols = flowDim + paramDim + flowDim;
  int dstRows = flowDim + regDim + flowDim;
  int slackColOffset = flowDim + paramDim;
  int slackRowOffset = flowDim + regDim;
  auto regCoefs = makeRegCoefs(settings.regOrder);
  auto localRegCols = 2*regCoefs.size();

  SCOPEDMESSAGE(INFO, stringFormat("Number of regs: %d", regCount));
  SCOPEDMESSAGE(INFO, stringFormat("Number of flows: %d", flowCount));
  SCOPEDMESSAGE(INFO, stringFormat("Number of parameters: %d", paramDim));
  SCOPEDMESSAGE(INFO, stringFormat("Number of rows: %d", dstRows));
  SCOPEDMESSAGE(INFO, stringFormat("Number of cols: %d", dstCols));


  // The Adst matrix has this structure. It consists of four sub matrices,
  // aligned in two rows and two columns.
  // Adst = [I -mats.A; Reg 0]
  // The Bdst matrix has this structure. It is split into an upper and a lower matrix:
  // Bdst = [mats.B; 0]

  std::vector<Triplet> Adst;
  Eigen::VectorXd Bdst = Eigen::VectorXd::Zero(dstRows);
  Array<Spani> spans(regCount), slackSpans(flowCount);

  for (int i = 0; i < flowCount; i++) {
    int offset = slackRowOffset + 2*i;
    slackSpans[i] = Spani(offset, offset + 2);
  }

  SCOPEDMESSAGE(INFO, stringFormat("Building flow equations... (%d)", flowDim));
  for (int i = 0; i < flowDim; i++) {
    // Build the upper-left part of Adst
    Adst.push_back(Triplet(i, i, 1.0));

    // Add the slack for outliers
    Adst.push_back(Triplet(i, i + slackColOffset, 1.0));
    Adst.push_back(Triplet(i + slackRowOffset, i + slackColOffset, 1.0));

    // and fill the upper part of Bdst
    Bdst(i) = mats.B(i, 0);

    // Build the upper right part of Adst
    for (int j = 0; j < paramDim; j++) {
      Adst.push_back(Triplet(i, flowDim + j, -mats.A(i, j)));
    }
  }

  // Fill in the regularization coefficients of the lower left part of Adst
  // Thanks to sparsity, most of their residuals will be more or less exactly 0
  // once the problem is solved.
  SCOPEDMESSAGE(INFO, stringFormat("Building reg equations... (%d)", regCount));
  for (int i = 0; i < regCount; i++) {
    int offset = 2*i;
    int rowOffset = flowDim + offset;
    spans[i] = Spani(rowOffset, rowOffset + 2);
    for (int j = 0; j < regCoefs.size(); j++) {
      int localCol = offset + 2*j;
      auto c = regCoefs[j];
      Adst.push_back(Triplet(rowOffset + 0, localCol + 0, c));
      Adst.push_back(Triplet(rowOffset + 1, localCol + 1, c));
    }
  }
  SCOPEDMESSAGE(INFO, "Done building reg equations.");

  Eigen::SparseMatrix<double> AdstMat(dstRows, dstCols);
  AdstMat.setFromTriplets(Adst.begin(), Adst.end());
  double totalElemCount = double(dstRows)*double(dstCols);
  SCOPEDMESSAGE(INFO, stringFormat("Number of nonzero elements: %d", Adst.size()));
  SCOPEDMESSAGE(INFO, stringFormat("Sparsity degree: %.3g", double(Adst.size())/totalElemCount));

  // How many non-zero regularization residuals that we allow for.
  // Proportional to the total duration.
  int passiveCount = 1 + int(floor(totalDuration/settings.nonZeroPeriod));
  int activeCount = std::max(regCount - passiveCount, 0);

  SparsityConstrained::ConstraintGroup group{spans, activeCount};
  SparsityConstrained::ConstraintGroup slackGroup{slackSpans, int(ceil(settings.inlierFrac*flowCount))};
  auto flowAndParametersVector = SparsityConstrained::solve(AdstMat, Bdst,
      Array<SparsityConstrained::ConstraintGroup>{group, slackGroup},
      settings.spcst);
  if (flowAndParametersVector.size() == 0) {
    return CommonResults();
  }
  assert(flowAndParametersVector.size() == dstCols);
  Arrayd flowAndParameters(dstCols, flowAndParametersVector.data());
  Arrayd parameters = flowAndParameters.slice(flowDim, flowDim + paramDim).dup();
  Arrayd flowData = flowAndParameters.sliceTo(flowDim);
  Array<HorizontalMotion<double> > motions(flowCount);
  for (int i = 0; i < flowCount; i++) {
    int offset = 2*i;
    auto mx = Velocity<double>::knots(flowData[offset + 0]);
    auto my = Velocity<double>::knots(flowData[offset + 1]);
    motions[i] = HorizontalMotion<double>{mx, my};
  }
  return CommonResults{motions, parameters};
}

LinearCorrector::LinearCorrector(const FlowSettings &flowSettings,
    Arrayd windParams, Arrayd currentParams) :
    _flowSettings(flowSettings), _windParams(windParams), _currentParams(currentParams) {}

Array<CalibratedNav<double> > LinearCorrector::operator()(const Array<Nav> &navs) const {
  return navs.map<CalibratedNav<double> >([&](const Nav &x) {
    return (*this)(x);
  });
}

arma::mat asMatrix(const MDArray2d &x) {
  return arma::mat(x.ptr(), x.rows(), x.cols(), false, true);
}

arma::mat asMatrix(const Arrayd &x) {
  return arma::mat(x.ptr(), x.size(), 1, false, true);
}

CalibratedNav<double> LinearCorrector::operator()(const Nav &nav) const {
  MDArray2d Aw(2, _flowSettings.windParamCount()), Bw(2, 1);
  MDArray2d Ac(2, _flowSettings.currentParamCount()), Bc(2, 1);

  makeTrueWindMatrixExpression(nav, _flowSettings, &Aw, &Bw);
  makeTrueCurrentMatrixExpression(nav, _flowSettings, &Ac, &Bc);
  arma::vec2 windMat = asMatrix(Aw)*asMatrix(_windParams) + asMatrix(Bw);
  arma::vec2 currentMat = asMatrix(Ac)*asMatrix(_currentParams) + asMatrix(Bc);

  HorizontalMotion<double> wind{Velocity<double>::knots(windMat[0]),
    Velocity<double>::knots(windMat[1])};
  HorizontalMotion<double> current{Velocity<double>::knots(currentMat[0]),
    Velocity<double>::knots(currentMat[1])};
  CalibratedNav<double> dst;
  dst.rawAwa.set(nav.awa());
  dst.rawAws.set(nav.aws());
  dst.rawMagHdg.set(nav.magHdg());
  dst.rawWatSpeed.set(nav.watSpeed());
  dst.gpsMotion.set(nav.gpsMotion());

  // TODO: Fill in more things here.

  dst.trueWindOverGround.set(wind);
  dst.trueCurrentOverGround.set(current);
  return dst;
}

std::string LinearCorrector::toString() const {
  std::stringstream ss;
  ss << "LinearCorrector(windParams="<< _windParams << ", currentParams=" << _currentParams << ")";
  return ss.str();
}


Results calibrate(CommonCalibrationSettings commonSettings,
    FlowSettings flowSettings, Array<Nav> navs) {
    assert(std::is_sorted(navs.begin(), navs.end()));
  auto totalDuration = navs.last().time() - navs.first().time();
  auto windResults = calibrateSparse(makeTrueWindMatrices(navs, flowSettings),
      totalDuration, commonSettings);
  auto currentResults = calibrateSparse(makeTrueCurrentMatrices(navs, flowSettings),
      totalDuration, commonSettings);
  LinearCorrector corrector(flowSettings, windResults.parameters, currentResults.parameters);
  return Results{corrector, windResults.recoveredFlow, currentResults.recoveredFlow};
}



}
}
