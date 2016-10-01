/*
 * BoatStateReconstructor.cpp
 *
 *  Created on: 29 Sep 2016
 *      Author: jonas
 */

#include <server/nautical/calib/BoatStateReconstructor.h>
#include <server/nautical/calib/Fitness.h>
#include <ceres/ceres.h>
#include <random>
#include <server/common/string.h>
#include <server/nautical/calib/DebugPlot.h>

namespace sail {

template <typename T>
bool finiteResiduals(int n, const T *x) {
  for (int i = 0; i < n; i++) {
    if (!isFinite(x[i])) {
      return false;
    }
  }
  return true;
}

template <typename T>
void foreachSpan(const TimeStampToIndexMapper &mapper,
    const Array<TimedValue<T> > &values,
    std::function<void(int, Spani)> cb) {
  assert(std::is_sorted(values.begin(), values.end()));
  int currentPosition = 0;
  while (currentPosition < values.size()) {
    int currentIndex = mapper.map(values[currentPosition].time);
    int nextPosition = currentPosition + 1;
    while (nextPosition < values.size()) {
      int nextIndex = mapper.map(values[nextPosition].time);
      if (nextIndex != currentIndex) {
        break;
      }
      nextPosition++;
    }
    if (currentIndex != -1) {
      cb(currentIndex, Spani(currentPosition, nextPosition));
    }
    currentPosition = nextPosition;
  }
}

// Forced instantiation for unit tests.
template void foreachSpan<Velocity<double>>(
    const TimeStampToIndexMapper &mapper,
    const Array<TimedValue<Velocity<double>> > &values,
    std::function<void(int, Spani)> cb);

template <typename T>
ValueAccumulator<T> makeValueAccumulator(
    const BoatParameterLayout::IndexAndOffsetMap &sensorIndices,
  const TimeStampToIndexMapper &mapper,
  const std::map<std::string, Array<TimedValue<T> > > &srcData) {
  typedef ValueAccumulator<T> DstType;
  DstType dst;
  int sampleCounter = 0;
  for (auto kv: srcData) {
    foreachSpan<T>(mapper, kv.second, [&](int sampleIndex, Spani span) {
      sampleCounter += span.width();
    });
  }
  dst.values.reserve(sampleCounter);

  for (auto kv: srcData) {
    auto f = sensorIndices.find(kv.first);
    assert(f != sensorIndices.end());
    int sensorIndex = f->second.sensorIndex;
    foreachSpan<T>(mapper, kv.second, [&](int sampleIndex, Spani span) {
      for (auto i: span) {
        auto tagged = typename DstType::TaggedValue{
          sensorIndex, sampleIndex, kv.second[i].value};
        dst.values.push_back(tagged);
      }
    });
  }
  assert(dst.values.size() == sampleCounter);
  std::sort(dst.values.begin(), dst.values.end());
  for (int i = 0; i < dst.values.size(); i++) {
    int index = dst.values[i].sampleIndex;
    auto f = dst.valuesPerIndex.find(index);
    if (f == dst.valuesPerIndex.end()) {
      dst.valuesPerIndex.insert(std::pair<int, Spani>(index, Spani(i, i+1)));
    } else {
      f->second.extend(i);
      f->second.extend(i+1);
    }
  }
  return dst;
}

// Forced instantiations for unit tests.
template ValueAccumulator<Angle<double>> makeValueAccumulator<Angle<double>>(
    const BoatParameterLayout::IndexAndOffsetMap &layout, const TimeStampToIndexMapper &mapper,
  const std::map<std::string, Array<TimedValue<Angle<double>> > > &srcData);
template ValueAccumulator<Velocity<double>> makeValueAccumulator<Velocity<double>>(
    const BoatParameterLayout::IndexAndOffsetMap &layout, const TimeStampToIndexMapper &mapper,
  const std::map<std::string, Array<TimedValue<Velocity<double>> > > &srcData);

template <typename T>
void evaluateMotionReg(
    const HorizontalMotion<T> &dif,
    T weight, Velocity<T> bw,
    T *dst) {
  dst[0] = weight*(dif[0]/bw);
  dst[1] = weight*(dif[1]/bw);
}

template <typename Settings>
struct RegCost {
  double *globalScale;
  double regWeight;

  RegCost(double *gs, double w) : globalScale(gs), regWeight(w) {}

  static const int outputCount = 10;

  template <typename T>
  bool operator()(const T *X, const T *Y, T *dst) const {
    ReconstructedBoatState<T, Settings> A, B;
    A.readFrom(X);
    B.readFrom(Y);
    T weight = MakeConstant<T>::apply(regWeight);

    auto vbw = BandWidthForType<T, Velocity<double>>::get();
    auto abw = BandWidthForType<T, Angle<double>>::get();

    // TODO: Now we are regularizing for everything,
    // possibly even state variables that we don't attempt
    // to reconstruct. That is not wrong, but it is maybe
    // a waste of time.
    evaluateMotionReg<T>(
        A.boatOverGround.value - B.boatOverGround.value,
        weight, vbw, dst + 0);
    evaluateMotionReg<T>(
        A.windOverGround.value - B.windOverGround.value,
        weight, vbw, dst + 2);
    evaluateMotionReg<T>(
        A.currentOverGround.value - B.currentOverGround.value,
        weight, vbw, dst + 4);
    dst[6] = weight*((A.heel.value - B.heel.value).normalizedAt0()/abw);
    dst[7] = weight*((A.pitch.value - B.pitch.value).normalizedAt0()/abw);
    evaluateMotionReg<T>(A.heading.value - B.heading.value,
        weight, vbw, dst + 8);
    static_assert(10 == outputCount, "Bad dim");
    assert(finiteResiduals<T>(outputCount, dst));
    return true;
  }
};

template <typename Settings>
struct DataCost;

template <typename Settings>
Array<BoatState<double> > makeBoatStates(
    const Array<BoatState<double>> &initialStates,
    Array<double> parameters) {
  typedef ReconstructedBoatState<double, Settings> State;
  int n = initialStates.size();
  Array<BoatState<double>> dst(n);
  for (int i = 0; i < n; i++) {
    auto init = initialStates[i];
    auto state = State::make(init);
    state.readFrom(parameters.blockPtr(i,
        State::dynamicValueDimension));
    dst[i] = state.makeBoatState(init);
  }
  return dst;
}

struct ChunkAccumulator {
  TimeStampToIndexMapper mapper;
  Array<BoatState<double>> initialStates;

#define DECLARE_VALUE_ACC(HANDLE) \
  ValueAccumulator<typename TypeForCode<HANDLE>::type> HANDLE;
  FOREACH_MEASURE_TO_CONSIDER(DECLARE_VALUE_ACC)
#undef DECLARE_VALUE_ACC

  ChunkAccumulator() {}
  ChunkAccumulator(
      const BoatParameterLayout &layout,
      const CalibDataChunk &chunk);

  bool hasData(int i) const {
#define TEST_FOR_INDEX(HANDLE) \
  if (!HANDLE.getValueRange(i).empty()) {return true;}
  FOREACH_MEASURE_TO_CONSIDER(TEST_FOR_INDEX)
#undef TEST_FOR_INDEX
    return false;
  }
};

ChunkAccumulator::ChunkAccumulator(
    const BoatParameterLayout &layout,
    const CalibDataChunk &chunk) :
        mapper(chunk.timeMapper),
        initialStates(chunk.initialStates) {
#define INIT_ACC(HANDLE) \
  HANDLE = makeValueAccumulator(layout.HANDLE.sensors, chunk.timeMapper, chunk.HANDLE);
  FOREACH_MEASURE_TO_CONSIDER(INIT_ACC)
#undef DECLARE_VALUE_ACC
}


namespace {

  void outputSensorOffsetData(DataCode code,
      BoatParameterLayout::PerType data,
      HtmlNode::Ptr dst0) {
    if (!data.sensors.empty()) {
      auto row0 = HtmlTag::make(dst0, "tr");
      HtmlTag::tagWithData(row0, "td", wordIdentifierForCode(code));
      auto dst = HtmlTag::make(row0, "td");

      {
        auto overview = HtmlTag::make(dst, "table");
        {
          auto row = HtmlTag::make(overview, "tr");
          HtmlTag::tagWithData(row, "td", "offset");
          HtmlTag::tagWithData(row, "td",
              stringFormat("%d", data.offset));
        }{
          auto row = HtmlTag::make(overview, "tr");
          HtmlTag::tagWithData(row, "td", "param count");
          HtmlTag::tagWithData(row, "td",
              stringFormat("%d", data.paramCount));
        }
      }{
        auto perSensor = HtmlTag::make(dst, "table");
        {
          auto row = HtmlTag::make(perSensor, "tr");
          HtmlTag::tagWithData(row, "th", "Source");
          HtmlTag::tagWithData(row, "th", "Offset");
          HtmlTag::tagWithData(row, "th", "Index");
        }
        for (auto kv: data.sensors) {
          auto row = HtmlTag::make(perSensor, "tr");
          HtmlTag::tagWithData(row, "td", kv.first);
          HtmlTag::tagWithData(row, "td",
              stringFormat("%d", kv.second.sensorOffset));
          HtmlTag::tagWithData(row, "td",
              stringFormat("%d", kv.second.sensorIndex));
        }
      }
    }
  }

  void outputParamLayout(const BoatParameterLayout &src,
      HtmlNode::Ptr dst) {
    {
      auto table = HtmlTag::make(dst, "table");
      {
        auto row = HtmlTag::make(table, "tr");
        HtmlTag::tagWithData(row, "th", "Type");
        HtmlTag::tagWithData(row, "th", "Data");
      }{
        auto row = HtmlTag::make(table, "tr");
        HtmlTag::tagWithData(row, "td", "Heel offset");
        HtmlTag::tagWithData(row, "td",
            stringFormat("%d", src.heelConstantOffset));
      }{
        auto row = HtmlTag::make(table, "tr");
        HtmlTag::tagWithData(row, "td", "Leeway offset");
        HtmlTag::tagWithData(row, "td",
            stringFormat("%d", src.leewayConstantOffset));
      }
  #define OUTPUT_SENSOR_OFFSET(HANDLE, INDEX, SHORTNAME, TYPE, DESCRIPTION) \
      outputSensorOffsetData(DataCode::HANDLE, src.HANDLE, dst);
  FOREACH_CHANNEL(OUTPUT_SENSOR_OFFSET)
  #undef OUTPUT_SENSOR_OFFSET
    }
    HtmlTag::tagWithData(dst, "p",
        stringFormat("Total parameter count: %d", src.paramCount));
  }
}

void addParameterBlock(ceres::Problem *dst,
    Array<double> X) {
  dst->AddParameterBlock(X.ptr(), X.size());
}

void outputParameterCountOverview(
    const Array<double> &calibrationParameters,
    const Array<Array<double>> &stateVariables,
    HtmlNode::Ptr dst) {
  auto table = HtmlTag::make(dst, "table");
  int total = 0;
  {
    auto row = HtmlTag::make(table, "tr");
    HtmlTag::tagWithData(row, "td", "Calibration parameters");
    int n = calibrationParameters.size();
    HtmlTag::tagWithData(row, "td",
        stringFormat("%d", n));
    total += n;
  }
  for (int i = 0; i < stateVariables.size(); i++) {
    auto row = HtmlTag::make(table, "tr");
    HtmlTag::tagWithData(row, "td",
        stringFormat("State variables chunk %d", i+1));
    int n = stateVariables[i].size();
    HtmlTag::tagWithData(row, "td",
        stringFormat("%d", n));
    total += n;
  }{
    auto row = HtmlTag::make(table, "tr");
    HtmlTag::tagWithData(row, "td", "TOTAL");
    HtmlTag::tagWithData(row, "td", stringFormat("%d", total));
  }
}


void makeCostTableHeaders(HtmlNode::Ptr table) {
  if (table) {
    auto row = HtmlTag::make(table, "tr");
    HtmlTag::tagWithData(row, "th", "Chunk index");
    HtmlTag::tagWithData(row, "th", "State count");
    HtmlTag::tagWithData(row, "th", "Regularization cost count");
    HtmlTag::tagWithData(row, "th", "Data cost count");
  }
}

template <typename T>
void attribAndValue(const std::string &attrib,
    const T &value, HtmlNode::Ptr dst) {
  auto row = HtmlTag::make(dst, "tr");
  HtmlTag::tagWithData(row, "td", attrib);
  {
    auto e = HtmlTag::make(row, "td");
    e->stream() << value;
  }
}

void outputSummary(
    const ceres::Solver::Summary &summary,
    HtmlNode::Ptr dst) {
  if (dst) {
    HtmlTag::tagWithData(dst, "h2", "Ceres solver summary");
    {
      auto table = HtmlTag::make(dst, "table");
      {
        auto row = HtmlTag::make(table, "tr");
        HtmlTag::tagWithData(row, "th", "Attribute");
        HtmlTag::tagWithData(row, "th", "Value");
      }
      attribAndValue("initial cost", summary.initial_cost, table);
      attribAndValue("final cost", summary.final_cost, table);
    }
    auto reportPage = HtmlTag::initializePage(
        HtmlTag::linkToSubPage(dst, "Full report"),
        "Full report");
    {
      auto pre = HtmlTag::make(reportPage, "pre");
      pre->stream() << summary.FullReport();
    }

  }
}

namespace {
  void dispSetting(const char *label,
      bool value, std::set<bool> warnOn, HtmlNode::Ptr dst) {
    auto row = HtmlTag::make(dst, "tr");
    HtmlTag::tagWithData(row, "td", label);
    HtmlTag::tagWithData(row, "td",
        {{"class", warnOn.count(value) == 1? "warning" : "none"}},
        value? "TRUE" : "FALSE");

  }


  template <typename Settings>
  void dispSettings(HtmlNode::Ptr dst) {
    HtmlTag::tagWithData(dst, "h2", "Static optimizer settings");
    auto table = HtmlTag::make(dst, "table");
    {
      auto header = HtmlTag::make(table, "tr");
      HtmlTag::tagWithData(header, "th", "Attribute");
      HtmlTag::tagWithData(header, "th", "Value");
    }
#define DISP_SETTING(what, warnOnValue) \
  dispSetting(#what, Settings::what, warnOnValue, table);
    DISP_SETTING(withBoatOverGround, {true});
    DISP_SETTING(withWindOverGround, {false});
    DISP_SETTING(withCurrentOverGround, {false});
    DISP_SETTING(withHeel, {});
    DISP_SETTING(withPitch, {});
#undef DISP_SETTING
    }
}

template <typename BoatStateSettings>
class BoatStateReconstructor {
public:
  static_assert(AreFitnessSettings<BoatStateSettings>::value,
      "The parameter you passed are not valid settings");

  template <typename T>
  using State = ReconstructedBoatState<T, BoatStateSettings>;

  typedef State<double> Stated;

  static const int dynamicStateDim = Stated::dynamicValueDimension;

  BoatStateReconstructor(
      const Array<CalibDataChunk> &chunks,
      const BoatParameters<double> &initialParameters,
      const ReconstructionSettings &settings,
      const HtmlNode::Ptr &logNode) :
        _log(logNode),
        _initialParameters(initialParameters),
        _layout(initialParameters) {
    dispSettings<BoatStateSettings>(logNode);
    int n = chunks.size();
    _chunks = Array<ChunkAccumulator>(n);
    for (int i = 0; i < n; i++) {
      const auto &chunk = chunks[i];
      _chunks[i] = ChunkAccumulator(
          _layout, chunk);
    }

    if (logNode) {
      HtmlTag::tagWithData(logNode, "h2", "Parameter layout");
      outputParamLayout(_layout, logNode);
    }
  }

  ReconstructionResults reconstruct() const {
    double globalScale = 1.0;

    Array<double> calibrationParameters
      = initializeCalibrationParameters();

    ceres::Problem problem;
    addParameterBlock(&problem, calibrationParameters);

    Array<Array<double>> stateParameters = initializeStateParameters();
    assert(stateParameters.size() == _chunks.size());

    if (_log) {
      HtmlTag::tagWithData(_log, "h2", "Parameter blocks to optimize");
      outputParameterCountOverview(
          calibrationParameters, stateParameters, _log);
    }

    // Build the costs.
    HtmlTag::tagWithData(_log, "h2", "Costs");
    {
      auto table = HtmlTag::make(_log, "table");
      makeCostTableHeaders(table);
      for (int i = 0; i < _chunks.size(); i++) {
        auto row = HtmlTag::make(table, "tr");
        HtmlTag::tagWithData(row, "td", stringFormat("%d", i));
        addCostsForChunk(
            &globalScale,
            calibrationParameters,
            stateParameters[i],
            _chunks[i], &problem, row);
      }
    }

    ceres::Solver::Options options;
    options.minimizer_progress_to_stdout = true;
    options.logging_type = ceres::LoggingType::PER_MINIMIZER_ITERATION;
    options.num_threads = 4;
    ceres::Solver::Summary summary;

    //ceres::Solve(options, &problem, &summary);

    outputSummary(summary, _log);

    ReconstructionResults results;
    results.parameters = _initialParameters;
    results.parameters.readFrom(calibrationParameters.ptr());
    results.chunks = makeAllBoatStates(stateParameters);
    return results;
  }

  Array<ReconstructedChunk> makeAllBoatStates(
      const Array<Array<double>> &parameters) const {
    int n = parameters.size();
    assert(n == _chunks.size());
    Array<ReconstructedChunk> dst(n);
    for (int i = 0; i < n; i++) {
      dst[i] = ReconstructedChunk{
        _chunks[i].mapper,
        makeBoatStates<BoatStateSettings>(
                  _chunks[i].initialStates, parameters[i])
      };
    }
    return dst;
  }

  Array<double> initializeCalibrationParameters() const {
    Array<double> dst(_initialParameters.paramCount());
    _initialParameters.writeTo(dst.ptr());
    return dst;
  }

  Array<Array<double>> initializeStateParameters() const {
    int n = _chunks.size();
    Array<Array<double>> dst(n);
    for (int i = 0; i < n; i++) {
      dst[i] = initializeStateParameters(_chunks[i]);
    }
    return dst;
  }

  void addCostsForChunk(
      double *globalScale,
      Array<double> calibrationParameters,
      Array<double> stateParameters,
      const ChunkAccumulator &chunk, ceres::Problem *dst,
      HtmlNode::Ptr tableRow) const {
    int stateCount = chunk.initialStates.size();
    HtmlTag::tagWithData(tableRow,
        "td", stringFormat("%d", stateCount));
    int regCount = chunk.initialStates.size()-1;
    HtmlTag::tagWithData(tableRow,
        "td", stringFormat("%d", regCount));
    for (int i = 0; i < regCount; i++) {
      typedef RegCost<BoatStateSettings> CostFunctor;
      auto cost = new CostFunctor(globalScale, _settings.regWeight);
      auto wrapped = new ceres::AutoDiffCostFunction<CostFunctor,
          CostFunctor::outputCount, dynamicStateDim,
          dynamicStateDim>(cost);
      dst->AddResidualBlock(wrapped, nullptr,
          stateParameters.blockPtr(i+0, dynamicStateDim),
          stateParameters.blockPtr(i+1, dynamicStateDim));
    }
    int dataCount = 0;
    for (int i = 0; i < stateCount; i++) {
      if (chunk.hasData(i)) {
        typedef DataCost<BoatStateSettings> CostFunctor;
        auto cost = new CostFunctor{
        #define GET_VALUE_RANGE(HANDLE) \
          chunk.HANDLE.getValueRange(i),
        FOREACH_MEASURE_TO_CONSIDER(GET_VALUE_RANGE)
        #undef GET_VALUE_RANGE
              chunk,
              chunk.initialStates[i],
              *this
             };
        auto wrapped = new
            ceres::DynamicAutoDiffCostFunction<CostFunctor>(
                cost);
        wrapped->SetNumResiduals(cost->outputCount());
        wrapped->AddParameterBlock(calibrationParameters.size());
        wrapped->AddParameterBlock(dynamicStateDim);
        dst->AddResidualBlock(wrapped, nullptr, std::vector<double*>{
          calibrationParameters.ptr(),
          stateParameters.blockPtr(i, dynamicStateDim)
        });
        dataCount++;
      }
    }
    HtmlTag::tagWithData(tableRow,
        "td", stringFormat("%d", dataCount));

  }

  Array<double> initializeStateParameters(
      const ChunkAccumulator &acc) const {
    Array<double> dst(acc.initialStates.size()*dynamicStateDim);
    // Having perfectly random numbers is not an issue in this case.
    std::default_random_engine rng;
    std::uniform_real_distribution<double> distrib(0.0, 2.0*M_PI);
    for (int i = 0; i < acc.initialStates.size(); i++) {
      auto tmp = State<double>::make(acc.initialStates[i]);
      tmp.heading = HorizontalMotion<double>::polar(
          1.0e-6_kn, distrib(rng)*1.0_rad);
      tmp.writeTo(dst.blockPtr(i, dynamicStateDim));
    }
    return dst;
  }

  HtmlNode::Ptr _log;
  BoatParameters<double> _initialParameters;
  BoatParameterLayout _layout;
  Array<ChunkAccumulator> _chunks;
  ReconstructionSettings _settings;
};


template <typename T, DataCode code>
class IndexedSensors {
public:
  static const int maxSensors = 8;

  int count = 0;

  IndexedSensors(const BoatParameterLayout &layout, const T *parameters) {
    count = ChannelFieldAccess<code>::apply(layout).count;
    for (int i = 0; i < count; i++) {
      _sensors[i] = layout.getModel(i, parameters);
    }
  }

  const DistortionModel<T, code> &operator[](int i) const {
    assert(0 <= i);
    assert(i < count);
    return _sensors[i];
  }
private:
  DistortionModel<T, code> _sensors[maxSensors];
};

template <typename Settings>
struct DataCost {
#define MAKE_RANGE(HANDLE) \
  Spani HANDLE;
FOREACH_MEASURE_TO_CONSIDER(MAKE_RANGE)
#undef GET_VALUE_RANGE
  const ChunkAccumulator &chunk;
  const BoatState<double> &initState;
  const BoatStateReconstructor<Settings> &rec;

  template <typename T>
  bool operator()(T const* const* parameters,
      T *residuals) const {
    auto calibParams = parameters[0];
    auto stateParams = parameters[1];

    auto state = ReconstructedBoatState<T, Settings>::make(initState);
    state.readFrom(stateParams);

    int offset = 0;
#define EVAL_RESIDUALS(HANDLE) \
  if (!computeResidualsForType<T, DataCode::HANDLE>(state, calibParams, residuals, &offset)) {assert(false); return false;}
FOREACH_MEASURE_TO_CONSIDER(EVAL_RESIDUALS)
#undef EVAL_RESIDUALS

    if (!computeHeelResiduals<T>(state, calibParams, residuals, &offset)) {
      assert(false);
      return false;
    }
    if (!computeLeewayResiduals<T>(state, calibParams, residuals, &offset)) {
      assert(false);
      return false;
    }
    assert(offset == outputCount());
    assert(finiteResiduals<T>(offset, residuals));
    return true;
  }

  template <typename T>
  bool computeHeelResiduals(
      const ReconstructedBoatState<T, Settings> &state,
      const T *calibParams, T *residuals, int *offset) const {
    if (!HeelFitness<T, Settings>::apply(state,
        calibParams[rec._layout.heelConstantOffset]
                    *BoatParameters<T>::heelConstantUnit(),
        residuals + *offset)) {
      assert(false);
      return false;
    }
    *offset += HeelFitness<T, Settings>::outputCount;
    return true;
  }

  template <typename T>
  bool computeLeewayResiduals(
      const ReconstructedBoatState<T, Settings> &state,
      const T *calibParams, T *residuals, int *offset) const {
    if (!LeewayFitness<T, Settings>::apply(state,
        calibParams[rec._layout.leewayConstantOffset]
                    *BoatParameters<T>::leewayConstantUnit(),
                    residuals + *offset)) {
      return false;
    }
    *offset += LeewayFitness<T, Settings>::outputCount;
    return true;
  }

  template <typename T, DataCode code>
  bool computeResidualsForType(
      const ReconstructedBoatState<T, Settings> &state,
      const T *calibParams, T *residuals, int *offset) const {
    for (auto i: *ChannelFieldAccess<code>::get(*this)) {
      const auto &v = ChannelFieldAccess<code>::get(chunk)->values[i];
      typedef Fitness<T, code, Settings> F;
      if (!F::apply(state, rec._layout.template getModel<T, code>(
          v.sensorIndex, calibParams),
        v.value, residuals + *offset)) {
        assert(false);
        return false;
      }
      *offset += F::outputCount;
    }
    return true;
  }

  int outputCount() const {
    return AWA.width()*AWAFitness<double, Settings>::outputCount +
        AWS.width()*AWSFitness<double, Settings>::outputCount +
        MAG_HEADING.width()*MagHeadingFitness<double, Settings>::outputCount +
        WAT_SPEED.width()*WatSpeedFitness<double, Settings>::outputCount +
        ORIENT.width()*OrientFitness<double, Settings>::outputCount
        + HeelFitness<double, Settings>::outputCount
        + LeewayFitness<double, Settings>::outputCount;
  }
};

namespace {

  template <FunctionCode fcode, DataCode code>
  void initializeFields(const CalibDataChunk &src,
      SensorFunctionSet<fcode, double> *dst) {
    typedef typename
        SensorFunctionType<fcode, double, code>::type FType;
    const auto &srcMap =
        *ChannelFieldAccess<code>::template get<CalibDataChunk>(src);
    auto &dstMap =
        *ChannelFieldAccess<code>
          ::template get<SensorFunctionSet<fcode, double> >(*dst);
    for (const auto &kv: srcMap) {
      dstMap[kv.first] = FType();
    }
  }

  template <FunctionCode code>
  void initializeSensorSetFromChunk(
      const CalibDataChunk &chunk,
      SensorFunctionSet<code, double> *dst) {
    initializeFields<code, AWA>(chunk, dst);
    initializeFields<code, AWS>(chunk, dst);
    initializeFields<code, MAG_HEADING>(chunk, dst);
    initializeFields<code, WAT_SPEED>(chunk, dst);
  }

  template <FunctionCode code>
  SensorFunctionSet<code, double> initializeSensorSet(
      const Array<CalibDataChunk> &chunks) {
    SensorFunctionSet<code, double> dst;
    for (auto chunk: chunks) {
      initializeSensorSetFromChunk<code>(chunk, &dst);
    }
    return dst;
  }
}

namespace {
  struct CalibChunkSampleVisitor {
    std::map<DataCode, int> counts;
    template <DataCode code, typename T, typename Data>
    void visit(const Data &d) {
      int n = 0;
      for (auto kv: d) {
        n += kv.second.size();
      }
      counts[code] = n;
    }
  };

  void outputChunkOverview(
      const Array<CalibDataChunk> &chunks,
      HtmlNode::Ptr log) {
    std::map<std::pair<int, DataCode>, int> countMap;
    for (int i = 0; i < chunks.size(); i++) {
      CalibChunkSampleVisitor visitor;
      visitFieldsConst(
          chunks[i], &visitor);
      for (auto kv: visitor.counts) {
        countMap.insert({{i, kv.first}, kv.second});
      }
    }

    auto codes = getAllDataCodes();
    auto h1 = SubTable::header(1, 1,
        SubTable::constant("Index"));
    auto chunkIndices = SubTable::header(chunks.size(), 1,
        [](HtmlNode::Ptr dst, int i, int j) {dst->stream() << i+1;});
    auto leftCol = vcat(h1, chunkIndices);
    auto channelHeaders = SubTable::header(1, codes.size(),
        [&](HtmlNode::Ptr dst, int i, int j) {
      dst->stream() << wordIdentifierForCode(codes[j]);
    });
    auto sampleCounts = SubTable::cell(chunks.size(), codes.size(),
        [&](HtmlNode::Ptr dst, int i, int j) {
      auto f = countMap.find({i, codes[j]});
      if (f != countMap.end()) {
        dst->stream() << f->second;
      }
    });
    auto miscHeaders = SubTable::header(1, 3,
        [&](HtmlNode::Ptr dst,
        int i, int j) {
      const char *h[3] = {"Reconstruction sample count",
          "Sampling period", "Duration"};
      dst->stream() << h[j];
    });
    auto misc = SubTable::cell(chunks.size(), 3,
        [&](HtmlNode::Ptr dst, int i, int j) {
      auto x = chunks[i].timeMapper;
      switch (j) {
      case 0:
        dst->stream() << x.sampleCount;
        break;
      case 1:
        dst->stream() << x.period.seconds() << " s";
        break;
      case 2:
        dst->stream() << (double(x.sampleCount-1)*x.period).str();
        break;
      default:
        break;
      };
    });

    auto all = hcat(
        hcat(leftCol, vcat(channelHeaders, sampleCounts)),
        vcat(miscHeaders, misc));
    renderTable(log, all);
  }

  struct InitialStateCharacteristics {
    bool allMotionsDefined = true;
    bool allPositionsDefined = true;
    Velocity<double> maxGpsSpeed = 0.0_kn;
  };

  InitialStateCharacteristics analyzeInitialStates(
      const Array<BoatState<double>> &src) {
    InitialStateCharacteristics c;
    for (auto x: src) {
      auto goodMotion = isFinite(x.boatOverGround());
      c.allMotionsDefined &= goodMotion;
      c.allPositionsDefined &= isFinite(x.position());
      if (goodMotion) {
        c.maxGpsSpeed = std::max(
            c.maxGpsSpeed, x.boatOverGround().norm());
      }
    }
    return c;
  }

  void outputInitialStateOverview(
      const Array<CalibDataChunk> &chunks,
      HtmlNode::Ptr logNode) {
    std::vector<InitialStateCharacteristics> ch;
    for (auto chunk: chunks) {
      ch.push_back(analyzeInitialStates(chunk.initialStates));
    }
    const char *headerStrings[4] = {
        "Index", "Motions defined", "Positions defined", "Max gps speed"
    };
    auto h = SubTable::header(1, 4,
        [&](HtmlNode::Ptr dst, int i, int j) {
      dst->stream() << headerStrings[j];
    });
    auto inds = SubTable::header(chunks.size(), 1,
        [&](HtmlNode::Ptr dst, int i, int j) {
      dst->stream() << i+1;
    });
    auto yesOrNo = [](HtmlNode::Ptr dst, bool v) {
      if (v) {
        HtmlTag::tagWithData(dst, "p", {{"class", "success"}}, "Yes");
      } else {
        HtmlTag::tagWithData(dst, "p", {{"class", "warning"}}, "No");
      }
    };
    auto motionsDefined = SubTable::cell(chunks.size(), 1,
            [&](HtmlNode::Ptr dst, int i, int j) {
          yesOrNo(dst, ch[i].allMotionsDefined);
        });
    auto positionsDefined = SubTable::cell(chunks.size(), 1,
            [&](HtmlNode::Ptr dst, int i, int j) {
          yesOrNo(dst, ch[i].allPositionsDefined);
        });
    auto speeds = SubTable::cell(chunks.size(), 1,
        [&](HtmlNode::Ptr dst, int i, int j) {
      auto vel = ch[i].maxGpsSpeed.knots();
      dst->stream() << vel << " knots";
    });
    auto data = hcat(motionsDefined, hcat(positionsDefined, speeds));
    renderTable(logNode, vcat(h, hcat(inds, data)));
  }

  template <DataCode code>
  void registerSensors(const CalibDataChunk &chunk,
      BoatParameters<double> *dst) {
    const auto &src0 = ChannelFieldAccess<code>::get(chunk);
    for (const auto &kv: *src0) {
      auto &dst0 = *(ChannelFieldAccess<code>::get(dst->sensors));
      dst0[kv.first] = DistortionModel<double, code>();
    }
  }

  template <DataCode code>
  void outputChannelTableRow(
      const std::map<std::string, DistortionModel<double, code>> &map,
      HtmlNode::Ptr dst) {
    int n = map.size();
    if (0 < n) {
      auto row = HtmlTag::make(dst, "tr");
      HtmlTag::tagWithData(row, "td", wordIdentifierForCode(code));
      auto sensorData = HtmlTag::make(row, "td");
      {
        HtmlTag::tagWithData(sensorData, "p",
                stringFormat("%d sensors", n));
        auto sensorTable = HtmlTag::make(sensorData, "table");
        for (auto kv: map) {
          auto row = HtmlTag::make(sensorTable, "tr");
          HtmlTag::tagWithData(row, "td", kv.first);
          auto p = HtmlTag::make(row, "td");
          kv.second.outputSummary(&(p->stream()));
        }
      }
    }
  }

  void outputBoatParameters(
      const BoatParameters<double> &params,
      HtmlNode::Ptr dst) {
    auto table = HtmlTag::make(dst, "table");
    {
      auto row = HtmlTag::make(table, "tr");
      HtmlTag::tagWithData(row, "th", "Type");
      HtmlTag::tagWithData(row, "th", "Data");
    }{
      auto row = HtmlTag::make(table, "tr");
      HtmlTag::tagWithData(row, "td", "Heel constant");
      HtmlTag::tagWithData(row, "td",
          stringFormat("%.3g",
              params.heelConstant/params.heelConstantUnit()));
    }{
      auto row = HtmlTag::make(table, "tr");
      HtmlTag::tagWithData(row, "td", "Leeway constant");
      HtmlTag::tagWithData(row, "td",
          stringFormat("%.3g",
              params.leewayConstant/params.leewayConstantUnit()));
    }
#define OUTPUT_CHANNEL_TABLE_ROW(HANDLE, CODE, SHORTNAME, TYPE, DESCRIPTION) \
  outputChannelTableRow<HANDLE>(*(ChannelFieldAccess<HANDLE>::get(params.sensors)), table);
FOREACH_CHANNEL(OUTPUT_CHANNEL_TABLE_ROW)
#undef OUTPUT_CHANNEL_TABLE_ROW
  }
}

BoatParameters<double> initializeBoatParameters(
    const Array<CalibDataChunk> &chunks) {
  BoatParameters<double> dst;
  for (auto chunk: chunks) {
#define REGISTER_SENSORS(CODE) \
    registerSensors<CODE>(chunk, &dst);
FOREACH_MEASURE_TO_CONSIDER(REGISTER_SENSORS)
#undef REGISTER_SENSORS
  }
  return dst;
}

namespace {
  template <DataCode code>
  int layoutSensors(int offset,
      const std::map<std::string, DistortionModel<double, code>> &params,
      BoatParameterLayout::PerType *dst) {
    int offset0 = offset;
    dst->offset = offset;
    int counter = 0;
    for (auto kv: params) {
      dst->sensors[kv.first] =
          BoatParameterLayout::IndexAndOffset{counter, offset};
      offset += DistortionModel<double, code>::paramCount;
      counter++;
    }
    dst->paramCount = offset - offset0;
    assert(counter == params.size());
    return offset;
  }
}

BoatParameterLayout::BoatParameterLayout(
    const BoatParameters<double> &parameters) {
  int offset = 2; // after leeway and heel.
#define LAYOUT_SENSORS(HANDLE, INDEX, SHORTNAME, TYPE, DESCRIPTION) \
  offset = layoutSensors<DataCode::HANDLE>(offset, parameters.sensors.HANDLE, &HANDLE);
FOREACH_CHANNEL(LAYOUT_SENSORS)
#undef LAYOUT_SENSORS
  paramCount = offset;
  assert(paramCount == parameters.paramCount());
}

template <typename T, DataCode code>
struct MakeReprojectionPlot {

  static void apply(TimeStampToIndexMapper mapper,
      const ReconstructedChunk &chunk,
      const Array<TimedValue<T>> &values,
      HtmlNode::Ptr dst) {
    HtmlTag::tagWithData(dst, "p",
        std::string("Nothing to render for ")
      + wordIdentifierForCode(code));
  }
};

template <DataCode code, typename T = typename TypeForCode<code>::type>
Array<TimedValue<T>>
  makeTimedValuesFromRecChunk(
      const ReconstructedChunk &chunk) {
  int n = chunk.mapper.sampleCount;
  CHECK(chunk.states.size() == n);
  Array<TimedValue<T>> dst(n);
  for (int i = 0; i < n; i++) {
    dst[i] = TimedValue<T>(
        chunk.mapper.unmap(i),
        BoatStateValue<double, code>::get(chunk.states[i]));
  }
  return dst;
}

template <>
struct MakeReprojectionPlot<Velocity<double>, AWS> {
  static void apply(TimeStampToIndexMapper mapper,
        const ReconstructedChunk &chunk,
        const Array<TimedValue<Velocity<double>>> &values,
        HtmlNode::Ptr dst) {
    std::cout << "  number of raw values to plot: "
        << values.size() << std::endl;
    TemporalSignalPlot<Velocity<double>> plot;
    plot.add(StrokeType::Dot, values);
    plot.add(StrokeType::Dot, values);
    /*if (chunk.states.empty()) {
      HtmlTag::tagWithData(dst,
          "p", "No reprojected data to show (missing)");
    } else {
      auto projected = makeTimedValuesFromRecChunk<AWS>(chunk);
      std::cout << "  number of projected values: "
          << projected.size() << std::endl;
      plot.add(StrokeType::Line,
          projected);
    }*/
    plot.renderTo(dst);
  }
};

template <DataCode HANDLE,
  typename T = typename TypeForCode<HANDLE>::type>
void makeChunkPlotRow(
    const TimeStampToIndexMapper &mapper,
    const ReconstructedChunk &chunk,
    const std::map<std::string, Array<TimedValue<T>>> &values,
    HtmlNode::Ptr dst) {
  auto row = HtmlTag::make(dst, "tr");
  HtmlTag::tagWithData(row, "td",
      wordIdentifierForCode(HANDLE));
  {
    auto ul = HtmlTag::make(HtmlTag::make(row, "td"), "ul");
    for (auto kv: values) {
      std::cout << "Making chunk plot for " << kv.first << std::endl;
      auto li = HtmlTag::make(ul, "li");
      auto subPage = HtmlTag::linkToSubPage(li, kv.first, true);
      MakeReprojectionPlot<T, HANDLE>::apply(
          mapper, chunk, kv.second, subPage);
    }
  }
}

void makeReprojectionPlotForChunk(
    const ReconstructedChunk &reconstructed,
    const CalibDataChunk &chunk,
    HtmlNode::Ptr dst) {
  auto table = HtmlTag::make(dst, "table");
  {
    auto h = HtmlTag::make(table, "tr");
    HtmlTag::tagWithData(h, "th", "Type");
    HtmlTag::tagWithData(h, "th", "Plots");
  }
#define MAKE_PLOT_ROW(HANDLE) \
  makeChunkPlotRow<HANDLE>(chunk.timeMapper, reconstructed, chunk.HANDLE, table);
  FOREACH_MEASURE_TO_CONSIDER(MAKE_PLOT_ROW)
#undef MAKE_PLOT_ROW
}

void makePlotsPerChunk(
    const ReconstructionResults &results,
    const Array<CalibDataChunk> &chunks,
    HtmlNode::Ptr dst) {
  assert(results.chunks.size() == chunks.size()
      || results.chunks.empty());
  int n = chunks.size();
  for (int i = 0; i < n; i++) {
    HtmlTag::tagWithData(dst, "h3", stringFormat("Chunk %d",
        i+1));
    makeReprojectionPlotForChunk(
        results.chunks.empty()?
            ReconstructedChunk()
            : results.chunks[i],
        chunks[i], dst);
  }
}

ReconstructionResults reconstruct(
    const Array<CalibDataChunk> &chunks,
    const ReconstructionSettings &settings,
    HtmlNode::Ptr logNode) {
  HtmlTag::tagWithData(logNode, "h1",
      stringFormat("Reconstruction results for %d chunks",
          chunks.size()));
  if (logNode) {
    HtmlTag::tagWithData(logNode, "h2", "Input data");
    HtmlTag::tagWithData(logNode, "h3", "Chunks");
    outputChunkOverview(chunks, logNode);
    HtmlTag::tagWithData(logNode, "h3", "Initial states");
    outputInitialStateOverview(chunks, logNode);
  }

  auto initialParameters = initializeBoatParameters(chunks);
  BoatParameterLayout layout(initialParameters);
  if (logNode) {
    HtmlTag::tagWithData(logNode, "h2", "Initial parameters");
    outputBoatParameters(initialParameters, logNode);
  }

  BoatStateReconstructor<DefaultSettings> reconstructor(
      chunks, initialParameters, settings, logNode);

  //ReconstructionResults results;
  auto results = reconstructor.reconstruct();
  assert(results.parameters.paramCount()
      == initialParameters.paramCount());

  if (logNode) {
    HtmlTag::tagWithData(logNode, "h2", "Reprojection plots");
    makePlotsPerChunk(results, chunks, logNode);
  }

  if (logNode) {
    HtmlTag::tagWithData(logNode, "h2", "Reconstruction results");
    outputBoatParameters(results.parameters, logNode);
  }

  return results;
}





}


