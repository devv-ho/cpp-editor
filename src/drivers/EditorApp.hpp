// Defines EditorApp -- the FTXUI event loop.
//
// Owns the screen, InputAdapter, and FtxuiRenderer. run() is a plain loop:
// read event -> process -> check quit -> render. No key or mode logic here.

#pragma once

#include <ftxui/component/screen_interactive.hpp>
#include <string>

#include "adapters/InputAdapter.hpp"
#include "core/entities/Document.hpp"
#include "core/usecases/LspService.hpp"
#include "drivers/FtxuiRenderer.hpp"

namespace editor::drivers {

class EditorApp {
public:
    EditorApp(core::Document& doc, core::usecases::LspService& lsp, std::string uri);

    void run();

private:
    core::Document& doc_;
    core::usecases::LspService& lsp_;
    std::string uri_;
    adapters::InputAdapter input_;
    FtxuiRenderer renderer_;
    ftxui::ScreenInteractive screen_;
};

}  // namespace editor::drivers
