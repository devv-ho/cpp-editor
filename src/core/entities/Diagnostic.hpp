#pragma once

#include <cstddef>
#include <string>

namespace editor::core {

struct Diagnostic {
    std::size_t line;  // 0-based, as returned by LSP
    std::size_t col;   // 0-based
    std::string message;
    int severity;  // 1=error 2=warning 3=info 4=hint (LSP DiagnosticSeverity)
};

}  // namespace editor::core
