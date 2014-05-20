/*
 *  Created on: 2014-05-20
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include "Histogram.h"
#include  <assert.h>

namespace sail {

HistogramMap::HistogramMap(int count, double leftBd, double rightBd) {
  init(count, leftBd, rightBd);
}


HistogramMap::HistogramMap(int count, Arrayd values) {
  double minv = values[0];
  double maxv = minv;
  int n = values.size();
  for (int i = 1; i < n; i++) {
    double x = values[i];
    minv = std::min(minv, x);
    maxv = std::max(maxv, x);
  }
  const double marg = 1.0e-9;
  init(count, minv - marg, maxv + marg);
}


int HistogramMap::toBin(double value) const {
  int index = int(_index2left.inv(value));
  if (validIndex(index)) {
    return index;
  } else {
    return -1;
  }
}

double HistogramMap::toLeftBound(int binIndex) const {
  assert(validIndex(binIndex));
  return _index2left(binIndex);
}

double HistogramMap::toRightBound(int binIndex) const {
  assert(validIndex(binIndex));
  return _index2left(binIndex + 1.0);
}

double HistogramMap::toCenter(int binIndex) const {
  assert(validIndex(binIndex));
  return _index2left(binIndex + 0.5);
}
Arrayi HistogramMap::countPerBin(Arrayd values) const {
  Arrayi hist(_binCount);
  hist.setTo(0);
  for (auto value: values) {
    int index = toBin(value);
    if (validIndex(index)) {
      hist[index]++;
    }
  }
  return hist;
}

Arrayi HistogramMap::assignBins(Arrayd values) const {
  return values.map<int>([&](double x) {return toBin(x);});
}


void HistogramMap::init(int count, double leftBd, double rightBd) {
  _binCount = count;
  _index2left = LineKM(0, count, leftBd, rightBd);
}

namespace {
  void drawBin(double left, double right, double height, MDArray2d dst) {
    assert(dst.rows() == 3);
    assert(dst.cols() == 2);

    dst(0, 0) = left;
    dst(0, 1) = 0;

    dst(1, 0) = left;
    dst(1, 1) = height;

    dst(2, 0) = right;
    dst(2, 1) = height;
  }
}

MDArray2d HistogramMap::makePlotData(Arrayi counts) const {
  assert(_binCount == counts.size());
  MDArray2d plotData(3*_binCount + 1, 2);
  for (int i = 0; i < _binCount; i++) {
    double left = toLeftBound(i);
    double right = toRightBound(i);
    drawBin(left, right, counts[i], plotData.sliceRowBlock(i, 3));
  }
  int lastRow = plotData.rows()-1;
  plotData(lastRow, 0) = toRightBound(_binCount-1);
  plotData(lastRow, 1) = 0;
  return plotData;
}


} /* namespace sail */
