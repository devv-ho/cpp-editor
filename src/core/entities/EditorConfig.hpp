#pragma once

namespace editor::core {

struct LspFeatures {
    bool go_to_definition = true;
    bool go_to_declaration = true;
    bool find_references = true;
    bool go_to_implementation = true;
    bool type_definition = true;
    bool completion = true;
    bool signature_help = true;
    bool rename = true;
    bool code_action = true;
    bool formatting = true;
    bool hover = true;
    bool inlay_hints = true;
    bool document_highlight = true;
    bool did_change = true;
    bool did_save = true;
    bool did_close = true;
    bool document_symbol = true;
    bool workspace_symbol = true;
    bool semantic_tokens = true;
};

struct EditorConfig {
    LspFeatures lsp;
};

}  // namespace editor::core
