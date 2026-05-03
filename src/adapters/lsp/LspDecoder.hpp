// Defines LspDecoder -- stateful parser for the LSP JSON-RPC 2.0 wire protocol.
//
// Feed raw bytes from the subprocess stdout via feed(). Call next_message()
// repeatedly until it returns std::nullopt to drain all complete messages.

#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "adapters/lsp/LspMessage.hpp"

namespace editor::adapters::lsp {

class LspDecoder {
public:
    // Appends raw bytes to the internal buffer.
    void feed(std::string_view data) { buf_ += data; }

    // Returns the next fully-buffered message, or std::nullopt if more data is needed.
    std::optional<LspMessage> next_message() {
        auto content_length = parse_content_length();
        if (!content_length) {
            return std::nullopt;
        }

        // Header ends at the first \r\n\r\n.
        auto header_end = buf_.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return std::nullopt;
        }

        std::size_t body_start = header_end + 4;
        if (buf_.size() < body_start + *content_length) {
            return std::nullopt;
        }

        std::string body = buf_.substr(body_start, *content_length);
        buf_.erase(0, body_start + *content_length);

        return parse_body(body);
    }

private:
    std::string buf_;

    // Parses the Content-Length value from the header, or nullopt if not yet present.
    std::optional<std::size_t> parse_content_length() const {
        const std::string_view prefix = "Content-Length: ";
        auto pos = buf_.find(prefix);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        auto value_start = pos + prefix.size();
        auto line_end = buf_.find("\r\n", value_start);
        if (line_end == std::string::npos) {
            return std::nullopt;
        }
        return std::stoul(buf_.substr(value_start, line_end - value_start));
    }

    static LspMessage parse_body(const std::string& body) {
        auto json = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);

        LspMessage msg;
        if (json.contains("id") && json["id"].is_number_integer()) {
            msg.id = json["id"].get<int>();
        }
        if (json.contains("method") && json["method"].is_string()) {
            msg.method = json["method"].get<std::string>();
        }
        msg.params = json.value("params", nlohmann::json{});
        msg.result = json.value("result", nlohmann::json{});
        msg.error = json.value("error", nlohmann::json{});
        return msg;
    }
};

}  // namespace editor::adapters::lsp
