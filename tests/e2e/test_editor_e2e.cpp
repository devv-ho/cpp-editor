// E2e tests: spawn real editor binary over a PTY, assert on screen state.
//
// validate_status(mode):
//   Checks the last non-empty line of the latest frame starts exactly with
//   " {mode}  " (the prefix FtxuiRenderer::render_statusbar() produces).
//   Exact prefix match prevents buffer content from producing a false positive.
//
// validate_buffer(expected):
//   Each non-empty line in `expected` must appear as a substring of at least
//   one line in the buffer region (all frame lines except the status bar).
//
// Use fmt::kNormal / fmt::kInsert as mode tokens — defined in EditorScreenFormat.hpp
// alongside FtxuiRenderer's format so a renderer change only needs one update.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "EditorScreenFormat.hpp"
#include "PtyHarness.hpp"

namespace {

#ifndef EDITOR_BINARY
#define EDITOR_BINARY "editor"
#endif

std::string make_temp(const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / "editor_e2e_test.cpp";
    std::ofstream f(path);
    f << content;
    return path.string();
}

}  // namespace

using editor::e2e::PtyHarness;
namespace fmt = editor::e2e::fmt;

// ── Startup ───────────────────────────────────────────────────────────────────

TEST(EditorE2e, StartsAndDisplaysFileContent) {
    auto path = make_temp("int main() {}\n");
    PtyHarness h(EDITOR_BINARY, {path});

    EXPECT_TRUE(h.validate_status(fmt::kNormal, 3000)) << h.current_frame();
    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("int main() {}"))) << h.current_frame();
}

TEST(EditorE2e, StartsInNormalMode) {
    auto path = make_temp("hello\n");
    PtyHarness h(EDITOR_BINARY, {path});

    EXPECT_TRUE(h.validate_status(fmt::kNormal, 3000)) << h.current_frame();
}

// ── Quit ──────────────────────────────────────────────────────────────────────

TEST(EditorE2e, EscInNormalModeQuits) {
    auto path = make_temp("bye\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("\x1b");

    EXPECT_EQ(h.wait_exit(3000), 0);
}

// ── Mode switching ────────────────────────────────────────────────────────────

TEST(EditorE2e, EnterInsertModeWithI) {
    auto path = make_temp("line\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("i");

    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, EnterInsertAfterWithA) {
    auto path = make_temp("line\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("a");

    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, EscFromInsertReturnsToNormal) {
    auto path = make_temp("line\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("\x1b");

    EXPECT_TRUE(h.validate_status(fmt::kNormal, 2000)) << h.current_frame();
}

// ── Text insertion ────────────────────────────────────────────────────────────

TEST(EditorE2e, InsertModeTypingAppearsOnScreen) {
    auto path = make_temp("\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("aims");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("aims"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, InsertNewlineSplitsLine) {
    auto path = make_temp("\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("a");
    h.write("\r");
    h.write("b");

    EXPECT_TRUE(h.validate_buffer("a\nb", 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, BackspaceDeletesChar) {
    auto path = make_temp("\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("xz");
    h.write("\x7f");  // Backspace — deletes 'z'

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("x"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

// ── Normal mode motions ───────────────────────────────────────────────────────
// Strategy: pre-load text into the file, verify motion by inserting a marker
// char and checking the resulting buffer content on screen.

TEST(EditorE2e, LMovesCursorRight) {
    // "az": cursor starts at col 0, l -> col 1, i -> insert "X" -> "aXz"
    auto path = make_temp("az\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("l");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("aXz"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, HMovesCursorLeft) {
    // "az": l -> col 1, h -> col 0, i -> insert "X" -> "Xaz"
    auto path = make_temp("az\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("l");
    h.write("h");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("Xaz"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, JMovesDown) {
    // j -> line 1, i -> insert "X" -> "Xbar"
    auto path = make_temp("foo\nbar\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("j");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("Xbar"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, KMovesUp) {
    // j -> line 1, k -> line 0, i -> insert "X" -> "Xfoo"
    auto path = make_temp("foo\nbar\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("j");
    h.write("k");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("Xfoo"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, DollarMovesToEndOfLine) {
    // "ab": $ -> col 1, a (insert_after) -> insert "X" -> "abX"
    auto path = make_temp("ab\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("$");
    h.write("a");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("abX"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, ZeroMovesToStartOfLine) {
    // "ab": l -> col 1, 0 -> col 0, i -> insert "X" -> "Xab"
    auto path = make_temp("ab\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("l");
    h.write("0");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("Xab"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, GMovesToLastLine) {
    // G -> last line, i -> insert "X" -> "Xbaz"
    auto path = make_temp("foo\nbar\nbaz\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("G");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("Xbaz"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}

TEST(EditorE2e, GGMovesToFirstLine) {
    // G -> last line, gg -> first line, i -> insert "X" -> "Xfoo"
    auto path = make_temp("foo\nbar\nbaz\n");
    PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.validate_status(fmt::kNormal, 3000));
    h.write("G");
    h.write("g");
    h.write("g");
    h.write("i");
    ASSERT_TRUE(h.validate_status(fmt::kInsert, 2000));
    h.write("X");

    EXPECT_TRUE(h.validate_buffer(fmt::buffer_line("Xfoo"), 2000)) << h.current_frame();
    EXPECT_TRUE(h.validate_status(fmt::kInsert, 2000)) << h.current_frame();
}
