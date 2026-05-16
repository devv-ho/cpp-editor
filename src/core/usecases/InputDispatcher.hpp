#pragma once

#include "core/entities/Document.hpp"
#include "core/interfaces/Command.hpp"
#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"

namespace editor::core {

// Executes a Command against a Document given the current mode.
// Stateless except for pending_g (Normal-mode gg sequence).
// Returns the resulting EditorMode after execution.
class InputDispatcher {
public:
    EditorMode dispatch(Command cmd, EditorMode mode, Document& doc) {
        if (mode == EditorMode::Normal) {
            return dispatch_normal(cmd, doc);
        } else {
            return dispatch_insert(cmd, doc);
        }
    }

private:
    bool pending_g_ = false;

    EditorMode dispatch_normal(Command cmd, Document& doc) {
        if (pending_g_) {
            pending_g_ = false;
            if (cmd == Command::pending_g) {
                commands::move_top(doc);
                return EditorMode::Normal;
            }
        }

        switch (cmd) {
            case Command::move_left:
                commands::move_left(doc);
                break;
            case Command::move_right:
                commands::move_right(doc);
                break;
            case Command::move_down:
                commands::move_down(doc);
                break;
            case Command::move_up:
                commands::move_up(doc);
                break;
            case Command::move_bottom:
                commands::move_bottom(doc);
                break;
            case Command::move_sol:
                commands::move_sol(doc);
                break;
            case Command::move_eol:
                commands::move_eol(doc);
                break;
            case Command::pending_g:
                pending_g_ = true;
                break;
            case Command::enter_insert:
                commands::enter_insert(doc);
                return EditorMode::Insert;
            case Command::enter_insert_after:
                commands::enter_insert_after(doc);
                return EditorMode::Insert;
            case Command::quit:
                break;
            default:
                break;
        }
        return EditorMode::Normal;
    }

    EditorMode dispatch_insert(Command cmd, Document& doc) {
        switch (cmd) {
            case Command::enter_normal:
                commands::enter_normal(doc);
                return EditorMode::Normal;
            case Command::insert_newline:
                commands::insert_newline(doc);
                break;
            case Command::backspace:
                commands::backspace(doc);
                break;
            default:
                break;
        }
        return EditorMode::Insert;
    }
};

}  // namespace editor::core
