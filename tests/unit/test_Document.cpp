#include "core/entities/Document.hpp"

#include <gtest/gtest.h>

using editor::core::Document;
using editor::core::Position;

// ── Construction
// ──────────────────────────────────────────────────────────────

TEST(DocumentTest, DefaultConstructsToEmptyBuffer) {
  Document doc;
  EXPECT_EQ(doc.line_count(), 1u);
  EXPECT_EQ(doc.position(), (Position{0, 0}));
}

TEST(DocumentTest, ConstructFromText) {
  Document doc{"hello\nworld"};
  EXPECT_EQ(doc.line_count(), 2u);
  EXPECT_EQ(doc.line(0).value(), "hello");
  EXPECT_EQ(doc.line(1).value(), "world");
}

// ── insert_char
// ───────────────────────────────────────────────────────────────

TEST(DocumentTest, InsertCharAppendsToCursor) {
  Document doc{"ab"};
  EXPECT_TRUE(doc.insert_char('x').has_value());
  EXPECT_EQ(doc.line(0).value(), "xab");
}

TEST(DocumentTest, InsertCharAdvancesCursor) {
  Document doc{""};
  doc.insert_char('a');
  doc.insert_char('b');
  doc.insert_char('c');
  EXPECT_EQ(doc.line(0).value(), "abc");
}

// ── delete_char
// ───────────────────────────────────────────────────────────────

TEST(DocumentTest, DeleteCharRemovesAtCursor) {
  Document doc{"hello"};
  EXPECT_TRUE(doc.delete_char().has_value());
  EXPECT_EQ(doc.line(0).value(), "ello");
}

TEST(DocumentTest, DeleteCharOnEmptyLineReturnsError) {
  Document doc{""};
  EXPECT_FALSE(doc.delete_char().has_value());
}

// ── insert_newline
// ────────────────────────────────────────────────────────────

TEST(DocumentTest, InsertNewlineSplitsLineAndMovesCursor) {
  Document doc{"helloworld"};
  doc.cursor().set_position({0, 5});
  EXPECT_TRUE(doc.insert_newline().has_value());
  EXPECT_EQ(doc.line_count(), 2u);
  EXPECT_EQ(doc.line(0).value(), "hello");
  EXPECT_EQ(doc.line(1).value(), "world");
  EXPECT_EQ(doc.position(), (Position{1, 0}));
}

// ── to_string
// ─────────────────────────────────────────────────────────────────

TEST(DocumentTest, ToStringRoundTrips) {
  const std::string text{"foo\nbar\nbaz"};
  Document doc{text};
  EXPECT_EQ(doc.to_string(), text);
}

// ── path ─────────────────────────────────────────────────────────────────────

TEST(DocumentTest, PathDefaultsToEmpty) {
  Document doc;
  EXPECT_TRUE(doc.path().empty());
}

TEST(DocumentTest, SetPathStoresPath) {
  Document doc;
  doc.set_path("/tmp/test.cpp");
  EXPECT_EQ(doc.path(), "/tmp/test.cpp");
}
