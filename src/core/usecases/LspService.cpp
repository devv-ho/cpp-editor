#include "core/usecases/LspService.hpp"

#include <unistd.h>

#include <string_view>

#include "adapters/lsp/LspEncoder.hpp"
#include "adapters/lsp/LspMessage.hpp"

namespace editor::core::usecases {

// ── Construction / destruction ────────────────────────────────────────────────

LspService::LspService(std::unique_ptr<editor::adapters::lsp::ClangdProcess> process,
                       EditorConfig config)
    : process_(std::move(process)), config_(config) {
    dispatch_thread_ = std::thread(&LspService::dispatch_loop, this);
    handshake();
}

LspService::~LspService() {
    process_.reset();
    if (dispatch_thread_.joinable()) dispatch_thread_.join();
}

// ── Document sync ─────────────────────────────────────────────────────────────

void LspService::did_open(const std::string& uri, const std::string& text) {
    send_notification(
        "textDocument/didOpen",
        {{"textDocument", {{"uri", uri}, {"languageId", "cpp"}, {"version", 1}, {"text", text}}}});
    // Semantic tokens are requested after publishDiagnostics arrives (file ready signal).
}

void LspService::did_change(const std::string& uri, const std::string& text, int version) {
    if (!config_.lsp.did_change) return;
    send_notification("textDocument/didChange",
                      {{"textDocument", {{"uri", uri}, {"version", version}}},
                       {"contentChanges", nlohmann::json::array({{{"text", text}}})}});
    // Semantic tokens refreshed after publishDiagnostics arrives.
}

void LspService::did_save(const std::string& uri) {
    if (!config_.lsp.did_save) return;
    send_notification("textDocument/didSave", {{"textDocument", {{"uri", uri}}}});
}

void LspService::did_close(const std::string& uri) {
    if (!config_.lsp.did_close) return;
    send_notification("textDocument/didClose", {{"textDocument", {{"uri", uri}}}});
}

// ── Navigation ────────────────────────────────────────────────────────────────

void LspService::go_to_definition(const std::string& uri, std::size_t line, std::size_t col,
                                  LocationCallback cb) {
    if (!config_.lsp.go_to_definition) return;
    send_request("textDocument/definition", text_document_position(uri, line, col),
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto locs = parse_locations(result);
                     set_locations(locs);
                     cb(locs);
                 });
}

void LspService::go_to_declaration(const std::string& uri, std::size_t line, std::size_t col,
                                   LocationCallback cb) {
    if (!config_.lsp.go_to_declaration) return;
    send_request("textDocument/declaration", text_document_position(uri, line, col),
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto locs = parse_locations(result);
                     set_locations(locs);
                     cb(locs);
                 });
}

void LspService::go_to_implementation(const std::string& uri, std::size_t line, std::size_t col,
                                      LocationCallback cb) {
    if (!config_.lsp.go_to_implementation) return;
    send_request("textDocument/implementation", text_document_position(uri, line, col),
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto locs = parse_locations(result);
                     set_locations(locs);
                     cb(locs);
                 });
}

void LspService::go_to_type_definition(const std::string& uri, std::size_t line, std::size_t col,
                                       LocationCallback cb) {
    if (!config_.lsp.type_definition) return;
    send_request("textDocument/typeDefinition", text_document_position(uri, line, col),
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto locs = parse_locations(result);
                     set_locations(locs);
                     cb(locs);
                 });
}

void LspService::find_references(const std::string& uri, std::size_t line, std::size_t col,
                                 LocationCallback cb) {
    if (!config_.lsp.find_references) return;
    auto params = text_document_position(uri, line, col);
    params["context"] = {{"includeDeclaration", true}};
    send_request("textDocument/references", params,
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto locs = parse_locations(result);
                     set_locations(locs);
                     cb(locs);
                 });
}

// ── Information ───────────────────────────────────────────────────────────────

void LspService::hover(const std::string& uri, std::size_t line, std::size_t col,
                       HoverCallback cb) {
    if (!config_.lsp.hover) return;
    send_request("textDocument/hover", text_document_position(uri, line, col),
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     if (result.is_null()) {
                         set_hover("");
                         cb("");
                         return;
                     }
                     std::string text;
                     if (result.contains("contents")) {
                         const auto& c = result["contents"];
                         if (c.is_string())
                             text = c.get<std::string>();
                         else if (c.is_object() && c.contains("value"))
                             text = c["value"].get<std::string>();
                         else if (c.is_array()) {
                             for (const auto& item : c) {
                                 if (item.is_string())
                                     text += item.get<std::string>() + "\n";
                                 else if (item.is_object() && item.contains("value"))
                                     text += item["value"].get<std::string>() + "\n";
                             }
                         }
                     }
                     set_hover(text);
                     cb(text);
                 });
}

void LspService::signature_help(const std::string& uri, std::size_t line, std::size_t col,
                                SignatureCallback cb) {
    if (!config_.lsp.signature_help) return;
    send_request("textDocument/signatureHelp", text_document_position(uri, line, col),
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     if (result.is_null() || !result.contains("signatures")) {
                         set_signature("");
                         cb("");
                         return;
                     }
                     const auto& sigs = result["signatures"];
                     std::string label = sigs.empty() ? "" : sigs[0].value("label", "");
                     set_signature(label);
                     cb(label);
                 });
}

void LspService::completion(const std::string& uri, std::size_t line, std::size_t col,
                            CompletionCallback cb) {
    if (!config_.lsp.completion) return;
    auto params = text_document_position(uri, line, col);
    params["context"] = {{"triggerKind", 1}};
    send_request("textDocument/completion", params,
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto items = parse_completion(result);
                     set_completion(items);
                     cb(items);
                 });
}

void LspService::document_highlight(const std::string& uri, std::size_t line, std::size_t col,
                                    HighlightCallback cb) {
    if (!config_.lsp.document_highlight) return;
    send_request("textDocument/documentHighlight", text_document_position(uri, line, col),
                 [this, cb = std::move(cb), uri](const nlohmann::json& result) {
                     std::vector<LspLocation> locs;
                     if (result.is_array()) {
                         for (const auto& h : result) {
                             if (!h.contains("range")) continue;
                             LspLocation loc;
                             loc.uri = uri;
                             loc.line = h["range"]["start"]["line"].get<std::size_t>();
                             loc.col = h["range"]["start"]["character"].get<std::size_t>();
                             locs.push_back(std::move(loc));
                         }
                     }
                     set_highlights(locs);
                     cb(locs);
                 });
}

void LspService::inlay_hints(const std::string& uri, std::size_t start_line, std::size_t end_line,
                             InlayHintCallback cb) {
    if (!config_.lsp.inlay_hints) return;
    nlohmann::json params = {{"textDocument", {{"uri", uri}}},
                             {"range",
                              {{"start", {{"line", start_line}, {"character", 0}}},
                               {"end", {{"line", end_line}, {"character", 0}}}}}};
    send_request("textDocument/inlayHint", params,
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto hints = parse_inlay_hints(result);
                     set_inlay_hints(hints);
                     cb(hints);
                 });
}

void LspService::document_symbol(const std::string& uri, SymbolCallback cb) {
    if (!config_.lsp.document_symbol) return;
    send_request("textDocument/documentSymbol", {{"textDocument", {{"uri", uri}}}},
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto syms = parse_symbols(result);
                     set_symbols(syms);
                     cb(syms);
                 });
}

void LspService::workspace_symbol(const std::string& query, WorkspaceSymbolCallback cb) {
    if (!config_.lsp.workspace_symbol) return;
    send_request("workspace/symbol", {{"query", query}},
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     auto syms = parse_symbols(result);
                     set_symbols(syms);
                     cb(syms);
                 });
}

void LspService::semantic_tokens_full(const std::string& uri, SemanticTokensCallback cb) {
    if (!config_.lsp.semantic_tokens) return;
    send_request("textDocument/semanticTokens/full", {{"textDocument", {{"uri", uri}}}},
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     // result is already msg.result (the "result" field value).
                     if (result.is_null()) return;
                     auto toks = parse_semantic_tokens(result);
                     cb(toks);
                 });
}

// ── Edit operations ───────────────────────────────────────────────────────────

void LspService::rename(const std::string& uri, std::size_t line, std::size_t col,
                        const std::string& new_name, RenameCallback cb) {
    if (!config_.lsp.rename) return;
    auto params = text_document_position(uri, line, col);
    params["newName"] = new_name;
    send_request("textDocument/rename", params,
                 [cb = std::move(cb)](const nlohmann::json& result) { cb(result); });
}

void LspService::code_action(const std::string& uri, std::size_t line, std::size_t col,
                             CodeActionCallback cb) {
    if (!config_.lsp.code_action) return;
    nlohmann::json params = {{"textDocument", {{"uri", uri}}},
                             {"range",
                              {{"start", {{"line", line}, {"character", col}}},
                               {"end", {{"line", line}, {"character", col}}}}},
                             {"context", {{"diagnostics", nlohmann::json::array()}}}};
    send_request(
        "textDocument/codeAction", params,
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_code_actions(result)); });
}

void LspService::formatting(const std::string& uri, FormattingCallback cb) {
    if (!config_.lsp.formatting) return;
    nlohmann::json params = {{"textDocument", {{"uri", uri}}},
                             {"options", {{"tabSize", 4}, {"insertSpaces", true}}}};
    send_request("textDocument/formatting", params,
                 [this, cb = std::move(cb)](const nlohmann::json& result) {
                     // Store edits for the UI thread to apply via take_pending_edits().
                     auto edits = parse_text_edits(result);
                     std::function<void()> notify;
                     {
                         std::lock_guard lock(overlay_mutex_);
                         pending_edits_ = std::move(edits);
                         notify = on_update_;
                     }
                     if (notify) notify();
                     cb(result);
                 });
}

// ── Diagnostics ───────────────────────────────────────────────────────────────

std::vector<editor::core::Diagnostic> LspService::diagnostics(const std::string& uri) const {
    std::lock_guard lock(diagnostics_mutex_);
    auto it = diagnostics_.find(uri);
    if (it == diagnostics_.end()) return {};
    return it->second;
}

// ── Overlay getters ───────────────────────────────────────────────────────────

std::string LspService::hover_text() const {
    std::lock_guard lock(overlay_mutex_);
    return hover_text_;
}
std::string LspService::signature_text() const {
    std::lock_guard lock(overlay_mutex_);
    return signature_text_;
}
std::vector<LspLocation> LspService::locations() const {
    std::lock_guard lock(overlay_mutex_);
    return locations_;
}
std::vector<LspCompletionItem> LspService::completion_items() const {
    std::lock_guard lock(overlay_mutex_);
    return completion_items_;
}
std::vector<LspDocumentSymbol> LspService::symbols() const {
    std::lock_guard lock(overlay_mutex_);
    return symbols_;
}
std::vector<LspInlayHint> LspService::inlay_hints() const {
    std::lock_guard lock(overlay_mutex_);
    return inlay_hints_;
}
std::vector<LspLocation> LspService::highlights() const {
    std::lock_guard lock(overlay_mutex_);
    return highlights_;
}

// ── Overlay setters ───────────────────────────────────────────────────────────

void LspService::set_on_update(std::function<void()> cb) {
    std::lock_guard lock(overlay_mutex_);
    on_update_ = std::move(cb);
}

std::vector<LspSemanticToken> LspService::semantic_tokens() const {
    std::lock_guard lock(overlay_mutex_);
    return semantic_tokens_;
}
void LspService::set_semantic_tokens(std::vector<LspSemanticToken> tokens) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        semantic_tokens_ = std::move(tokens);
        cb = on_update_;
    }
    if (cb) cb();
}

void LspService::set_hover(std::string text) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        hover_text_ = std::move(text);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::set_signature(std::string text) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        signature_text_ = std::move(text);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::set_locations(std::vector<LspLocation> locs) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        locations_ = std::move(locs);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::set_completion(std::vector<LspCompletionItem> items) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        completion_items_ = std::move(items);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::set_symbols(std::vector<LspDocumentSymbol> syms) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        symbols_ = std::move(syms);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::set_inlay_hints(std::vector<LspInlayHint> hints) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        inlay_hints_ = std::move(hints);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::set_highlights(std::vector<LspLocation> locs) {
    std::function<void()> cb;
    {
        std::lock_guard lock(overlay_mutex_);
        highlights_ = std::move(locs);
        cb = on_update_;
    }
    if (cb) cb();
}
void LspService::clear_overlay() {
    std::lock_guard lock(overlay_mutex_);
    hover_text_.clear();
    signature_text_.clear();
    locations_.clear();
    completion_items_.clear();
    symbols_.clear();
    inlay_hints_.clear();
    highlights_.clear();
    semantic_tokens_.clear();
}

std::vector<LspTextEdit> LspService::take_pending_edits() {
    std::lock_guard lock(overlay_mutex_);
    return std::move(pending_edits_);
}

// ── Internal ──────────────────────────────────────────────────────────────────

void LspService::handshake() {
    send_request("initialize",
                 {{"processId", static_cast<int>(getpid())},
                  {"rootUri", nullptr},
                  {"capabilities",
                   {{"textDocument",
                     {{"definition", {{"dynamicRegistration", false}}},
                      {"declaration", {{"dynamicRegistration", false}}},
                      {"references", {{"dynamicRegistration", false}}},
                      {"implementation", {{"dynamicRegistration", false}}},
                      {"typeDefinition", {{"dynamicRegistration", false}}},
                      {"hover", {{"dynamicRegistration", false}}},
                      {"signatureHelp", {{"dynamicRegistration", false}}},
                      {"completion", {{"dynamicRegistration", false}}},
                      {"documentHighlight", {{"dynamicRegistration", false}}},
                      {"inlayHint", {{"dynamicRegistration", false}}},
                      {"rename", {{"dynamicRegistration", false}}},
                      {"codeAction", {{"dynamicRegistration", false}}},
                      {"formatting", {{"dynamicRegistration", false}}},
                      {"documentSymbol", {{"dynamicRegistration", false}}},
                      {"semanticTokens",
                       {{"dynamicRegistration", false},
                        {"requests", {{"full", true}}},
                        {"formats", {"relative"}},
                        {"tokenTypes", nlohmann::json::array()},
                        {"tokenModifiers", nlohmann::json::array()},
                        {"multilineTokenSupport", false},
                        {"overlappingTokenSupport", false}}}}}}}},
                 [this](const nlohmann::json& result) {
                     if (!result.contains("capabilities")) return;
                     const auto& srv = result["capabilities"];
                     if (!srv.contains("semanticTokensProvider")) return;
                     const auto& prov = srv["semanticTokensProvider"];
                     if (!prov.contains("legend")) return;
                     const auto& legend = prov["legend"];
                     if (!legend.contains("tokenTypes")) return;
                     std::lock_guard lock(overlay_mutex_);
                     semantic_token_types_.clear();
                     for (const auto& t : legend["tokenTypes"])
                         semantic_token_types_.push_back(t.get<std::string>());
                 });
    send_notification("initialized", nlohmann::json::object());
}

void LspService::dispatch_loop() {
    while (true) {
        auto msg = process_->receive();
        if (!msg) break;
        handle_message(*msg);
    }
}

int LspService::send_request(std::string_view method, const nlohmann::json& params,
                             std::function<void(const nlohmann::json&)> cb) {
    int id = next_id_.fetch_add(1);
    if (cb) {
        std::lock_guard lock(pending_mutex_);
        pending_[id] = std::move(cb);
    }
    process_->send(editor::adapters::lsp::encode_request(id, method, params));
    return id;
}

void LspService::send_notification(std::string_view method, const nlohmann::json& params) {
    process_->send(editor::adapters::lsp::encode_notification(method, params));
}

void LspService::handle_message(const editor::adapters::lsp::LspMessage& msg) {
    // Response: has id, no method.
    if (msg.id && msg.method.empty()) {
        std::function<void(const nlohmann::json&)> cb;
        {
            std::lock_guard lock(pending_mutex_);
            auto it = pending_.find(*msg.id);
            if (it != pending_.end()) {
                cb = std::move(it->second);
                pending_.erase(it);
            }
        }
        if (cb) cb(msg.result);
        return;
    }
    // Notification from server.
    if (msg.method == "textDocument/publishDiagnostics") {
        handle_diagnostics(msg.params);
    }
}

void LspService::handle_diagnostics(const nlohmann::json& params) {
    if (!params.contains("uri") || !params.contains("diagnostics")) return;

    std::string uri = params["uri"].get<std::string>();
    std::vector<editor::core::Diagnostic> diags;

    for (const auto& d : params["diagnostics"]) {
        if (!d.contains("range") || !d.contains("message")) continue;
        editor::core::Diagnostic diag;
        diag.line = d["range"]["start"]["line"].get<std::size_t>();
        diag.col = d["range"]["start"]["character"].get<std::size_t>();
        diag.message = d["message"].get<std::string>();
        diag.severity = d.value("severity", DiagnosticSeverity::kError);
        diags.push_back(std::move(diag));
    }

    std::function<void()> cb;
    {
        std::lock_guard lock(diagnostics_mutex_);
        diagnostics_[uri] = std::move(diags);
    }
    {
        std::lock_guard lock(overlay_mutex_);
        cb = on_update_;
    }
    if (cb) cb();
    // publishDiagnostics means clangd finished analysing the file -- safe to
    // request semantic tokens now (legend is also guaranteed to be stored).
    if (config_.lsp.semantic_tokens)
        semantic_tokens_full(uri, [this](auto toks) { set_semantic_tokens(std::move(toks)); });
}

// ── Static helpers ────────────────────────────────────────────────────────────

nlohmann::json LspService::text_document_position(const std::string& uri, std::size_t line,
                                                  std::size_t col) {
    return {{"textDocument", {{"uri", uri}}}, {"position", {{"line", line}, {"character", col}}}};
}

std::vector<LspLocation> LspService::parse_locations(const nlohmann::json& j) {
    std::vector<LspLocation> result;
    if (j.is_null()) return result;

    auto process_one = [&](const nlohmann::json& loc) {
        if (!loc.contains("uri") || !loc.contains("range")) return;
        LspLocation l;
        l.uri = loc["uri"].get<std::string>();
        l.line = loc["range"]["start"]["line"].get<std::size_t>();
        l.col = loc["range"]["start"]["character"].get<std::size_t>();
        result.push_back(std::move(l));
    };

    if (j.is_array()) {
        for (const auto& loc : j) process_one(loc);
    } else if (j.is_object()) {
        process_one(j);
    }
    return result;
}

std::vector<LspCompletionItem> LspService::parse_completion(const nlohmann::json& j) {
    std::vector<LspCompletionItem> result;
    if (j.is_null()) return result;

    const nlohmann::json* items = nullptr;
    if (j.is_array()) {
        items = &j;
    } else if (j.is_object() && j.contains("items")) {
        items = &j["items"];
    }
    if (!items) return result;

    for (const auto& item : *items) {
        LspCompletionItem ci;
        ci.label = item.value("label", "");
        ci.insert_text = item.value("insertText", ci.label);
        ci.detail = item.value("detail", "");
        result.push_back(std::move(ci));
    }
    return result;
}

std::vector<LspCodeAction> LspService::parse_code_actions(const nlohmann::json& j) {
    std::vector<LspCodeAction> result;
    if (!j.is_array()) return result;
    for (const auto& a : j) {
        LspCodeAction ca;
        ca.title = a.value("title", "");
        ca.workspace_edit = a.value("edit", nlohmann::json{});
        result.push_back(std::move(ca));
    }
    return result;
}

std::vector<LspDocumentSymbol> LspService::parse_symbols(const nlohmann::json& j) {
    std::vector<LspDocumentSymbol> result;
    if (!j.is_array()) return result;

    std::function<void(const nlohmann::json&)> process = [&](const nlohmann::json& sym) {
        LspDocumentSymbol s;
        s.name = sym.value("name", "");
        // DocumentSymbol uses "kind" int; SymbolInformation also has "location".
        int kind = sym.value("kind", 0);
        // LSP SymbolKind: 5=class, 12=function, 13=variable, etc.
        s.kind = std::to_string(kind);
        if (sym.contains("location")) {
            s.line = sym["location"]["range"]["start"]["line"].get<std::size_t>();
            s.col = sym["location"]["range"]["start"]["character"].get<std::size_t>();
        } else if (sym.contains("range")) {
            s.line = sym["range"]["start"]["line"].get<std::size_t>();
            s.col = sym["range"]["start"]["character"].get<std::size_t>();
        }
        result.push_back(std::move(s));
        // Recurse into children (DocumentSymbol hierarchy).
        if (sym.contains("children") && sym["children"].is_array()) {
            for (const auto& child : sym["children"]) process(child);
        }
    };

    for (const auto& sym : j) process(sym);
    return result;
}

std::vector<LspInlayHint> LspService::parse_inlay_hints(const nlohmann::json& j) {
    std::vector<LspInlayHint> result;
    if (!j.is_array()) return result;
    for (const auto& h : j) {
        LspInlayHint hint;
        if (h.contains("position")) {
            hint.line = h["position"]["line"].get<std::size_t>();
            hint.col = h["position"]["character"].get<std::size_t>();
        }
        if (!h.contains("label")) {
            result.push_back(std::move(hint));
            continue;
        }
        const auto& lbl = h["label"];
        if (lbl.is_string())
            hint.label = lbl.get<std::string>();
        else if (lbl.is_array()) {
            for (const auto& part : lbl)
                if (part.contains("value")) hint.label += part["value"].get<std::string>();
        }
        result.push_back(std::move(hint));
    }
    return result;
}

std::vector<LspTextEdit> LspService::parse_text_edits(const nlohmann::json& j) {
    std::vector<LspTextEdit> result;
    if (!j.is_array()) return result;
    for (const auto& e : j) {
        if (!e.contains("range") || !e.contains("newText")) continue;
        const auto& range = e["range"];
        if (!range.contains("start") || !range.contains("end")) continue;
        LspTextEdit edit;
        edit.start_line = range["start"]["line"].get<std::size_t>();
        edit.start_col = range["start"]["character"].get<std::size_t>();
        edit.end_line = range["end"]["line"].get<std::size_t>();
        edit.end_col = range["end"]["character"].get<std::size_t>();
        edit.new_text = e["newText"].get<std::string>();
        result.push_back(std::move(edit));
    }
    return result;
}

// LSP semantic tokens use delta encoding: each token is 5 consecutive integers:
//   [deltaLine, deltaStartChar, length, tokenTypeIndex, tokenModifiers]
// deltaLine is relative to the previous token's line;
// deltaStartChar is relative to the previous token's start col IF on the same line,
// otherwise absolute from col 0.
std::vector<LspSemanticToken> LspService::parse_semantic_tokens(const nlohmann::json& j) const {
    std::vector<LspSemanticToken> result;
    if (!j.contains("data") || !j["data"].is_array()) return result;

    const auto& data = j["data"];
    if (data.size() % 5 != 0) return result;

    std::vector<std::string> types;
    {
        std::lock_guard lock(overlay_mutex_);
        types = semantic_token_types_;
    }

    std::size_t cur_line = 0;
    std::size_t cur_col = 0;

    for (std::size_t i = 0; i < data.size(); i += 5) {
        std::size_t delta_line = data[i].get<std::size_t>();
        std::size_t delta_col = data[i + 1].get<std::size_t>();
        std::size_t length = data[i + 2].get<std::size_t>();
        std::size_t type_idx = data[i + 3].get<std::size_t>();

        if (delta_line > 0) {
            cur_line += delta_line;
            cur_col = delta_col;
        } else {
            cur_col += delta_col;
        }

        std::string type_name =
            type_idx < types.size() ? types[type_idx] : std::to_string(type_idx);

        result.push_back({cur_line, cur_col, length, std::move(type_name)});
    }
    return result;
}

}  // namespace editor::core::usecases
