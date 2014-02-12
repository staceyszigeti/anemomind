/*
 * mathutils.cpp
 *
 *  Created on: 17 janv. 2014
 *      Author: jonas
 */

#include "mathutils.h"

namespace sail {

arma::sp_mat makeSpSel(Arrayb sel) {
  int elemCount = countTrue(sel);
  int count = sel.size();
  arma::umat IJ(2, elemCount);
  arma::vec X(elemCount);

  int counter = 0;
  for (int i = 0; i < count; i++) {
    if (sel[i]) {
      IJ(0, counter) = counter;
      IJ(1, counter) = i;
      X[counter] = 1.0;
      counter++;
    }
  }
  assert(counter == elemCount);
  return arma::sp_mat(IJ, X, elemCount, count);
}


arma::sp_mat vcat(arma::sp_mat A, arma::sp_mat B) {
  int Arows = A.n_rows;
  int Brows = B.n_rows;
  int ABrows = Arows + Brows;
  arma::sp_mat I = arma::speye(ABrows, ABrows);
  return I.cols(0, Arows-1)*A + I.cols(Arows, ABrows-1)*B;
}

arma::sp_mat hcat(arma::sp_mat A, arma::sp_mat B) {
  int Acols = A.n_cols;
  int Bcols = B.n_cols;
  int ABcols = Acols + Bcols;
  arma::sp_mat I = arma::speye(ABcols, ABcols);
  return A*I.rows(0, Acols-1) + B*I.rows(Acols, ABcols-1);
}

arma::sp_mat dcat(arma::sp_mat A, arma::sp_mat B) {
  return vcat(hcat(A, arma::sp_mat(A.n_rows, B.n_cols)),
              hcat(arma::sp_mat(B.n_rows, A.n_cols), B));
}

arma::sp_mat makePermutationMat(Arrayi ordering) {
  assert(false); // "Not tested yet"
  int n = ordering.size();
  arma::umat IJ(2, n);
  arma::vec X(n);
  for (int i = 0; i < n; i++) {
    IJ(0, i) = i;
    IJ(1, i) = ordering[i];
    X[i] = 1.0;
  }
  return arma::sp_mat(IJ, X, n, n);
}

arma::sp_mat kronWithSpEye(arma::sp_mat M, int eyeDim) {
  int dstElemCount = eyeDim*M.n_nonzero;
  arma::umat IJ(2, dstElemCount);
  arma::vec X(dstElemCount);
  int counter = 0;
  for (int srcCol = 0; srcCol < M.n_cols; srcCol++) { // For every column
    arma::uword colptr = M.col_ptrs[srcCol];
    const arma::uword *rowIndicesInCol = M.row_indices + colptr;
    const double *xInCol = M.values + colptr;
    arma::uword countInCol = M.col_ptrs[srcCol+1] - M.col_ptrs[srcCol];
    for (int k = 0; k < countInCol; k++) { // For every element of this column
      int srcRow = rowIndicesInCol[k];
      int x = xInCol[k];
      int dstOffset = eyeDim*counter;
      int dstRowOffset = eyeDim*srcRow;
      int dstColOffset = eyeDim*srcCol;
      for (int i = 0; i < eyeDim; i++) {
        int at = dstOffset + i;
        IJ(0, at) = dstRowOffset + i;
        IJ(1, at) = dstColOffset + i;
        X[at] = x;
      }
      counter++;
    }
  }
  assert(counter == M.n_nonzero);
  return arma::sp_mat(IJ, X, eyeDim*M.n_rows, eyeDim*M.n_cols);
}

} /* namespace sail */
