// LSP Base Protocol wire constants.
// All values are mandated by the spec -- not invented here:
//   https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#baseProtocol

#pragma once

#include <string_view>

namespace editor::adapters::lsp {

inline constexpr std::string_view kContentLengthPrefix = "Content-Length: ";
inline constexpr std::string_view kHeaderSep = "\r\n\r\n";

}  // namespace editor::adapters::lsp
