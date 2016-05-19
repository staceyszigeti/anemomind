/*
 * DgbsvWrapperTest.cpp
 *
 *  Created on: Apr 22, 2016
 *      Author: jonas
 */

#include <server/math/lapack/DgbsvWrapper.h>
#include <gtest/gtest.h>
#include <server/common/ArrayIO.h>

using namespace sail;

TEST(DgbsvWrapperTest, TestSolve1x1) {

  BandMatrix<double> A(1, 1, 0, 0);
  A(0, 0) = 2.0;

  MDArray2d B(1, 1);
  B(0, 0) = 9.0;

  EXPECT_TRUE(easyDgbsvInPlace(&A, &B));
  EXPECT_NEAR(B(0, 0), 4.5, 1.0e-6);
}

TEST(DgbsvWrapperTest, TestSolveDiagMultiRHS) {

  BandMatrix<double> A(3, 3, 0, 0);
  A(0, 0) = 2.0;
  A(1, 1) = 4.0;
  A(2, 2) = 8.0;

  MDArray2d B(3, 2);
  B(0, 0) = 9.0;
  B(1, 0) = 89.0;
  B(2, 0) = -30.0;

  B(0, 1) = 1.0;
  B(1, 1) = 2.0;
  B(2, 1) = 3.0;


  EXPECT_TRUE(easyDgbsvInPlace(&A, &B));

  EXPECT_NEAR(B(0, 0), 4.5, 1.0e-6);
  EXPECT_NEAR(B(1, 0), 22.25, 1.0e-6);
  EXPECT_NEAR(B(2, 0), -3.75, 1.0e-6);

  EXPECT_NEAR(B(0, 1), 0.5, 1.0e-6);
  EXPECT_NEAR(B(1, 1), 0.5, 1.0e-6);
  EXPECT_NEAR(B(2, 1), 0.375, 1.0e-6);
}

TEST(DgbsvWrapperTest, ThickBand) {
  BandMatrix<double> A(3, 3, 1, 1);
  A(0, 0) = 1; A(0, 1) = 2;
  A(1, 0) = 4; A(1, 1) = 3; A(1, 2) = 5;
               A(2, 1) = 7; A(2, 2) = 3;

  MDArray2d B(3, 1);
  B(0, 0) = 5;
  B(1, 0) = 25;
  B(2, 0) = 23;


  EXPECT_TRUE(easyDgbsvInPlace(&A, &B));

  EXPECT_NEAR(B(0, 0), 1.0, 1.0e-6);
  EXPECT_NEAR(B(1, 0), 2.0, 1.0e-6);
  EXPECT_NEAR(B(2, 0), 3.0, 1.0e-6);
}
