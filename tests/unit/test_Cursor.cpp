// Unit tests for Cursor.

#include <gtest/gtest.h>

#include "core/entities/Buffer.hpp"
#include "core/entities/Cursor.hpp"

using editor::core::Buffer;
using editor::core::Cursor;
using editor::core::Position;

// -- Helpers ------------------------------------------------------------------

struct CursorFixture : ::testing::Test {
    Buffer buf_ = Buffer::from_text("hello\nworld\nfoo");
    Cursor cur_{buf_};
};

// -- Initial state ------------------------------------------------------------

TEST_F(CursorFixture, InitialPositionIsOrigin) { EXPECT_EQ(cur_.position(), (Position{0, 0})); }

// -- move_left / move_right ---------------------------------------------------

TEST_F(CursorFixture, MoveRightAdvancesCol) {
    cur_.move_right();
    EXPECT_EQ(cur_.col(), 1u);
}

TEST_F(CursorFixture, MoveLeftDecrementsCol) {
    cur_.move_right();
    cur_.move_left();
    EXPECT_EQ(cur_.col(), 0u);
}

TEST_F(CursorFixture, MoveLeftClampsAtSOL) {
    cur_.move_left();
    EXPECT_EQ(cur_.col(), 0u);
}

TEST_F(CursorFixture, MoveRightClampsAtLineLength) {
    // "hello" has length 5, col is allowed up to 5 (past last char)
    for (int i = 0; i < 10; ++i) {
        cur_.move_right();
    }
    EXPECT_EQ(cur_.col(), 5u);
}

TEST_F(CursorFixture, MoveRightOnEmptyLineStaysAtZero) {
    Buffer empty = Buffer::from_text("");
    Cursor c{empty};
    c.move_right();
    EXPECT_EQ(c.col(), 0u);
}

// -- move_up / move_down ------------------------------------------------------

TEST_F(CursorFixture, MoveDownAdvancesLine) {
    cur_.move_down();
    EXPECT_EQ(cur_.line(), 1u);
}

TEST_F(CursorFixture, MoveUpDecrementsLine) {
    cur_.move_down();
    cur_.move_up();
    EXPECT_EQ(cur_.line(), 0u);
}

TEST_F(CursorFixture, MoveUpClampsAtFirstLine) {
    cur_.move_up();
    EXPECT_EQ(cur_.line(), 0u);
}

TEST_F(CursorFixture, MoveDownClampsAtLastLine) {
    for (int i = 0; i < 10; ++i) {
        cur_.move_down();
    }
    EXPECT_EQ(cur_.line(), 2u);
}

TEST_F(CursorFixture, MoveDownClampsColToShorterLine) {
    // line 0: "hello" (len 5), line 2: "foo" (len 3)
    // move col to 5, then down twice -- clamp_col clamps to len-1 on shorter line
    for (int i = 0; i < 5; ++i) {
        cur_.move_right();
    }
    cur_.move_down();
    cur_.move_down();
    EXPECT_EQ(cur_.col(), 2u);
}

// -- move_top / move_bottom ---------------------------------------------------

TEST_F(CursorFixture, MoveTopGoesToOrigin) {
    cur_.move_down();
    cur_.move_right();
    cur_.move_top();
    EXPECT_EQ(cur_.position(), (Position{0, 0}));
}

TEST_F(CursorFixture, MoveBottomGoesToLastLine) {
    cur_.move_bottom();
    EXPECT_EQ(cur_.line(), 2u);
    EXPECT_EQ(cur_.col(), 0u);
}

// -- move_sol / move_eol ------------------------------------------------------

TEST_F(CursorFixture, MoveSOLGoesToColZero) {
    for (int i = 0; i < 3; ++i) {
        cur_.move_right();
    }
    cur_.move_sol();
    EXPECT_EQ(cur_.col(), 0u);
}

TEST_F(CursorFixture, MoveEOLGoesToLastChar) {
    cur_.move_eol();
    EXPECT_EQ(cur_.col(), 4u);  // "hello" last char at col 4
}

TEST_F(CursorFixture, MoveEOLOnEmptyLineStaysAtZero) {
    Buffer empty = Buffer::from_text("");
    Cursor c{empty};
    c.move_eol();
    EXPECT_EQ(c.col(), 0u);
}

// -- set_position -------------------------------------------------------------

TEST_F(CursorFixture, SetPositionClampsOutOfRangeLine) {
    cur_.set_position({99, 0});
    EXPECT_EQ(cur_.line(), 2u);
}

TEST_F(CursorFixture, SetPositionClampsOutOfRangeCol) {
    cur_.set_position({0, 99});
    EXPECT_EQ(cur_.col(), 4u);
}
