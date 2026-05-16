#pragma once

#include <string_view>

namespace editor::drivers::screen_fmt {

// Mode tokens — must match FtxuiRenderer::render_statusbar() labels.
inline constexpr std::string_view kNormal = "NORMAL";
inline constexpr std::string_view kInsert = "INSERT";

// Status bar layout constants.
inline constexpr std::string_view kModeLeading = " ";    // before mode token
inline constexpr std::string_view kModeTrailing = " ";   // after mode token
inline constexpr std::string_view kUriSeparator = "  ";  // between mode block and uri

}  // namespace editor::drivers::screen_fmt
