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

// One decoded semantic token span.
struct LspSemanticToken {
    std::size_t line;
    std::size_t col;
    std::size_t length;
    std::string token_type;  // e.g. "keyword", "type", "function", "variable"
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
    using SemanticTokensCallback = std::function<void(std::vector<LspSemanticToken>)>;

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
    void semantic_tokens_full(const std::string& uri, SemanticTokensCallback cb);

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

    // ── Latest results (pull-based, read by renderer) ─────────────────────────
    // Each field holds the most recent result for that feature. Cleared when
    // the user moves the cursor (managed by InputDispatcher via clear_*).

    std::string hover_text() const;
    std::string signature_text() const;
    std::vector<LspLocation> locations() const;  // definition/references/etc.
    std::vector<LspCompletionItem> completion_items() const;
    std::vector<LspDocumentSymbol> symbols() const;
    std::vector<LspInlayHint> inlay_hints() const;
    std::vector<LspLocation> highlights() const;
    std::vector<LspSemanticToken> semantic_tokens() const;

    void set_hover(std::string text);
    void set_signature(std::string text);
    void set_locations(std::vector<LspLocation> locs);
    void set_completion(std::vector<LspCompletionItem> items);
    void set_symbols(std::vector<LspDocumentSymbol> syms);
    void set_inlay_hints(std::vector<LspInlayHint> hints);
    void set_highlights(std::vector<LspLocation> locs);
    void set_semantic_tokens(std::vector<LspSemanticToken> tokens);

    void clear_overlay();  // clears hover, signature, locations, completion

    // Called from the UI thread after setup. cb is invoked from the LSP
    // dispatch thread whenever overlay data or diagnostics change, so the
    // renderer can request a screen refresh without polling.
    void set_on_update(std::function<void()> cb);

    // ── Config ────────────────────────────────────────────────────────────────

    const EditorConfig& config() const { return config_; }

    // Parse helpers exposed for unit testing.
    static std::vector<LspLocation> parse_locations(const nlohmann::json& j);
    static std::vector<LspCompletionItem> parse_completion(const nlohmann::json& j);
    static std::vector<LspCodeAction> parse_code_actions(const nlohmann::json& j);
    static std::vector<LspDocumentSymbol> parse_symbols(const nlohmann::json& j);
    static std::vector<LspInlayHint> parse_inlay_hints(const nlohmann::json& j);
    std::vector<LspSemanticToken> parse_semantic_tokens(const nlohmann::json& j) const;

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

    // Latest overlay results (protected by overlay_mutex_).
    std::function<void()> on_update_;  // fires when overlay/diagnostics change
    mutable std::mutex overlay_mutex_;
    std::string hover_text_;
    std::string signature_text_;
    std::vector<LspLocation> locations_;
    std::vector<LspCompletionItem> completion_items_;
    std::vector<LspDocumentSymbol> symbols_;
    std::vector<LspInlayHint> inlay_hints_;
    std::vector<LspLocation> highlights_;
    std::vector<LspSemanticToken> semantic_tokens_;

    // Token type legend received from clangd's initialize response.
    std::vector<std::string> semantic_token_types_;

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
};

}  // namespace editor::core::usecases
