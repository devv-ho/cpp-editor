// Unit tests for InputTranslator.

#include <gtest/gtest.h>

#include "adapters/InputTranslator.hpp"

using editor::adapters::translate;
using editor::core::Command;
using editor::core::EditorMode;

// -- Normal mode --------------------------------------------------------------

TEST(InputTranslatorTest, Normal_H) {
    EXPECT_EQ(translate(ftxui::Event::Character('h'), EditorMode::Normal), Command::move_left);
}

TEST(InputTranslatorTest, Normal_J) {
    EXPECT_EQ(translate(ftxui::Event::Character('j'), EditorMode::Normal), Command::move_down);
}

TEST(InputTranslatorTest, Normal_K) {
    EXPECT_EQ(translate(ftxui::Event::Character('k'), EditorMode::Normal), Command::move_up);
}

TEST(InputTranslatorTest, Normal_L) {
    EXPECT_EQ(translate(ftxui::Event::Character('l'), EditorMode::Normal), Command::move_right);
}

TEST(InputTranslatorTest, Normal_GLower) {
    EXPECT_EQ(translate(ftxui::Event::Character('g'), EditorMode::Normal), Command::pending_g);
}

TEST(InputTranslatorTest, Normal_GUpper) {
    EXPECT_EQ(translate(ftxui::Event::Character('G'), EditorMode::Normal), Command::move_bottom);
}

TEST(InputTranslatorTest, Normal_Dollar) {
    EXPECT_EQ(translate(ftxui::Event::Character('$'), EditorMode::Normal), Command::move_eol);
}

TEST(InputTranslatorTest, Normal_Zero) {
    EXPECT_EQ(translate(ftxui::Event::Character('0'), EditorMode::Normal), Command::move_sol);
}

TEST(InputTranslatorTest, Normal_I) {
    EXPECT_EQ(translate(ftxui::Event::Character('i'), EditorMode::Normal), Command::enter_insert);
}

TEST(InputTranslatorTest, Normal_A) {
    EXPECT_EQ(translate(ftxui::Event::Character('a'), EditorMode::Normal),
              Command::enter_insert_after);
}

TEST(InputTranslatorTest, Normal_EscapeIsQuit) {
    EXPECT_EQ(translate(ftxui::Event::Escape, EditorMode::Normal), Command::quit);
}

TEST(InputTranslatorTest, Normal_Backspace) {
    EXPECT_EQ(translate(ftxui::Event::Backspace, EditorMode::Normal), Command::backspace);
}

TEST(InputTranslatorTest, Normal_Return) {
    EXPECT_EQ(translate(ftxui::Event::Return, EditorMode::Normal), Command::insert_newline);
}

TEST(InputTranslatorTest, Normal_UnmappedCharReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::Character('z'), EditorMode::Normal), std::nullopt);
}

TEST(InputTranslatorTest, Normal_ArrowKeyReturnsNullopt) {
    EXPECT_EQ(translate(ftxui::Event::ArrowUp, EditorMode::Normal), std::nullopt);
}

// -- Insert mode --------------------------------------------------------------

TEST(InputTranslatorTest, Insert_EscapeIsEnterNormal) {
    EXPECT_EQ(translate(ftxui::Event::Escape, EditorMode::Insert), Command::enter_normal);
}

TEST(InputTranslatorTest, Insert_Backspace) {
    EXPECT_EQ(translate(ftxui::Event::Backspace, EditorMode::Insert), Command::backspace);
}

TEST(InputTranslatorTest, Insert_Return) {
    EXPECT_EQ(translate(ftxui::Event::Return, EditorMode::Insert), Command::insert_newline);
}

TEST(InputTranslatorTest, Insert_TypingCharsReturnInsertChar) {
    EXPECT_EQ(translate(ftxui::Event::Character('a'), EditorMode::Insert), Command::insert_char);
    EXPECT_EQ(translate(ftxui::Event::Character('h'), EditorMode::Insert), Command::insert_char);
    EXPECT_EQ(translate(ftxui::Event::Character('l'), EditorMode::Insert), Command::insert_char);
    EXPECT_EQ(translate(ftxui::Event::Character('z'), EditorMode::Insert), Command::insert_char);
    EXPECT_EQ(translate(ftxui::Event::Character('i'), EditorMode::Insert), Command::insert_char);
}

// -- Normal mode: LSP and multi-key prefix keys --------------------------------

TEST(InputTranslatorTest, Normal_KUpperIslspHover) {
    EXPECT_EQ(translate(ftxui::Event::Character('K'), EditorMode::Normal), Command::lsp_hover);
}

TEST(InputTranslatorTest, Normal_SpaceIsPendingSpace) {
    EXPECT_EQ(translate(ftxui::Event::Character(' '), EditorMode::Normal), Command::pending_space);
}

// Second keys for g* sequences (resolved by dispatcher in AfterG state).
TEST(InputTranslatorTest, Normal_D_IsLspDefinition) {
    EXPECT_EQ(translate(ftxui::Event::Character('d'), EditorMode::Normal), Command::lsp_definition);
}

TEST(InputTranslatorTest, Normal_DUpper_IsLspDeclaration) {
    EXPECT_EQ(translate(ftxui::Event::Character('D'), EditorMode::Normal),
              Command::lsp_declaration);
}

TEST(InputTranslatorTest, Normal_IUpper_IsLspImplementation) {
    // 'I' (uppercase) maps to lsp_implementation to avoid collision with 'i'=enter_insert.
    EXPECT_EQ(translate(ftxui::Event::Character('I'), EditorMode::Normal),
              Command::lsp_implementation);
}

TEST(InputTranslatorTest, Normal_ILower_IsEnterInsert_NotLspImplementation) {
    // 'i' (lowercase) must NOT map to lsp_implementation.
    EXPECT_NE(translate(ftxui::Event::Character('i'), EditorMode::Normal),
              Command::lsp_implementation);
    EXPECT_EQ(translate(ftxui::Event::Character('i'), EditorMode::Normal), Command::enter_insert);
}

TEST(InputTranslatorTest, Normal_Y_IsLspTypeDefinition) {
    EXPECT_EQ(translate(ftxui::Event::Character('y'), EditorMode::Normal),
              Command::lsp_type_definition);
}

TEST(InputTranslatorTest, Normal_R_IsLspReferences) {
    EXPECT_EQ(translate(ftxui::Event::Character('r'), EditorMode::Normal), Command::lsp_references);
}

TEST(InputTranslatorTest, Normal_N_IsLspRename) {
    EXPECT_EQ(translate(ftxui::Event::Character('n'), EditorMode::Normal), Command::lsp_rename);
}

TEST(InputTranslatorTest, Normal_C_IsLspCodeAction) {
    EXPECT_EQ(translate(ftxui::Event::Character('c'), EditorMode::Normal),
              Command::lsp_code_action);
}

TEST(InputTranslatorTest, Normal_AUpper_IsLspCodeAction) {
    // 'A' (uppercase) is the confirm key for <Space>cA; must map to lsp_code_action.
    EXPECT_EQ(translate(ftxui::Event::Character('A'), EditorMode::Normal),
              Command::lsp_code_action);
}

TEST(InputTranslatorTest, Normal_ALower_IsEnterInsertAfter_NotCodeAction) {
    // 'a' (lowercase) must NOT map to lsp_code_action.
    EXPECT_NE(translate(ftxui::Event::Character('a'), EditorMode::Normal),
              Command::lsp_code_action);
    EXPECT_EQ(translate(ftxui::Event::Character('a'), EditorMode::Normal),
              Command::enter_insert_after);
}

TEST(InputTranslatorTest, Normal_F_IsLspFormatting) {
    EXPECT_EQ(translate(ftxui::Event::Character('f'), EditorMode::Normal), Command::lsp_formatting);
}

TEST(InputTranslatorTest, Normal_SLower_IsLspDocumentSymbol) {
    EXPECT_EQ(translate(ftxui::Event::Character('s'), EditorMode::Normal),
              Command::lsp_document_symbol);
}

TEST(InputTranslatorTest, Normal_SUpper_IsLspWorkspaceSymbol) {
    EXPECT_EQ(translate(ftxui::Event::Character('S'), EditorMode::Normal),
              Command::lsp_workspace_symbol);
}
