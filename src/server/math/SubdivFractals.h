/*
 *  Created on: 2014-
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */
#ifndef SUBDIVFRACTALS_H_
#define SUBDIVFRACTALS_H_

#include <server/common/MDArray.h>
#include <cmath>
#include <iostream>
#include <server/common/string.h>

namespace sail {


// Helper object in the computation of the
// curve
class MaxSlope {
 public:
  MaxSlope() : _signalRange(NAN), _maxSlope(NAN) {}

  // Signal range:
  //  For instance if the signal models wind conditions in a spatial dimension,
  //  then signalRange controls approximately the bounds of the signal, e.g. 25 m/s which
  //  is storm.
  // maxSlope:
  //  how much the signal will maximally change along the dimension. An indicative
  //  value might be that if you move 30 meters, the wind strength increases from 0 m/s to 8 m/s.
  //  Then set maxSlope = 8/30, or something like that.
  MaxSlope(double signalRange, double maxSlope) :
    _signalRange(signalRange), _maxSlope(maxSlope) {
    assert(signalRange > 0);
    assert(maxSlope > 0);
  }

  double eval(double width) const {
    assert(width > 0);
    if (_maxSlope*width > _signalRange) {
      return _signalRange/width;
    }
    return _maxSlope;
  }

  double fitValue(double y0, double y1, double alpha,
      double beta, double w) const {
      assert(-1 <= alpha && alpha <= 1);
      assert(-1 <= beta && beta <= 1);
      assert(0 < w);
      double m = eval(w);
      return 0.5*((y0 + alpha*m*w) + (y1 - beta*m*w));
  }

 private:
  double _signalRange, _maxSlope;
};



class Vertex {
 public:
  Vertex() : _classIndex(-1), _value(NAN) {}
  Vertex(double value, int classIndex) :
    _value(value), _classIndex(classIndex) {}

  double value() const {
    return _value;
  }

  int classIndex() const {
    return _classIndex;
  }
 private:
  double _value;
  int _classIndex;
};

class Rule {
 public:
  Rule(MaxSlope slope, double alpha, double beta, int newClass) :
    _slope(slope), _alpha(alpha), _beta(beta), _newClass(newClass) {
  }

  Rule() : _alpha(NAN), _beta(NAN), _newClass(-1) {}

  Vertex combine(const Vertex &a, const Vertex &b, double w) const {
    //T value = 0.5*(a.value() + b.value()) + _irreg*ref;
    std::cout << EXPR_AND_VAL_AS_STRING(w) << std::endl;
    std::cout << EXPR_AND_VAL_AS_STRING(0.5*w) << std::endl;
    std::cout << EXPR_AND_VAL_AS_STRING(a.value()) << std::endl;
    std::cout << EXPR_AND_VAL_AS_STRING(b.value()) << std::endl;
    std::cout << EXPR_AND_VAL_AS_STRING(_alpha) << std::endl;
    std::cout << EXPR_AND_VAL_AS_STRING(_beta) << std::endl;
    double value = _slope.fitValue(a.value(), b.value(), _alpha, _beta, 0.5*w);
    return Vertex(value, _newClass);
  }

  bool defined() const {
    return _newClass != -1;
  }

  double alpha() const {
    return _alpha;
  }

  double beta() const {
    return _beta;
  }

  int newClass() const {
    return _newClass;
  }
 private:
  double _alpha, _beta;
  MaxSlope _slope;
  int _newClass;
};

inline std::ostream &operator << (std::ostream &s, const Rule &r) {
  s << "Rule(alpha=" << r.alpha() << ", beta=" << r.beta() << ", newClass=" << r.newClass() << ")";
  return s;
}




template <int Dim>
class IndexList {
 public:
  IndexList() : _size(Dim) {
    for (int i = 0; i < Dim; i++) {
      _inds[i] = i;
    }
  }

  int size() const {
    return _size;
  }

  IndexList remove(int index) const {
    return IndexList(index, *this);
  }

  int operator[] (int index) const {
    return _inds[index];
  }

  bool empty() const {
    return _size == 0;
  }
 private:
  IndexList(int indexToRemove, const IndexList &src) {
    _size = src._size - 1;
    for (int i = 0; i < indexToRemove; i++) {
      _inds[i] = src._inds[i];
    }
    for (int i = indexToRemove+1; i < src._size; i++) {
      _inds[i-1] = src._inds[i];
    }
  }

  int _inds[Dim];
  int _size;
};


template <int Dim>
class IndexBox {
 public:
  typedef IndexBox<Dim-1> NextBox;
  typedef IndexBox<Dim> ThisType;
  IndexBox() : _actualSize(0), _offset(0), _size(0) {}

  IndexBox(int size_) : _actualSize(size_), _offset(0), _size(size_) {}

  static ThisType sameSize(int size) {
    return IndexBox(size, 0, size, NextBox::sameSize(size));
  }

  ThisType slice(int dim, int from, int to) const {
    if (dim == 0) {
      return IndexBox(_actualSize, _offset + from, to - from, _next);
    } else {
      return IndexBox(_actualSize, _offset, _size, _next.slice(dim-1, from, to));
    }
  }

  ThisType sliceLow(int dim) const {
    if (dim == 0) {
      return IndexBox(_actualSize, _offset, 1, _next);
    } else {
      return IndexBox(_actualSize, _offset, _size, _next.sliceLow(dim-1));
    }
  }

  ThisType sliceHigh(int dim) const {
    if (dim == 0) {
      return IndexBox(_actualSize, _offset + _size-1, 1, _next);
    } else {
      return IndexBox(_actualSize, _offset, _size, _next.sliceHigh(dim-1));
    }
  }


  ThisType slice(int dim, int index) const {
    return slice(dim, index, index + 1);
  }

  int numel() const {
    return _size*_next.numel();
  }


  // Public so that the + operator can use it.
  IndexBox(int actualSize_, int offset_, int size_,
    const NextBox &next_) : _actualSize(actualSize_),
    _offset(offset_), _size(size_), _next(next_) {}

  int actualSize() const {return _actualSize;}
  int offset() const {return _offset;}
  int size() const {return _size;}

  int calcIndex(const int *inds) const {
    assert(0 <= inds[0] && inds[0] < _size);
    return withOffset(inds[0]) + _actualSize*_next.calcIndex(inds + 1);
  }

  void calcInds(int index, int *inds) const {
    inds[0] = index % _actualSize;
    _next.calcInds(index/_actualSize, inds+1);
  }

  bool hasMidpoint() const {
    return (_size % 2 == 1) && _next.hasMidpoint();
  }

  int lowIndex() const {
    return _offset + _actualSize*_next.lowIndex();
  }

  int highIndex() const {
    return withOffset(_size-1) + _actualSize*_next.highIndex();
  }

  int midpointIndex() const {
    return withOffset(_size/2) + _actualSize*_next.midpointIndex();
  }

  void generate(Vertex *vertices,
                const MDArray<Rule, 2> &rules, double width,
      const IndexList<Dim> &indexList = IndexList<Dim>()) const {
      if (!indexList.empty()) {
        assert(hasMidpoint());
        int low = lowIndex();
        auto &lowv = vertices[low];
        int lowType = lowv.classIndex();
        assert(lowType != -1);

        int middle = midpointIndex();

        int high = highIndex();
        auto &highv = vertices[high];
        int highType = highv.classIndex();
        assert(highType != -1);

        vertices[middle] = rules(lowType, highType).combine(lowv, highv, width);

        for (int ka = 0; ka < indexList.size(); ka++) {
          IndexList<Dim> slicedList = indexList.remove(ka);
          assert(slicedList.size() < indexList.size());
          int kabel = indexList[ka];
          sliceLow(kabel).generate(vertices, rules, width, slicedList);
          sliceHigh(kabel).generate(vertices, rules, width, slicedList);
        }
      }
    }
 private:
  int withOffset(int x) const {return x + _offset;}
  int _actualSize;
  int _offset, _size;
  NextBox _next;
};

template <>
class IndexBox<0> {
 public:
  typedef IndexBox<0> ThisType;

  IndexBox() {}

  static ThisType sameSize(int size) {
    return ThisType();
  }

  ThisType slice(int dim, int from, int to) const {
    return ThisType();
  }

  ThisType sliceLow(int index) const {
    return ThisType();
  }

  ThisType sliceHigh(int index) const {
    return ThisType();
  }

  int numel() const {
    return 1;
  }

  int calcIndex(const int *inds) const {
    return 0;
  }

  bool hasMidpoint() const {return true;}

  int midpointIndex() const {return 0;}
  int lowIndex() const {return 0;}
  int highIndex() const {return 0;}

  void calcInds(int index, int *inds) const {}
 private:
};

template <int Dim>
IndexBox<Dim+1> operator+(const IndexBox<1> &a, const IndexBox<Dim> &b) {
  return IndexBox<Dim+1>(a.actualSize(), a.offset(), a.size(), b);
}





template <int Dim>
class SubdivFractals {
 public:
  static constexpr int vertexDim() {return 3;}
  static constexpr int vertexCount() {return std::pow(vertexDim(), Dim);}
  static constexpr int ctrlDim() {return 2;}
  static constexpr int ctrlCount() {return std::pow(ctrlDim(), Dim);}

  SubdivFractals(MDArray<Rule, 2> rules) :
    _rules(rules) {
    assert(rules.isSquare());
    int n = rules.rows();
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        assert(rules(i, j).defined());
      }
    }
  }

  template <typename CoordType=double>
  double eval(CoordType coords[Dim],
      Vertex ctrl[ctrlCount()],
      int depth, double width = 1.0) const {
    Vertex vertices[vertexCount()];
    if (depth == 0) {
      IndexBox<Dim> box = IndexBox<Dim>::sameSize(2);
      assert(box.numel() == ctrlCount());
      double result = 0.0*ctrl[0].value();
      for (int i = 0; i < ctrlCount(); i++) {
        int inds[Dim];
        box.calcInds(i, inds);
        double weight = 1.0;
        for (int j = 0; j < Dim; j++) {
          auto x = coords[j];
          weight *= (inds[j] == 0? 1.0 - x : x);
        }
        result = result + weight*ctrl[i].value();
      }
      return result;
    } else {
      IndexBox<Dim> vertexBox = IndexBox<Dim>::sameSize(vertexDim());
      IndexBox<Dim> ctrlBox = IndexBox<Dim>::sameSize(ctrlDim());

      Vertex vertices[vertexCount()];

      // Initialized the arrays
      for (int i = 0; i < ctrlCount(); i++) {
        int inds[Dim];
        ctrlBox.calcInds(i, inds);
        for (int j = 0; j < Dim; j++) {
          inds[j] *= 2;
        }
        int index = vertexBox.calcIndex(inds);
        vertices[index] = ctrl[i];
      }

      // Generate
      vertexBox.generate(vertices, _rules, width);

      // Assign cell indices and compute local coordinates
      int cellInds[Dim];
      double localCoords[Dim];
      for (int i = 0; i < Dim; i++) {
        int c = (coords[i] < 0.5? 0 : 1);
        cellInds[i] = c;
        localCoords[i] = 2.0*coords[i] - c;
      }

      // Slice out the local values
      Vertex localCtrl[ctrlCount()];
      for (int i = 0; i < ctrlCount(); i++) {
        int inds[Dim];
        ctrlBox.calcInds(i, inds);
        for (int j = 0; j < Dim; j++) {
          inds[j] += cellInds[j];
        }
        int index = vertexBox.calcIndex(inds);
        localCtrl[i] = vertices[index];
      }

      return eval(localCoords, localCtrl, depth - 1, 0.5*width);
    }
  }
 private:
  MDArray<Rule, 2> _rules;
};


}




#endif /* SUBDIVFRACTALS_H_ */
