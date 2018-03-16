/*
 * HexMeshTest.cpp
 *
 *  Created on: 16 Mar 2018
 *      Author: jonas
 */

#include <gtest/gtest.h>
#include <server/nautical/tgtspeed/HexMesh.h>

using namespace sail;

TEST(HexMeshTest, BasicTest) {
  HexMesh mesh(3, 2.0);

  {
    auto xw = Eigen::Vector2d(3, 4);
    auto xg = mesh.worldToGrid(xw);
    EXPECT_LT(0.1, (xw - xg).norm());
    EXPECT_NEAR((mesh.gridToWorld(mesh.worldToGrid(xw)) - xw).norm(),
        0.0, 1.0e-6);
  }

  {
    auto x = Eigen::Vector2d(0.0, 0.0);
    auto y = mesh.worldToGrid(x);
    EXPECT_LT(0.1, y(0));
    EXPECT_LT(0.1, y(1));
  }

  {
    auto x = Eigen::Vector2d(0.0, 0.0);
    auto y = mesh.worldToGrid(x);
    EXPECT_LT(0.1, y(0));
    EXPECT_LT(0.1, y(1));
  }

  {
    auto x = Eigen::Vector2d(0.0, 0.0);
    auto y = mesh.gridToWorld(x);
    EXPECT_LT(y(0), -0.1);
    EXPECT_LT(y(1), -0.1);
  }
}
