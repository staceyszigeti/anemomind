/*
 * StateReconstructor.h
 *
 *  Created on: 22 Sep 2016
 *      Author: jonas
 */

#ifndef SERVER_NAUTICAL_CALIB_BOATSTATERECONSTRUCTOR_H_
#define SERVER_NAUTICAL_CALIB_BOATSTATERECONSTRUCTOR_H_

#include <unordered_map>
#include <map>
#include <server/common/Span.h>
#include <server/common/TimeStamp.h>
#include <server/nautical/BoatState.h>
#include <server/common/TimedValue.h>
#include <server/nautical/calib/BoatParameters.h>
#include <server/common/Array.h>
#include <server/common/HtmlLog.h>

namespace sail {

#define FOREACH_MEASURE_TO_CONSIDER(OP) \
  OP(AWA) \
  OP(AWS) \
  OP(MAG_HEADING) \
  OP(WAT_SPEED) \
  OP(ORIENT)

struct TimeStampToIndexMapper {
public:
  TimeStamp offset;
  Duration<double> period;
  int sampleCount = 0;

  bool empty() const {
    return 0 == sampleCount;
  }

  TimeStampToIndexMapper() : sampleCount(0) {}
  TimeStampToIndexMapper(TimeStamp offs, Duration<double> per,
      int n) : offset(offs), period(per), sampleCount(n) {}

  int map(TimeStamp t) const {
    int index = int(round((t - offset)/period));
    return 0 <= index && index < sampleCount? index : -1;
  }

  TimeStamp unmap(int i) const {
    return offset + double(i)*period;
  }
};

// A CalibDataChunk are measurements that are grouped together
// They are dense without any gaps.
struct CalibDataChunk {
  Array<BoatState<double>> initialStates;
  TimeStampToIndexMapper timeMapper;

#define MAKE_DATA_MAP(HANDLE, CODE, SHORTNAME, TYPE, DESCRIPTION) \
  std::map<std::string, Array<TimedValue<TYPE>>> HANDLE;
FOREACH_CHANNEL(MAKE_DATA_MAP)
#undef MAKE_DATA_MAP
};


template <typename T>
void foreachSpan(const TimeStampToIndexMapper &mapper,
    const Array<TimedValue<T> > &values,
    std::function<void(int, Spani)> cb);

// A supposedly memory- and cache efficient data structure
// used by the optimizer
template <typename T>
struct ValueAccumulator {
  struct TaggedValue {
    int sensorIndex;
    int sampleIndex;
    T value;

    bool operator<(const TaggedValue &other) const {
      return sampleIndex < other.sampleIndex;
    }
  };

  std::unordered_map<int, Spani> valuesPerIndex;
  std::vector<TaggedValue> values;

  Spani getValueRange(int i) const {
    auto f = valuesPerIndex.find(i);
    return f == valuesPerIndex.end()? Spani(0, 0) : f->second;
  }
};

struct ReconstructedChunk {
  TimeStampToIndexMapper mapper;
  Array<BoatState<double>> states;
};

struct ReconstructionResults {
  BoatParameters<double> parameters;
  Array<ReconstructedChunk> chunks;
};

class ChannelRef {
  DataCode code;
  std::string sourceName;
};


struct ReconstructionSettings {
  LeewayConstant<double> leewayReg = 0.1_kn*0.1_kn;
  double regWeight = 1.0;
  int windowSize = 60;
  Duration<double> samplingPeriod = 1.0_s;
};

BoatParameters<double> initializeBoatParameters(
    const Array<CalibDataChunk> &chunks);

// This object specifies how the different parameters can be found
// in the vector that we are optimizing. So if we have pointer
// at the beginning of that vector, the different offsets of
// this objects are w.r.t. that pointer.
struct BoatParameterLayout {
  const int heelConstantOffset = 0;
  const int leewayConstantOffset = 1;

  struct IndexAndOffset {
    int sensorIndex = 0;
    int sensorOffset = 0;
    IndexAndOffset() {}
    IndexAndOffset(int i, int o) : sensorIndex(i), sensorOffset(o) {}
  };
  typedef std::map<std::string, IndexAndOffset> IndexAndOffsetMap;

  struct PerType {
    int offset = 0;
    int paramCount = 0;
    IndexAndOffsetMap sensors;
  };

#define DECLARE_PER_TYPE(HANDLE, INDEX, SHORTNAME, TYPE, DESCRIPTION) \
  PerType HANDLE;
FOREACH_CHANNEL(DECLARE_PER_TYPE)
#undef DECLARE_PER_TYPE

  template <typename T, DataCode code>
  DistortionModel<T, code> getModel(int index, const T *params) const {
    const auto &e = *ChannelFieldAccess<code>::get(*this);
    DistortionModel<T, code> dst;
    dst.readFrom(
        params + e.offset +
        DistortionModel<T, code>::paramCount*index);
    return dst;
  }

  int paramCount = 0;

  BoatParameterLayout(const BoatParameters<double> &parameters);
};

template <typename T>
ValueAccumulator<T> makeValueAccumulator(
  const BoatParameterLayout::IndexAndOffsetMap &layout, const TimeStampToIndexMapper &mapper,
  const std::map<std::string, Array<TimedValue<T> > > &srcData,
  HtmlNode::Ptr log);

ReconstructionResults reconstruct(
    const Array<CalibDataChunk> &chunks,
    const ReconstructionSettings &settings,
    HtmlNode::Ptr logNode = HtmlNode::Ptr());


}
#endif /* SERVER_NAUTICAL_CALIB_BOATSTATERECONSTRUCTOR_H_ */
