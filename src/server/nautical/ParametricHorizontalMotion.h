/*
 *  Created on: 2014-07-03
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 *
 *  An abstract class that models a vector field to look-up for instance wind or
 *  current, based on some paramters.
 *
 *  It is intended to be used in an optimization context, and therefore
 *  uses the adouble type.
 */

#ifndef HORIZONTALMOTIONPARAM_H_
#define HORIZONTALMOTIONPARAM_H_

#include <device/Arduino/libraries/PhysicalQuantity/PhysicalQuantity.h>
#include <adolc/adouble.h>

namespace sail {

class Nav;
class ParametricHorizontalMotion {
 public:
  // Returns the number of parameters used to
  // parameterize the vector field
  virtual int paramCount() const = 0;

  // Initializes the parameters used to parameterize the vector field
  virtual void initialize(double *outParams) const = 0;

  // Initializes the parameters to reasonable random values.
  virtual void initializeRandom(double *outParams) const = 0;

  // Returns the local Horizontal motion at a Nav
  virtual HorizontalMotion<adouble> get(const Nav &nav, adouble *params) const = 0;
  virtual ~ParametricHorizontalMotion() {}
};

class ConstantHorizontalMotion : public ParametricHorizontalMotion {
 public:
  int paramCount() const {return 2;}

  void initialize(double *outParams) const {
    outParams[0] = 0;
    outParams[1] = 0;
  }

  void initializeRandom(double *outParams) const;


  HorizontalMotion<adouble> get(const Nav &nav, adouble *params) const {
    return HorizontalMotion<adouble>(Velocity<adouble>::metersPerSecond(params[0]),
                                     Velocity<adouble>::metersPerSecond(params[1]));
  }
 private:
};

class ZeroHorizontalMotion : public ParametricHorizontalMotion {
 public:
  int paramCount() const {return 0;}

  void initialize(double *outParams) const {}

  void initializeRandom(double *outParams) const {}

  HorizontalMotion<adouble> get(const Nav &nav, adouble *params) const {
    return HorizontalMotion<adouble>(Velocity<adouble>::metersPerSecond(0),
                                     Velocity<adouble>::metersPerSecond(0));
  }
 private:
};

} /* namespace mmm */

#endif /* HORIZONTALMOTIONPARAM_H_ */
