// Defines InputDispatcher -- routes Key events to EditorCommands based on current mode.

#pragma once

#include "core/entities/Document.hpp"
#include "core/interfaces/Key.hpp"
#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"

namespace editor::core {

// Routes Key events to EditorCommands. Holds modal state and the pending-g flag
// needed to detect the gg motion (two consecutive g presses).
class InputDispatcher {
public:
    explicit InputDispatcher(EditorMode initial_mode = EditorMode::Normal) : mode_{initial_mode} {}

    [[nodiscard]] EditorMode mode() const noexcept { return mode_; }

    EditorMode dispatch(Key key, Document& doc) {
        if (mode_ == EditorMode::Normal) {
            dispatch_normal(key, doc);
        } else {
            dispatch_insert(key, doc);
        }
        return mode_;
    }

    // Handles raw printable characters in insert mode (ignored in normal mode).
    EditorMode dispatch_char(char ch, Document& doc) {
        if (mode_ == EditorMode::Insert) {
            commands::insert_char(doc, ch);
        }
        return mode_;
    }

private:
    EditorMode mode_;
    bool pending_g_ = false;

    void dispatch_normal(Key key, Document& doc) {
        if (pending_g_) {
            pending_g_ = false;
            if (key == Key::g) {
                commands::move_top(doc);
                return;
            }
        }

        switch (key) {
            case Key::h:
                commands::move_left(doc);
                break;
            case Key::l:
                commands::move_right(doc);
                break;
            case Key::j:
                commands::move_down(doc);
                break;
            case Key::k:
                commands::move_up(doc);
                break;
            case Key::g:
                pending_g_ = true;
                break;
            case Key::G:
                commands::move_bottom(doc);
                break;
            case Key::zero:
                commands::move_sol(doc);
                break;
            case Key::dollar:
                commands::move_eol(doc);
                break;
            case Key::i:
                commands::enter_insert(doc);
                mode_ = EditorMode::Insert;
                break;
            case Key::a:
                commands::enter_insert_after(doc);
                mode_ = EditorMode::Insert;
                break;
            default:
                break;
        }
    }

    void dispatch_insert(Key key, Document& doc) {
        switch (key) {
            case Key::escape:
                commands::enter_normal(doc);
                mode_ = EditorMode::Normal;
                break;
            case Key::enter:
                commands::insert_newline(doc);
                break;
            case Key::backspace:
                commands::backspace(doc);
                break;
            default:
                break;
        }
    }
};

}  // namespace editor::core
