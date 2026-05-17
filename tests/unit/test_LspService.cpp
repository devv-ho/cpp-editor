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

TEST(LspServiceOverlayTest, ClearOverlayPreservesSymbolsAndInlayHints) {
    // symbols/inlay_hints are populated by explicit requests, not hover flows,
    // so clear_overlay() intentionally leaves them intact.
    auto lsp = make_null_lsp();
    lsp->set_symbols({{.name = "S", .kind = "Function", .line = 0, .col = 0}});
    lsp->set_inlay_hints({{.line = 0, .col = 0, .label = "x:"}});

    lsp->clear_overlay();

    EXPECT_FALSE(lsp->symbols().empty());
    EXPECT_FALSE(lsp->inlay_hints().empty());
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
