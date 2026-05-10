// Defines LspService -- orchestrates LSP communication with clangd.
//
// Sits above ClangdProcess (transport) and handles protocol logic:
// the initialize handshake, outgoing requests/notifications, and
// routing of incoming messages to diagnostics storage.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "adapters/lsp/ClangdProcess.hpp"
#include "core/entities/Diagnostic.hpp"

namespace editor::core::usecases {

class LspService {
public:
    explicit LspService(std::unique_ptr<editor::adapters::lsp::ClangdProcess> process);
    ~LspService();

    LspService(const LspService&) = delete;
    LspService& operator=(const LspService&) = delete;

    // Notifies clangd that a file was opened. Triggers initial diagnostics.
    void did_open(const std::string& uri, const std::string& text);

    // Returns the latest diagnostics for the given URI (empty if none yet).
    std::vector<editor::core::Diagnostic> diagnostics(const std::string& uri) const;

private:
    std::unique_ptr<editor::adapters::lsp::ClangdProcess> process_;
    std::atomic<int> next_id_{1};

    std::thread dispatch_thread_;
    mutable std::mutex diagnostics_mutex_;
    std::unordered_map<std::string, std::vector<editor::core::Diagnostic>> diagnostics_;

    // Sends initialize + initialized to complete the LSP handshake.
    void handshake();

    // Runs on dispatch_thread_: calls receive() in a loop and routes messages.
    void dispatch_loop();

    // Sends a request (has id, expects a response).
    int send_request(std::string_view method, const nlohmann::json& params);

    // Sends a notification (no id, no response expected).
    void send_notification(std::string_view method, const nlohmann::json& params);

    void handle_message(const editor::adapters::lsp::LspMessage& msg);
    void handle_diagnostics(const nlohmann::json& params);
};

}  // namespace editor::core::usecases
