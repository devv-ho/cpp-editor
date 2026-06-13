#include "drivers/EditorApp.hpp"

#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/terminal.hpp>

#include "core/usecases/EditorCommands.hpp"
#include "core/usecases/EditorMode.hpp"

namespace editor::drivers {

EditorApp::EditorApp(core::Document& doc, core::usecases::LspService& lsp, std::string uri)
    : doc_(doc),
      lsp_(lsp),
      uri_(std::move(uri)),
      input_(lsp_, uri_),
      renderer_(doc_, lsp_, uri_),
      screen_(ftxui::ScreenInteractive::Fullscreen()) {}

void EditorApp::run() {
    core::EditorMode current_mode = core::EditorMode::Normal;

    // Wake the FTXUI event loop whenever LSP data arrives asynchronously.
    lsp_.set_on_update([this] { screen_.Post(ftxui::Event::Custom); });

    std::function<ftxui::Element()> render_fn = [this, &current_mode] {
        // InstallTerminalInfo (called during Loop startup) may downgrade color
        // support based on DA1/DA2 responses. Force TrueColor so that our
        // ftxui::color() decorators are not silently stripped.
        ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
        return renderer_.render(current_mode);
    };

    auto component = ftxui::CatchEvent(ftxui::Renderer(render_fn),
                                       [this, &current_mode](const ftxui::Event& event) {
                                           // Apply any pending formatting edits on the UI thread.
                                           apply_pending_edits();
                                           auto result = input_.process(event, doc_);
                                           current_mode = result.mode;
                                           if (result.quit) {
                                               screen_.ExitLoopClosure()();
                                           }
                                           return true;
                                       });

    screen_.Loop(component);
}

void EditorApp::apply_pending_edits() {
    auto edits = lsp_.take_pending_edits();
    if (edits.empty()) return;
    // Apply in reverse document order so earlier edits don't shift later offsets.
    std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b) {
        if (a.start_line != b.start_line) return a.start_line > b.start_line;
        return a.start_col > b.start_col;
    });
    for (const auto& e : edits) {
        doc_.buffer().apply_text_edit(e.start_line, e.start_col, e.end_line, e.end_col, e.new_text);
    }
    // Clamp cursor after edits may have reduced the buffer.
    auto line = std::min(doc_.position().line, doc_.buffer().line_count() - 1);
    doc_.cursor().set_position({line, 0});
    core::commands::clamp_normal(doc_);
}

}  // namespace editor::drivers
