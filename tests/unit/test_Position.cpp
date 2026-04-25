#include "core/entities/Position.hpp"

#include <gtest/gtest.h>

using editor::core::Position;

TEST(PositionTest, DefaultConstructsToOrigin) {
  Position p;
  EXPECT_EQ(p.line, 0u);
  EXPECT_EQ(p.col, 0u);
}

TEST(PositionTest, AggregateInit) {
  Position p{3, 7};
  EXPECT_EQ(p.line, 3u);
  EXPECT_EQ(p.col, 7u);
}

TEST(PositionTest, EqualityHolds) {
  EXPECT_EQ((Position{1, 2}), (Position{1, 2}));
  EXPECT_NE((Position{1, 2}), (Position{1, 3}));
  EXPECT_NE((Position{2, 2}), (Position{1, 2}));
}

TEST(PositionTest, SpaceshipOrdersByLineThenCol) {
  EXPECT_LT((Position{0, 5}), (Position{1, 0}));
  EXPECT_LT((Position{1, 0}), (Position{1, 1}));
  EXPECT_GT((Position{2, 0}), (Position{1, 9}));
}

TEST(PositionTest, ToStringFormat) {
  EXPECT_EQ((Position{0, 0}.to_string()), "(0, 0)");
  EXPECT_EQ((Position{3, 7}.to_string()), "(3, 7)");
}
