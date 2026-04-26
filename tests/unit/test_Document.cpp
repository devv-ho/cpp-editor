// Unit tests for Document.

#include <gtest/gtest.h>

#include "core/entities/Document.hpp"

using editor::core::BufferError;
using editor::core::Document;
using editor::core::Position;

// -- Construction -------------------------------------------------------------

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

// -- insert_char --------------------------------------------------------------

TEST(DocumentTest, InsertCharAtPosition) {
    Document doc{"hllo"};
    EXPECT_TRUE(doc.insert_char({0, 1}, 'e').has_value());
    EXPECT_EQ(doc.line(0).value(), "hello");
}

TEST(DocumentTest, InsertCharOutOfRangeReturnsError) {
    Document doc{"hello"};
    EXPECT_EQ(doc.insert_char({5, 0}, 'x').error(), BufferError::LineOutOfRange);
}

// -- delete_char --------------------------------------------------------------

TEST(DocumentTest, DeleteCharAtPosition) {
    Document doc{"hello"};
    EXPECT_TRUE(doc.delete_char({0, 0}).has_value());
    EXPECT_EQ(doc.line(0).value(), "ello");
}

TEST(DocumentTest, DeleteCharOnEmptyLineReturnsError) {
    Document doc{""};
    EXPECT_FALSE(doc.delete_char({0, 0}).has_value());
}

// -- join_with_prev -----------------------------------------------------------

TEST(DocumentTest, JoinWithPrevMergesLines) {
    Document doc{"hello\nworld"};
    EXPECT_TRUE(doc.join_with_prev({1, 0}).has_value());
    EXPECT_EQ(doc.line_count(), 1u);
    EXPECT_EQ(doc.line(0).value(), "helloworld");
}

TEST(DocumentTest, JoinWithPrevOnFirstLineReturnsError) {
    Document doc{"hello"};
    EXPECT_FALSE(doc.join_with_prev({0, 0}).has_value());
}

// -- insert_newline -----------------------------------------------------------

TEST(DocumentTest, InsertNewlineSplitsLine) {
    Document doc{"helloworld"};
    EXPECT_TRUE(doc.insert_newline({0, 5}).has_value());
    EXPECT_EQ(doc.line_count(), 2u);
    EXPECT_EQ(doc.line(0).value(), "hello");
    EXPECT_EQ(doc.line(1).value(), "world");
}

// -- to_string ----------------------------------------------------------------

TEST(DocumentTest, ToStringRoundTrips) {
    const std::string text{"foo\nbar\nbaz"};
    Document doc{text};
    EXPECT_EQ(doc.to_string(), text);
}

// -- path ---------------------------------------------------------------------

TEST(DocumentTest, PathDefaultsToEmpty) {
    Document doc;
    EXPECT_TRUE(doc.path().empty());
}

TEST(DocumentTest, SetPathStoresPath) {
    Document doc;
    doc.set_path("/tmp/test.cpp");
    EXPECT_EQ(doc.path(), "/tmp/test.cpp");
}
