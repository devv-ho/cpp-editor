// Defines LspEncoder -- serialises outgoing LSP messages to JSON-RPC 2.0 wire format.
//
// Wire format: "Content-Length: N\r\n\r\n" followed by N bytes of UTF-8 JSON.

#pragma once

#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace editor::adapters::lsp {

// Encodes a JSON-RPC request (has id -- server must reply).
[[nodiscard]] inline std::string encode_request(int id, std::string_view method,
                                                const nlohmann::json& params) {
    nlohmann::json body = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", method},
        {"params", params},
    };
    std::string payload = body.dump();
    return std::format("Content-Length: {}\r\n\r\n{}", payload.size(), payload);
}

// Encodes a JSON-RPC notification (no id -- server must not reply).
[[nodiscard]] inline std::string encode_notification(std::string_view method,
                                                     const nlohmann::json& params) {
    nlohmann::json body = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };
    std::string payload = body.dump();
    return std::format("Content-Length: {}\r\n\r\n{}", payload.size(), payload);
}

}  // namespace editor::adapters::lsp
