// EditorScreenFormat.hpp -- screen format helpers for e2e tests.
// Derives status bar format from src/drivers/ScreenFormat.hpp so that
// a renderer format change propagates here automatically.

#pragma once

#include <string>
#include <string_view>

#include "drivers/ScreenFormat.hpp"

namespace editor::e2e::fmt {

// Re-export mode tokens.
using drivers::screen_fmt::kInsert;
using drivers::screen_fmt::kNormal;

// Returns the exact prefix that the status bar line starts with for a given
// mode token. PtyHarness::validate_status() checks this prefix against the
// last non-empty frame line.
inline std::string status_prefix(std::string_view mode) {
    using namespace drivers::screen_fmt;
    return std::string(kModeLeading) + std::string(mode) + std::string(kModeTrailing) +
           std::string(kUriSeparator);
}

// Returns the expected text for a buffer line. Currently an identity function;
// exists so tests are insulated from future render_buffer() changes (e.g. line
// numbers, decorators) that would require a prefix/suffix to be stripped.
inline std::string buffer_line(std::string_view content) { return std::string(content); }

}  // namespace editor::e2e::fmt
