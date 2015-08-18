/*
 *  Created on: 2015
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#ifndef SERVER_COMMON_OPTIONAL_H_
#define SERVER_COMMON_OPTIONAL_H_

#include <cassert>

template <typename T>
class Optional {
 public:
  Optional() : _defined(false) {}
  Optional(T x) : _defined(true), _value(x) {}

  // If a there is a public field
  // in a class of type DefinedValue,
  // calling this operator on that field
  // gives the feel of calling an accessor of the class.
  T operator()() const {
    assert(_defined); // <-- only active in debug mode.
    return _value;
  }

  T get(T defaultValue) const {
    return (_defined? _value : defaultValue);
  }

  void set(T x) {
    _defined = true;
    _value = x;
  }

  void setOnce(T x) {
    assert(!_defined);
    set(x);
  }

  template <typename S>
  Optional<S> applyFun(std::function<S(T)> fun) const {
    if (_defined) {
      return Optional<S>(fun(_value));
    }
    return Optional<S>();
  }

  template <typename S>
  Optional<S> applyFun(std::function<S(T, T)> fun, const Optional<S> &other) const {
    if (_defined && other._defined) {
      return Optional<S>(fun(_value, other._value));
    }
    return Optional<S>();
  }

  Optional<T> operator+(const Optional<T> &other) const {
    return applyFun<T>([&](T a, T b) {return a + b;}, other);
  }

  Optional<T> operator-(const Optional<T> &other) const {
    return applyFun<T>([&](T a, T b) {return a - b;}, other);
  }

  bool defined() const {return _defined;}
  bool undefined() const {return !_defined;}

  const Optional<T> &otherwise(const Optional<T> &other) const {
    if (_defined) {
      return *this;
    }
    return other;
  }
 private:
  bool _defined;
  T _value;
};


#endif /* SERVER_COMMON_OPTIONAL_H_ */
