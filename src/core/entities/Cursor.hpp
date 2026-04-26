#pragma once

#include "Buffer.hpp"
#include "Position.hpp"

#include <algorithm>
#include <cstddef>

namespace editor::core {

/// Owns the caret position and enforces buffer boundary invariants.
/// All movement methods clamp silently — callers never receive out-of-range
/// positions.
///
/// Holds a reference to Buffer. Safe because Cursor is always a private member
/// of Document, which also owns the Buffer — so Buffer always outlives Cursor.
class Cursor {
public:
  explicit Cursor(const Buffer &buffer) : buffer_{buffer} {}

  // ── Queries ───────────────────────────────────────────────────────────────

  [[nodiscard]] Position position() const noexcept { return pos_; }
  [[nodiscard]] std::size_t line() const noexcept { return pos_.line; }
  [[nodiscard]] std::size_t col() const noexcept { return pos_.col; }

  // ── Motions ───────────────────────────────────────────────────────────────

  void move_left() noexcept {
    if (pos_.col > 0)
      --pos_.col;
  }

  void move_right() noexcept {
    auto len = line_length(pos_.line);
    if (pos_.col < len)
      ++pos_.col;
  }

  void move_up() noexcept {
    if (pos_.line > 0) {
      --pos_.line;
      clamp_col();
    }
  }

  void move_down() noexcept {
    if (pos_.line + 1 < buffer_.line_count()) {
      ++pos_.line;
      clamp_col();
    }
  }

  /// gg — first line, first column.
  void move_top() noexcept { pos_ = {0, 0}; }

  /// G — last line, first column.
  void move_bottom() noexcept {
    pos_.line = buffer_.line_count() - 1;
    pos_.col = 0;
  }

  /// 0 — start of current line.
  void move_sol() noexcept { pos_.col = 0; }

  /// $ — last character of current line.
  void move_eol() noexcept {
    auto len = line_length(pos_.line);
    pos_.col = len > 0 ? len - 1 : 0;
  }

  void set_position(Position pos) noexcept {
    pos_.line = std::min(pos.line, buffer_.line_count() - 1);
    pos_.col = pos.col;
    clamp_col();
  }

private:
  const Buffer &buffer_;
  Position pos_{};

  [[nodiscard]] std::size_t line_length(std::size_t line) const noexcept {
    return buffer_.line_length(line).value_or(0);
  }

  void clamp_col() noexcept {
    auto len = line_length(pos_.line);
    pos_.col = len > 0 ? std::min(pos_.col, len - 1) : 0;
  }
};

} // namespace editor::core
