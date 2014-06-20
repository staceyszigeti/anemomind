/*
 *  Created on: 2014-03-28
 *      Author: Jonas Östlund <uppfinnarjonas@gmail.com>
 */

#include <gtest/gtest.h>

#include <server/nautical/grammars/Grammar.h>
#include <server/nautical/grammars/WindOrientedGrammar.h>

using namespace sail;

TEST(GrammarTest, Grammar001Info) {


  WindOrientedGrammarSettings settings;

  WindOrientedGrammar g(settings);

  Array<HNode> info = g.nodeInfo();
  EXPECT_EQ(info[24].description(), "Off");
  EXPECT_EQ(info.size(), 41);
  EXPECT_EQ(info[29].index(), 29);
}
