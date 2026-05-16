#pragma once

#include <ftxui/component/event.hpp>
#include <optional>

#include "core/interfaces/Command.hpp"
#include "core/usecases/EditorMode.hpp"

namespace editor::adapters {

// Maps a raw terminal event to a Command given the current editor mode.
// Returns nullopt for Insert-mode typing (caller treats event as insert_char).
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
        return std::nullopt;
    }

    // Insert mode: all characters are raw typing.
    return std::nullopt;
}

}  // namespace editor::adapters
