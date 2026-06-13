#include "drivers/FtxuiRenderer.hpp"

#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/entities/Diagnostic.hpp"
#include "core/usecases/EditorMode.hpp"
#include "drivers/ScreenFormat.hpp"
#include "drivers/SyntaxHighlighter.hpp"

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

namespace {

ftxui::Color token_color(const std::string& type) {
    if (type == "keyword" || type == "modifier") return ftxui::Color::Blue;
    if (type == "type" || type == "class" || type == "struct" || type == "enum" ||
        type == "enumMember" || type == "typeParameter" || type == "concept")
        return ftxui::Color::Cyan;
    if (type == "function" || type == "method" || type == "operator") return ftxui::Color::Yellow;
    if (type == "string") return ftxui::Color::Green;
    if (type == "number") return ftxui::Color::Magenta;
    if (type == "comment") return ftxui::Color::GrayLight;
    if (type == "macro" || type == "namespace" || type == "preprocessor")
        return ftxui::Color::BlueLight;
    if (type == "parameter" || type == "variable") return ftxui::Color::Default;
    if (type == "syn_operator") return ftxui::Color::Cyan;
    if (type == "bracket") return ftxui::Color::White;
    return ftxui::Color::Default;
}

// Splits `line` into coloured spans using the tokens that fall on this line.
// Tokens are assumed sorted by col; gaps between tokens get the default colour.
// `cursor_col` is used to inject the cursor inversion at the right position.
ftxui::Element colorize_line(
    const std::string& line,
    const std::vector<std::pair<std::size_t, std::pair<std::size_t, std::string>>>& spans,
    std::optional<std::size_t> cursor_col) {
    std::vector<ftxui::Element> elems;
    std::size_t pos = 0;

    auto push_span = [&](std::size_t start, std::size_t end, const std::string& type) {
        if (start >= end || start >= line.size()) return;
        end = std::min(end, line.size());
        std::string text = line.substr(start, end - start);
        ftxui::Color c = token_color(type);

        if (!cursor_col || *cursor_col < start || *cursor_col >= end) {
            auto elem = ftxui::text(text);
            if (c != ftxui::Color::Default) elem = elem | ftxui::color(c);
            elems.push_back(elem);
            return;
        }
        // Cursor falls inside this span: split into before/at/after.
        std::size_t rel = *cursor_col - start;
        if (rel > 0) {
            auto e = ftxui::text(text.substr(0, rel));
            if (c != ftxui::Color::Default) e = e | ftxui::color(c);
            elems.push_back(e);
        }
        {
            auto e = ftxui::text(std::string(1, text[rel])) | ftxui::inverted;
            if (c != ftxui::Color::Default) e = e | ftxui::color(c);
            elems.push_back(e);
        }
        if (rel + 1 < text.size()) {
            auto e = ftxui::text(text.substr(rel + 1));
            if (c != ftxui::Color::Default) e = e | ftxui::color(c);
            elems.push_back(e);
        }
    };

    for (const auto& [col, len_type] : spans) {
        auto [len, type] = len_type;
        // Gap before token.
        if (pos < col) push_span(pos, col, "");
        push_span(col, col + len, type);
        pos = col + len;
    }
    // Trailing gap (includes cursor if it falls past all tokens).
    if (pos < line.size()) {
        push_span(pos, line.size(), "");
    } else if (cursor_col && elems.empty()) {
        // Empty line: show cursor block.
        elems.push_back(ftxui::text(" ") | ftxui::inverted);
    } else if (cursor_col && *cursor_col >= line.size()) {
        // Cursor past end of line (e.g. on trailing newline position).
        elems.push_back(ftxui::text(" ") | ftxui::inverted);
    }

    if (elems.empty()) return ftxui::text("");
    return ftxui::hbox(std::move(elems));
}

}  // namespace

ftxui::Element FtxuiRenderer::render_buffer() const {
    const auto& buf = doc_.buffer();
    const auto& cursor = doc_.cursor();
    std::size_t cursor_line = cursor.line();
    std::size_t cursor_col = cursor.col();

    auto highlights = lsp_.highlights();
    auto sem_tokens = lsp_.semantic_tokens();

    // Syntactic highlighting: single O(n) pass over the whole file.
    auto syn_map = highlight_file(buf.to_string());

    // Build LSP per-line map (already sorted by line then col from clangd).
    using SpanList = std::vector<std::pair<std::size_t, std::pair<std::size_t, std::string>>>;
    std::unordered_map<std::size_t, SpanList> lsp_map;
    for (const auto& tok : sem_tokens)
        lsp_map[tok.line].emplace_back(tok.col, std::make_pair(tok.length, tok.token_type));

    std::vector<ftxui::Element> lines;
    lines.reserve(buf.line_count());

    for (std::size_t i = 0; i < buf.line_count(); ++i) {
        std::string line_text(buf.line(i).value_or(""));

        bool is_highlighted = false;
        for (const auto& h : highlights) {
            if (h.line == i) {
                is_highlighted = true;
                break;
            }
        }

        // Merge: start from syntactic spans, let LSP overwrite on overlap.
        SpanList syn_spans = syn_map.count(i) ? syn_map.at(i) : SpanList{};
        std::sort(syn_spans.begin(), syn_spans.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        const SpanList& lsp_spans = lsp_map.count(i) ? lsp_map.at(i) : SpanList{};
        SpanList merged = merge_spans(std::move(syn_spans), lsp_spans);

        std::optional<std::size_t> cur =
            (i == cursor_line) ? std::optional{cursor_col} : std::nullopt;

        auto elem = colorize_line(line_text, merged, cur);
        if (is_highlighted) elem = elem | ftxui::bgcolor(ftxui::Color::GrayDark);
        lines.push_back(elem);
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
