/*
 * mathutils.h
 *
 *  Created on: 17 janv. 2014
 *      Author: jonas
 */

#ifndef MATHUTILS_H_
#define MATHUTILS_H_

#include <armadillo>
#include "../common/MDArray.h"

namespace arma {

template <typename T>
bool hasSize(T mat, int rows, int cols) {
  return mat.n_rows == rows && mat.n_cols == cols;
}

template <typename T>
void createMat(T &mat, int rows, int cols) {
  if (!hasSize(mat, rows, cols)) {
    mat = T(rows, cols);
  }
}

typedef arma::Mat<double>::fixed<2, 3> mat23;
typedef arma::Mat<double>::fixed<3, 2> mat32;

} /* namespace arma */

namespace sail {

template <int dims>
MDArray2d toRows(Array<arma::vec::fixed<dims> > vecs) {
  int count = vecs.size();
  MDArray2d dst(count, dims);
  for (int i = 0; i < count; i++) {
    double *v = vecs[i].memptr();
    for (int j = 0; j < dims; j++) {
      dst(i, j) = v[j];
    }
  }
  return dst;
}

#define MAKEDENSE(X) ((X) + arma::zeros((X).n_rows, (X.n_cols)))

// Makes a sparse matrix to select elements from a vector.
arma::sp_mat makeSpSel(Arrayb sel);

template <typename T>
auto SQNORM(const T &X) -> decltype(arma::dot(X, X)) {
  // X can for instance be arma::mat and then arma::dot(X, X) will be double.
  // decltype is the easiest way to infer the return type.
  return arma::dot(X, X);
}

// Concatenation of sparse matrices.
// No support for this yet in Armadillo.
arma::sp_mat vcat(arma::sp_mat A, arma::sp_mat B); //vertical
arma::sp_mat hcat(arma::sp_mat A, arma::sp_mat B); //horizontal
arma::sp_mat dcat(arma::sp_mat A, arma::sp_mat B); //diagonal

arma::sp_mat makePermutationMat(Arrayi ordering);

// Replaces the operation arma::kron(M, arma::sp_eye(eyeDim, eyeDim))
// which is not yet implemented in Armadillo
arma::sp_mat kronWithSpEye(arma::sp_mat M, int eyeDim);
arma::vec invElements(arma::vec v);
arma::sp_mat spDiag(arma::vec v);
}

#endif /* MATHUTILS_H_ */
