#include "core/usecases/LspService.hpp"

#include <unistd.h>

#include <string_view>

#include "adapters/lsp/LspEncoder.hpp"
#include "adapters/lsp/LspMessage.hpp"

namespace editor::core::usecases {

LspService::LspService(std::unique_ptr<editor::adapters::lsp::ClangdProcess> process)
    : process_(std::move(process)) {
    dispatch_thread_ = std::thread(&LspService::dispatch_loop, this);
    handshake();
}

LspService::~LspService() {
    // Destroying process_ closes the pipes, which causes ClangdProcess::receive()
    // to return nullopt, which breaks dispatch_loop's receive() call.
    process_.reset();
    if (dispatch_thread_.joinable()) {
        dispatch_thread_.join();
    }
}

void LspService::did_open(const std::string& uri, const std::string& text) {
    send_notification("textDocument/didOpen", {
                                                  {"textDocument",
                                                   {
                                                       {"uri", uri},
                                                       {"languageId", "cpp"},
                                                       {"version", 1},
                                                       {"text", text},
                                                   }},
                                              });
}

std::vector<editor::core::Diagnostic> LspService::diagnostics(const std::string& uri) const {
    std::lock_guard lock(diagnostics_mutex_);
    auto it = diagnostics_.find(uri);
    if (it == diagnostics_.end()) {
        return {};
    }
    return it->second;
}

void LspService::handshake() {
    // initialize: tell clangd our capabilities and workspace root.
    // processId: our PID so clangd can exit if the editor crashes.
    // rootUri: null for now -- set to the project root in Task 9/10 so
    //          clangd can locate compile_commands.json for build flags.
    send_request("initialize", {
                                   {"processId", static_cast<int>(getpid())},
                                   {"rootUri", nullptr},
                                   {"capabilities", nlohmann::json::object()},
                               });

    // initialized: notify clangd the handshake is complete (no response expected).
    send_notification("initialized", nlohmann::json::object());
}

void LspService::dispatch_loop() {
    while (true) {
        auto msg = process_->receive();
        if (!msg) {
            break;  // process_ was destroyed or clangd exited
        }
        handle_message(*msg);
    }
}

int LspService::send_request(std::string_view method, const nlohmann::json& params) {
    int id = next_id_.fetch_add(1);
    process_->send(editor::adapters::lsp::encode_request(id, method, params));
    return id;
}

void LspService::send_notification(std::string_view method, const nlohmann::json& params) {
    process_->send(editor::adapters::lsp::encode_notification(method, params));
}

void LspService::handle_message(const editor::adapters::lsp::LspMessage& msg) {
    if (msg.method == "textDocument/publishDiagnostics") {
        handle_diagnostics(msg.params);
    }
    // Responses (msg.id set, msg.method empty) are acknowledged but not yet
    // tracked -- request/response correlation will be added when needed.
}

void LspService::handle_diagnostics(const nlohmann::json& params) {
    if (!params.contains("uri") || !params.contains("diagnostics")) {
        return;
    }

    std::string uri = params["uri"].get<std::string>();
    std::vector<editor::core::Diagnostic> diags;

    for (const auto& d : params["diagnostics"]) {
        if (!d.contains("range") || !d.contains("message")) {
            continue;
        }
        editor::core::Diagnostic diag;
        diag.line = d["range"]["start"]["line"].get<std::size_t>();
        diag.col = d["range"]["start"]["character"].get<std::size_t>();
        diag.message = d["message"].get<std::string>();
        diag.severity = d.value("severity", 1);
        diags.push_back(std::move(diag));
    }

    std::lock_guard lock(diagnostics_mutex_);
    diagnostics_[uri] = std::move(diags);
}

}  // namespace editor::core::usecases
