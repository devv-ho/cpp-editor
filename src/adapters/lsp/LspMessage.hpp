// Defines LspMessage, a parsed incoming LSP JSON-RPC 2.0 message.

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace editor::adapters::lsp {

// Represents one parsed message received from the language server.
// Requests and responses carry an id; notifications do not.
struct LspMessage {
    std::optional<int> id;  // present on requests and responses
    std::string method;     // empty on responses
    nlohmann::json params;  // null if absent
    nlohmann::json result;  // null if absent
    nlohmann::json error;   // null if absent
};

}  // namespace editor::adapters::lsp
