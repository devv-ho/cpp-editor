#pragma once

#include <ftxui/component/event.hpp>
#include <optional>

#include "core/interfaces/Command.hpp"

namespace editor::adapters {

[[nodiscard]] inline std::optional<core::Command> translate(const ftxui::Event& event) {
    // Control keys -- always commands regardless of mode.
    if (event == ftxui::Event::Escape) return core::Command::enter_normal;
    if (event == ftxui::Event::Backspace) return core::Command::backspace;
    if (event == ftxui::Event::Return) return core::Command::insert_newline;

    if (!event.is_character()) return std::nullopt;

    const std::string& ch = event.character();

    // Normal mode motion / mode-switch commands.
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

}  // namespace editor::adapters
