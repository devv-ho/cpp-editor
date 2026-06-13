#include "drivers/EditorApp.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/terminal.hpp>

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
                                           auto result = input_.process(event, doc_);
                                           current_mode = result.mode;
                                           if (result.quit) {
                                               screen_.ExitLoopClosure()();
                                           }
                                           return true;
                                       });

    screen_.Loop(component);
}

}  // namespace editor::drivers
