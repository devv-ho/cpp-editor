#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "PtyHarness.hpp"

namespace {

// Path to the editor binary set by CMake via -DEDITOR_BINARY.
#ifndef EDITOR_BINARY
#define EDITOR_BINARY "editor"
#endif

// Creates a temp file with content and returns its path.
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

    EXPECT_TRUE(h.wait_for("int main", 3000)) << "file content not rendered:\n" << h.screen_text();
}

TEST(EditorE2e, StartsInNormalMode) {
    auto path = make_temp("hello\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    EXPECT_TRUE(h.wait_for("NORMAL", 3000)) << "NORMAL mode indicator missing:\n"
                                            << h.screen_text();
}

// ── Quit ──────────────────────────────────────────────────────────────────────

TEST(EditorE2e, EscInNormalModeQuits) {
    auto path = make_temp("bye\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.wait_for("NORMAL", 3000));
    h.write("\x1b");  // ESC

    int exit_code = h.wait_exit(3000);
    EXPECT_EQ(exit_code, 0);
}

// ── Mode switching ────────────────────────────────────────────────────────────

TEST(EditorE2e, EnterInsertModeWithI) {
    auto path = make_temp("line\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.wait_for("NORMAL", 3000));
    h.write("i");

    EXPECT_TRUE(h.wait_for("INSERT", 2000)) << "INSERT mode indicator missing:\n"
                                            << h.screen_text();
}

TEST(EditorE2e, EscFromInsertReturnsToNormal) {
    auto path = make_temp("line\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.wait_for("NORMAL", 3000));
    h.write("i");
    ASSERT_TRUE(h.wait_for("INSERT", 2000));
    h.write("\x1b");

    EXPECT_TRUE(h.wait_for("NORMAL", 2000)) << "did not return to NORMAL:\n" << h.screen_text();
}

// ── Text insertion ────────────────────────────────────────────────────────────

TEST(EditorE2e, InsertModeTypingAppearsOnScreen) {
    auto path = make_temp("\n");
    editor::e2e::PtyHarness h(EDITOR_BINARY, {path});

    ASSERT_TRUE(h.wait_for("NORMAL", 3000));
    h.write("i");
    ASSERT_TRUE(h.wait_for("INSERT", 2000));
    h.write("hello");

    EXPECT_TRUE(h.wait_for("hello", 2000)) << "typed text not visible:\n" << h.screen_text();
}
