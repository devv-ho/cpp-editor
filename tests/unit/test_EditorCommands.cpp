#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "adapters/lsp/ClangdProcess.hpp"
#include "core/entities/Document.hpp"
#include "core/entities/EditorConfig.hpp"
#include "core/interfaces/Command.hpp"
#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/InputDispatcher.hpp"
#include "core/usecases/LspService.hpp"

using editor::core::Command;
using editor::core::Document;
using editor::core::EditorConfig;
using editor::core::EditorMode;
using editor::core::InputDispatcher;
using editor::core::usecases::LspService;
namespace cmd = editor::core::commands;

// Null ClangdProcess: never spawns a subprocess; receive() returns nullopt immediately.
class NullClangdProcess : public editor::adapters::lsp::ClangdProcess {
public:
    NullClangdProcess() : ClangdProcess("") {}
};

// Builds a LspService backed by a NullClangdProcess for unit tests that need
// an InputDispatcher but don't exercise LSP functionality.
struct TestLsp {
    std::unique_ptr<LspService> service;
    TestLsp() {
        // ClangdProcess("") will fail execvp but that's fine: the dispatch
        // thread will exit immediately once receive() returns nullopt.
        EditorConfig cfg;
        cfg.lsp = {};  // all features enabled; won't actually send anything
        service = std::make_unique<LspService>(
            std::make_unique<editor::adapters::lsp::ClangdProcess>("false"), cfg);
    }
    InputDispatcher make_dispatcher() { return InputDispatcher(*service, "file:///test.cpp"); }
};

// -- move_left / move_right ---------------------------------------------------

TEST(EditorCommandsTest, MoveRightClampsAtLastChar) {
    Document doc{"hello"};
    for (int i = 0; i < 10; ++i) cmd::move_right(doc);
    EXPECT_EQ(doc.position().col, 4u);
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
    Document doc{"hello\nhi"};
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_right(doc);
    cmd::move_down(doc);
    EXPECT_EQ(doc.position().col, 1u);
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
    cmd::move_right(doc);
    cmd::enter_insert_after(doc);
    EXPECT_EQ(doc.position().col, 2u);
}

TEST(EditorCommandsTest, EnterInsertAfterAtEOLDoesNotExceedLen) {
    Document doc{"hi"};
    cmd::move_right(doc);
    cmd::enter_insert_after(doc);
    EXPECT_EQ(doc.position().col, 2u);
}

TEST(EditorCommandsTest, EnterNormalClampsColToLastChar) {
    Document doc{"hello"};
    doc.cursor().set_position({0, 5});
    cmd::enter_normal(doc);
    EXPECT_EQ(doc.position().col, 4u);
}

// -- insert_char --------------------------------------------------------------

TEST(EditorCommandsTest, InsertCharInsertsAndAdvancesCursor) {
    Document doc{"hllo"};
    cmd::move_right(doc);
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
    for (int i = 0; i < 5; ++i) cmd::move_right(doc);
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
    cmd::move_right(doc);
    cmd::enter_insert(doc);
    cmd::backspace(doc);
    EXPECT_EQ(doc.line(0).value(), "hllo");
    EXPECT_EQ(doc.position().col, 1u);
}

TEST(EditorCommandsTest, BackspaceAtSOLJoinsWithPrevLine) {
    Document doc{"hello\nworld"};
    cmd::move_down(doc);
    cmd::enter_insert(doc);
    cmd::backspace(doc);
    EXPECT_EQ(doc.line_count(), 1u);
    EXPECT_EQ(doc.line(0).value(), "helloworld");
    EXPECT_EQ(doc.position().line, 0u);
    EXPECT_EQ(doc.position().col, 5u);
}

TEST(EditorCommandsTest, BackspaceAtOriginDoesNothing) {
    Document doc{"hello"};
    cmd::enter_insert(doc);
    cmd::backspace(doc);
    EXPECT_EQ(doc.line(0).value(), "hello");
    EXPECT_EQ(doc.position().col, 0u);
}

// -- InputDispatcher ----------------------------------------------------------

TEST(InputDispatcherTest, EnterInsertModeFromNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    auto mode = d.dispatch(Command::enter_insert, EditorMode::Normal, doc);
    EXPECT_EQ(mode, EditorMode::Insert);
}

TEST(InputDispatcherTest, EscapeFromInsertReturnsNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    auto mode = d.dispatch(Command::enter_normal, EditorMode::Insert, doc);
    EXPECT_EQ(mode, EditorMode::Normal);
}

TEST(InputDispatcherTest, MoveRightInNormalMode) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::move_right, EditorMode::Normal, doc);
    EXPECT_EQ(doc.position().col, 1u);
}

TEST(InputDispatcherTest, GGMovesToFirstLine) {
    Document doc{"foo\nbar\nbaz"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::move_down, EditorMode::Normal, doc);
    d.dispatch(Command::move_down, EditorMode::Normal, doc);
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_EQ(doc.position().line, 0u);
}

TEST(InputDispatcherTest, SingleGDoesNotMove) {
    Document doc{"foo\nbar"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::move_down, EditorMode::Normal, doc);
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_EQ(doc.position().line, 1u);
}

TEST(InputDispatcherTest, GFollowedByNonGClearsPending) {
    Document doc{"foo\nbar\nbaz"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::move_down, EditorMode::Normal, doc);
    d.dispatch(Command::move_down, EditorMode::Normal, doc);
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    d.dispatch(Command::move_left, EditorMode::Normal, doc);
    EXPECT_EQ(doc.position().line, 2u);
}

TEST(InputDispatcherTest, AKeyEntersInsertAndAdvancesCursor) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    auto mode = d.dispatch(Command::enter_insert_after, EditorMode::Normal, doc);
    EXPECT_EQ(mode, EditorMode::Insert);
    EXPECT_EQ(doc.position().col, 1u);
}

TEST(InputDispatcherTest, InsertNewlineInInsertMode) {
    Document doc{"helloworld"};
    for (int i = 0; i < 5; ++i) cmd::move_right(doc);
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::insert_newline, EditorMode::Insert, doc);
    EXPECT_EQ(doc.line_count(), 2u);
}

// -- LSP key binding dispatch -------------------------------------------------

TEST(InputDispatcherTest, GDDispatchesGoToDefinition) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_definition, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, GCapitalDDispatchesGoToDeclaration) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_declaration, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, GIDispatchesGoToImplementation) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_implementation, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, GYDispatchesGoToTypeDefinition) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_type_definition, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, GRDispatchesFindReferences) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_g, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_references, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, KDispatchesHover) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    EXPECT_NO_THROW(d.dispatch(Command::lsp_hover, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, SpaceFDispatchesFormatting) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_space, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_formatting, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, SpaceSDispatchesDocumentSymbol) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_space, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_document_symbol, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, SpaceCapitalSDispatchesWorkspaceSymbol) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_space, EditorMode::Normal, doc);
    EXPECT_NO_THROW(d.dispatch(Command::lsp_workspace_symbol, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, SpaceRNDispatchesRename) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_space, EditorMode::Normal, doc);
    d.dispatch(Command::lsp_references, EditorMode::Normal, doc);  // 'r' -> AfterSpaceR
    EXPECT_NO_THROW(d.dispatch(Command::lsp_rename, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, SpaceCADispatchesCodeAction) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::pending_space, EditorMode::Normal, doc);
    d.dispatch(Command::lsp_code_action, EditorMode::Normal, doc);  // 'c' -> AfterSpaceC
    EXPECT_NO_THROW(d.dispatch(Command::lsp_code_action, EditorMode::Normal, doc));
}

TEST(InputDispatcherTest, MotionClearsOverlay) {
    Document doc{"hello"};
    TestLsp lsp_;
    lsp_.service->set_hover("some hover");
    lsp_.service->set_signature("sig");
    lsp_.service->set_locations({{"file:///a.cpp", 0, 0}});
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::move_right, EditorMode::Normal, doc);
    EXPECT_TRUE(lsp_.service->hover_text().empty());
    EXPECT_TRUE(lsp_.service->signature_text().empty());
    EXPECT_TRUE(lsp_.service->locations().empty());
}

TEST(InputDispatcherTest, InsertCharCallsDidChange) {
    Document doc{"hello"};
    TestLsp lsp_;
    auto d = lsp_.make_dispatcher();
    d.dispatch(Command::enter_insert, EditorMode::Normal, doc);
    // did_change with dead process must not crash
    EXPECT_NO_THROW(d.dispatch(Command::insert_char, EditorMode::Insert, doc, 'X'));
    EXPECT_EQ(doc.line(0).value(), "Xhello");
}

// -- InputAdapter integration: real key sequences through translate+dispatch ---
// These tests go through InputAdapter::process() which calls translate() then
// dispatch(), catching bugs where translate()'s Command output doesn't match
// what the dispatcher's pending-state handler expects.

#include "adapters/InputAdapter.hpp"
using editor::adapters::InputAdapter;

// Helper: feed a sequence of character events through InputAdapter.
static editor::core::EditorMode send_keys(InputAdapter& adapter, editor::core::Document& doc,
                                          std::string_view keys) {
    editor::core::EditorMode mode = editor::core::EditorMode::Normal;
    for (char ch : keys) {
        auto result = adapter.process(ftxui::Event::Character(std::string(1, ch)), doc);
        mode = result.mode;
    }
    return mode;
}

TEST(InputAdapterIntegrationTest, LowerI_EntersInsertMode) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    auto mode = send_keys(adapter, doc, "i");
    EXPECT_EQ(mode, EditorMode::Insert);
}

TEST(InputAdapterIntegrationTest, LowerA_EntersInsertModeAndAdvancesCursor) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    auto mode = send_keys(adapter, doc, "a");
    EXPECT_EQ(mode, EditorMode::Insert);
    EXPECT_EQ(doc.position().col, 1u);
}

TEST(InputAdapterIntegrationTest, GI_DoesNotEnterInsertMode) {
    // 'g' then 'I' (uppercase) → gI → lsp_implementation, stays Normal.
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    auto mode = send_keys(adapter, doc, "gI");
    EXPECT_EQ(mode, EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, GI_vs_Gi_Differ) {
    // 'gi' (lowercase i after g) — 'i' is enter_insert, swallowed in AfterG → stays Normal.
    // 'gI' (uppercase I after g) — lsp_implementation → stays Normal.
    // Both stay Normal but for different reasons; key point: neither enters Insert mode.
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter a1(*lsp_.service, "file:///test.cpp");
    InputAdapter a2(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(a1, doc, "gi"), EditorMode::Normal);
    EXPECT_EQ(send_keys(a2, doc, "gI"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, SpaceCUpperA_DoesNotEnterInsertMode) {
    // <Space>cA (uppercase A) → lsp_code_action confirm, stays Normal.
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    auto mode = send_keys(adapter, doc, " cA");
    EXPECT_EQ(mode, EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, SpaceCLowerA_DoesNotEnterInsertMode) {
    // <Space>ca (lowercase a after <Space>c) — 'a' is enter_insert_after which is
    // swallowed in AfterSpaceC state → stays Normal (code action not fired, but
    // crucially does NOT switch to Insert mode).
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    auto mode = send_keys(adapter, doc, " ca");
    EXPECT_EQ(mode, EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, GD_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, "gd"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, GCapitalD_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, "gD"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, GY_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, "gy"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, GR_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, "gr"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, SpaceF_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, " f"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, SpaceS_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, " s"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, SpaceCapitalS_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, " S"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, SpaceRN_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, " rn"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, K_StaysNormal) {
    Document doc{"hello"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    EXPECT_EQ(send_keys(adapter, doc, "K"), EditorMode::Normal);
}

TEST(InputAdapterIntegrationTest, GG_MovesToTop) {
    Document doc{"foo\nbar\nbaz"};
    TestLsp lsp_;
    InputAdapter adapter(*lsp_.service, "file:///test.cpp");
    send_keys(adapter, doc, "jj");  // move down twice
    EXPECT_EQ(doc.position().line, 2u);
    send_keys(adapter, doc, "gg");
    EXPECT_EQ(doc.position().line, 0u);
}
