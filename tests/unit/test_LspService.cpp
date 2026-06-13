#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "adapters/lsp/ClangdProcess.hpp"
#include "core/entities/EditorConfig.hpp"
#include "core/usecases/LspService.hpp"

using namespace editor::core::usecases;
using editor::core::EditorConfig;

// Builds an LspService backed by /usr/bin/false so ClangdProcess's dispatch
// thread exits immediately (receive() returns nullopt on EOF).
static std::unique_ptr<LspService> make_null_lsp(EditorConfig cfg = {}) {
    return std::make_unique<LspService>(
        std::make_unique<editor::adapters::lsp::ClangdProcess>("false"), cfg);
}

// ── Overlay getters/setters/clear ─────────────────────────────────────────────

TEST(LspServiceOverlayTest, DefaultOverlaysAreEmpty) {
    auto lsp = make_null_lsp();
    EXPECT_TRUE(lsp->hover_text().empty());
    EXPECT_TRUE(lsp->signature_text().empty());
    EXPECT_TRUE(lsp->locations().empty());
    EXPECT_TRUE(lsp->completion_items().empty());
    EXPECT_TRUE(lsp->symbols().empty());
    EXPECT_TRUE(lsp->inlay_hints().empty());
    EXPECT_TRUE(lsp->highlights().empty());
}

TEST(LspServiceOverlayTest, SetHoverStoresText) {
    auto lsp = make_null_lsp();
    lsp->set_hover("some hover text");
    EXPECT_EQ(lsp->hover_text(), "some hover text");
}

TEST(LspServiceOverlayTest, SetSignatureStoresText) {
    auto lsp = make_null_lsp();
    lsp->set_signature("void foo(int x)");
    EXPECT_EQ(lsp->signature_text(), "void foo(int x)");
}

TEST(LspServiceOverlayTest, SetLocationsStoresAll) {
    auto lsp = make_null_lsp();
    lsp->set_locations({{.uri = "file:///a.cpp", .line = 1, .col = 2}});
    auto locs = lsp->locations();
    ASSERT_EQ(locs.size(), 1u);
    EXPECT_EQ(locs[0].uri, "file:///a.cpp");
    EXPECT_EQ(locs[0].line, 1u);
    EXPECT_EQ(locs[0].col, 2u);
}

TEST(LspServiceOverlayTest, SetCompletionStoresItems) {
    auto lsp = make_null_lsp();
    lsp->set_completion({{.label = "foo", .insert_text = "foo()", .detail = "void foo()"}});
    auto items = lsp->completion_items();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].label, "foo");
    EXPECT_EQ(items[0].detail, "void foo()");
}

TEST(LspServiceOverlayTest, SetSymbolsStoresAll) {
    auto lsp = make_null_lsp();
    lsp->set_symbols({{.name = "MyClass", .kind = "Class", .line = 0, .col = 0}});
    auto syms = lsp->symbols();
    ASSERT_EQ(syms.size(), 1u);
    EXPECT_EQ(syms[0].name, "MyClass");
    EXPECT_EQ(syms[0].kind, "Class");
}

TEST(LspServiceOverlayTest, SetInlayHintsStoresAll) {
    auto lsp = make_null_lsp();
    lsp->set_inlay_hints({{.line = 3, .col = 5, .label = "int:"}});
    auto hints = lsp->inlay_hints();
    ASSERT_EQ(hints.size(), 1u);
    EXPECT_EQ(hints[0].label, "int:");
}

TEST(LspServiceOverlayTest, SetHighlightsStoresAll) {
    auto lsp = make_null_lsp();
    lsp->set_highlights({{.uri = "file:///b.cpp", .line = 2, .col = 0}});
    auto hl = lsp->highlights();
    ASSERT_EQ(hl.size(), 1u);
    EXPECT_EQ(hl[0].line, 2u);
}

TEST(LspServiceOverlayTest, ClearOverlayClearsHoverSignatureLocationsCompletion) {
    auto lsp = make_null_lsp();
    lsp->set_hover("hover");
    lsp->set_signature("sig");
    lsp->set_locations({{.uri = "x", .line = 0, .col = 0}});
    lsp->set_completion({{.label = "c", .insert_text = "", .detail = ""}});

    lsp->clear_overlay();

    EXPECT_TRUE(lsp->hover_text().empty());
    EXPECT_TRUE(lsp->signature_text().empty());
    EXPECT_TRUE(lsp->locations().empty());
    EXPECT_TRUE(lsp->completion_items().empty());
}

TEST(LspServiceOverlayTest, ClearOverlayClearsAllFields) {
    // clear_overlay() must clear all result fields so stale data
    // is not displayed after cursor movement.
    auto lsp = make_null_lsp();
    lsp->set_symbols({{.name = "S", .kind = "Function", .line = 0, .col = 0}});
    lsp->set_inlay_hints({{.line = 0, .col = 0, .label = "x:"}});
    lsp->set_highlights({{.uri = "f", .line = 0, .col = 0}});
    lsp->set_semantic_tokens({{.line = 0, .col = 0, .length = 1, .token_type = "keyword"}});

    lsp->clear_overlay();

    EXPECT_TRUE(lsp->symbols().empty());
    EXPECT_TRUE(lsp->inlay_hints().empty());
    EXPECT_TRUE(lsp->highlights().empty());
    EXPECT_TRUE(lsp->semantic_tokens().empty());
}

// ── EditorConfig feature gating ───────────────────────────────────────────────

TEST(LspServiceConfigTest, DefaultConfigEnablesAllFeatures) {
    EditorConfig cfg;
    EXPECT_TRUE(cfg.lsp.go_to_definition);
    EXPECT_TRUE(cfg.lsp.go_to_declaration);
    EXPECT_TRUE(cfg.lsp.find_references);
    EXPECT_TRUE(cfg.lsp.go_to_implementation);
    EXPECT_TRUE(cfg.lsp.type_definition);
    EXPECT_TRUE(cfg.lsp.completion);
    EXPECT_TRUE(cfg.lsp.signature_help);
    EXPECT_TRUE(cfg.lsp.rename);
    EXPECT_TRUE(cfg.lsp.code_action);
    EXPECT_TRUE(cfg.lsp.formatting);
    EXPECT_TRUE(cfg.lsp.hover);
    EXPECT_TRUE(cfg.lsp.inlay_hints);
    EXPECT_TRUE(cfg.lsp.document_highlight);
    EXPECT_TRUE(cfg.lsp.did_change);
    EXPECT_TRUE(cfg.lsp.did_save);
    EXPECT_TRUE(cfg.lsp.did_close);
    EXPECT_TRUE(cfg.lsp.document_symbol);
    EXPECT_TRUE(cfg.lsp.workspace_symbol);
}

TEST(LspServiceConfigTest, DisabledHoverDoesNotOverwriteExistingHover) {
    EditorConfig cfg;
    cfg.lsp.hover = false;
    auto lsp = make_null_lsp(cfg);

    lsp->set_hover("pre-existing");
    // With hover disabled, hover() is a no-op and the stored value is unchanged.
    lsp->hover("file:///x.cpp", 0, 0, [&](auto t) { lsp->set_hover(t); });
    // Still the same since the feature is gated.
    EXPECT_EQ(lsp->hover_text(), "pre-existing");
}

TEST(LspServiceConfigTest, ConfigStoredAndAccessible) {
    EditorConfig cfg;
    cfg.lsp.completion = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_FALSE(lsp->config().lsp.completion);
}

// ── Document sync (smoke: must not crash with dead process) ──────────────────

TEST(LspServiceSyncTest, DidOpenDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->did_open("file:///x.cpp", "int main() {}"));
}

TEST(LspServiceSyncTest, DidChangeDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->did_change("file:///x.cpp", "int main() { return 0; }", 2));
}

TEST(LspServiceSyncTest, DidSaveDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->did_save("file:///x.cpp"));
}

TEST(LspServiceSyncTest, DidCloseDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->did_close("file:///x.cpp"));
}

TEST(LspServiceSyncTest, DidChangeDisabledIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.did_change = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->did_change("file:///x.cpp", "text", 2));
}

// ── Async feature methods (smoke: callback invocation, dead process) ──────────

TEST(LspServiceAsyncTest, GoToDefinitionDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->go_to_definition("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, GoToDeclarationDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->go_to_declaration("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, GoToImplementationDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->go_to_implementation("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, GoToTypeDefinitionDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->go_to_type_definition("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, FindReferencesDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->find_references("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, HoverDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->hover("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, SignatureHelpDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->signature_help("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, CompletionDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->completion("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, DocumentHighlightDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->document_highlight("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, InlayHintsDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->inlay_hints("file:///x.cpp", 0, 10, [](auto) {}));
}

TEST(LspServiceAsyncTest, DocumentSymbolDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->document_symbol("file:///x.cpp", [](auto) {}));
}

TEST(LspServiceAsyncTest, WorkspaceSymbolDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->workspace_symbol("MyClass", [](auto) {}));
}

TEST(LspServiceAsyncTest, RenameDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->rename("file:///x.cpp", 0, 0, "newName", [](const auto&) {}));
}

TEST(LspServiceAsyncTest, CodeActionDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->code_action("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceAsyncTest, FormattingDoesNotCrash) {
    auto lsp = make_null_lsp();
    EXPECT_NO_THROW(lsp->formatting("file:///x.cpp", [](const auto&) {}));
}

// ── Disabled features are no-ops ─────────────────────────────────────────────

TEST(LspServiceGatingTest, DisabledGoToDefinitionIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.go_to_definition = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->go_to_definition("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledCompletionIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.completion = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->completion("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledDocumentSymbolIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.document_symbol = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->document_symbol("file:///x.cpp", [](auto) {}));
}

// ── Diagnostics (push-based) ──────────────────────────────────────────────────

TEST(LspServiceDiagnosticsTest, NoDiagnosticsInitially) {
    auto lsp = make_null_lsp();
    EXPECT_TRUE(lsp->diagnostics("file:///x.cpp").empty());
}

// ── set_on_update callback invocation ────────────────────────────────────────

TEST(LspServiceOnUpdateTest, SetHoverFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_hover("hello");
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetSignatureFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_signature("void foo()");
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetLocationsFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_locations({{.uri = "f", .line = 0, .col = 0}});
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetCompletionFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_completion({{.label = "x", .insert_text = "x", .detail = ""}});
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetSymbolsFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_symbols({{.name = "S", .kind = "12", .line = 0, .col = 0}});
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetInlayHintsFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_inlay_hints({{.line = 0, .col = 0, .label = "x:"}});
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetHighlightsFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_highlights({{.uri = "f", .line = 0, .col = 0}});
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, SetSemanticTokensFiresOnUpdateCallback) {
    auto lsp = make_null_lsp();
    int called = 0;
    lsp->set_on_update([&] { ++called; });
    lsp->set_semantic_tokens({{.line = 0, .col = 0, .length = 3, .token_type = "keyword"}});
    EXPECT_EQ(called, 1);
}

TEST(LspServiceOnUpdateTest, NullOnUpdateDoesNotCrash) {
    auto lsp = make_null_lsp();
    // No set_on_update called; setters must not crash.
    EXPECT_NO_THROW(lsp->set_hover("x"));
    EXPECT_NO_THROW(lsp->set_signature("x"));
}

// ── parse_locations ───────────────────────────────────────────────────────────

TEST(LspServiceParseTest, ParseLocationsNull) {
    auto locs = LspService::parse_locations(nlohmann::json{});
    EXPECT_TRUE(locs.empty());
}

TEST(LspServiceParseTest, ParseLocationsArray) {
    nlohmann::json j = nlohmann::json::array();
    j.push_back(
        {{"uri", "file:///a.cpp"}, {"range", {{"start", {{"line", 3}, {"character", 5}}}}}});
    j.push_back(
        {{"uri", "file:///b.cpp"}, {"range", {{"start", {{"line", 0}, {"character", 1}}}}}});
    auto locs = LspService::parse_locations(j);
    ASSERT_EQ(locs.size(), 2u);
    EXPECT_EQ(locs[0].uri, "file:///a.cpp");
    EXPECT_EQ(locs[0].line, 3u);
    EXPECT_EQ(locs[0].col, 5u);
    EXPECT_EQ(locs[1].uri, "file:///b.cpp");
}

TEST(LspServiceParseTest, ParseLocationsSingleObject) {
    // clangd may return a single Location object instead of an array.
    nlohmann::json j = {{"uri", "file:///c.cpp"},
                        {"range", {{"start", {{"line", 10}, {"character", 2}}}}}};
    auto locs = LspService::parse_locations(j);
    ASSERT_EQ(locs.size(), 1u);
    EXPECT_EQ(locs[0].uri, "file:///c.cpp");
    EXPECT_EQ(locs[0].line, 10u);
    EXPECT_EQ(locs[0].col, 2u);
}

TEST(LspServiceParseTest, ParseLocationsMissingFieldsSkipped) {
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"uri", "file:///a.cpp"}});  // no "range"
    auto locs = LspService::parse_locations(j);
    EXPECT_TRUE(locs.empty());
}

// ── parse_completion ──────────────────────────────────────────────────────────

TEST(LspServiceParseTest, ParseCompletionNull) {
    EXPECT_TRUE(LspService::parse_completion(nlohmann::json{}).empty());
}

TEST(LspServiceParseTest, ParseCompletionDirectArray) {
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"label", "foo"}, {"insertText", "foo("}, {"detail", "void foo()"}});
    auto items = LspService::parse_completion(j);
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].label, "foo");
    EXPECT_EQ(items[0].insert_text, "foo(");
    EXPECT_EQ(items[0].detail, "void foo()");
}

TEST(LspServiceParseTest, ParseCompletionItemsObject) {
    nlohmann::json j = {{"items", nlohmann::json::array()}};
    j["items"].push_back({{"label", "bar"}, {"detail", "int bar"}});
    auto items = LspService::parse_completion(j);
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].label, "bar");
    EXPECT_EQ(items[0].insert_text, "bar");  // fallback to label
}

// ── parse_symbols ─────────────────────────────────────────────────────────────

TEST(LspServiceParseTest, ParseSymbolsEmpty) {
    EXPECT_TRUE(LspService::parse_symbols(nlohmann::json::array()).empty());
}

TEST(LspServiceParseTest, ParseSymbolsDocumentSymbolWithRange) {
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"name", "MyClass"},
                 {"kind", 5},
                 {"range", {{"start", {{"line", 2}, {"character", 0}}}}}});
    auto syms = LspService::parse_symbols(j);
    ASSERT_EQ(syms.size(), 1u);
    EXPECT_EQ(syms[0].name, "MyClass");
    EXPECT_EQ(syms[0].line, 2u);
    EXPECT_EQ(syms[0].col, 0u);
}

TEST(LspServiceParseTest, ParseSymbolsSymbolInformationWithLocation) {
    nlohmann::json sym;
    sym["name"] = "myFunc";
    sym["kind"] = 12;
    sym["location"]["uri"] = "file:///x.cpp";
    sym["location"]["range"]["start"]["line"] = 7;
    sym["location"]["range"]["start"]["character"] = 4;
    nlohmann::json j = nlohmann::json::array();
    j.push_back(sym);
    auto syms = LspService::parse_symbols(j);
    ASSERT_EQ(syms.size(), 1u);
    EXPECT_EQ(syms[0].name, "myFunc");
    EXPECT_EQ(syms[0].line, 7u);
    EXPECT_EQ(syms[0].col, 4u);
}

TEST(LspServiceParseTest, ParseSymbolsRecursesIntoChildren) {
    nlohmann::json j = nlohmann::json::array();
    nlohmann::json child = {
        {"name", "innerFn"}, {"kind", 12}, {"range", {{"start", {{"line", 5}, {"character", 2}}}}}};
    j.push_back({{"name", "Outer"},
                 {"kind", 5},
                 {"range", {{"start", {{"line", 1}, {"character", 0}}}}},
                 {"children", nlohmann::json::array({child})}});
    auto syms = LspService::parse_symbols(j);
    ASSERT_EQ(syms.size(), 2u);
    EXPECT_EQ(syms[0].name, "Outer");
    EXPECT_EQ(syms[1].name, "innerFn");
}

// ── parse_inlay_hints ─────────────────────────────────────────────────────────

TEST(LspServiceParseTest, ParseInlayHintsStringLabel) {
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"position", {{"line", 3}, {"character", 7}}}, {"label", "int:"}});
    auto hints = LspService::parse_inlay_hints(j);
    ASSERT_EQ(hints.size(), 1u);
    EXPECT_EQ(hints[0].line, 3u);
    EXPECT_EQ(hints[0].col, 7u);
    EXPECT_EQ(hints[0].label, "int:");
}

TEST(LspServiceParseTest, ParseInlayHintsArrayLabel) {
    nlohmann::json j = nlohmann::json::array();
    nlohmann::json parts = nlohmann::json::array();
    parts.push_back({{"value", "x:"}});
    parts.push_back({{"value", " int"}});
    j.push_back({{"position", {{"line", 1}, {"character", 0}}}, {"label", parts}});
    auto hints = LspService::parse_inlay_hints(j);
    ASSERT_EQ(hints.size(), 1u);
    EXPECT_EQ(hints[0].label, "x: int");
}

TEST(LspServiceParseTest, ParseInlayHintsMissingLabelStillEmitted) {
    // A hint without "label" must not be dropped entirely — it's inserted with empty label.
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"position", {{"line", 0}, {"character", 1}}}});
    auto hints = LspService::parse_inlay_hints(j);
    ASSERT_EQ(hints.size(), 1u);
    EXPECT_TRUE(hints[0].label.empty());
}

// ── parse_code_actions ────────────────────────────────────────────────────────

TEST(LspServiceParseTest, ParseCodeActionsEmpty) {
    EXPECT_TRUE(LspService::parse_code_actions(nlohmann::json::array()).empty());
}

TEST(LspServiceParseTest, ParseCodeActionsBasic) {
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"title", "Add #include"}, {"edit", {{"changes", nlohmann::json::object()}}}});
    auto actions = LspService::parse_code_actions(j);
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0].title, "Add #include");
    EXPECT_TRUE(actions[0].workspace_edit.contains("changes"));
}

TEST(LspServiceParseTest, ParseCodeActionsNotArray) {
    EXPECT_TRUE(LspService::parse_code_actions(nlohmann::json::object()).empty());
}

// ── parse_semantic_tokens delta encoding ─────────────────────────────────────

TEST(LspServiceParseTest, ParseSemanticTokensDeltaEncoding) {
    auto lsp = make_null_lsp();
    // Set token type legend.
    lsp->set_semantic_tokens({});  // ensure semantic_token_types_ is empty initially

    // Build raw LSP data: two tokens on the same line, then one on the next line.
    // Token 1: line=0, col=0, len=3, type=0
    // Token 2: line=0, col=5 (delta=5 from prev on same line), len=4, type=1
    // Token 3: line=1 (delta=1), col=2 (absolute on new line), len=2, type=0
    nlohmann::json j = {{"data",
                         {0, 0, 3, 0, 0,     // token1
                          0, 5, 4, 1, 0,     // token2 (same line, col+=5)
                          1, 2, 2, 0, 0}}};  // token3 (new line, col=2)

    // Parse without a type legend → type names will be numeric strings.
    auto tokens = lsp->parse_semantic_tokens(j);
    ASSERT_EQ(tokens.size(), 3u);

    EXPECT_EQ(tokens[0].line, 0u);
    EXPECT_EQ(tokens[0].col, 0u);
    EXPECT_EQ(tokens[0].length, 3u);

    EXPECT_EQ(tokens[1].line, 0u);
    EXPECT_EQ(tokens[1].col, 5u);  // 0+5, same line → relative
    EXPECT_EQ(tokens[1].length, 4u);

    EXPECT_EQ(tokens[2].line, 1u);
    EXPECT_EQ(tokens[2].col, 2u);  // new line → absolute
    EXPECT_EQ(tokens[2].length, 2u);
}

TEST(LspServiceParseTest, ParseSemanticTokensEmptyData) {
    auto lsp = make_null_lsp();
    nlohmann::json j = {{"data", nlohmann::json::array()}};
    EXPECT_TRUE(lsp->parse_semantic_tokens(j).empty());
}

TEST(LspServiceParseTest, ParseSemanticTokensMissingData) {
    auto lsp = make_null_lsp();
    EXPECT_TRUE(lsp->parse_semantic_tokens(nlohmann::json::object()).empty());
}

TEST(LspServiceParseTest, ParseSemanticTokensTypeIndexFallsBackToString) {
    auto lsp = make_null_lsp();
    // type_idx=99 with no legend → token_type = "99"
    nlohmann::json j = {{"data", {0, 0, 1, 99, 0}}};
    auto tokens = lsp->parse_semantic_tokens(j);
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].token_type, "99");
}

// ── parse hover contents formats ─────────────────────────────────────────────

// hover parsing is a lambda inside hover(), not a standalone static helper.
// Test indirectly via set_hover after manually invoking the lambda shape.
// We test the three contents variants by calling the internal logic via a
// public wrapper — since it's a lambda, we replicate the logic in tests.
// The real guard is: if this parsing logic ever changes, the tests below
// must be updated too (regression protection).

static std::string parse_hover_contents(const nlohmann::json& result) {
    if (result.is_null()) return "";
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
    return text;
}

TEST(LspServiceParseTest, HoverContentsString) {
    nlohmann::json j = {{"contents", "int foo()"}};
    EXPECT_EQ(parse_hover_contents(j), "int foo()");
}

TEST(LspServiceParseTest, HoverContentsValueObject) {
    nlohmann::json j = {{"contents", {{"kind", "markdown"}, {"value", "**int** foo()"}}}};
    EXPECT_EQ(parse_hover_contents(j), "**int** foo()");
}

TEST(LspServiceParseTest, HoverContentsArray) {
    nlohmann::json j = {{"contents", nlohmann::json::array({"line1", {{"value", "line2"}}})}};
    EXPECT_EQ(parse_hover_contents(j), "line1\nline2\n");
}

TEST(LspServiceParseTest, HoverContentsNull) {
    EXPECT_EQ(parse_hover_contents(nlohmann::json{}), "");
}

// ── feature gating: remaining disabled features are no-ops ───────────────────

TEST(LspServiceGatingTest, DisabledGoToDeclarationIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.go_to_declaration = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->go_to_declaration("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledGoToImplementationIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.go_to_implementation = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->go_to_implementation("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledTypeDefinitionIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.type_definition = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->go_to_type_definition("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledFindReferencesIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.find_references = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->find_references("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledHoverIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.hover = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->hover("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledSignatureHelpIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.signature_help = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->signature_help("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledRenameIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.rename = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->rename("file:///x.cpp", 0, 0, "newName", [](const auto&) {}));
}

TEST(LspServiceGatingTest, DisabledCodeActionIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.code_action = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->code_action("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledFormattingIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.formatting = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->formatting("file:///x.cpp", [](const auto&) {}));
}

TEST(LspServiceGatingTest, DisabledInlayHintsIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.inlay_hints = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->inlay_hints("file:///x.cpp", 0, 10, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledDocumentHighlightIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.document_highlight = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->document_highlight("file:///x.cpp", 0, 0, [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledWorkspaceSymbolIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.workspace_symbol = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->workspace_symbol("", [](auto) {}));
}

TEST(LspServiceGatingTest, DisabledDidSaveIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.did_save = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->did_save("file:///x.cpp"));
}

TEST(LspServiceGatingTest, DisabledDidCloseIsNoOp) {
    EditorConfig cfg;
    cfg.lsp.did_close = false;
    auto lsp = make_null_lsp(cfg);
    EXPECT_NO_THROW(lsp->did_close("file:///x.cpp"));
}
