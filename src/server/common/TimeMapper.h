/*
 * TimeMapper.h
 *
 *  Created on: 31 Oct 2016
 *      Author: jonas
 */

#ifndef SERVER_COMMON_TIMEMAPPER_H_
#define SERVER_COMMON_TIMEMAPPER_H_

#include <server/common/TimeStamp.h>

namespace sail {

struct TimeMapper {
public:
  TimeStamp offset;
  Duration<double> period;
  int sampleCount = 0;

  bool empty() const {
    return 0 == sampleCount;
  }

  TimeMapper() : sampleCount(0) {}
  TimeMapper(TimeStamp offs, Duration<double> per,
      int n) : offset(offs), period(per), sampleCount(n) {}

  int map(TimeStamp t) const {
    int index = int(round((t - offset)/period));
    return 0 <= index && index < sampleCount? index : -1;
  }

  TimeStamp unmap(int i) const {
    return offset + double(i)*period;
  }
};

}

#endif /* SERVER_COMMON_TIMEMAPPER_H_ */