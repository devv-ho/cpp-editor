// Translates ftxui::Event to the editor's internal Key vocabulary.

#pragma once

#include <ftxui/component/event.hpp>
#include <optional>

#include "core/interfaces/Key.hpp"

namespace editor::adapters {

[[nodiscard]] inline std::optional<editor::core::Key> translate(const ftxui::Event& event) {
    if (event == ftxui::Event::Escape) {
        return core::Key::escape;
    }
    if (event == ftxui::Event::Backspace) {
        return core::Key::backspace;
    }
    if (event == ftxui::Event::Return) {
        return core::Key::enter;
    }

    if (!event.is_character()) {
        return std::nullopt;
    }

    const std::string& ch = event.character();
    if (ch == "h") {
        return core::Key::h;
    }
    if (ch == "j") {
        return core::Key::j;
    }
    if (ch == "k") {
        return core::Key::k;
    }
    if (ch == "l") {
        return core::Key::l;
    }
    if (ch == "g") {
        return core::Key::g;
    }
    if (ch == "G") {
        return core::Key::G;
    }
    if (ch == "$") {
        return core::Key::dollar;
    }
    if (ch == "0") {
        return core::Key::zero;
    }
    if (ch == "i") {
        return core::Key::i;
    }
    if (ch == "a") {
        return core::Key::a;
    }

    return std::nullopt;
}

}  // namespace editor::adapters
