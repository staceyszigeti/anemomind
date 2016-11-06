/*
 * DiscreteOutlierFilter.cpp
 *
 *  Created on: 4 Nov 2016
 *      Author: jonas
 */

#include <server/common/DiscreteOutlierFilter.h>
#include <server/common/ArrayBuilder.h>
#include <server/common/Span.h>
#include <iostream>
#include <server/common/ArrayIO.h>

namespace sail {
namespace DiscreteOutlierFilter {

Duration<double> addDurs(
    const Duration<double> &a,
    const Duration<double> &b) {
  return a + b;
}

Array<Duration<double>> getDurs(const Array<Span<TimeStamp>> &spans) {
  int n = spans.size();
  int last = n-1;
  Array<Duration<double>> durs(n);
  for (int i = 0; i < last; i++) {
    durs[i] = spans[i+1].minv() - spans[i].minv();
  }
  durs[last] = spans[last].maxv() - spans[last].minv();
  return durs;
}

TimeSpanIndexer::TimeSpanIndexer(
    const Array<Span<TimeStamp>> &timeSpans) :
        _tree(addDurs, getDurs(timeSpans)) {
  if (!timeSpans.empty()) {
    _offset = timeSpans.first().minv();
  }
}


int TimeSpanIndexer::lookUp(TimeStamp t) const {
  if (!_offset.defined()) {
    return -1;
  }

  return _tree.findLeafIndex(t - _offset);
}


std::ostream &operator<<(std::ostream &s,
    const BackPointer &bptr) {
  s << "BackPointer(prev=" << bptr.previous << ", cost=" << bptr.cost << ")";
  return s;
}

Array<Span<int>> makeSpans(const Array<int> &pts) {
  int n = pts.size() - 1;
  Array<Span<int>> dst(n);
  for (int i = 0; i < n; i++) {
    dst[i] = Span<int>(pts[i], pts[i+1]);
  }
  return dst;
}

double evalTransitionCost(
    std::function<double(int, int)> f,
    Span<int> a, Span<int> b) {
  int i = a.maxv()-1;
  int j = b.minv();
  assert(i < j);
  return f(i, j);
}

Array<bool> buildSampleMask(const Array<bool> &segmentMask,
    const Array<Span<int>> &spans) {
  assert(0 < segmentMask.size());
  assert(segmentMask.size() == spans.size());
  int n = spans.last().maxv();
  auto dst = Array<bool>::fill(n, false);
  for (int i = 0; i < segmentMask.size(); i++) {
    auto seg = spans[i];
    if (segmentMask[i]) {
      for (auto j: seg) {
        dst[j] = true;
      }
    }
  }
  return dst;
}

Array<Array<BackPointer>> buildBackPointers(
    const Array<Span<int>> &spans,
        std::function<double(int, int)> costFun,
        Array<int> backSteps,
        double threshold) {
  int n = spans.size();
  auto data = Array<BackPointer>(n*backSteps.size());
  Array<Array<BackPointer>> dst(n);
  for (int i = 1; i < n+1; i++) {
    auto sub = data.sliceBlock(i-1, backSteps.size());
    dst[i-1] = sub;
    for (int j = 0; j < backSteps.size(); j++) {
      BackPointer ptr;
      ptr.previous = std::max(-1, i - backSteps[j]);
      int stepSize = std::max(0, i - ptr.previous - 1);
      ptr.cost = stepSize*threshold;
      if (0 <= ptr.previous && i < n) {
        ptr.cost += evalTransitionCost(
            costFun, spans[ptr.previous], spans[i]);
      }
      sub[j] = ptr;
    }
  }

  return dst;
}

Array<bool> unwindBackPointers(const Array<BackPointer> &path) {
  int n = path.size()-1;
  auto segmentMask = Array<bool>::fill(n, false);
  int i = n;
  while (true) {
    i = path[i].previous;
    if (0 <= i) {
      segmentMask[i] = true;
    } else {
      break;
    }
  }
  return segmentMask;
}

Array<bool> solveBackPointers(
    const Array<Array<BackPointer>> &pointers) {
  int n = pointers.size();
  Array<BackPointer> path(n+1);
  for (int i_ = 0; i_ < n; i_++) {
    int i = i_+1;
    auto ptrs = pointers[i_];
    BackPointer best;
    best.cost = std::numeric_limits<double>::max();
    for (int j = 0; j < ptrs.size(); j++) {
      BackPointer candidate;
      candidate = ptrs[j];
      if (0 <= candidate.previous) {
        candidate.cost += path[candidate.previous].cost;
      }
      best = std::min(best, candidate);
    }
    path[i] = best;
  }
  return unwindBackPointers(path);
}


Array<bool> solveMask(const Array<Span<int>> spans,
    const std::function<double(int,int)> costFun,
    const Settings &settings) {
  auto pointers = buildBackPointers(
      spans, costFun, settings.backSteps,
      settings.threshold);
  auto mask = solveBackPointers(pointers);
  return buildSampleMask(mask, spans);
}

Array<int> computeSplits(int n,
    std::function<bool(int, int)> cut) {
  int pairCount = n-1;
  if (pairCount <= 0) {
    return Array<int>{0, n};
  }
  ArrayBuilder<int> splits(pairCount+2);
  splits.add(0);
  ArrayBuilder<double> splitCosts(pairCount);
  for (int i = 0; i < pairCount; i++) {
    if (cut(i, i+1)) {
      int index = i + 1;
      splits.add(index);
      splitCosts.add(index);
    }
  }
  splits.add(n);
  return splits.get();
}

Array<Span<int>> segment(int n,
    std::function<bool(int, int)> cut) {
  return makeSpans(computeSplits(n, cut));
}

Array<bool> computeOutlierMaskFromPairwiseCosts(
    int n, std::function<double(int, int)> cost,
    std::function<bool(int, int)> cut,
    const Settings &settings) {
  auto spans = segment(n, [=](int i, int j) {
    return cut(i, j) || settings.threshold < cost(i, j);
  });
  return solveMask(spans, cost, settings);
}

}
}
