// Editor entry point. Composition root only -- no business logic.

#include <cstdlib>
#include <iostream>
#include <memory>

#include "adapters/lsp/ClangdProcess.hpp"
#include "core/usecases/FileService.hpp"
#include "core/usecases/LspService.hpp"
#include "drivers/EditorApp.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: editor <file>\n";
        return EXIT_FAILURE;
    }

    const std::string path = argv[1];

    editor::core::Document doc = [&] {
        try {
            return editor::core::usecases::FileService::load(path);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            std::exit(EXIT_FAILURE);
        }
    }();

    std::string uri = editor::core::usecases::FileService::path_to_uri(path);

    auto process = std::make_unique<editor::adapters::lsp::ClangdProcess>();
    editor::core::usecases::LspService lsp(std::move(process));
    lsp.did_open(uri, doc.buffer().to_string());

    editor::drivers::EditorApp app(doc, lsp, uri);
    app.run();

    return EXIT_SUCCESS;
}
