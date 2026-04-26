#include "core/entities/Buffer.hpp"
#include "core/entities/Cursor.hpp"

#include <gtest/gtest.h>

using editor::core::Buffer;
using editor::core::Cursor;
using editor::core::Position;

// ── Helpers
// ───────────────────────────────────────────────────────────────────

struct CursorFixture : ::testing::Test {
  Buffer buf{"hello\nworld\nfoo"};
  Cursor cur{buf};
};

// ── Initial state
// ─────────────────────────────────────────────────────────────

TEST_F(CursorFixture, InitialPositionIsOrigin) {
  EXPECT_EQ(cur.position(), (Position{0, 0}));
}

// ── move_left / move_right
// ────────────────────────────────────────────────────

TEST_F(CursorFixture, MoveRightAdvancesCol) {
  cur.move_right();
  EXPECT_EQ(cur.col(), 1u);
}

TEST_F(CursorFixture, MoveLeftDecrementsCol) {
  cur.move_right();
  cur.move_left();
  EXPECT_EQ(cur.col(), 0u);
}

TEST_F(CursorFixture, MoveLeftClampsAtSOL) {
  cur.move_left();
  EXPECT_EQ(cur.col(), 0u);
}

TEST_F(CursorFixture, MoveRightClampsAtEOL) {
  // "hello" has length 5, max col in normal mode is 4
  for (int i = 0; i < 10; ++i)
    cur.move_right();
  EXPECT_EQ(cur.col(), 4u);
}

TEST_F(CursorFixture, MoveRightOnEmptyLineStaysAtZero) {
  Buffer empty{""};
  Cursor c{empty};
  c.move_right();
  EXPECT_EQ(c.col(), 0u);
}

// ── move_up / move_down
// ───────────────────────────────────────────────────────

TEST_F(CursorFixture, MoveDownAdvancesLine) {
  cur.move_down();
  EXPECT_EQ(cur.line(), 1u);
}

TEST_F(CursorFixture, MoveUpDecrementsLine) {
  cur.move_down();
  cur.move_up();
  EXPECT_EQ(cur.line(), 0u);
}

TEST_F(CursorFixture, MoveUpClampsAtFirstLine) {
  cur.move_up();
  EXPECT_EQ(cur.line(), 0u);
}

TEST_F(CursorFixture, MoveDownClampsAtLastLine) {
  for (int i = 0; i < 10; ++i)
    cur.move_down();
  EXPECT_EQ(cur.line(), 2u);
}

TEST_F(CursorFixture, MoveDownClampsColToShorterLine) {
  // line 0: "hello" (5), line 2: "foo" (3)
  // move col to 4, then down twice — col should clamp to 2 on "foo"
  for (int i = 0; i < 4; ++i)
    cur.move_right();
  cur.move_down();
  cur.move_down();
  EXPECT_EQ(cur.col(), 2u);
}

// ── move_top / move_bottom
// ────────────────────────────────────────────────────

TEST_F(CursorFixture, MoveTopGoesToOrigin) {
  cur.move_down();
  cur.move_right();
  cur.move_top();
  EXPECT_EQ(cur.position(), (Position{0, 0}));
}

TEST_F(CursorFixture, MoveBottomGoesToLastLine) {
  cur.move_bottom();
  EXPECT_EQ(cur.line(), 2u);
  EXPECT_EQ(cur.col(), 0u);
}

// ── move_sol / move_eol
// ───────────────────────────────────────────────────────

TEST_F(CursorFixture, MoveSOLGoesToColZero) {
  for (int i = 0; i < 3; ++i)
    cur.move_right();
  cur.move_sol();
  EXPECT_EQ(cur.col(), 0u);
}

TEST_F(CursorFixture, MoveEOLGoesToLastChar) {
  cur.move_eol();
  EXPECT_EQ(cur.col(), 4u); // "hello" last char at col 4
}

TEST_F(CursorFixture, MoveEOLOnEmptyLineStaysAtZero) {
  Buffer empty{""};
  Cursor c{empty};
  c.move_eol();
  EXPECT_EQ(c.col(), 0u);
}

// ── set_position
// ──────────────────────────────────────────────────────────────

TEST_F(CursorFixture, SetPositionClampsOutOfRangeLine) {
  cur.set_position({99, 0});
  EXPECT_EQ(cur.line(), 2u);
}

TEST_F(CursorFixture, SetPositionClampsOutOfRangeCol) {
  cur.set_position({0, 99});
  EXPECT_EQ(cur.col(), 4u);
}
