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

  // ── Buffer mutations ──────────────────────────────────────────────────────
  // These only modify text. Cursor movement after a mutation is the
  // responsibility of the command layer (EditorCommands).

  [[nodiscard]] std::expected<void, BufferError> insert_char(Position pos,
                                                             char ch) {
    return buffer_.insert_char(pos, ch);
  }

  [[nodiscard]] std::expected<void, BufferError> delete_char(Position pos) {
    return buffer_.delete_char(pos);
  }

  [[nodiscard]] std::expected<void, BufferError> join_with_prev(Position pos) {
    return buffer_.join_with_prev(pos);
  }

  [[nodiscard]] std::expected<void, BufferError> insert_newline(Position pos) {
    return buffer_.insert_newline(pos);
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
