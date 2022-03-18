#include "../grower.h"

#include "../counters.h"
#include "../graph.h"
#include "gtest/gtest.h"

using namespace testing;

TEST(GrowerTest, G23) {
  Graph::set_global_graph_info(2, 3);
  for (int codegrees = 0; codegrees <= 2; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_EQ(s.canonicals[0].size(), 0);
    EXPECT_EQ(s.canonicals[1].size(), 1);
    EXPECT_EQ(s.canonicals[2].size(), 2);
    EXPECT_EQ(s.canonicals[3].size(), 0);
  }
}

TEST(GrowerTest, G23_B) {
  Graph::set_global_graph_info(2, 3);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 2; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(3, 2));
  }
}

TEST(GrowerTest, G24) {
  Graph::set_global_graph_info(2, 4);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 2; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(3, 2));
  }
}

TEST(GrowerTest, G25) {
  Graph::set_global_graph_info(2, 5);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 2; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(5, 3));
  }
}

TEST(GrowerTest, G26) {
  Graph::set_global_graph_info(2, 6);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 2; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(5, 3));
  }
}

TEST(GrowerTest, G34) {
  Graph::set_global_graph_info(3, 4);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 3; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(4, 3));
  }
}

TEST(GrowerTest, G45) {
  Graph::set_global_graph_info(4, 5);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 4; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(5, 4));
  }
}

TEST(GrowerTest, G56) {
  Graph::set_global_graph_info(5, 6);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 5; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(6, 5));
  }
}

TEST(GrowerTest, G67) {
  Graph::set_global_graph_info(6, 7);
  Counters::initialize();
  for (int codegrees = 0; codegrees <= 6; codegrees++) {
    Grower s(codegrees);
    s.grow();
    EXPECT_TRUE(Counters::get_min_theta() == Fraction(7, 6));
  }
}
