#include "drivers/FtxuiRenderer.hpp"

#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

#include "core/entities/Diagnostic.hpp"
#include "core/usecases/EditorMode.hpp"
#include "drivers/ScreenFormat.hpp"

namespace editor::drivers {

FtxuiRenderer::FtxuiRenderer(const core::Document& doc, const core::usecases::LspService& lsp,
                             const std::string& uri)
    : doc_(doc), lsp_(lsp), uri_(uri) {}

ftxui::Element FtxuiRenderer::render(core::EditorMode mode) const {
    return ftxui::vbox({
        render_buffer() | ftxui::flex,
        render_inlay_hints(),
        render_diagnostics(),
        render_hover(),
        render_signature(),
        render_completion(),
        render_locations(),
        render_symbols(),
        render_statusbar(mode),
    });
}

ftxui::Element FtxuiRenderer::render_buffer() const {
    const auto& buf = doc_.buffer();
    const auto& cursor = doc_.cursor();
    std::size_t cursor_line = cursor.line();
    std::size_t cursor_col = cursor.col();

    auto highlights = lsp_.highlights();

    std::vector<ftxui::Element> lines;
    lines.reserve(buf.line_count());

    for (std::size_t i = 0; i < buf.line_count(); ++i) {
        std::string line_text(buf.line(i).value_or(""));

        // Check if this line has a document highlight.
        bool is_highlighted = false;
        for (const auto& h : highlights) {
            if (h.line == i) {
                is_highlighted = true;
                break;
            }
        }

        if (i != cursor_line) {
            auto elem = ftxui::text(line_text);
            if (is_highlighted) elem = elem | ftxui::bgcolor(ftxui::Color::GrayDark);
            lines.push_back(elem);
            continue;
        }

        std::string before = line_text.substr(0, cursor_col);
        std::string at =
            cursor_col < line_text.size() ? std::string(1, line_text[cursor_col]) : " ";
        std::string after = cursor_col < line_text.size() ? line_text.substr(cursor_col + 1) : "";

        auto cursor_elem = ftxui::hbox({
            ftxui::text(before),
            ftxui::text(at) | ftxui::inverted,
            ftxui::text(after),
        });
        if (is_highlighted) cursor_elem = cursor_elem | ftxui::bgcolor(ftxui::Color::GrayDark);
        lines.push_back(cursor_elem);
    }

    return ftxui::vbox(std::move(lines));
}

ftxui::Element FtxuiRenderer::render_statusbar(core::EditorMode mode) const {
    using namespace screen_fmt;
    std::string_view mode_label = mode == core::EditorMode::Normal ? kNormal : kInsert;
    return ftxui::hbox({
               ftxui::text(std::string(kModeLeading) + std::string(mode_label) +
                           std::string(kModeTrailing)) |
                   ftxui::inverted,
               ftxui::text(std::string(kUriSeparator) + uri_),
           }) |
           ftxui::bold;
}

ftxui::Element FtxuiRenderer::render_diagnostics() const {
    auto diags = lsp_.diagnostics(uri_);
    if (diags.empty()) return ftxui::text("");

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

ftxui::Element FtxuiRenderer::render_hover() const {
    auto text = lsp_.hover_text();
    if (text.empty()) return ftxui::text("");
    // Trim trailing newline, split on \n, render each line.
    while (!text.empty() && text.back() == '\n') text.pop_back();
    std::vector<ftxui::Element> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        lines.push_back(ftxui::text(text.substr(start, end - start)));
        if (end == text.size()) break;
        start = end + 1;
    }
    return ftxui::vbox(std::move(lines)) | ftxui::border | ftxui::color(ftxui::Color::Cyan);
}

ftxui::Element FtxuiRenderer::render_signature() const {
    auto sig = lsp_.signature_text();
    if (sig.empty()) return ftxui::text("");
    return ftxui::text("  " + sig) | ftxui::color(ftxui::Color::Yellow);
}

ftxui::Element FtxuiRenderer::render_completion() const {
    auto items = lsp_.completion_items();
    if (items.empty()) return ftxui::text("");
    std::vector<ftxui::Element> rows;
    std::size_t shown = std::min(items.size(), std::size_t{10});
    for (std::size_t i = 0; i < shown; ++i) {
        std::string label = items[i].label;
        if (!items[i].detail.empty()) label += "  " + items[i].detail;
        rows.push_back(ftxui::text(label));
    }
    if (items.size() > shown)
        rows.push_back(ftxui::text("  +" + std::to_string(items.size() - shown) + " more..."));
    return ftxui::vbox(std::move(rows)) | ftxui::border | ftxui::color(ftxui::Color::White);
}

ftxui::Element FtxuiRenderer::render_locations() const {
    auto locs = lsp_.locations();
    if (locs.empty()) return ftxui::text("");
    std::vector<ftxui::Element> rows;
    rows.push_back(ftxui::text("Locations:") | ftxui::bold);
    for (const auto& loc : locs) {
        std::string label =
            "  " + loc.uri + ":" + std::to_string(loc.line + 1) + ":" + std::to_string(loc.col + 1);
        rows.push_back(ftxui::text(label));
    }
    return ftxui::vbox(std::move(rows)) | ftxui::color(ftxui::Color::Green);
}

ftxui::Element FtxuiRenderer::render_symbols() const {
    auto syms = lsp_.symbols();
    if (syms.empty()) return ftxui::text("");
    std::vector<ftxui::Element> rows;
    rows.push_back(ftxui::text("Symbols:") | ftxui::bold);
    std::size_t shown = std::min(syms.size(), std::size_t{20});
    for (std::size_t i = 0; i < shown; ++i) {
        std::string label = "  " + syms[i].name + "  [" + syms[i].kind + "]" + "  " +
                            std::to_string(syms[i].line + 1) + ":" +
                            std::to_string(syms[i].col + 1);
        rows.push_back(ftxui::text(label));
    }
    if (syms.size() > shown)
        rows.push_back(ftxui::text("  +" + std::to_string(syms.size() - shown) + " more..."));
    return ftxui::vbox(std::move(rows)) | ftxui::color(ftxui::Color::Magenta);
}

ftxui::Element FtxuiRenderer::render_inlay_hints() const {
    auto hints = lsp_.inlay_hints();
    if (hints.empty()) return ftxui::text("");
    // Inlay hints are overlaid per line. For simplicity render as a compact strip.
    std::vector<ftxui::Element> items;
    for (const auto& h : hints) {
        items.push_back(ftxui::text(std::to_string(h.line + 1) + ":" + std::to_string(h.col + 1) +
                                    " " + h.label));
    }
    return ftxui::hbox(std::move(items)) | ftxui::color(ftxui::Color::GrayDark);
}

}  // namespace editor::drivers
