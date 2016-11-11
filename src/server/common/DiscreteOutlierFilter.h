/*
 * DiscreteOutlierFilter.h
 *
 *  Created on: 4 Nov 2016
 *      Author: jonas
 */

#ifndef SERVER_COMMON_DISCRETEOUTLIERFILTER_H_
#define SERVER_COMMON_DISCRETEOUTLIERFILTER_H_

#include <server/common/ArrayBuilder.h>
#include <server/common/TimedValue.h>
#include <server/common/ReduceTree.h>
#include <server/common/TimeStamp.h>
#include <server/common/Span.h>

namespace sail {
namespace DiscreteOutlierFilter {


class TimeSpanIndexer {
public:
  TimeSpanIndexer(const Array<Span<TimeStamp>> &timeSpans);
  int lookUp(TimeStamp t) const;
  TimeStamp getTime(int i) const;
private:
  Array<Span<TimeStamp>> _spans;
  TimeStamp _offset;
  ReduceTree<Duration<double>> _tree;
};

struct BackPointer {
  int previous = -1;
  double cost = 0.0;

  bool operator<(const BackPointer &other) const {
    return cost < other.cost;
  }
};

template <typename T>
Array<bool> identifyOutliers(
    const Array<TimedValue<T>> &data,
    std::function<double(const TimedValue<T>&,
        const TimedValue<T> &)> cost,
    Array<Duration<double>> backSteps,
    double threshold);



struct Settings {
  double threshold = 1.0e6;
  Array<int> backSteps = Array<int>{1, 2, 4, 8, 16, 32, 64, 128};
};

Array<bool> computeOutlierMaskFromPairwiseCosts(
    int n,
    std::function<double(int, int)> cost,
    std::function<bool(int, int)> cut,
    const Settings &settings);


// Note: don't forget to make sure the cost is non-negative,
// which it should be in most cases.
template <typename Iterator, typename T = typename Iterator::type>
Array<bool> computeOutlierMask(Iterator begin, Iterator end,
    std::function<double(T, T)> cost,
    std::function<bool(T, T)> cut,
    const Settings &settings) {
  return computeOutlierMaskFromPairwiseCosts(
      std::distance(begin, end),
      [=](int i, int j) {
         return cost(*(begin + i), *(begin + j));
      }, [=](int i, int j) {
        return cut(*(begin + i), *(begin + j));
      }, settings);
}

template <typename Iterator, typename T = typename Iterator::type>
Array<bool> computeOutlierMask(Iterator begin, Iterator end,
    std::function<double(T, T)> cost,
    const Settings &settings) {
  return computeOutlierMask<Iterator, T>(begin, end, cost,
      [](T x, T y) {return false;}, settings);
}


}
}

#endif /* SERVER_COMMON_DISCRETEOUTLIERFILTER_H_ */