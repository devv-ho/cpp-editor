// EditorScreenFormat.hpp -- canonical screen format constants shared between
// PtyHarness validators and tests. Must stay in sync with FtxuiRenderer.
//
// FtxuiRenderer::render_statusbar() produces:
//   " {MODE}  {uri}"   (one leading space, two trailing spaces before uri)
//
// FtxuiRenderer::render_buffer() produces:
//   raw file content lines; cursor position rendered with inverted style
//   (ANSI is stripped by PtyHarness before validation)

#pragma once

#include <string>
#include <string_view>

namespace editor::e2e::fmt {

// ── Mode tokens ───────────────────────────────────────────────────────────────
// These must exactly match the labels in FtxuiRenderer::render_statusbar().

inline constexpr std::string_view kNormal = "NORMAL";
inline constexpr std::string_view kInsert = "INSERT";

// ── Status bar ────────────────────────────────────────────────────────────────

// Returns the full expected status bar prefix for a given mode token.
// Format: " {mode}  " (one leading space, two trailing spaces).
// PtyHarness::validate_status() checks that the status bar line starts with
// this prefix — the uri suffix is not validated to keep tests path-independent.
inline std::string status_prefix(std::string_view mode) { return " " + std::string(mode) + "  "; }

// ── Buffer ────────────────────────────────────────────────────────────────────

// Returns the expected text for a buffer line. Currently an identity function;
// exists so tests are insulated from future render_buffer() changes (e.g. line
// numbers, decorators) that would require a prefix/suffix to be stripped.
inline std::string buffer_line(std::string_view content) { return std::string(content); }

}  // namespace editor::e2e::fmt
