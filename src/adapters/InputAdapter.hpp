#pragma once

#include <ftxui/component/event.hpp>

#include "adapters/InputTranslator.hpp"
#include "core/entities/Document.hpp"
#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/InputDispatcher.hpp"

namespace editor::adapters {

struct DispatchResult {
    bool quit = false;
    core::EditorMode mode = core::EditorMode::Normal;
};

class InputAdapter {
public:
    DispatchResult process(const ftxui::Event& event, core::Document& doc) {
        if (mode_ == core::EditorMode::Normal) {
            if (auto cmd = translate(event)) {
                // ESC in normal mode = quit.
                if (*cmd == core::Command::enter_normal) {
                    return {.quit = true, .mode = mode_};
                }
                mode_ = dispatcher_.dispatch(*cmd, mode_, doc);
            }
            // Unmapped events in normal mode are ignored.
        } else {
            // Insert mode: only ESC/Enter/Backspace are commands.
            // Everything else -- including keys that map to Normal commands -- is
            // treated as a character to insert.
            if (auto cmd = translate(event); cmd && is_insert_command(*cmd)) {
                mode_ = dispatcher_.dispatch(*cmd, mode_, doc);
            } else if (event.is_character()) {
                core::commands::insert_char(doc, event.character()[0]);
            }
        }
        return {.quit = false, .mode = mode_};
    }

private:
    core::EditorMode mode_ = core::EditorMode::Normal;
    core::InputDispatcher dispatcher_;

    static bool is_insert_command(core::Command cmd) {
        return cmd == core::Command::enter_normal || cmd == core::Command::insert_newline ||
               cmd == core::Command::backspace;
    }
};

}  // namespace editor::adapters
