/*
 * StepMinimizer.h
 *
 *  Created on: 21 janv. 2014
 *      Author: jonas
 *
 *
 * Minimizes a function
 * 	f: R -> R
 * by trying discrete steps and gradually reducing the step size.
 */

#ifndef STEPMINIMIZER_H_
#define STEPMINIMIZER_H_

#include <functional>

namespace sail {

class StepMinimizerState {
 public:
  StepMinimizerState(double x, double step, double value) : _x(x), _step(step), _value(value) {}

  double getX() const {
    return _x;
  }
  double getStep() const {
    return _step;
  }
  double getValue() const {
    return _value;
  }

  void reduceStep() {
    _step *= 0.5;
  }
  void increaseStep() {
    _step *= 2.0;
  }

  double getLeft() const {
    return _x - _step;
  }
  double getRight() const {
    return _x + _step;
  }

  StepMinimizerState make(double x, double value) const {
    return StepMinimizerState(x, _step, value);
  }
 private:
  double _x, _step, _value;
};

class StepMinimizer {
 public:
  StepMinimizer();

  StepMinimizerState takeStep(StepMinimizerState state, std::function<double(double)> fun);
  StepMinimizerState minimize(StepMinimizerState state, std::function<double(double)> fun);

  // The acceptor function lets us incorporate additional criteria in order for a solution to be accepted.
  void setAcceptor(std::function<bool(double, double)> acceptor);
 private:
  double _leftLimit, _rightLimit;
  int _maxIter;

  std::function<bool(double, double)> _acceptor;
};





}

#endif /* STEPMINIMIZER_H_ */
