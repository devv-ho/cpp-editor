// Defines EditorCommands -- one free function per vim action.
//
// Each function receives a Document& and mutates buffer and/or cursor.
// Mode policy (normal: col <= len-1, insert: col <= len) is enforced here.

#pragma once

#include <tuple>

#include "core/entities/Document.hpp"

namespace editor::core::commands {

// -- Helpers ------------------------------------------------------------------

inline void clamp_normal(Document& doc) {
    auto& cursor = doc.cursor();
    std::size_t len = doc.buffer().line_length(cursor.line()).value_or(0);
    if (len > 0 && cursor.col() >= len) {
        cursor.set_position({cursor.line(), len - 1});
    } else if (len == 0) {
        cursor.set_position({cursor.line(), 0});
    }
}

// -- Normal mode motions ------------------------------------------------------

inline void move_left(Document& doc) { doc.cursor().move_left(); }

inline void move_right(Document& doc) {
    auto& cursor = doc.cursor();
    std::size_t len = doc.buffer().line_length(cursor.line()).value_or(0);
    if (len > 0 && cursor.col() < len - 1) {
        cursor.move_right();
    }
}

inline void move_up(Document& doc) {
    doc.cursor().move_up();
    clamp_normal(doc);
}

inline void move_down(Document& doc) {
    doc.cursor().move_down();
    clamp_normal(doc);
}

inline void move_top(Document& doc) { doc.cursor().move_top(); }

inline void move_bottom(Document& doc) { doc.cursor().move_bottom(); }

inline void move_sol(Document& doc) { doc.cursor().move_sol(); }

inline void move_eol(Document& doc) { doc.cursor().move_eol(); }

// -- Mode transitions ---------------------------------------------------------

// 'i' enters insert mode at the current position -- no cursor movement needed.
inline void enter_insert(Document& doc) { (void)doc; }

// 'a' enters insert mode one position after the cursor (vim's append).
inline void enter_insert_after(Document& doc) {
    auto& cursor = doc.cursor();
    std::size_t len = doc.buffer().line_length(cursor.line()).value_or(0);
    if (cursor.col() < len) {
        cursor.move_right();
    }
}

// ESC returns to normal mode; clamps col to len-1 (normal mode ceiling).
inline void enter_normal(Document& doc) { clamp_normal(doc); }

// -- Insert mode edits --------------------------------------------------------

inline void insert_char(Document& doc, char ch) {
    auto pos = doc.position();
    std::ignore = doc.insert_char(pos, ch);
    doc.cursor().move_right();
}

inline void insert_newline(Document& doc) {
    auto pos = doc.position();
    std::ignore = doc.insert_newline(pos);
    doc.cursor().set_position({pos.line + 1, 0});
}

inline void backspace(Document& doc) {
    auto pos = doc.position();
    if (pos.col > 0) {
        doc.cursor().move_left();
        std::ignore = doc.delete_char({pos.line, pos.col - 1});
    } else if (pos.line > 0) {
        std::size_t prev_len = doc.buffer().line_length(pos.line - 1).value_or(0);
        std::ignore = doc.join_with_prev(pos);
        doc.cursor().set_position({pos.line - 1, prev_len});
    }
}

}  // namespace editor::core::commands
