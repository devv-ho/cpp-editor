// E2e tests: spawn real editor binary over a PTY, assert on screen state.
//
// expect_screen(expected, timeout_ms):
//   Each non-empty line in `expected` must appear as a substring of at least
//   one line in the latest rendered frame. This checks the real screen state,
//   not an accumulated text buffer, so mode transitions are reliable.
//
// Statusbar format rendered by FtxuiRenderer:
//   " NORMAL   file:///..."  or  " INSERT   file:///..."
// Use " NORMAL " / " INSERT " (with spaces) to distinguish from buffer text.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

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

// ── Startup ───────────────────────────────────────────────────────────────────

TEST(EditorE2e, StartsAndDisplaysFileContent) {
    auto path = make_temp("int main() {}\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    EXPECT_TRUE(h.expect_screen("int main() {}\n NORMAL ", 3000)) << "screen:\n"
                                                                  << h.current_frame();
}

TEST(EditorE2e, StartsInNormalMode) {
    auto path = make_temp("hello\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    EXPECT_TRUE(h.expect_screen(" NORMAL ", 3000)) << "screen:\n" << h.current_frame();
}

// ── Quit ──────────────────────────────────────────────────────────────────────

TEST(EditorE2e, EscInNormalModeQuits) {
    auto path = make_temp("bye\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("\x1b");

    EXPECT_EQ(h.wait_exit(3000), 0);
}

// ── Mode switching ────────────────────────────────────────────────────────────

TEST(EditorE2e, EnterInsertModeWithI) {
    auto path = make_temp("line\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("i");

    EXPECT_TRUE(h.expect_screen(" INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, EnterInsertAfterWithA) {
    auto path = make_temp("line\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("a");

    EXPECT_TRUE(h.expect_screen(" INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, EscFromInsertReturnsToNormal) {
    auto path = make_temp("line\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("\x1b");

    EXPECT_TRUE(h.expect_screen(" NORMAL ", 2000)) << "screen:\n" << h.current_frame();
}

// ── Text insertion ────────────────────────────────────────────────────────────

TEST(EditorE2e, InsertModeTypingAppearsOnScreen) {
    auto path = make_temp("\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("aims");

    EXPECT_TRUE(h.expect_screen("aims\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, InsertNewlineSplitsLine) {
    auto path = make_temp("\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("a");
    h.write("\r");
    h.write("b");

    // Both "a" and "b" visible on separate lines, still in INSERT
    EXPECT_TRUE(h.expect_screen("a\nb\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, BackspaceDeletesChar) {
    auto path = make_temp("\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("xz");
    h.write("\x7f");  // Backspace — deletes 'z'

    EXPECT_TRUE(h.expect_screen("x\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

// ── Normal mode motions ───────────────────────────────────────────────────────
// Strategy: pre-load text into the file, verify motion by inserting a marker
// char and checking the resulting buffer content on screen.

TEST(EditorE2e, LMovesCursorRight) {
    // "az": cursor starts at col 0, l -> col 1, i -> insert "X" -> "aXz"
    auto path = make_temp("az\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("l");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("aXz\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, HMovesCursorLeft) {
    // "az": l -> col 1, h -> col 0, i -> insert "X" -> "Xaz"
    auto path = make_temp("az\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("l");
    h.write("h");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("Xaz\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, JMovesDown) {
    // j -> line 1, i -> insert "X" -> "Xbar"
    auto path = make_temp("foo\nbar\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("j");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("Xbar\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, KMovesUp) {
    // j -> line 1, k -> line 0, i -> insert "X" -> "Xfoo"
    auto path = make_temp("foo\nbar\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("j");
    h.write("k");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("Xfoo\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, DollarMovesToEndOfLine) {
    // "ab": $ -> col 1, a (insert_after) -> insert "X" -> "abX"
    auto path = make_temp("ab\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("$");
    h.write("a");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("abX\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, ZeroMovesToStartOfLine) {
    // "ab": l -> col 1, 0 -> col 0, i -> insert "X" -> "Xab"
    auto path = make_temp("ab\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("l");
    h.write("0");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("Xab\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, GMovesToLastLine) {
    // G -> last line, i -> insert "X" -> "Xbaz"
    auto path = make_temp("foo\nbar\nbaz\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("G");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("Xbaz\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}

TEST(EditorE2e, GGMovesToFirstLine) {
    // G -> last line, gg -> first line, i -> insert "X" -> "Xfoo"
    auto path = make_temp("foo\nbar\nbaz\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.expect_screen(" NORMAL ", 3000));
    h.write("G");
    h.write("g");
    h.write("g");
    h.write("i");
    ASSERT_TRUE(h.expect_screen(" INSERT ", 2000));
    h.write("X");

    EXPECT_TRUE(h.expect_screen("Xfoo\n INSERT ", 2000)) << "screen:\n" << h.current_frame();
}
