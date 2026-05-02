// Unit tests for EditorCommands and InputDispatcher.

#include <gtest/gtest.h>

#include "core/entities/Document.hpp"
#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/InputDispatcher.hpp"

using editor::core::Document;
using editor::core::EditorMode;
using editor::core::InputDispatcher;
using editor::core::Key;
namespace cmd = editor::core::commands;

// -- move_left / move_right (normal mode policy) ------------------------------

TEST(EditorCommandsTest, MoveRightClampsAtLastChar) {
    Document doc{"hello"};
    for (int i = 0; i < 10; ++i) {
        cmd::move_right(doc);
    }
    EXPECT_EQ(doc.position().col, 4u);  // len=5, max col=4
}

TEST(EditorCommandsTest, MoveLeftStopsAtSOL) {
    Document doc{"hello"};
    cmd::move_left(doc);
    EXPECT_EQ(doc.position().col, 0u);
}

TEST(EditorCommandsTest, MoveRightThenLeft) {
    Document doc{"hello"};
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_left(doc);
    EXPECT_EQ(doc.position().col, 1u);
}

// -- move_up / move_down ------------------------------------------------------

TEST(EditorCommandsTest, MoveDownAdvancesLine) {
    Document doc{"foo\nbar"};
    cmd::move_down(doc);
    EXPECT_EQ(doc.position().line, 1u);
}

TEST(EditorCommandsTest, MoveUpDecrementsLine) {
    Document doc{"foo\nbar"};
    cmd::move_down(doc);
    cmd::move_up(doc);
    EXPECT_EQ(doc.position().line, 0u);
}

TEST(EditorCommandsTest, MoveDownClampsColToShorterLine) {
    // line 0: "hello" (len 5), line 1: "hi" (len 2)
    Document doc{"hello\nhi"};
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_right(doc);  // col=3
    cmd::move_down(doc);
    EXPECT_EQ(doc.position().col, 1u);  // clamped to len-1=1
}

// -- move_top / move_bottom ---------------------------------------------------

TEST(EditorCommandsTest, MoveTopGoesToOrigin) {
    Document doc{"foo\nbar\nbaz"};
    cmd::move_down(doc);
    cmd::move_right(doc);
    cmd::move_top(doc);
    EXPECT_EQ(doc.position().line, 0u);
    EXPECT_EQ(doc.position().col, 0u);
}

TEST(EditorCommandsTest, MoveBottomGoesToLastLine) {
    Document doc{"foo\nbar\nbaz"};
    cmd::move_bottom(doc);
    EXPECT_EQ(doc.position().line, 2u);
}

// -- move_sol / move_eol ------------------------------------------------------

TEST(EditorCommandsTest, MoveSOLGoesToColZero) {
    Document doc{"hello"};
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_sol(doc);
    EXPECT_EQ(doc.position().col, 0u);
}

TEST(EditorCommandsTest, MoveEOLGoesToLastChar) {
    Document doc{"hello"};
    cmd::move_eol(doc);
    EXPECT_EQ(doc.position().col, 4u);
}

// -- enter_insert / enter_insert_after / enter_normal -------------------------

TEST(EditorCommandsTest, EnterInsertKeepsCursorPosition) {
    Document doc{"hello"};
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::enter_insert(doc);
    EXPECT_EQ(doc.position().col, 2u);
}

TEST(EditorCommandsTest, EnterInsertAfterAdvancesCursor) {
    Document doc{"hello"};
    cmd::move_right(doc);  // col=1
    cmd::enter_insert_after(doc);
    EXPECT_EQ(doc.position().col, 2u);
}

TEST(EditorCommandsTest, EnterInsertAfterAtEOLDoesNotExceedLen) {
    Document doc{"hi"};
    cmd::move_right(doc);  // col=1 (last char)
    cmd::enter_insert_after(doc);
    EXPECT_EQ(doc.position().col, 2u);  // insert mode allows col==len
}

TEST(EditorCommandsTest, EnterNormalClampsColToLastChar) {
    Document doc{"hello"};
    // Manually move cursor to col 5 (insert mode position)
    doc.cursor().set_position({0, 5});
    cmd::enter_normal(doc);
    EXPECT_EQ(doc.position().col, 4u);
}

// -- insert_char --------------------------------------------------------------

TEST(EditorCommandsTest, InsertCharInsertsAndAdvancesCursor) {
    Document doc{"hllo"};
    cmd::move_right(doc);  // col=1
    cmd::enter_insert(doc);
    cmd::insert_char(doc, 'e');
    EXPECT_EQ(doc.line(0).value(), "hello");
    EXPECT_EQ(doc.position().col, 2u);
}

TEST(EditorCommandsTest, InsertCharAtSOL) {
    Document doc{"ello"};
    cmd::enter_insert(doc);
    cmd::insert_char(doc, 'h');
    EXPECT_EQ(doc.line(0).value(), "hello");
    EXPECT_EQ(doc.position().col, 1u);
}

// -- insert_newline -----------------------------------------------------------

TEST(EditorCommandsTest, InsertNewlineSplitsLineAndMovesCursor) {
    Document doc{"helloworld"};
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_right(doc);  // col=5
    cmd::enter_insert(doc);
    cmd::insert_newline(doc);
    EXPECT_EQ(doc.line_count(), 2u);
    EXPECT_EQ(doc.line(0).value(), "hello");
    EXPECT_EQ(doc.line(1).value(), "world");
    EXPECT_EQ(doc.position().line, 1u);
    EXPECT_EQ(doc.position().col, 0u);
}

// -- backspace ----------------------------------------------------------------

TEST(EditorCommandsTest, BackspaceDeletesCharLeft) {
    Document doc{"hello"};
    cmd::move_right(doc);
    cmd::move_right(doc);  // col=2
    cmd::enter_insert(doc);
    cmd::backspace(doc);
    EXPECT_EQ(doc.line(0).value(), "hllo");
    EXPECT_EQ(doc.position().col, 1u);
}

TEST(EditorCommandsTest, BackspaceAtSOLJoinsWithPrevLine) {
    Document doc{"hello\nworld"};
    cmd::move_down(doc);  // line=1, col=0
    cmd::enter_insert(doc);
    cmd::backspace(doc);
    EXPECT_EQ(doc.line_count(), 1u);
    EXPECT_EQ(doc.line(0).value(), "helloworld");
    EXPECT_EQ(doc.position().line, 0u);
    EXPECT_EQ(doc.position().col, 5u);  // end of "hello"
}

TEST(EditorCommandsTest, BackspaceAtOriginDoesNothing) {
    Document doc{"hello"};
    cmd::enter_insert(doc);
    cmd::backspace(doc);
    EXPECT_EQ(doc.line(0).value(), "hello");
    EXPECT_EQ(doc.position().col, 0u);
}

// -- InputDispatcher ----------------------------------------------------------

TEST(InputDispatcherTest, InitialModeIsNormal) {
    InputDispatcher d;
    EXPECT_EQ(d.mode(), EditorMode::Normal);
}

TEST(InputDispatcherTest, IKeyEntersInsertMode) {
    Document doc{"hello"};
    InputDispatcher d;
    d.dispatch(Key::i, doc);
    EXPECT_EQ(d.mode(), EditorMode::Insert);
}

TEST(InputDispatcherTest, EscapeReturnsToNormalMode) {
    Document doc{"hello"};
    InputDispatcher d;
    d.dispatch(Key::i, doc);
    d.dispatch(Key::escape, doc);
    EXPECT_EQ(d.mode(), EditorMode::Normal);
}

TEST(InputDispatcherTest, GGMovesToFirstLine) {
    Document doc{"foo\nbar\nbaz"};
    InputDispatcher d;
    d.dispatch(Key::j, doc);
    d.dispatch(Key::j, doc);  // line=2
    d.dispatch(Key::g, doc);
    d.dispatch(Key::g, doc);  // gg
    EXPECT_EQ(doc.position().line, 0u);
}

TEST(InputDispatcherTest, SingleGDoesNotMove) {
    Document doc{"foo\nbar"};
    InputDispatcher d;
    d.dispatch(Key::j, doc);  // line=1
    d.dispatch(Key::g, doc);  // pending g -- no move yet
    EXPECT_EQ(doc.position().line, 1u);
}

TEST(InputDispatcherTest, GFollowedByNonGClearsPending) {
    Document doc{"foo\nbar\nbaz"};
    InputDispatcher d;
    d.dispatch(Key::j, doc);
    d.dispatch(Key::j, doc);             // line=2
    d.dispatch(Key::g, doc);             // pending g
    d.dispatch(Key::h, doc);             // clears pending, moves left instead
    EXPECT_EQ(doc.position().line, 2u);  // still on line 2
}

TEST(InputDispatcherTest, DispatchCharInsertsInInsertMode) {
    Document doc{"hllo"};
    InputDispatcher d;
    d.dispatch(Key::i, doc);  // insert mode
    d.dispatch(Key::l, doc);  // l is a Key -- but in insert mode routes to insert_key
    // Use dispatch_char for printable chars
    Document doc2{"hllo"};
    InputDispatcher d2;
    d2.dispatch(Key::i, doc2);
    d2.dispatch_char('e', doc2);
    EXPECT_EQ(doc2.line(0).value(), "ehllo");
}

TEST(InputDispatcherTest, DispatchCharIgnoredInNormalMode) {
    Document doc{"hello"};
    InputDispatcher d;
    d.dispatch_char('x', doc);
    EXPECT_EQ(doc.line(0).value(), "hello");
}

TEST(InputDispatcherTest, AKeyEntersInsertAndAdvancesCursor) {
    Document doc{"hello"};
    InputDispatcher d;
    d.dispatch(Key::a, doc);
    EXPECT_EQ(d.mode(), EditorMode::Insert);
    EXPECT_EQ(doc.position().col, 1u);
}
