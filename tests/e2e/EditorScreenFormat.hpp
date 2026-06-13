// EditorScreenFormat.hpp -- screen format helpers for e2e tests.
// Derives status bar and diagnostic formats from src/drivers/ScreenFormat.hpp
// so that a renderer format change propagates here automatically.

#pragma once

#include <string>
#include <string_view>

#include "drivers/ScreenFormat.hpp"

namespace editor::e2e::fmt {

// Re-export mode tokens.
using drivers::screen_fmt::kInsert;
using drivers::screen_fmt::kNormal;

// Re-export diagnostic severity labels.
using drivers::screen_fmt::kDiagError;
using drivers::screen_fmt::kDiagHint;
using drivers::screen_fmt::kDiagInfo;
using drivers::screen_fmt::kDiagWarning;

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

// Returns the expected diagnostic line prefix for a given severity label.
// Format: "{severity} {line}:{col}  {message}" -- the prefix up to and
// including the severity token is enough for PtyHarness::validate_buffer().
inline std::string diag_prefix(std::string_view severity) { return std::string(severity); }

}  // namespace editor::e2e::fmt
