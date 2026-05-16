// Defines LspService -- orchestrates LSP communication with clangd.
//
// Sits above ClangdProcess (transport) and handles protocol logic:
// the initialize handshake, outgoing requests/notifications, and
// routing of incoming messages to result storage.
//
// All LSP features are gated by EditorConfig::LspFeatures. Disabled
// features are no-ops at the call site -- no wire traffic is generated.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "adapters/lsp/ClangdProcess.hpp"
#include "core/entities/Diagnostic.hpp"
#include "core/entities/EditorConfig.hpp"

namespace editor::core::usecases {

// A resolved LSP location (file + line + col).
struct LspLocation {
    std::string uri;
    std::size_t line;
    std::size_t col;
};

// One completion item.
struct LspCompletionItem {
    std::string label;
    std::string insert_text;
    std::string detail;
};

// One code action.
struct LspCodeAction {
    std::string title;
    nlohmann::json workspace_edit;  // raw; applied via did_change path
};

// Document symbol (outline entry).
struct LspDocumentSymbol {
    std::string name;
    std::string kind;
    std::size_t line;
    std::size_t col;
};

// Inlay hint.
struct LspInlayHint {
    std::size_t line;
    std::size_t col;
    std::string label;
};

class LspService {
public:
    explicit LspService(std::unique_ptr<editor::adapters::lsp::ClangdProcess> process,
                        EditorConfig config = {});
    ~LspService();

    LspService(const LspService&) = delete;
    LspService& operator=(const LspService&) = delete;

    // ── Document sync ─────────────────────────────────────────────────────────

    void did_open(const std::string& uri, const std::string& text);
    void did_change(const std::string& uri, const std::string& text, int version);
    void did_save(const std::string& uri);
    void did_close(const std::string& uri);

    // ── Navigation (async -- result via callback) ──────────────────────────────

    using LocationCallback = std::function<void(std::vector<LspLocation>)>;

    void go_to_definition(const std::string& uri, std::size_t line, std::size_t col,
                          LocationCallback cb);
    void go_to_declaration(const std::string& uri, std::size_t line, std::size_t col,
                           LocationCallback cb);
    void go_to_implementation(const std::string& uri, std::size_t line, std::size_t col,
                              LocationCallback cb);
    void go_to_type_definition(const std::string& uri, std::size_t line, std::size_t col,
                               LocationCallback cb);
    void find_references(const std::string& uri, std::size_t line, std::size_t col,
                         LocationCallback cb);

    // ── Information (async -- result via callback) ─────────────────────────────

    using HoverCallback = std::function<void(std::string)>;
    using SignatureCallback = std::function<void(std::string)>;
    using CompletionCallback = std::function<void(std::vector<LspCompletionItem>)>;
    using HighlightCallback = std::function<void(std::vector<LspLocation>)>;
    using InlayHintCallback = std::function<void(std::vector<LspInlayHint>)>;
    using SymbolCallback = std::function<void(std::vector<LspDocumentSymbol>)>;
    using WorkspaceSymbolCallback = std::function<void(std::vector<LspDocumentSymbol>)>;

    void hover(const std::string& uri, std::size_t line, std::size_t col, HoverCallback cb);
    void signature_help(const std::string& uri, std::size_t line, std::size_t col,
                        SignatureCallback cb);
    void completion(const std::string& uri, std::size_t line, std::size_t col,
                    CompletionCallback cb);
    void document_highlight(const std::string& uri, std::size_t line, std::size_t col,
                            HighlightCallback cb);
    void inlay_hints(const std::string& uri, std::size_t start_line, std::size_t end_line,
                     InlayHintCallback cb);
    void document_symbol(const std::string& uri, SymbolCallback cb);
    void workspace_symbol(const std::string& query, WorkspaceSymbolCallback cb);

    // ── Edit operations (async -- result via callback) ─────────────────────────

    using RenameCallback = std::function<void(nlohmann::json)>;  // raw WorkspaceEdit
    using CodeActionCallback = std::function<void(std::vector<LspCodeAction>)>;
    using FormattingCallback = std::function<void(nlohmann::json)>;  // raw TextEdit[]

    void rename(const std::string& uri, std::size_t line, std::size_t col,
                const std::string& new_name, RenameCallback cb);
    void code_action(const std::string& uri, std::size_t line, std::size_t col,
                     CodeActionCallback cb);
    void formatting(const std::string& uri, FormattingCallback cb);

    // ── Diagnostics (push-based, updated by publishDiagnostics) ───────────────

    std::vector<editor::core::Diagnostic> diagnostics(const std::string& uri) const;

    // ── Config ────────────────────────────────────────────────────────────────

    const EditorConfig& config() const { return config_; }

private:
    std::unique_ptr<editor::adapters::lsp::ClangdProcess> process_;
    EditorConfig config_;
    std::atomic<int> next_id_{1};

    std::thread dispatch_thread_;

    // Pending request callbacks keyed by request id.
    mutable std::mutex pending_mutex_;
    std::unordered_map<int, std::function<void(const nlohmann::json&)>> pending_;

    mutable std::mutex diagnostics_mutex_;
    std::unordered_map<std::string, std::vector<editor::core::Diagnostic>> diagnostics_;

    void handshake();
    void dispatch_loop();

    // Sends a request and registers cb to be called with the result/error JSON.
    int send_request(std::string_view method, const nlohmann::json& params,
                     std::function<void(const nlohmann::json&)> cb = {});

    void send_notification(std::string_view method, const nlohmann::json& params);

    void handle_message(const editor::adapters::lsp::LspMessage& msg);
    void handle_diagnostics(const nlohmann::json& params);

    // Helpers for building common LSP param structures.
    static nlohmann::json text_document_position(const std::string& uri, std::size_t line,
                                                 std::size_t col);

    // Parse helpers.
    static std::vector<LspLocation> parse_locations(const nlohmann::json& j);
    static std::vector<LspCompletionItem> parse_completion(const nlohmann::json& j);
    static std::vector<LspCodeAction> parse_code_actions(const nlohmann::json& j);
    static std::vector<LspDocumentSymbol> parse_symbols(const nlohmann::json& j);
    static std::vector<LspInlayHint> parse_inlay_hints(const nlohmann::json& j);
};

}  // namespace editor::core::usecases
