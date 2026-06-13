// Defines Buffer, which owns file text as a sequence of lines.

#pragma once

#include <algorithm>
#include <expected>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "Position.hpp"

namespace editor::core {

enum class BufferError {
    LineOutOfRange,
    ColOutOfRange,
};

// Owns the file text as a sequence of lines (without newline characters).
// All mutating operations return std::expected -- no exceptions cross layer
// boundaries.
//
// Use Buffer::from_text() to construct from a string. The default constructor
// produces a single empty line.
class Buffer {
public:
    Buffer() : lines_{""} {}

    // Creates a Buffer from text, normalising CRLF to LF.
    [[nodiscard]] static Buffer from_text(std::string_view text) {
        Buffer buf;
        buf.lines_.clear();

        std::string normalized;
        normalized.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
                continue;
            }
            normalized += text[i];
        }

        for (const auto& part : normalized | std::views::split('\n')) {
            buf.lines_.emplace_back(part.begin(), part.end());
        }

        // std::views::split on an empty string yields 0 subranges on libstdc++ C++23.
        if (buf.lines_.empty()) {
            buf.lines_.emplace_back("");
        }

        return buf;
    }

    // Queries ----------------------------------------------------------------

    [[nodiscard]] std::size_t line_count() const noexcept { return lines_.size(); }

    [[nodiscard]] std::expected<std::string_view, BufferError> line(
        std::size_t idx) const noexcept {
        if (idx >= lines_.size()) {
            return std::unexpected(BufferError::LineOutOfRange);
        }
        return lines_[idx];
    }

    [[nodiscard]] std::expected<std::size_t, BufferError> line_length(
        std::size_t idx) const noexcept {
        if (idx >= lines_.size()) {
            return std::unexpected(BufferError::LineOutOfRange);
        }
        return lines_[idx].size();
    }

    [[nodiscard]] bool is_empty() const noexcept { return lines_.size() == 1 && lines_[0].empty(); }

    [[nodiscard]] std::string to_string() const {
        std::string output;
        for (std::size_t i = 0; i < lines_.size(); ++i) {
            output += lines_[i];
            if (i + 1 < lines_.size()) {
                output += '\n';
            }
        }
        return output;
    }

    // Mutations --------------------------------------------------------------

    [[nodiscard]] std::expected<void, BufferError> insert_char(Position pos, char ch) {
        if (pos.line >= lines_.size()) {
            return std::unexpected(BufferError::LineOutOfRange);
        }
        auto& line = lines_[pos.line];
        if (pos.col > line.size()) {
            return std::unexpected(BufferError::ColOutOfRange);
        }
        line.insert(pos.col, 1, ch);
        return {};
    }

    [[nodiscard]] std::expected<void, BufferError> delete_char(Position pos) {
        if (pos.line >= lines_.size()) {
            return std::unexpected(BufferError::LineOutOfRange);
        }
        auto& line = lines_[pos.line];
        if (pos.col >= line.size()) {
            return std::unexpected(BufferError::ColOutOfRange);
        }
        line.erase(pos.col, 1);
        return {};
    }

    // Join the previous line with the current line -- triggered by backspace at col 0.
    [[nodiscard]] std::expected<void, BufferError> join_with_prev(Position pos) {
        if (pos.line == 0 || pos.line >= lines_.size()) {
            return std::unexpected(BufferError::LineOutOfRange);
        }
        lines_[pos.line - 1] += lines_[pos.line];
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(pos.line));
        return {};
    }

    // Split the current line at pos -- triggered by Enter in insert mode.
    [[nodiscard]] std::expected<void, BufferError> insert_newline(Position pos) {
        if (pos.line >= lines_.size()) {
            return std::unexpected(BufferError::LineOutOfRange);
        }
        auto& line = lines_[pos.line];
        if (pos.col > line.size()) {
            return std::unexpected(BufferError::ColOutOfRange);
        }
        std::string tail = line.substr(pos.col);
        line.erase(pos.col);
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(pos.line) + 1, std::move(tail));
        return {};
    }

    // Applies one LSP TextEdit: replaces [start_line:start_col, end_line:end_col)
    // with new_text (which may contain '\n' for multi-line replacements).
    //
    // Callers applying multiple edits MUST call this in reverse document order
    // (highest line/col first) so earlier edits don't shift later offsets.
    void apply_text_edit(std::size_t start_line, std::size_t start_col, std::size_t end_line,
                         std::size_t end_col, std::string_view new_text) {
        // Clamp to valid range.
        start_line = std::min(start_line, lines_.size() > 0 ? lines_.size() - 1 : 0);
        end_line = std::min(end_line, lines_.size() > 0 ? lines_.size() - 1 : 0);
        start_col = std::min(start_col, lines_[start_line].size());
        end_col = std::min(end_col, lines_[end_line].size());

        // Collect the text that survives before the range and after the range.
        std::string prefix = lines_[start_line].substr(0, start_col);
        std::string suffix = lines_[end_line].substr(end_col);

        // Remove lines in [start_line, end_line].
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(start_line),
                     lines_.begin() + static_cast<std::ptrdiff_t>(end_line) + 1);

        // Split new_text on '\n' and insert the replacement lines.
        std::vector<std::string> replacement;
        std::size_t pos = 0;
        while (true) {
            auto nl = new_text.find('\n', pos);
            if (nl == std::string_view::npos) {
                replacement.emplace_back(new_text.substr(pos));
                break;
            }
            replacement.emplace_back(new_text.substr(pos, nl - pos));
            pos = nl + 1;
        }

        // Attach prefix/suffix to the first/last replacement line.
        replacement.front() = prefix + replacement.front();
        replacement.back() += suffix;

        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(start_line), replacement.begin(),
                      replacement.end());

        // Maintain invariant: never empty.
        if (lines_.empty()) lines_.emplace_back("");
    }

private:
    // Invariant: lines_ is never empty. Both constructors guarantee at least one element.
    std::vector<std::string> lines_;
};

}  // namespace editor::core
