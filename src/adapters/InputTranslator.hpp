// Translates ftxui::Event to the editor's internal Key vocabulary.

#pragma once

#include <ftxui/component/event.hpp>
#include <optional>

#include "core/interfaces/Key.hpp"

namespace editor::adapters {

[[nodiscard]] inline std::optional<editor::core::Key> translate(const ftxui::Event& event) {
    if (event == ftxui::Event::Escape) {
        return core::Key::Escape;
    }
    if (event == ftxui::Event::Backspace) {
        return core::Key::Backspace;
    }
    if (event == ftxui::Event::Return) {
        return core::Key::Return;
    }

    if (!event.is_character()) {
        return std::nullopt;
    }

    const std::string& ch = event.character();
    if (ch == "h") {
        return core::Key::H;
    }
    if (ch == "j") {
        return core::Key::J;
    }
    if (ch == "k") {
        return core::Key::K;
    }
    if (ch == "l") {
        return core::Key::L;
    }
    if (ch == "g") {
        return core::Key::G_lower;
    }
    if (ch == "G") {
        return core::Key::G_upper;
    }
    if (ch == "$") {
        return core::Key::Dollar;
    }
    if (ch == "0") {
        return core::Key::Zero;
    }
    if (ch == "i") {
        return core::Key::I;
    }
    if (ch == "a") {
        return core::Key::A;
    }

    return std::nullopt;
}

}  // namespace editor::adapters
