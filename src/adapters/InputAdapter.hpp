#pragma once

#include <ftxui/component/event.hpp>
#include <string>

#include "adapters/InputTranslator.hpp"
#include "core/entities/Document.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/InputDispatcher.hpp"
#include "core/usecases/LspService.hpp"

namespace editor::adapters {

struct DispatchResult {
    bool quit = false;
    core::EditorMode mode = core::EditorMode::Normal;
};

class InputAdapter {
public:
    InputAdapter(core::usecases::LspService& lsp, std::string uri)
        : dispatcher_(lsp, std::move(uri)) {}

    DispatchResult process(const ftxui::Event& event, core::Document& doc) {
        if (auto cmd = translate(event, mode_)) {
            if (*cmd == core::Command::quit) return {.quit = true, .mode = mode_};
            char ch = event.is_character() ? event.character()[0] : '\0';
            mode_ = dispatcher_.dispatch(*cmd, mode_, doc, ch);
        }
        return {.quit = false, .mode = mode_};
    }

private:
    core::EditorMode mode_ = core::EditorMode::Normal;
    core::InputDispatcher dispatcher_;
};

}  // namespace editor::adapters
