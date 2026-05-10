// Defines FtxuiRenderer -- FTXUI-based terminal renderer for the editor.
//
// Owns the FTXUI screen loop. Renders buffer lines, cursor, mode status bar,
// and LSP diagnostics. Routes keyboard events through InputDispatcher.
// ESC in normal mode quits.

#pragma once

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>

#include "core/entities/Document.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/InputDispatcher.hpp"
#include "core/usecases/LspService.hpp"

namespace editor::drivers {

class FtxuiRenderer {
public:
    FtxuiRenderer(core::Document& doc, core::usecases::LspService& lsp, std::string uri);

    // Enters the FTXUI event loop. Blocks until the user quits.
    void run();

private:
    core::Document& doc_;
    core::usecases::LspService& lsp_;
    std::string uri_;
    core::InputDispatcher dispatcher_;
    ftxui::ScreenInteractive screen_;

    ftxui::Element render_buffer() const;
    ftxui::Element render_statusbar() const;
    ftxui::Element render_diagnostics() const;
};

}  // namespace editor::drivers
