/*
 *  Created on: 2014-03-28
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 *
 *  Json interface for Physical quantities.
 *  Provides readField() and writeField() for most Quantity<Type> combinations.
 */

#ifndef PHYSICALQUANTITYJSON_H_
#define PHYSICALQUANTITYJSON_H_

#include <server/common/Array.h>
#include <Poco/JSON/Object.h>
#include <server/common/PhysicalQuantity.h>
#include <server/common/JsonPrimitive.h>
#include <server/common/JsonFwd.h> // To avoid circular dependency between templates
                                   // in case of a type Array<Vectorize<...> >

namespace sail {
namespace json {

namespace {

template <class Quantity, typename Value>
struct JsonQuantityTraits {
  static double serialize(const Quantity& x) {return std::numeric_limits<double>::signaling_NaN();}
  static Quantity deserialize(double v) { return Quantity(); }
  static const char* suffix() { return  ""; };
  static const char* quantityName() {return "";}
};

template <typename Value>
struct JsonQuantityTraits<Duration<Value>, Value> {
    static double serialize(const Duration<Value>& duration) { return duration.seconds(); }
    static Duration<Value> deserialize(double v) { return Duration<Value>::seconds(v); }
    static const char* suffix() { return  "_s"; };
    static const char* quantityName() {return "Duration";}
};

template <typename Value>
struct JsonQuantityTraits<Velocity<Value>, Value> {
    static double serialize(const Velocity<Value>& a) { return a.metersPerSecond(); }
    static Velocity<Value> deserialize(double v) { return Velocity<Value>::metersPerSecond(v); }
    static const char* suffix() { return "_mps"; };
    static const char* quantityName() {return "Velocity";}
};

template <typename Value>
struct JsonQuantityTraits<Angle<Value>, Value> {
    static double serialize(const Angle<Value>& a) { return a.radians(); }
    static Angle<Value> deserialize(double v) { return Angle<Value>::radians(v); }
    static const char* suffix() { return "_rad"; };
    static const char* quantityName() {return "Angle";}
};

template <typename Value>
struct JsonQuantityTraits<Length<Value>, Value> {
    static double serialize(const Length<Value>& a) { return a.meters(); }
    static Length<Value> deserialize(double v) { return Length<Value>::meters(v); }
    static const char* suffix() { return "_m"; };
    static const char* quantityName() {return "Length";}
};

}  // namespace

template<class Quantity>
bool deserializeField(Poco::Dynamic::Var cobj, std::string fieldPrefix,
               Quantity *out) {
  Poco::JSON::Object::Ptr obj = cobj.extract<Poco::JSON::Object::Ptr>();
  typedef struct JsonQuantityTraits<Quantity, typename Quantity::ValueType> TypeInfo;

    std::string fname = fieldPrefix + TypeInfo::suffix();
    bool is = obj->has(fname);
    if (is) {
        *out = TypeInfo::deserialize(obj->getValue<double>(fname));
        return true;
    }
    *out = Quantity();
    return false;
}

template<class Quantity>
void serializeField(Poco::JSON::Object::Ptr obj, std::string fieldPrefix,
                const Quantity &x) {
  double val =
      JsonQuantityTraits<Quantity, typename Quantity::ValueType>::serialize(x);
  if (!std::isnan(val)) {
    obj->set(
        fieldPrefix +
        JsonQuantityTraits<Quantity, typename Quantity::ValueType>::suffix(),
        val);
  }
}

template <typename Quantity, typename Value>
Poco::Dynamic::Var serialize(const PhysicalQuantity<Quantity, Value> &x) {
  typedef JsonQuantityTraits<Quantity, Value> TypeInfo;
  Poco::JSON::Object::Ptr obj(new Poco::JSON::Object());
  obj->set(std::string(TypeInfo::quantityName()) + TypeInfo::suffix(), x);
  return Poco::Dynamic::Var(obj);
}

template <typename Quantity, typename Value>
bool deserialize(Poco::JSON::Object::Ptr src, PhysicalQuantity<Quantity, Value> *x) {
  return deserializeField(src, std::string(Quantity::quantityName()), x);
}

template <typename Quantity, typename Value>
bool deserialize(Poco::Dynamic::Var src, PhysicalQuantity<Quantity, Value> *x) {
  typedef JsonQuantityTraits<Quantity, Value> TypeInfo;
  try {
    return deserializeField(src.extract<Poco::JSON::Object::Ptr>(), std::string(TypeInfo::quantityName()), x);
  } catch (Poco::Exception &e) {
    return false;
  }
}

template <typename T, int N>
Poco::Dynamic::Var serializeVectorize(const Vectorize<T, N> &x) {
  return serialize(Array<T>(N, x.data()));
}

template <typename T, int N>
Poco::Dynamic::Var serialize(const Vectorize<T, N> &x) {
  return serializeVectorize<T, N>(x);
}


template <typename T, int N>
bool deserialize(Poco::Dynamic::Var src, Vectorize<T, N> *x) {
  Array<T> arr;
  if (!deserialize(src, &arr)) {
    return false;
  }
  if (arr.size() != N) {
    return false;
  }
  *x = Vectorize<T, N>(arr.ptr());
  return true;
}


}
} /* namespace sail */

#endif /* PHYSICALQUANTITYJSON_H_ */
