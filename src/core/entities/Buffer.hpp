#pragma once

#include "Position.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace editor::core {

enum class BufferError {
  LineOutOfRange,
  ColOutOfRange,
};

/// Owns the file text as a sequence of lines (without newline characters).
/// All mutating operations return std::expected — no exceptions cross layer
/// boundaries.
class Buffer {
public:
  Buffer() : lines_{""} {}

  explicit Buffer(std::string_view text) {
    lines_.clear();
    std::size_t start = 0;
    while (true) {
      auto end = text.find('\n', start);
      if (end == std::string_view::npos) {
        lines_.emplace_back(text.substr(start));
        break;
      }
      lines_.emplace_back(text.substr(start, end - start));
      start = end + 1;
    }
    if (lines_.empty())
      lines_.emplace_back("");
  }

  // ── Queries ───────────────────────────────────────────────────────────────

  [[nodiscard]] std::size_t line_count() const noexcept {
    return lines_.size();
  }

  [[nodiscard]] std::expected<std::string_view, BufferError>
  line(std::size_t idx) const noexcept {
    if (idx >= lines_.size())
      return std::unexpected(BufferError::LineOutOfRange);
    return lines_[idx];
  }

  [[nodiscard]] std::expected<std::size_t, BufferError>
  line_length(std::size_t idx) const noexcept {
    if (idx >= lines_.size())
      return std::unexpected(BufferError::LineOutOfRange);
    return lines_[idx].size();
  }

  [[nodiscard]] bool is_empty() const noexcept {
    return lines_.size() == 1 && lines_[0].empty();
  }

  [[nodiscard]] std::string to_string() const {
    std::string out;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
      out += lines_[i];
      if (i + 1 < lines_.size())
        out += '\n';
    }
    return out;
  }

  // ── Mutations ─────────────────────────────────────────────────────────────

  [[nodiscard]] std::expected<void, BufferError> insert_char(Position pos,
                                                             char ch) {
    if (pos.line >= lines_.size())
      return std::unexpected(BufferError::LineOutOfRange);
    auto &ln = lines_[pos.line];
    if (pos.col > ln.size())
      return std::unexpected(BufferError::ColOutOfRange);
    ln.insert(pos.col, 1, ch);
    return {};
  }

  [[nodiscard]] std::expected<void, BufferError> delete_char(Position pos) {
    if (pos.line >= lines_.size())
      return std::unexpected(BufferError::LineOutOfRange);
    auto &ln = lines_[pos.line];
    if (pos.col >= ln.size())
      return std::unexpected(BufferError::ColOutOfRange);
    ln.erase(pos.col, 1);
    return {};
  }

  /// Split the current line at pos, inserting a new line below.
  [[nodiscard]] std::expected<void, BufferError> insert_newline(Position pos) {
    if (pos.line >= lines_.size())
      return std::unexpected(BufferError::LineOutOfRange);
    auto &ln = lines_[pos.line];
    if (pos.col > ln.size())
      return std::unexpected(BufferError::ColOutOfRange);
    std::string tail = ln.substr(pos.col);
    ln.erase(pos.col);
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(pos.line) + 1,
                  std::move(tail));
    return {};
  }

private:
  std::vector<std::string> lines_;
};

} // namespace editor::core
