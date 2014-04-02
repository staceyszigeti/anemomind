/*
 *  Created on: 2014-03-13
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#ifndef BANDMAT_H_
#define BANDMAT_H_

#include <server/common/MDArray.h>

namespace sail {

namespace BandMatInternal {
  // Used to produce regularization terms (used by addRegs)
  // It maps a vector I to [I 0] - [0 I].
  //
  // For instance, it maps [1] to [1 -1]
  //               it maps [1 -1] to [1 -2 1]
  //               it maps [1 -2 1] to [1 -3 3 -1] etc.
  Arrayd makeNextCoefs(Arrayd coefs);
}


template <typename T>
class BandMat {
 public:
  BandMat() : _rows(-1), _cols(-1), _left(-1), _right(-1) {}
  BandMat(int rows, int cols, int left, int right,
      MDArray<T, 2> data = MDArray<T, 2>()) :
    _rows(rows), _cols(cols), _left(left), _right(right) {
    initStorage(data);
  }

  BandMat<T> dup() {
    return BandMat(_rows, _cols, _left, _right, _data.dup());
  }


  T get(int i, int j) const {
    if (valid(i, j)) {
      return at(i, j);
    } else {
      return 0.0;
    }
  }

  void set(int i, int j, T x) {
    at(i, j) = x;
  }

  T &at(int i, int j) {
    assert(valid(i, j));
    int j0 = calcCol(i, j);
    return _data(i, j0);
  }
  const T &at(int i, int j) const {
    assert(valid(i, j));
    int j0 = calcCol(i, j);
    return _data(i, j0);
  }


  T &operator() (int i, int j) {return at(i, j);}

  int rows() const {return _rows;}
  int cols() const {return _cols;}
  int left() const {return _left;}
  int right() const {return _right;}
  int leftColIndex(int row) const {return std::max(0, row - _left);}
  int topRowIndex(int col) const {return std::max(0, col - _right);}
  int rightColIndex(int row) const {return std::min(_cols, row + _right + 1);}
  int bottomRowIndex(int col) const {return std::min(_rows, col + _left + 1);}
  MDArray<T, 2> toDense() {
    MDArray<T, 2> dst(_rows, _cols);
    dst.setAll(0.0);
    for (int i = 0; i < _rows; i++) {
      int lc = leftColIndex(i);
      int rc = rightColIndex(i);
      for (int j = lc; j < rc; j++) {
        dst(i, j) = get(i, j);
      }
    }
    return dst;
  }

  bool valid(int i, int j) const {
    return leftColIndex(i) <= j && j < rightColIndex(i) &&
        topRowIndex(j) <= i && i < bottomRowIndex(j);
  }

  void setAll(T x) {
    _data.setAll(x);
  }

  void addReg(Arrayd coefs, T w) {
    T w2 = w*w;
    assert(_rows == _cols);
    int dim = coefs.size();
    int n = _rows - dim + 1;
    for (int offs = 0; offs < n; offs++) {
      for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
          at(offs + i, offs + j) += w2*coefs[i]*coefs[j];
        }
      }
    }
  }

  void addRegs(Arrayi orders, Array<T> regWeights) {
    int count = regWeights.size();
    int maxOrder = 0;
    for (int i = 0; i < count; i++) {
      maxOrder = std::max(maxOrder, orders[i]);
    }

    Array<Arrayd> coefs(maxOrder+1);
    coefs[0] = Arrayd(1);
    coefs[0][0] = 1.0;
    for (int i = 0; i < maxOrder; i++) {
      coefs[i+1] = BandMatInternal::makeNextCoefs(coefs[i]);
    }

    for (int i = 0; i < count; i++) {
      addReg(coefs[orders[i]], regWeights[i]);
    }
  }

  void addNormalEq(int n, int *I, double *W) {
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        at(I[i], I[j]) += W[i]*W[j];
      }
    }
  }

  MDArray<T, 2> getDataForDebug() {return _data;}
 private:
  // Map (i, j) col index a col index of the underlying storage.
  // (The row index is the same)
  int calcCol(int i, int j) const { return j - i + _left;}

  int _rows, _cols, _left, _right;
  MDArray<T, 2> _data;

  void initStorage(MDArray<T, 2> data) {
    int rows = _rows;
    int cols = _left + 1 + _right;
    if (data.empty()) {
      _data = MDArray<T, 2>(rows, cols);
    } else {
      assert(data.isMatrix(rows, cols));
      _data = data;
    }
  }
};

typedef BandMat<double> BandMatd;

namespace BMGE {
  template <typename T>
  void scaleRow(int row, BandMat<T> *Aio, T factor) {
    int from = (*Aio).leftColIndex(row);
    int to = (*Aio).rightColIndex(row);
    for (int col = from; col < to; col++) {
      (*Aio)(row, col) *= factor;
    }
  }

  template <typename T>
  void scaleRow(int row, MDArray<T, 2> *Bio, T factor) {
    int n = Bio->cols();
    for (int col = 0; col < n; col++) {
      (*Bio)(row, col) *= factor;
    }
  }

  template <typename T>
  void scaleRowsTo1(int index, BandMat<T> *Aio, MDArray<T, 2> *Bio, double tol) {
    T x = (*Aio)(index, index);
    assert(tol < (x < 0? -x : x));
    T factor = 1.0/x;
    scaleRow(index, Aio, factor);
    scaleRow(index, Bio, factor);
  }

  template <typename T>
  void eliminateSub(int i0, int i1, BandMat<T> *Aio, MDArray<T, 2> *Bio) {
    BandMat<T> &A = *Aio;
    MDArray<T, 2> &B = *Bio;

    T factor = -A.get(i1, i0)/A.get(i0, i0);

    {
      int from = A.leftColIndex(i1);
      int to = A.rightColIndex(i1);
      for (int j = from; j < to; j++) {
        A(i1, j) += factor*A.get(i0, j);
      }
    }{
      int from = 0;
      int to = B.cols();
      for (int j = from; j < to; j++) {
        B(i1, j) += factor*B(i0, j);
      }
    }


  }

  template <typename T>
  void eliminateForward(BandMat<T> *Aio, MDArray<T, 2> *Bio, double tol) {
    BandMat<T> &A = *Aio;
    MDArray<T, 2> &B = *Bio;
    for (int i = 0; i < A.rows(); i++) {
      scaleRowsTo1(i, Aio, Bio, tol);
      int to = A.bottomRowIndex(i);
      for (int idst = i+1; idst < to; idst++) {
        eliminateSub(i, idst, Aio, Bio);
      }
    }
  }

  template <typename T>
  void eliminateBackward(BandMat<T> *Aio, MDArray<T, 2> *Bio) {
    BandMat<T> &A = *Aio;
    MDArray<T, 2> &B = *Bio;
    int rows = A.rows();
    for (int i = rows-1; i >= 0; i--) {
      int from = A.topRowIndex(i);
      for (int idst = from; idst < i; idst++) {
        eliminateSub(i, idst, Aio, Bio);
      }
    }
  }
}

/*
 * Solve a system with a band matrix using Gaussian elimination.
 * No pivoting is used, which can lead to loss of accuracy for small elements.
 * This is checked for with the tol parameter.
 */
template <typename T>
void bandMatGaussElimDestructive(BandMat<T> *Aio, MDArray<T, 2> *Bio, double tol = 1.0e-6) {
  using namespace BMGE;
  BandMat<T> &A = *Aio;
  MDArray<T, 2> &B = *Bio;
  eliminateForward(Aio, Bio, tol);
  eliminateBackward(Aio, Bio);
}

template <typename T>
MDArray<T, 2> bandMatGaussElim(BandMat<T> A, MDArray<T, 2> B, double tol = 1.0e-6) {
  BandMat<T> Atemp = A.dup();
  MDArray<T, 2> Bdst = B.dup();
  bandMatGaussElimDestructive(&Atemp, &Bdst, tol);
  return Bdst;
}

}

#endif /* BANDMAT_H_ */
