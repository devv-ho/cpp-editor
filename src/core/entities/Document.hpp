#pragma once

#include "Buffer.hpp"
#include "Cursor.hpp"
#include "Position.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace editor::core {

/// Composes Buffer and Cursor. The single unit of state the editor operates on.
/// Use cases and commands receive a Document reference — they never touch
/// Buffer or Cursor directly.
class Document {
public:
  explicit Document(std::string_view text = "")
      : buffer_{text}, cursor_{buffer_} {}

  // ── Buffer access ─────────────────────────────────────────────────────────

  [[nodiscard]] const Buffer &buffer() const noexcept { return buffer_; }

  [[nodiscard]] std::size_t line_count() const noexcept {
    return buffer_.line_count();
  }

  [[nodiscard]] std::expected<std::string_view, BufferError>
  line(std::size_t idx) const noexcept {
    return buffer_.line(idx);
  }

  // ── Cursor access ─────────────────────────────────────────────────────────

  [[nodiscard]] Position position() const noexcept {
    return cursor_.position();
  }
  [[nodiscard]] Cursor &cursor() noexcept { return cursor_; }

  // ── Mutations ─────────────────────────────────────────────────────────────

  [[nodiscard]] std::expected<void, BufferError> insert_char(char ch) {
    auto result = buffer_.insert_char(cursor_.position(), ch);
    if (result)
      cursor_.advance_col();
    return result;
  }

  /// Delete key: remove character at cursor position. Cursor does not move.
  [[nodiscard]] std::expected<void, BufferError> delete_char() {
    return buffer_.delete_char(cursor_.position());
  }

  /// Backspace: move cursor left then delete the character to its left.
  /// At col 0, joins the current line with the previous line.
  [[nodiscard]] std::expected<void, BufferError> delete_char_before() {
    if (cursor_.col() == 0) {
      if (cursor_.line() == 0)
        return std::unexpected(BufferError::LineOutOfRange);
      const std::size_t prev_len =
          buffer_.line_length(cursor_.line() - 1).value_or(0);
      auto result = buffer_.join_with_prev(cursor_.position());
      if (result)
        cursor_.set_position({cursor_.line() - 1, prev_len});
      return result;
    }
    cursor_.retreat_col();
    return buffer_.delete_char(cursor_.position());
  }

  [[nodiscard]] std::expected<void, BufferError> insert_newline() {
    auto result = buffer_.insert_newline(cursor_.position());
    if (result)
      cursor_.set_position({cursor_.line() + 1, 0});
    return result;
  }

  // ── Serialisation ─────────────────────────────────────────────────────────

  [[nodiscard]] std::string to_string() const { return buffer_.to_string(); }

  // ── File path ─────────────────────────────────────────────────────────────

  void set_path(std::filesystem::path path) noexcept {
    path_ = std::move(path);
  }

  [[nodiscard]] const std::filesystem::path &path() const noexcept {
    return path_;
  }

private:
  Buffer buffer_;
  Cursor cursor_;
  std::filesystem::path path_;
};

} // namespace editor::core
