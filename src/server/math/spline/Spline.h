/*
 * Spline.h
 *
 *  Created on: 12 Oct 2016
 *      Author: jonas
 */

#ifndef SERVER_MATH_SPLINE_H_
#define SERVER_MATH_SPLINE_H_

#include <iostream>
#include <server/math/Polynomial.h>
#include <cmath>
#include <server/common/Span.h>
#include <server/common/logging.h>
#include <Eigen/Dense>
#include <map>

/*
 * Overview:
 *
 *   - SplineBasisFunction
 *     represents a single basis function, parameterized by
 *     the numeric type T (e.g. double) and the degree. A basis
 *     function is composed by a number of polynomial pieces. The
 *     simplest basis function is just one polynomial of the constant
 *     value of 1 from -0.5 to 0.5. Higher order basis functions
 *     are obtained through convolution.
 *
 *   - RawSplineBasis represents the linear basis obtained by
 *     taking a basis function and shifting it in time to span
 *     a function space.
 *
 *   - BoundaryIndices is an internal helper class of this file.
 *
 *   - SmoothBoundarySplineBasis
 *     This class is composed of a RawSplineBasis, but reduces the
 *     degrees of freedom of the signals that can be represented by
 *     imposing smoothness constraints at the boundaries, that derivatives
 *     of order greater or equal to 2 should be 0. The SmoothBoundarySplineBasis
 *     is probably the class you will want to use when fitting a curve
 *     to time samples.
 *
 * On the theory:
 *   The SplineBasisFunction corresponds to the beta_i functions in
 *   Eq. 2 and 3 of this paper: http://bigwww.epfl.ch/publications/unser9902.pdf
 *   (Unser, Michael: Splines - A perfect fit for Signal and Image processing)
 *   Fig. 2 in that paper can be interpreted as a visualization of RawSplineBasis.
 *
 * More here:
 *   http://bigwww.epfl.ch/tutorials/unser_icip_05.pdf
 *
 *
 *
 *
 */

namespace sail {

template <typename T, int PieceCount>
struct SplineBasisFunction {
  static const int pieceCount = PieceCount;
  typedef SplineBasisFunction<T, PieceCount> ThisType;
  typedef Polynomial<T, PieceCount> Piece;

  static SplineBasisFunction make() {
    SplineBasisFunction b;
    b.initializeBasis();
    return b;
  }

  Piece getPiece(int i) const {
    return 0 <= i && i < PieceCount?
        _pieces[i] : Piece::zero();
  }

  static int supportWidth() {
    return PieceCount;
  }

  static double rightmost() {
    return 0.5*supportWidth();
  }

  static double leftmost() {
    return -rightmost();
  }

  static double polynomialBoundary(int index) {
    return leftmost() + T(index);
  }

  static int pieceIndex(T x) {
    auto diff = x - leftmost();
    return int(std::floor(diff));
  }

  T operator()(T x) const {
    return getPiece(pieceIndex(x)).eval(x);
  }

  ThisType derivative() const {
    ThisType dst;
    for (int i = 0; i < PieceCount; i++) {
      dst._pieces[i] = _pieces[i].derivativeSameOrder();
    }
    return dst;
  }

  ThisType scale(T s) const {
    ThisType dst;
    for (int i = 0; i < PieceCount; i++) {
      dst._pieces[i] = s*_pieces[i];
    }
    return dst;
  }
private:
  Piece _pieces[PieceCount];

  SplineBasisFunction() {}

  void initializeBasis() {
    if (pieceCount == 1) {
      _pieces[0] = Piece(1.0);
    } else {
      initializeFrom(SplineBasisFunction<T, cmax(1, PieceCount-1)>::make());
    }
  }

  void initializeFrom(const SplineBasisFunction<T, PieceCount-1> &x) {
    auto left = Polynomial<T, 2>{-0.5, 1.0};
    auto right = Polynomial<T, 2>{0.5, 1.0};
    for (int i = 0; i < PieceCount; i++) {
      auto split = Polynomial<T, 1>(x.polynomialBoundary(i));
      auto leftPolyPrimitive = x.getPiece(i-1).primitive();
      auto rightPolyPrimitive = x.getPiece(i).primitive();
      auto leftItg = eval(leftPolyPrimitive, split)
                  - eval(leftPolyPrimitive, left);
      auto rightItg =  eval(rightPolyPrimitive, right)
           - eval(rightPolyPrimitive, split);
      _pieces[i] = leftItg + rightItg;
    }
  }

  // To make it compile when PieceCount=1
  void initializeFrom(const ThisType &x) {}
};

template <typename T, int Degree>
class RawSplineBasis {
public:
  typedef RawSplineBasis<T, Degree> ThisType;
  typedef SplineBasisFunction<T, Degree+1> SplineType;
  static const int extraBasesPerBoundary = (Degree + 1)/2;
  static const int coefsPerPoint = 1 + 2*extraBasesPerBoundary;

  struct Weights {
    static const int dim = coefsPerPoint;
    int inds[dim];
    T weights[dim];

    Weights() {
      for (int i = 0; i < dim; i++) {
        inds[i] = 0;
        weights[i] = 0.0;
      }
    }

    bool isSet(int i) const {
      return weights[i] != T(0.0);
    }


    template <typename S>
    S evaluate(const S *data) const {
      S sum = S(0.0);
      for (int i = 0; i < dim; i++) {
        if (isSet(i)) {
          sum += weights[i]*data[inds[i]];
        }
      }
      return sum;
    }

    bool add(int index, T value) {
      if (value == T(0.0)) {
        return true;
      }
      for (int i = 0; i < coefsPerPoint; i++) {
        int &id = inds[i];
        T &w = weights[i];
        if (id == index || w == T(0.0)) {
          id = index;
          w += value;
          return true;
        }
      }
      return false;
    }

    void addScaled(T s, const Weights &other) {
      for (int i = 0; i < dim; i++) {
        CHECK(add(other.inds[i], s*other.weights[i]));
      }
    }

    Weights operator*(T s) const {
      Weights dst(*this);
      for (int i = 0; i < dim; i++) {
        dst.weights[i] *= s;
      }
      return dst;
    }

    Span<int> getSpan() const {
      Span<int> dst;
      for (int i = 0; i < dim; i++) {
        if (isSet(i)) {
          dst.extend(inds[i]);
        }
      }
      return dst;
    }

    void shiftTo(int offset) {
      for (int i = 0; i < dim; i++) {
        inds[i] -= offset;
        if (!isSet(i) || inds[i] < 0) {
          inds[i] = 0;
        }
      }
    }

    Span<int> getSpanAndOffsetAt0() {
      auto span = getSpan();
      shiftTo(span.minv());
      return Span<int>(span.minv(), span.minv() + dim);
    }
  };

  RawSplineBasis() : _intervalCount(0),
      _basisFunction(SplineType::make()) {}

  RawSplineBasis(int intervalCount) :
    _intervalCount(intervalCount),
    _basisFunction(SplineType::make()) {}

  // The interval that the data we are interpolating is spanning
  double lowerDataBound() const {return -0.5;}
  double upperDataBound() const {
    return lowerDataBound() + _intervalCount;
  }

  Span<double> dataSpan() const {
    return Span<double>(lowerDataBound(), upperDataBound());
  }

  int coefCount() const {
    return _intervalCount + 2*extraBasesPerBoundary;
  }

  double basisLocation(int basisIndex) const {
    return -extraBasesPerBoundary + basisIndex;
  }

  T evaluateBasis(const SplineType &basis, int basisIndex, T loc) const {
    return basis(loc - basisLocation(basisIndex));
  }

  T evaluateBasis(int basisIndex, T loc) const {
    return evaluateBasis(_basisFunction, basisIndex, loc);
  }

  int computeIntervalIndex(T x) const {
    return std::min(
            std::max(0, int(floor(x - lowerDataBound()))),
              _intervalCount - 1);
  }

  Weights build(const SplineType &fun, T x) const {
    int interval = computeIntervalIndex(x);
    Weights dst;
    for (int i = 0; i < coefsPerPoint; i++) {
      int index = i + interval;
      dst.inds[i] = index;
      dst.weights[i] = evaluateBasis(fun, index, x);
    }
    return dst;
  }

  Weights build(T x) const {
    return build(_basisFunction, x);
  }

  T evaluate(const T *coefficients, T x) const {
    int offset = computeIntervalIndex(x);
    T sum(0.0);
    for (int i = 0; i < coefsPerPoint; i++) {
      int index = offset + i;
      sum += coefficients[index]*evaluateBasis(index, x);
    }
    return sum;
  }

  int intervalCount() const {return _intervalCount;}

  const SplineType &basisFunction() const {
    return _basisFunction;
  }

  Spani leftmostCoefs() const {
    return Spani(0, coefsPerPoint);
  }

  Spani rightmostCoefs() const {
    int n = coefCount();
    return Spani(n-coefsPerPoint, n);
  }

  ThisType derivative() const {
    return ThisType(_intervalCount, _basisFunction.derivative());
  }

  ThisType scale(T s) const {
    return ThisType(_intervalCount, _basisFunction.scale(s));
  }
private:
  RawSplineBasis(int i, const SplineType &st) : _intervalCount(i),
    _basisFunction(st) {}

  int _intervalCount;
  SplineType _basisFunction;
};


class BoundaryIndices {
public:
  BoundaryIndices(Spani left, Spani right, int ep);
  int varDim() const {return 2*_ep;}
  int totalDim() const;
  int rightDim() const;
  int overlap() const;

  bool isLeftIndex(int i) const;
  bool isRightIndex(int i) const;
  bool isInnerIndex(int i) const;

  int computeACol(int i) const;
  int computeBCol(int i) const;

  void add(int k, int *inds, double *weights);

  struct Solution {
    Eigen::MatrixXd left, right;
  };
  Solution solve() const;
private:
  Eigen::MatrixXd _A, _B;
  int epRight() const;
  int innerLimit() const;
  Spani _left, _right;
  int _ep;
  int _counter;
};


template <typename T, int Degree>
class SmoothBoundarySplineBasis {
public:
  typedef RawSplineBasis<T, Degree> RawType;
  typedef typename RawType::Weights Weights;
  typedef SmoothBoundarySplineBasis<T, Degree> ThisType;

  SmoothBoundarySplineBasis() : _basis(0) {}

  SmoothBoundarySplineBasis(int intervalCount) :
    _basis(intervalCount) {
    int upperDerivativeBound = Degree+1;
    int lowerDerivativeBound = upperDerivativeBound
        - RawType::extraBasesPerBoundary;
    if (intervalCount == 1) {
      lowerDerivativeBound = 1;
    }

    BoundaryIndices inds(_basis.leftmostCoefs(),
        _basis.rightmostCoefs(),
        RawType::extraBasesPerBoundary);

    CHECK(0 < lowerDerivativeBound);
    if (0 < RawType::extraBasesPerBoundary) {
      auto lb = _basis.lowerDataBound();
      auto ub = _basis.upperDataBound();
      auto der = _basis.basisFunction();
      for (int i = 1; i < upperDerivativeBound; i++) {
        der = der.derivative();
        if (lowerDerivativeBound <= i) {
          auto left = _basis.build(der, lb);
          auto right = _basis.build(der, ub);
          inds.add(Weights::dim, left.inds, left.weights);
          inds.add(Weights::dim, right.inds, right.weights);
        }
      }
      auto sol = inds.solve();
      _left = sol.left;
      _right = sol.right;
    }
  }

  int coefCount() const {
    return _basis.intervalCount();
  }

  const Eigen::MatrixXd &leftMat() const {return _left;}
  const Eigen::MatrixXd &rightMat() const {return _right;}

  int leftBd() const {
    return RawType::extraBasesPerBoundary;
  }

  int rightBd() const {
    return RawType::extraBasesPerBoundary + _basis.intervalCount();
  }

  int butLastOffset() const {
    return _basis.intervalCount() - _right.cols();
  }

  T getInternalCoef(int index, const T *coefs) const {
    int offset2 = rightBd();
    int index1 = index - offset2;
    if (index < leftBd()) {
      T sum(0.0);
      for (int i = 0; i < _left.cols(); i++) {
        sum += _left(index, i)*coefs[i];
      }
      return sum;
    } else if (0 <= index1) {
      auto lastCoefs = coefs + butLastOffset();
      T sum(0.0);
      for (int i = 0; i < _right.cols(); i++) {
        sum += _right(index1, i)*lastCoefs[i];
      }
      return sum;
    }
    return coefs[index - RawType::extraBasesPerBoundary];
  }


  int internalCoefCount() const {return _basis.coefCount();}

  T evaluate(const T *coefficients, T x) const {
    int offset = _basis.computeIntervalIndex(x);
    T sum(0.0);
    for (int i = 0; i < RawType::coefsPerPoint; i++) {
      int index = offset + i;
      auto c = getInternalCoef(index, coefficients);
      auto b = _basis.evaluateBasis(index, x);
      sum += c*b;
    }
    return sum;
  }

  Weights getInnerWeights(int innerCoef) const {
    Weights dst;
    int rb = rightBd();
    if (innerCoef < leftBd()) {
      for (int i = 0; i < _left.cols(); i++) {
        dst.add(i, _left(innerCoef, i));
      }
    } else if (rb <= innerCoef) {
      int index = innerCoef - rb;
      CHECK(index < _right.rows());
      int k = butLastOffset();
      for (int i = 0; i < _right.cols(); i++) {
        dst.add(i + k, _right(index, i));
      }
    } else {
      dst.add(innerCoef - RawType::extraBasesPerBoundary, 1.0);
    }
    return dst;
  }

  Weights build(T x) const {
    Weights dst;
    int offset = _basis.computeIntervalIndex(x);
    for (int i_ = 0; i_ < RawType::coefsPerPoint; i_++) {
      int index = i_ + offset;
      auto w = _basis.evaluateBasis(index, x);
      auto wsub = getInnerWeights(index);
      dst.addScaled(w, wsub);
    }
    return dst;
  }

  const RawSplineBasis<T, Degree> &raw() const {
    return _basis;
  }

  ThisType derivative() const {
    return ThisType(_left, _right, _basis.derivative());
  }

  ThisType scale(T s) const {
    return ThisType(_left, _right, _basis.scale(s));
  }

  Array<ThisType> derivatives() const {
    const int n = Degree+1;
    Array<ThisType> dst(n);
    dst[0] = *this;
    for (int i = 1; i < n; i++) {
      dst[i] = dst[i-1].derivative();
    }
    return dst;
  }
private:
  SmoothBoundarySplineBasis(const Eigen::MatrixXd &l, const Eigen::MatrixXd &r,
      const RawType &rt) : _left(l), _right(r), _basis(rt) {}

  Eigen::MatrixXd _left, _right;
  RawSplineBasis<T, Degree> _basis;
};

} /* namespace sail */

#endif /* SERVER_MATH_SPLINE_H_ */
