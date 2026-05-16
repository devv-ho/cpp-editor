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
    insert_newline,
    backspace,
    // Pending (multi-key sequences)
    pending_g,
    // Quit
    quit,
};

}  // namespace editor::core
