#pragma once

#include "core/entities/Document.hpp"
#include "core/interfaces/Command.hpp"
#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/LspService.hpp"

namespace editor::core {

// Executes a Command against a Document given the current mode.
// Holds pending multi-key state for g* and <Space>* sequences.
// LSP commands are dispatched asynchronously via LspService callbacks.
//
// Key sequence state machine (Normal mode):
//   idle  --'g'-->  after_g   --'g'-->  move_top
//                             --'d'-->  lsp_definition
//                             --'D'-->  lsp_declaration
//                             --'i'-->  lsp_implementation
//                             --'y'-->  lsp_type_definition
//                             --'r'-->  lsp_references
//                             --else--> idle
//   idle  --' '---> after_spc --'f'-->  lsp_formatting
//                             --'s'-->  lsp_document_symbol
//                             --'S'-->  lsp_workspace_symbol
//                             --'r'-->  after_spc_r --'n'--> lsp_rename
//                             --'c'-->  after_spc_c --'a'--> lsp_code_action
//                             --else--> idle
class InputDispatcher {
public:
    InputDispatcher(usecases::LspService& lsp, std::string uri) : lsp_(lsp), uri_(std::move(uri)) {}

    EditorMode dispatch(Command cmd, EditorMode mode, Document& doc, char ch = '\0') {
        if (mode == EditorMode::Normal) return dispatch_normal(cmd, doc);
        return dispatch_insert(cmd, doc, ch);
    }

private:
    usecases::LspService& lsp_;
    std::string uri_;

    enum class PendingState { None, AfterG, AfterSpace, AfterSpaceR, AfterSpaceC };
    PendingState pending_ = PendingState::None;
    int doc_version_ = 1;

    void reset_pending() { pending_ = PendingState::None; }

    EditorMode dispatch_normal(Command cmd, Document& doc) {
        auto pos = doc.cursor().position();

        // ── Resolve multi-key sequences ───────────────────────────────────────
        switch (pending_) {
            case PendingState::AfterG:
                reset_pending();
                switch (cmd) {
                    case Command::pending_g:
                        commands::move_top(doc);
                        return EditorMode::Normal;
                    case Command::lsp_definition:
                        lsp_.go_to_definition(uri_, pos.line, pos.col, [](auto) {});
                        return EditorMode::Normal;
                    case Command::lsp_declaration:
                        lsp_.go_to_declaration(uri_, pos.line, pos.col, [](auto) {});
                        return EditorMode::Normal;
                    case Command::lsp_implementation:
                        lsp_.go_to_implementation(uri_, pos.line, pos.col, [](auto) {});
                        return EditorMode::Normal;
                    case Command::lsp_type_definition:
                        lsp_.go_to_type_definition(uri_, pos.line, pos.col, [](auto) {});
                        return EditorMode::Normal;
                    case Command::lsp_references:
                        lsp_.find_references(uri_, pos.line, pos.col, [](auto) {});
                        return EditorMode::Normal;
                    default:
                        return EditorMode::Normal;  // unrecognised: swallow
                }

            case PendingState::AfterSpace:
                reset_pending();
                switch (cmd) {
                    case Command::lsp_formatting:
                        lsp_.formatting(uri_, [](const nlohmann::json&) {});
                        return EditorMode::Normal;
                    case Command::lsp_document_symbol:
                        lsp_.document_symbol(uri_, [](auto) {});
                        return EditorMode::Normal;
                    case Command::lsp_workspace_symbol:
                        lsp_.workspace_symbol("", [](auto) {});
                        return EditorMode::Normal;
                    case Command::lsp_references:  // 'r' -> wait for 'n'
                        pending_ = PendingState::AfterSpaceR;
                        return EditorMode::Normal;
                    case Command::lsp_code_action:  // 'c' -> wait for 'a'
                        pending_ = PendingState::AfterSpaceC;
                        return EditorMode::Normal;
                    default:
                        return EditorMode::Normal;
                }

            case PendingState::AfterSpaceR:
                reset_pending();
                if (cmd == Command::lsp_rename) {
                    // new_name will be provided by the UI layer once a prompt is wired up.
                    lsp_.rename(uri_, pos.line, pos.col, "", [](const nlohmann::json&) {});
                }
                return EditorMode::Normal;

            case PendingState::AfterSpaceC:
                reset_pending();
                if (cmd == Command::lsp_code_action) {
                    lsp_.code_action(uri_, pos.line, pos.col, [](auto) {});
                }
                return EditorMode::Normal;

            case PendingState::None:
                break;
        }

        // ── Single-key Normal commands ────────────────────────────────────────
        switch (cmd) {
            case Command::move_left:
                commands::move_left(doc);
                lsp_.clear_overlay();
                break;
            case Command::move_right:
                commands::move_right(doc);
                lsp_.clear_overlay();
                break;
            case Command::move_down:
                commands::move_down(doc);
                lsp_.clear_overlay();
                break;
            case Command::move_up:
                commands::move_up(doc);
                lsp_.clear_overlay();
                break;
            case Command::move_bottom:
                commands::move_bottom(doc);
                lsp_.clear_overlay();
                break;
            case Command::move_sol:
                commands::move_sol(doc);
                lsp_.clear_overlay();
                break;
            case Command::move_eol:
                commands::move_eol(doc);
                lsp_.clear_overlay();
                break;
            case Command::pending_g:
                pending_ = PendingState::AfterG;
                break;
            case Command::pending_space:
                pending_ = PendingState::AfterSpace;
                break;
            case Command::enter_insert:
                commands::enter_insert(doc);
                return EditorMode::Insert;
            case Command::enter_insert_after:
                commands::enter_insert_after(doc);
                return EditorMode::Insert;
            case Command::lsp_hover:
                lsp_.hover(uri_, pos.line, pos.col, [](auto) {});
                break;
            case Command::quit:
                break;
            default:
                break;
        }
        return EditorMode::Normal;
    }

    EditorMode dispatch_insert(Command cmd, Document& doc, char ch) {
        switch (cmd) {
            case Command::enter_normal:
                commands::enter_normal(doc);
                return EditorMode::Normal;
            case Command::insert_char:
                commands::insert_char(doc, ch);
                did_change(doc);
                break;
            case Command::insert_newline:
                commands::insert_newline(doc);
                did_change(doc);
                break;
            case Command::backspace:
                commands::backspace(doc);
                did_change(doc);
                break;
            default:
                break;
        }
        return EditorMode::Insert;
    }

    void did_change(Document& doc) {
        lsp_.did_change(uri_, doc.buffer().to_string(), ++doc_version_);
    }
};

}  // namespace editor::core
