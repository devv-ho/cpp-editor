// Defines Position, a zero-based (line, column) coordinate in the buffer.

#pragma once

#include <compare>
#include <cstddef>
#include <format>
#include <string>

namespace editor::core {

// A zero-based (line, column) coordinate in the buffer.
struct Position {
    std::size_t line{0};
    std::size_t col{0};

    [[nodiscard]] auto operator<=>(const Position&) const noexcept = default;

    [[nodiscard]] std::string to_string() const { return std::format("({}, {})", line, col); }
};

}  // namespace editor::core
