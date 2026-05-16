// Defines FtxuiRenderer -- pure drawing layer.
//
// Stateless: given the current document, LSP service, URI, and mode, produces
// an FTXUI Element. Owns no screen, no event loop, no input state.

#pragma once

#include <ftxui/dom/elements.hpp>
#include <string>

#include "core/entities/Document.hpp"
#include "core/usecases/EditorMode.hpp"
#include "core/usecases/LspService.hpp"

namespace editor::drivers {

class FtxuiRenderer {
public:
    FtxuiRenderer(const core::Document& doc, const core::usecases::LspService& lsp,
                  const std::string& uri);

    // Produces the full terminal frame for the current editor state.
    ftxui::Element render(core::EditorMode mode) const;

private:
    const core::Document& doc_;
    const core::usecases::LspService& lsp_;
    const std::string& uri_;

    ftxui::Element render_buffer() const;
    ftxui::Element render_statusbar(core::EditorMode mode) const;
    ftxui::Element render_diagnostics() const;
};

}  // namespace editor::drivers
