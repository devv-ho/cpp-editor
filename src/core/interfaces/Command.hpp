#pragma once

namespace editor::core {

enum class Command {
    // Normal mode motions
    move_left,
    move_right,
    move_up,
    move_down,
    move_top,
    move_bottom,
    move_sol,
    move_eol,
    // Mode switches
    enter_insert,
    enter_insert_after,
    enter_normal,
    // Insert mode commands
    insert_char,
    insert_newline,
    backspace,
    // Pending (multi-key sequences)
    pending_g,
    pending_space,
    // LSP navigation (Normal mode)
    lsp_definition,
    lsp_declaration,
    lsp_implementation,
    lsp_type_definition,
    lsp_references,
    lsp_hover,
    lsp_rename,
    lsp_code_action,
    lsp_formatting,
    lsp_document_symbol,
    lsp_workspace_symbol,
    // Quit
    quit,
};

}  // namespace editor::core
