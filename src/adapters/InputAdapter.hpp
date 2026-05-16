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
        if (auto cmd = translate(event, mode_)) {
            if (*cmd == core::Command::quit) return {.quit = true, .mode = mode_};
            mode_ = dispatcher_.dispatch(*cmd, mode_, doc);
        } else if (mode_ == core::EditorMode::Insert && event.is_character()) {
            core::commands::insert_char(doc, event.character()[0]);
        }
        return {.quit = false, .mode = mode_};
    }

private:
    core::EditorMode mode_ = core::EditorMode::Normal;
    core::InputDispatcher dispatcher_;
};

}  // namespace editor::adapters
