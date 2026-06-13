#pragma once

#include <ftxui/component/event.hpp>
#include <optional>

#include "core/interfaces/Command.hpp"
#include "core/usecases/EditorMode.hpp"

namespace editor::adapters {

// Maps a raw terminal event to a Command given the current editor mode.
// Multi-key sequences (g*, <Space>*) are resolved by InputDispatcher which
// holds the pending_g_ / pending_space_ state between calls.
//
// Normal mode key map:
//   h/j/k/l       -- motion
//   gg            -- move_top   (g handled as pending_g; second g resolved in dispatcher)
//   G             -- move_bottom
//   0/$           -- move_sol / move_eol
//   i/a           -- enter insert / insert-after
//   gd            -- lsp_definition
//   gD            -- lsp_declaration
//   gI            -- lsp_implementation ('i' = enter_insert; uppercase avoids collision)
//   gy            -- lsp_type_definition
//   gr            -- lsp_references
//   K             -- lsp_hover
//   <Space>rn     -- lsp_rename      (pending_space -> 'r' -> 'n'; currently no-ops until prompt
//   wired) <Space>cA     -- lsp_code_action (pending_space -> 'c' -> 'A'; uppercase avoids
//   'a'=enter_insert_after) <Space>f      -- lsp_formatting <Space>s      -- lsp_document_symbol
//   <Space>S      -- lsp_workspace_symbol
//   ESC           -- quit (Normal) / enter_normal (Insert)
[[nodiscard]] inline std::optional<core::Command> translate(const ftxui::Event& event,
                                                            core::EditorMode mode) {
    using core::EditorMode;

    if (event == ftxui::Event::Escape) {
        return mode == EditorMode::Insert ? core::Command::enter_normal : core::Command::quit;
    }
    if (event == ftxui::Event::Backspace) return core::Command::backspace;
    if (event == ftxui::Event::Return) return core::Command::insert_newline;

    if (!event.is_character()) return std::nullopt;

    const std::string& ch = event.character();

    if (mode == EditorMode::Normal) {
        if (ch == "h") return core::Command::move_left;
        if (ch == "j") return core::Command::move_down;
        if (ch == "k") return core::Command::move_up;
        if (ch == "l") return core::Command::move_right;
        if (ch == "g") return core::Command::pending_g;
        if (ch == "G") return core::Command::move_bottom;
        if (ch == "0") return core::Command::move_sol;
        if (ch == "$") return core::Command::move_eol;
        if (ch == "i") return core::Command::enter_insert;
        if (ch == "a") return core::Command::enter_insert_after;
        if (ch == "K") return core::Command::lsp_hover;
        if (ch == " ") return core::Command::pending_space;
        // Second keys of multi-key sequences (resolved by dispatcher state machine).
        if (ch == "d") return core::Command::lsp_definition;   // gd
        if (ch == "D") return core::Command::lsp_declaration;  // gD
        if (ch == "I")
            return core::Command::lsp_implementation;  // gI (uppercase; 'i' = enter_insert)
        if (ch == "y") return core::Command::lsp_type_definition;   // gy
        if (ch == "r") return core::Command::lsp_references;        // gr  / <Space>r(n)
        if (ch == "n") return core::Command::lsp_rename;            // <Space>rn
        if (ch == "c") return core::Command::lsp_code_action;       // <Space>c(A)
        if (ch == "A") return core::Command::lsp_code_action;       // <Space>cA confirm key
        if (ch == "f") return core::Command::lsp_formatting;        // <Space>f
        if (ch == "s") return core::Command::lsp_document_symbol;   // <Space>s
        if (ch == "S") return core::Command::lsp_workspace_symbol;  // <Space>S
        return std::nullopt;
    }

    // Insert mode: all characters are typed into the buffer.
    return core::Command::insert_char;
}

}  // namespace editor::adapters
