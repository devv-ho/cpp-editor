#pragma once

#include <cstddef>
#include <string>

namespace editor::core {

// LSP DiagnosticSeverity constants (spec §3.15.1).
struct DiagnosticSeverity {
    static constexpr int kError = 1;
    static constexpr int kWarning = 2;
    static constexpr int kInformation = 3;
    static constexpr int kHint = 4;
};

struct Diagnostic {
    std::size_t line;  // 0-based, as returned by LSP
    std::size_t col;   // 0-based
    std::string message;
    int severity;  // one of DiagnosticSeverity constants
};

}  // namespace editor::core
