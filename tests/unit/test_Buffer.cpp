#include "core/entities/Buffer.hpp"

#include <gtest/gtest.h>

using editor::core::Buffer;
using editor::core::BufferError;
using editor::core::Position;

// ── Construction
// ──────────────────────────────────────────────────────────────

TEST(BufferTest, DefaultConstructsToSingleEmptyLine) {
  Buffer b;
  EXPECT_EQ(b.line_count(), 1u);
  EXPECT_EQ(b.line(0).value(), "");
  EXPECT_TRUE(b.is_empty());
}

TEST(BufferTest, ConstructFromSingleLine) {
  Buffer b{"hello"};
  EXPECT_EQ(b.line_count(), 1u);
  EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, ConstructFromMultipleLines) {
  Buffer b{"foo\nbar\nbaz"};
  EXPECT_EQ(b.line_count(), 3u);
  EXPECT_EQ(b.line(0).value(), "foo");
  EXPECT_EQ(b.line(1).value(), "bar");
  EXPECT_EQ(b.line(2).value(), "baz");
}

TEST(BufferTest, ConstructFromEmptyString) {
  Buffer b{""};
  EXPECT_EQ(b.line_count(), 1u);
  EXPECT_TRUE(b.is_empty());
}

TEST(BufferTest, ConstructNormalisesCRLF) {
  Buffer b{"foo\r\nbar\r\nbaz"};
  EXPECT_EQ(b.line_count(), 3u);
  EXPECT_EQ(b.line(0).value(), "foo");
  EXPECT_EQ(b.line(1).value(), "bar");
  EXPECT_EQ(b.line(2).value(), "baz");
}

TEST(BufferTest, ConstructMixedLFAndCRLF) {
  Buffer b{"foo\nbar\r\nbaz"};
  EXPECT_EQ(b.line_count(), 3u);
  EXPECT_EQ(b.line(0).value(), "foo");
  EXPECT_EQ(b.line(1).value(), "bar");
  EXPECT_EQ(b.line(2).value(), "baz");
}

// ── Queries
// ───────────────────────────────────────────────────────────────────

TEST(BufferTest, LineOutOfRangeReturnsError) {
  Buffer b{"hello"};
  auto result = b.line(5);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, LineLengthReturnsCorrectSize) {
  Buffer b{"hello\nworld"};
  EXPECT_EQ(b.line_length(0).value(), 5u);
  EXPECT_EQ(b.line_length(1).value(), 5u);
}

TEST(BufferTest, LineLengthOutOfRangeReturnsError) {
  Buffer b{"hello"};
  EXPECT_EQ(b.line_length(1).error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, ToStringRoundTrips) {
  const std::string text = "foo\nbar\nbaz";
  Buffer b{text};
  EXPECT_EQ(b.to_string(), text);
}

// ── insert_char
// ───────────────────────────────────────────────────────────────

TEST(BufferTest, InsertCharInMiddleOfLine) {
  Buffer b{"hllo"};
  EXPECT_TRUE(b.insert_char({0, 1}, 'e').has_value());
  EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, InsertCharAtStartOfLine) {
  Buffer b{"ello"};
  EXPECT_TRUE(b.insert_char({0, 0}, 'h').has_value());
  EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, InsertCharAtEndOfLine) {
  Buffer b{"hell"};
  EXPECT_TRUE(b.insert_char({0, 4}, 'o').has_value());
  EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, InsertCharOutOfRangeLineReturnsError) {
  Buffer b{"hello"};
  EXPECT_EQ(b.insert_char({5, 0}, 'x').error(), BufferError::LineOutOfRange);
}

TEST(BufferTest, InsertCharOutOfRangeColReturnsError) {
  Buffer b{"hello"};
  EXPECT_EQ(b.insert_char({0, 99}, 'x').error(), BufferError::ColOutOfRange);
}

// ── delete_char
// ───────────────────────────────────────────────────────────────

TEST(BufferTest, DeleteCharFromMiddleOfLine) {
  Buffer b{"hxello"};
  EXPECT_TRUE(b.delete_char({0, 1}).has_value());
  EXPECT_EQ(b.line(0).value(), "hello");
}

TEST(BufferTest, DeleteCharOutOfRangeColReturnsError) {
  Buffer b{"hello"};
  EXPECT_EQ(b.delete_char({0, 5}).error(), BufferError::ColOutOfRange);
}

TEST(BufferTest, DeleteCharOnEmptyLineReturnsError) {
  Buffer b{""};
  EXPECT_EQ(b.delete_char({0, 0}).error(), BufferError::ColOutOfRange);
}

// ── join_with_prev
// ────────────────────────────────────────────────────────────

TEST(BufferTest, JoinWithPrevMergesLines) {
  Buffer b{"hello\nworld"};
  EXPECT_TRUE(b.join_with_prev({1, 0}).has_value());
  EXPECT_EQ(b.line_count(), 1u);
  EXPECT_EQ(b.line(0).value(), "helloworld");
}

TEST(BufferTest, JoinWithPrevOnFirstLineReturnsError) {
  Buffer b{"hello"};
  EXPECT_EQ(b.join_with_prev({0, 0}).error(), BufferError::LineOutOfRange);
}

// ── insert_newline
// ────────────────────────────────────────────────────────────

TEST(BufferTest, InsertNewlineAtMiddleSplitsLine) {
  Buffer b{"helloworld"};
  EXPECT_TRUE(b.insert_newline({0, 5}).has_value());
  EXPECT_EQ(b.line_count(), 2u);
  EXPECT_EQ(b.line(0).value(), "hello");
  EXPECT_EQ(b.line(1).value(), "world");
}

TEST(BufferTest, InsertNewlineAtStartCreatesEmptyFirstLine) {
  Buffer b{"hello"};
  EXPECT_TRUE(b.insert_newline({0, 0}).has_value());
  EXPECT_EQ(b.line(0).value(), "");
  EXPECT_EQ(b.line(1).value(), "hello");
}

TEST(BufferTest, InsertNewlineAtEndCreatesEmptyLastLine) {
  Buffer b{"hello"};
  EXPECT_TRUE(b.insert_newline({0, 5}).has_value());
  EXPECT_EQ(b.line(0).value(), "hello");
  EXPECT_EQ(b.line(1).value(), "");
}
