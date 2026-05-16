#include "drivers/FtxuiRenderer.hpp"

#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

#include "core/entities/Diagnostic.hpp"
#include "core/usecases/EditorMode.hpp"

namespace editor::drivers {

FtxuiRenderer::FtxuiRenderer(const core::Document& doc, const core::usecases::LspService& lsp,
                             const std::string& uri)
    : doc_(doc), lsp_(lsp), uri_(uri) {}

ftxui::Element FtxuiRenderer::render(core::EditorMode mode) const {
    return ftxui::vbox({
        render_buffer() | ftxui::flex,
        render_diagnostics(),
        render_statusbar(mode),
    });
}

ftxui::Element FtxuiRenderer::render_buffer() const {
    const auto& buf = doc_.buffer();
    const auto& cursor = doc_.cursor();
    std::size_t cursor_line = cursor.line();
    std::size_t cursor_col = cursor.col();

    std::vector<ftxui::Element> lines;
    lines.reserve(buf.line_count());

    for (std::size_t i = 0; i < buf.line_count(); ++i) {
        std::string line_text(buf.line(i).value_or(""));

        if (i != cursor_line) {
            lines.push_back(ftxui::text(line_text));
            continue;
        }

        std::string before = line_text.substr(0, cursor_col);
        std::string at =
            cursor_col < line_text.size() ? std::string(1, line_text[cursor_col]) : " ";
        std::string after = cursor_col < line_text.size() ? line_text.substr(cursor_col + 1) : "";

        lines.push_back(ftxui::hbox({
            ftxui::text(before),
            ftxui::text(at) | ftxui::inverted,
            ftxui::text(after),
        }));
    }

    return ftxui::vbox(std::move(lines));
}

ftxui::Element FtxuiRenderer::render_statusbar(core::EditorMode mode) const {
    std::string mode_label = mode == core::EditorMode::Normal ? "NORMAL" : "INSERT";
    return ftxui::hbox({
               ftxui::text(" " + mode_label + " ") | ftxui::inverted,
               ftxui::text("  " + uri_),
           }) |
           ftxui::bold;
}

ftxui::Element FtxuiRenderer::render_diagnostics() const {
    auto diags = lsp_.diagnostics(uri_);
    if (diags.empty()) {
        return ftxui::text("");
    }

    std::vector<ftxui::Element> items;
    for (const auto& d : diags) {
        std::string sev_label = d.severity == core::DiagnosticSeverity::kError     ? "E"
                                : d.severity == core::DiagnosticSeverity::kWarning ? "W"
                                : d.severity == core::DiagnosticSeverity::kHint    ? "H"
                                                                                   : "I";
        std::string label = "[" + sev_label + "] " + std::to_string(d.line + 1) + ":" +
                            std::to_string(d.col + 1) + "  " + d.message;
        auto color =
            d.severity == core::DiagnosticSeverity::kError     ? ftxui::color(ftxui::Color::Red)
            : d.severity == core::DiagnosticSeverity::kWarning ? ftxui::color(ftxui::Color::Yellow)
                                                               : ftxui::color(ftxui::Color::Cyan);
        items.push_back(ftxui::text(label) | color);
    }
    return ftxui::vbox(std::move(items));
}

}  // namespace editor::drivers
