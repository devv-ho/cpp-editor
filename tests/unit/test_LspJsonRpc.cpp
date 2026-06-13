#include <gtest/gtest.h>

#include "adapters/lsp/LspDecoder.hpp"
#include "adapters/lsp/LspEncoder.hpp"
#include "adapters/lsp/LspMessage.hpp"

using namespace editor::adapters::lsp;

// ── LspEncoder ────────────────────────────────────────────────────────────────

TEST(LspEncoderTest, EncodeRequestHasCorrectHeader) {
    nlohmann::json params = {{"textDocument", "file:///foo.cpp"}};
    std::string wire = encode_request(1, "textDocument/definition", params);

    EXPECT_TRUE(wire.starts_with("Content-Length: "));
    auto sep = wire.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);

    std::string header = wire.substr(0, sep);
    std::string body = wire.substr(sep + 4);
    std::size_t claimed_len = std::stoul(header.substr(std::string("Content-Length: ").size()));
    EXPECT_EQ(claimed_len, body.size());
}

TEST(LspEncoderTest, EncodeRequestBodyContainsId) {
    std::string wire = encode_request(42, "initialize", nlohmann::json::object());
    auto sep = wire.find("\r\n\r\n");
    auto body = nlohmann::json::parse(wire.substr(sep + 4));

    EXPECT_EQ(body["jsonrpc"], "2.0");
    EXPECT_EQ(body["id"], 42);
    EXPECT_EQ(body["method"], "initialize");
}

TEST(LspEncoderTest, EncodeNotificationHasNoId) {
    std::string wire = encode_notification("textDocument/didOpen", nlohmann::json::object());
    auto sep = wire.find("\r\n\r\n");
    auto body = nlohmann::json::parse(wire.substr(sep + 4));

    EXPECT_EQ(body["jsonrpc"], "2.0");
    EXPECT_FALSE(body.contains("id"));
    EXPECT_EQ(body["method"], "textDocument/didOpen");
}

TEST(LspEncoderTest, EncodeNotificationContentLengthMatchesBody) {
    nlohmann::json params = {{"uri", "file:///bar.cpp"}, {"version", 1}};
    std::string wire = encode_notification("textDocument/didOpen", params);

    auto sep = wire.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    std::string header = wire.substr(0, sep);
    std::string body = wire.substr(sep + 4);
    std::size_t claimed = std::stoul(header.substr(std::string("Content-Length: ").size()));
    EXPECT_EQ(claimed, body.size());
}

// ── LspDecoder ────────────────────────────────────────────────────────────────

TEST(LspDecoderTest, ParsesCompleteMessage) {
    std::string wire = encode_request(7, "textDocument/hover", nlohmann::json::object());

    LspDecoder decoder;
    decoder.feed(wire);
    auto msg = decoder.next_message();

    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->id, 7);
    EXPECT_EQ(msg->method, "textDocument/hover");
}

TEST(LspDecoderTest, ReturnsNulloptOnEmptyBuffer) {
    LspDecoder decoder;
    EXPECT_FALSE(decoder.next_message().has_value());
}

TEST(LspDecoderTest, ReturnsNulloptOnPartialHeader) {
    LspDecoder decoder;
    decoder.feed("Content-Length: 5");
    EXPECT_FALSE(decoder.next_message().has_value());
}

TEST(LspDecoderTest, ReturnsNulloptOnPartialBody) {
    LspDecoder decoder;
    decoder.feed("Content-Length: 100\r\n\r\n{\"jsonrpc\":\"2.0\"}");
    EXPECT_FALSE(decoder.next_message().has_value());
}

TEST(LspDecoderTest, HandlesMessageSplitAcrossTwoFeeds) {
    std::string wire = encode_notification("initialized", nlohmann::json::object());
    std::size_t mid = wire.size() / 2;

    LspDecoder decoder;
    decoder.feed(wire.substr(0, mid));
    EXPECT_FALSE(decoder.next_message().has_value());

    decoder.feed(wire.substr(mid));
    auto msg = decoder.next_message();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->method, "initialized");
}

TEST(LspDecoderTest, HandlesTwoMessagesInOneFeed) {
    std::string wire1 = encode_request(1, "initialize", nlohmann::json::object());
    std::string wire2 = encode_notification("initialized", nlohmann::json::object());

    LspDecoder decoder;
    decoder.feed(wire1 + wire2);

    auto msg1 = decoder.next_message();
    ASSERT_TRUE(msg1.has_value());
    EXPECT_EQ(msg1->id, 1);
    EXPECT_EQ(msg1->method, "initialize");

    auto msg2 = decoder.next_message();
    ASSERT_TRUE(msg2.has_value());
    EXPECT_FALSE(msg2->id.has_value());
    EXPECT_EQ(msg2->method, "initialized");

    EXPECT_FALSE(decoder.next_message().has_value());
}

TEST(LspDecoderTest, PopulatesResultAndErrorFields) {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"result", {{"capabilities", nlohmann::json::object()}}},
    };
    std::string payload = response.dump();
    std::string wire = std::format("Content-Length: {}\r\n\r\n{}", payload.size(), payload);

    LspDecoder decoder;
    decoder.feed(wire);
    auto msg = decoder.next_message();

    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->id, 3);
    EXPECT_TRUE(msg->method.empty());
    EXPECT_TRUE(msg->result.contains("capabilities"));
    EXPECT_TRUE(msg->error.is_null());
}

TEST(LspDecoderTest, MalformedJsonBodyYieldsEmptyMessage) {
    // "hello" is not valid JSON; decoder must not crash and must consume the message.
    std::string wire = "Content-Length: 5\r\n\r\nhello";
    LspDecoder decoder;
    decoder.feed(wire);
    // next_message() returns a value (consumed) but with no id and empty method.
    auto msg = decoder.next_message();
    ASSERT_TRUE(msg.has_value());
    EXPECT_FALSE(msg->id.has_value());
    EXPECT_TRUE(msg->method.empty());
}

TEST(LspDecoderTest, EmptyJsonObjectBodyYieldsEmptyMessage) {
    std::string payload = "{}";
    std::string wire = std::format("Content-Length: {}\r\n\r\n{}", payload.size(), payload);
    LspDecoder decoder;
    decoder.feed(wire);
    auto msg = decoder.next_message();
    ASSERT_TRUE(msg.has_value());
    EXPECT_FALSE(msg->id.has_value());
    EXPECT_TRUE(msg->method.empty());
}
