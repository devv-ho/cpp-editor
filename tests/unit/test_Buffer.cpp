// Unit tests for Buffer.

#include <gtest/gtest.h>

#include "core/entities/Buffer.hpp"

using editor::core::Buffer;
using editor::core::BufferError;

// -- Construction -------------------------------------------------------------

TEST(BufferTest, DefaultConstructsToSingleEmptyLine) {
    Buffer b;
    EXPECT_EQ(b.line_count(), 1u);
    EXPECT_EQ(b.line(0).value(), "");
    EXPECT_TRUE(b.is_empty());
}

TEST(BufferTest, ConstructFromSingleLine) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.line_count(), 1u);
    EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, ConstructFromMultipleLines) {
    Buffer b = Buffer::from_text("foo\nbar\nbaz");
    EXPECT_EQ(b.line_count(), 3u);
    EXPECT_EQ(b.line(0).value(), "foo");
    EXPECT_EQ(b.line(1).value(), "bar");
    EXPECT_EQ(b.line(2).value(), "baz");
}

TEST(BufferTest, ConstructFromEmptyString) {
    Buffer b = Buffer::from_text("");
    EXPECT_EQ(b.line_count(), 1u);
    EXPECT_TRUE(b.is_empty());
}

TEST(BufferTest, ConstructNormalisesCRLF) {
    Buffer b = Buffer::from_text("foo\r\nbar\r\nbaz");
    EXPECT_EQ(b.line_count(), 3u);
    EXPECT_EQ(b.line(0).value(), "foo");
    EXPECT_EQ(b.line(1).value(), "bar");
    EXPECT_EQ(b.line(2).value(), "baz");
}

TEST(BufferTest, ConstructMixedLFAndCRLF) {
    Buffer b = Buffer::from_text("foo\nbar\r\nbaz");
    EXPECT_EQ(b.line_count(), 3u);
    EXPECT_EQ(b.line(0).value(), "foo");
    EXPECT_EQ(b.line(1).value(), "bar");
    EXPECT_EQ(b.line(2).value(), "baz");
}

// -- Queries ------------------------------------------------------------------

TEST(BufferTest, LineOutOfRangeReturnsError) {
    Buffer b = Buffer::from_text("hello");
    auto result = b.line(5);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, LineLengthReturnsCorrectSize) {
    Buffer b = Buffer::from_text("hello\nworld");
    EXPECT_EQ(b.line_length(0).value(), 5u);
    EXPECT_EQ(b.line_length(1).value(), 5u);
}

TEST(BufferTest, LineLengthOutOfRangeReturnsError) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.line_length(1).error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, ToStringRoundTrips) {
    const std::string text = "foo\nbar\nbaz";
    Buffer b = Buffer::from_text(text);
    EXPECT_EQ(b.to_string(), text);
}

TEST(BufferTest, TrailingNewlineProducesTwoLines) {
    Buffer b = Buffer::from_text("hello\n");
    EXPECT_EQ(b.line_count(), 2u);
    EXPECT_EQ(b.line(1).value(), "");
}

TEST(BufferTest, ToStringPreservesTrailingNewline) {
    // "hello\n" splits into ["hello", ""], which to_string joins back as "hello\n".
    Buffer b = Buffer::from_text("hello\n");
    EXPECT_EQ(b.to_string(), "hello\n");
}

// -- insert_char --------------------------------------------------------------

TEST(BufferTest, InsertCharInMiddleOfLine) {
    Buffer b = Buffer::from_text("hllo");
    EXPECT_TRUE(b.insert_char({0, 1}, 'e').has_value());
    EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, InsertCharAtStartOfLine) {
    Buffer b = Buffer::from_text("ello");
    EXPECT_TRUE(b.insert_char({0, 0}, 'h').has_value());
    EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, InsertCharAtEndOfLine) {
    Buffer b = Buffer::from_text("hell");
    EXPECT_TRUE(b.insert_char({0, 4}, 'o').has_value());
    EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, InsertCharOutOfRangeLineReturnsError) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.insert_char({5, 0}, 'x').error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, InsertCharOutOfRangeColReturnsError) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.insert_char({0, 99}, 'x').error(), BufferError::ColOutOfRange);
}

// -- delete_char --------------------------------------------------------------

TEST(BufferTest, DeleteCharFromMiddleOfLine) {
    Buffer b = Buffer::from_text("hxello");
    EXPECT_TRUE(b.delete_char({0, 1}).has_value());
    EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, DeleteCharOutOfRangeColReturnsError) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.delete_char({0, 5}).error(), BufferError::ColOutOfRange);
}

TEST(BufferTest, DeleteCharOnEmptyLineReturnsError) {
    Buffer b = Buffer::from_text("");
    EXPECT_EQ(b.delete_char({0, 0}).error(), BufferError::ColOutOfRange);
}

// -- join_with_prev -----------------------------------------------------------

TEST(BufferTest, JoinWithPrevMergesLines) {
    Buffer b = Buffer::from_text("hello\nworld");
    EXPECT_TRUE(b.join_with_prev({1, 0}).has_value());
    EXPECT_EQ(b.line_count(), 1u);
    EXPECT_EQ(b.line(0).value(), "helloworld");
}

TEST(BufferTest, JoinWithPrevOnFirstLineReturnsError) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.join_with_prev({0, 0}).error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, JoinWithPrevBeyondLastLineReturnsError) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_EQ(b.join_with_prev({5, 0}).error(), BufferError::LineOutOfRange);
}

// -- insert_newline -----------------------------------------------------------

TEST(BufferTest, InsertNewlineAtMiddleSplitsLine) {
    Buffer b = Buffer::from_text("helloworld");
    EXPECT_TRUE(b.insert_newline({0, 5}).has_value());
    EXPECT_EQ(b.line_count(), 2u);
    EXPECT_EQ(b.line(0).value(), "hello");
    EXPECT_EQ(b.line(1).value(), "world");
}

TEST(BufferTest, InsertNewlineAtStartCreatesEmptyFirstLine) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_TRUE(b.insert_newline({0, 0}).has_value());
    EXPECT_EQ(b.line(0).value(), "");
    EXPECT_EQ(b.line(1).value(), "hello");
}

TEST(BufferTest, InsertNewlineAtEndCreatesEmptyLastLine) {
    Buffer b = Buffer::from_text("hello");
    EXPECT_TRUE(b.insert_newline({0, 5}).has_value());
    EXPECT_EQ(b.line(0).value(), "hello");
    EXPECT_EQ(b.line(1).value(), "");
}
