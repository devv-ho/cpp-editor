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
}

void LspService::did_change(const std::string& uri, const std::string& text, int version) {
    if (!config_.lsp.did_change) return;
    send_notification("textDocument/didChange",
                      {{"textDocument", {{"uri", uri}, {"version", version}}},
                       {"contentChanges", nlohmann::json::array({{{"text", text}}})}});
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
    send_request(
        "textDocument/definition", text_document_position(uri, line, col),
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_locations(result)); });
}

void LspService::go_to_declaration(const std::string& uri, std::size_t line, std::size_t col,
                                   LocationCallback cb) {
    if (!config_.lsp.go_to_declaration) return;
    send_request(
        "textDocument/declaration", text_document_position(uri, line, col),
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_locations(result)); });
}

void LspService::go_to_implementation(const std::string& uri, std::size_t line, std::size_t col,
                                      LocationCallback cb) {
    if (!config_.lsp.go_to_implementation) return;
    send_request(
        "textDocument/implementation", text_document_position(uri, line, col),
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_locations(result)); });
}

void LspService::go_to_type_definition(const std::string& uri, std::size_t line, std::size_t col,
                                       LocationCallback cb) {
    if (!config_.lsp.type_definition) return;
    send_request(
        "textDocument/typeDefinition", text_document_position(uri, line, col),
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_locations(result)); });
}

void LspService::find_references(const std::string& uri, std::size_t line, std::size_t col,
                                 LocationCallback cb) {
    if (!config_.lsp.find_references) return;
    auto params = text_document_position(uri, line, col);
    params["context"] = {{"includeDeclaration", true}};
    send_request(
        "textDocument/references", params,
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_locations(result)); });
}

// ── Information ───────────────────────────────────────────────────────────────

void LspService::hover(const std::string& uri, std::size_t line, std::size_t col,
                       HoverCallback cb) {
    if (!config_.lsp.hover) return;
    send_request("textDocument/hover", text_document_position(uri, line, col),
                 [cb = std::move(cb)](const nlohmann::json& result) {
                     if (result.is_null()) {
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
                     cb(text);
                 });
}

void LspService::signature_help(const std::string& uri, std::size_t line, std::size_t col,
                                SignatureCallback cb) {
    if (!config_.lsp.signature_help) return;
    send_request("textDocument/signatureHelp", text_document_position(uri, line, col),
                 [cb = std::move(cb)](const nlohmann::json& result) {
                     if (result.is_null() || !result.contains("signatures")) {
                         cb("");
                         return;
                     }
                     const auto& sigs = result["signatures"];
                     if (sigs.empty()) {
                         cb("");
                         return;
                     }
                     std::string label = sigs[0].value("label", "");
                     cb(label);
                 });
}

void LspService::completion(const std::string& uri, std::size_t line, std::size_t col,
                            CompletionCallback cb) {
    if (!config_.lsp.completion) return;
    auto params = text_document_position(uri, line, col);
    params["context"] = {{"triggerKind", 1}};  // Invoked
    send_request(
        "textDocument/completion", params,
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_completion(result)); });
}

void LspService::document_highlight(const std::string& uri, std::size_t line, std::size_t col,
                                    HighlightCallback cb) {
    if (!config_.lsp.document_highlight) return;
    send_request("textDocument/documentHighlight", text_document_position(uri, line, col),
                 [cb = std::move(cb), uri](const nlohmann::json& result) {
                     // documentHighlight returns ranges, not full locations.
                     // Attach the current URI so callers get consistent LspLocation.
                     std::vector<LspLocation> locs;
                     if (!result.is_array()) {
                         cb(locs);
                         return;
                     }
                     for (const auto& h : result) {
                         if (!h.contains("range")) continue;
                         LspLocation loc;
                         loc.uri = uri;
                         loc.line = h["range"]["start"]["line"].get<std::size_t>();
                         loc.col = h["range"]["start"]["character"].get<std::size_t>();
                         locs.push_back(std::move(loc));
                     }
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
    send_request(
        "textDocument/inlayHint", params,
        [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_inlay_hints(result)); });
}

void LspService::document_symbol(const std::string& uri, SymbolCallback cb) {
    if (!config_.lsp.document_symbol) return;
    send_request("textDocument/documentSymbol", {{"textDocument", {{"uri", uri}}}},
                 [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_symbols(result)); });
}

void LspService::workspace_symbol(const std::string& query, WorkspaceSymbolCallback cb) {
    if (!config_.lsp.workspace_symbol) return;
    send_request("workspace/symbol", {{"query", query}},
                 [cb = std::move(cb)](const nlohmann::json& result) { cb(parse_symbols(result)); });
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
                 [cb = std::move(cb)](const nlohmann::json& result) { cb(result); });
}

// ── Diagnostics ───────────────────────────────────────────────────────────────

std::vector<editor::core::Diagnostic> LspService::diagnostics(const std::string& uri) const {
    std::lock_guard lock(diagnostics_mutex_);
    auto it = diagnostics_.find(uri);
    if (it == diagnostics_.end()) return {};
    return it->second;
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
                      {"documentSymbol", {{"dynamicRegistration", false}}}}}}}},
                 {});
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

    std::lock_guard lock(diagnostics_mutex_);
    diagnostics_[uri] = std::move(diags);
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

}  // namespace editor::core::usecases
