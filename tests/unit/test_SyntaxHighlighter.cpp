#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "drivers/SyntaxHighlighter.hpp"

using namespace editor::drivers;

// ── Helpers ────────────────────────────────────────────────────────────────

// Return all spans on the given line, sorted by col.
static SpanList spans_on(const std::unordered_map<std::size_t, SpanList>& m, std::size_t line) {
    auto it = m.find(line);
    if (it == m.end()) return {};
    SpanList s = it->second;
    std::sort(s.begin(), s.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return s;
}

// Find the first span on `line` that starts at `col`.
static std::optional<std::pair<std::size_t, std::string>> span_at(
    const std::unordered_map<std::size_t, SpanList>& m, std::size_t line, std::size_t col) {
    for (const auto& [c, ls] : spans_on(m, line)) {
        if (c == col) return std::make_pair(ls.first, ls.second);
    }
    return std::nullopt;
}

// True if any span on `line` has the given `type`.
static bool has_type(const std::unordered_map<std::size_t, SpanList>& m, std::size_t line,
                     const std::string& type) {
    for (const auto& [c, ls] : spans_on(m, line))
        if (ls.second == type) return true;
    return false;
}

// ── Keywords ──────────────────────────────────────────────────────────────

TEST(SyntaxHighlighter, KeywordInt) {
    auto r = highlight_file("int x;");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->first, 3u);
    EXPECT_EQ(s->second, "keyword");
}

TEST(SyntaxHighlighter, KeywordReturn) {
    auto r = highlight_file("return 0;");
    EXPECT_TRUE(has_type(r, 0, "keyword"));
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "keyword");
    EXPECT_EQ(s->first, 6u);
}

TEST(SyntaxHighlighter, KeywordClass) {
    auto r = highlight_file("class Foo {};");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "keyword");
}

TEST(SyntaxHighlighter, KeywordConstexpr) {
    auto r = highlight_file("constexpr int N = 1;");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "keyword");
    EXPECT_EQ(s->first, 9u);
}

TEST(SyntaxHighlighter, NonKeywordIdentifierNoSpan) {
    auto r = highlight_file("myFunc();");
    // No keyword span at col 0.
    auto s = span_at(r, 0, 0);
    EXPECT_FALSE(s.has_value() && s->second == "keyword");
}

TEST(SyntaxHighlighter, MultipleKeywordsOnLine) {
    auto r = highlight_file("if (true) return;");
    auto sl = spans_on(r, 0);
    int kw_count = 0;
    for (const auto& [c, ls] : sl)
        if (ls.second == "keyword") ++kw_count;
    EXPECT_GE(kw_count, 2);  // "if" and "true" and "return"
}

// ── Line comments ─────────────────────────────────────────────────────────

TEST(SyntaxHighlighter, LineCommentBasic) {
    auto r = highlight_file("// hello world");
    EXPECT_TRUE(has_type(r, 0, "comment"));
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "comment");
}

TEST(SyntaxHighlighter, LineCommentAfterCode) {
    // "int x; // comment" — keyword before comment, comment after
    auto r = highlight_file("int x; // comment");
    EXPECT_TRUE(has_type(r, 0, "keyword"));
    EXPECT_TRUE(has_type(r, 0, "comment"));
    // Comment starts at col 7.
    auto s = span_at(r, 0, 7);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "comment");
}

TEST(SyntaxHighlighter, LineCommentDoesNotSpillToNextLine) {
    auto r = highlight_file("// comment\nint x;");
    // Line 1 has keyword, not comment.
    EXPECT_TRUE(has_type(r, 1, "keyword"));
    EXPECT_FALSE(has_type(r, 1, "comment"));
}

TEST(SyntaxHighlighter, SlashSlashInsideStringIsNotComment) {
    // std::cout << "// not a comment";
    auto r = highlight_file(R"(std::cout << "// not a comment";)");
    // col 13 starts the string.
    EXPECT_TRUE(has_type(r, 0, "string"));
    // No comment span.
    EXPECT_FALSE(has_type(r, 0, "comment"));
}

// ── Block comments ────────────────────────────────────────────────────────

TEST(SyntaxHighlighter, BlockCommentSingleLine) {
    auto r = highlight_file("/* hello */");
    EXPECT_TRUE(has_type(r, 0, "comment"));
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "comment");
}

TEST(SyntaxHighlighter, BlockCommentMultiLine) {
    auto r = highlight_file("/*\n * foo\n */");
    EXPECT_TRUE(has_type(r, 0, "comment"));
    EXPECT_TRUE(has_type(r, 1, "comment"));
    EXPECT_TRUE(has_type(r, 2, "comment"));
}

TEST(SyntaxHighlighter, BlockCommentCodeAfter) {
    auto r = highlight_file("/* c */ int x;");
    EXPECT_TRUE(has_type(r, 0, "comment"));
    EXPECT_TRUE(has_type(r, 0, "keyword"));
}

TEST(SyntaxHighlighter, StringInsideBlockCommentIsNotString) {
    auto r = highlight_file("/* \"not a string\" */");
    EXPECT_TRUE(has_type(r, 0, "comment"));
    EXPECT_FALSE(has_type(r, 0, "string"));
}

// ── String literals ───────────────────────────────────────────────────────

TEST(SyntaxHighlighter, StringBasic) {
    auto r = highlight_file(R"("hello")");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "string");
    EXPECT_EQ(s->first, 7u);  // includes both quotes
}

TEST(SyntaxHighlighter, StringWithEscapedQuote) {
    // "he said \"hi\""
    auto r = highlight_file(R"("he said \"hi\"")");
    EXPECT_TRUE(has_type(r, 0, "string"));
    EXPECT_FALSE(has_type(r, 0, "comment"));
}

TEST(SyntaxHighlighter, StringWithEscapedBackslash) {
    // "path\\to\\file"
    auto r = highlight_file(R"("path\\to\\file")");
    EXPECT_TRUE(has_type(r, 0, "string"));
    // Entire thing should be one string span starting at 0.
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "string");
}

TEST(SyntaxHighlighter, StringContainingLineCommentMarker) {
    // "// not a comment"
    auto r = highlight_file(R"("// not a comment")");
    EXPECT_TRUE(has_type(r, 0, "string"));
    EXPECT_FALSE(has_type(r, 0, "comment"));
}

TEST(SyntaxHighlighter, StringThenLineComment) {
    // "hello" // comment
    auto r = highlight_file(R"("hello" // comment)");
    EXPECT_TRUE(has_type(r, 0, "string"));
    EXPECT_TRUE(has_type(r, 0, "comment"));
}

TEST(SyntaxHighlighter, UnterminatedStringSpansToEOL) {
    // "hello world << std::endl;   — no closing quote
    auto r = highlight_file("\"hello world << std::endl;\nint x;");
    // Line 0: unterminated string
    EXPECT_TRUE(has_type(r, 0, "string"));
    // Line 1: back in Code state — keyword recognised
    EXPECT_TRUE(has_type(r, 1, "keyword"));
}

TEST(SyntaxHighlighter, StringThenCommentWithQuote) {
    // "hello world" // "a"
    auto r = highlight_file(R"("hello world" // "a")");
    EXPECT_TRUE(has_type(r, 0, "string"));
    EXPECT_TRUE(has_type(r, 0, "comment"));
    // The quote inside the comment must not produce a second string span.
    int string_count = 0;
    for (const auto& [c, ls] : spans_on(r, 0))
        if (ls.second == "string") ++string_count;
    EXPECT_EQ(string_count, 1);
}

// ── Char literals ─────────────────────────────────────────────────────────

TEST(SyntaxHighlighter, CharBasic) {
    auto r = highlight_file("'a'");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "char");
    EXPECT_EQ(s->first, 3u);
}

TEST(SyntaxHighlighter, CharEscapeSequence) {
    auto r = highlight_file("'\\n'");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "char");
    EXPECT_EQ(s->first, 4u);
}

TEST(SyntaxHighlighter, CharEscapedSingleQuote) {
    auto r = highlight_file("'\\''");
    EXPECT_TRUE(has_type(r, 0, "char"));
}

TEST(SyntaxHighlighter, UnterminatedCharSpansToEOL) {
    auto r = highlight_file("'a\nint x;");
    EXPECT_TRUE(has_type(r, 0, "char"));
    EXPECT_TRUE(has_type(r, 1, "keyword"));
}

// ── Preprocessor directives ───────────────────────────────────────────────

TEST(SyntaxHighlighter, PreprocessorInclude) {
    auto r = highlight_file("#include <vector>");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "preprocessor");
}

TEST(SyntaxHighlighter, PreprocessorDefine) {
    auto r = highlight_file("#define FOO 42");
    EXPECT_TRUE(has_type(r, 0, "preprocessor"));
}

TEST(SyntaxHighlighter, PreprocessorIfndef) {
    auto r = highlight_file("#ifndef MY_HPP\n#define MY_HPP\n#endif");
    EXPECT_TRUE(has_type(r, 0, "preprocessor"));
    EXPECT_TRUE(has_type(r, 1, "preprocessor"));
    EXPECT_TRUE(has_type(r, 2, "preprocessor"));
}

TEST(SyntaxHighlighter, PreprocessorDoesNotSpillToNextLine) {
    auto r = highlight_file("#include <vector>\nint x;");
    EXPECT_FALSE(has_type(r, 1, "preprocessor"));
    EXPECT_TRUE(has_type(r, 1, "keyword"));
}

// ── Number literals ───────────────────────────────────────────────────────

TEST(SyntaxHighlighter, NumberDecimal) {
    auto r = highlight_file("42");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "number");
    EXPECT_EQ(s->first, 2u);
}

TEST(SyntaxHighlighter, NumberHex) {
    auto r = highlight_file("0xFF");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "number");
    EXPECT_EQ(s->first, 4u);
}

TEST(SyntaxHighlighter, NumberFloat) {
    auto r = highlight_file("3.14f");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "number");
}

TEST(SyntaxHighlighter, NumberBinary) {
    auto r = highlight_file("0b1010");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "number");
    EXPECT_EQ(s->first, 6u);
}

TEST(SyntaxHighlighter, NumberScientific) {
    auto r = highlight_file("1.5e-3");
    EXPECT_TRUE(has_type(r, 0, "number"));
}

TEST(SyntaxHighlighter, NumberInsideCommentIsNotNumber) {
    auto r = highlight_file("// 42");
    EXPECT_FALSE(has_type(r, 0, "number"));
    EXPECT_TRUE(has_type(r, 0, "comment"));
}

TEST(SyntaxHighlighter, NumberInsideStringIsNotNumber) {
    auto r = highlight_file(R"("42")");
    EXPECT_FALSE(has_type(r, 0, "number"));
    EXPECT_TRUE(has_type(r, 0, "string"));
}

// ── Operators ─────────────────────────────────────────────────────────────

TEST(SyntaxHighlighter, OperatorSinglePlus) {
    auto r = highlight_file("a + b");
    EXPECT_TRUE(has_type(r, 0, "syn_operator"));
}

TEST(SyntaxHighlighter, OperatorMultiCharArrow) {
    auto r = highlight_file("p->x");
    EXPECT_TRUE(has_type(r, 0, "syn_operator"));
    // Arrow is 2 chars at col 1.
    auto s = span_at(r, 0, 1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "syn_operator");
    EXPECT_EQ(s->first, 2u);
}

TEST(SyntaxHighlighter, OperatorScopeResolution) {
    auto r = highlight_file("std::cout");
    auto s = span_at(r, 0, 3);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "syn_operator");
    EXPECT_EQ(s->first, 2u);
}

TEST(SyntaxHighlighter, OperatorLongestMatch) {
    // >>= should be one 3-char operator, not >> + =
    auto r = highlight_file("x >>= 1;");
    auto s = span_at(r, 0, 2);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "syn_operator");
    EXPECT_EQ(s->first, 3u);
}

TEST(SyntaxHighlighter, OperatorInsideCommentIsNotOperator) {
    auto r = highlight_file("// +");
    EXPECT_FALSE(has_type(r, 0, "syn_operator"));
    EXPECT_TRUE(has_type(r, 0, "comment"));
}

TEST(SyntaxHighlighter, OperatorInsideStringIsNotOperator) {
    auto r = highlight_file(R"("a + b")");
    EXPECT_FALSE(has_type(r, 0, "syn_operator"));
    EXPECT_TRUE(has_type(r, 0, "string"));
}

// ── Brackets ──────────────────────────────────────────────────────────────

TEST(SyntaxHighlighter, BracketParens) {
    auto r = highlight_file("foo()");
    EXPECT_TRUE(has_type(r, 0, "bracket"));
    auto s = span_at(r, 0, 3);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "bracket");
    EXPECT_EQ(s->first, 1u);
}

TEST(SyntaxHighlighter, BracketSquare) {
    auto r = highlight_file("arr[0]");
    auto s = span_at(r, 0, 3);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "bracket");
}

TEST(SyntaxHighlighter, BracketCurly) {
    auto r = highlight_file("{}");
    auto s = span_at(r, 0, 0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->second, "bracket");
}

TEST(SyntaxHighlighter, BracketInsideCommentIsNotBracket) {
    auto r = highlight_file("// ()");
    EXPECT_FALSE(has_type(r, 0, "bracket"));
}

// ── Context isolation edge cases ──────────────────────────────────────────

TEST(SyntaxHighlighter, BlockCommentContainingLineCommentMarker) {
    // /* // this is not a line comment */
    auto r = highlight_file("/* // this is not a line comment */");
    // Only comment spans, no code break.
    EXPECT_TRUE(has_type(r, 0, "comment"));
    int comment_count = 0;
    for (const auto& [c, ls] : spans_on(r, 0))
        if (ls.second == "comment") ++comment_count;
    // All comment spans (should be just one big span).
    EXPECT_GE(comment_count, 1);
    // No keyword inside the comment.
    EXPECT_FALSE(has_type(r, 0, "keyword"));
}

TEST(SyntaxHighlighter, StringInsideLineCommentIsNotString) {
    auto r = highlight_file("// \"hello\"");
    EXPECT_TRUE(has_type(r, 0, "comment"));
    EXPECT_FALSE(has_type(r, 0, "string"));
}

TEST(SyntaxHighlighter, KeywordInsideStringIsNotKeyword) {
    auto r = highlight_file(R"("return false;")");
    EXPECT_TRUE(has_type(r, 0, "string"));
    EXPECT_FALSE(has_type(r, 0, "keyword"));
}

TEST(SyntaxHighlighter, NumberInsideBlockCommentIsNotNumber) {
    auto r = highlight_file("/* 42 */");
    EXPECT_FALSE(has_type(r, 0, "number"));
    EXPECT_TRUE(has_type(r, 0, "comment"));
}

// ── Multi-line / cross-line ───────────────────────────────────────────────

TEST(SyntaxHighlighter, MultiLineInput) {
    const char* src =
        "int main() {\n"
        "    return 0;\n"
        "}\n";
    auto r = highlight_file(src);
    EXPECT_TRUE(has_type(r, 0, "keyword"));  // int
    EXPECT_TRUE(has_type(r, 1, "keyword"));  // return
    EXPECT_TRUE(has_type(r, 1, "number"));   // 0
}

TEST(SyntaxHighlighter, EmptyInput) {
    auto r = highlight_file("");
    EXPECT_TRUE(r.empty());
}

TEST(SyntaxHighlighter, WhitespaceOnlyInput) {
    auto r = highlight_file("   \n   \n");
    // No spans at all.
    for (const auto& [line, sl] : r) EXPECT_TRUE(sl.empty());
}

TEST(SyntaxHighlighter, BlockCommentAtEOF) {
    // Unterminated block comment: must not crash, must emit comment.
    auto r = highlight_file("/* unterminated");
    EXPECT_TRUE(has_type(r, 0, "comment"));
}

TEST(SyntaxHighlighter, PreprocessorAtEOF) {
    auto r = highlight_file("#pragma once");
    EXPECT_TRUE(has_type(r, 0, "preprocessor"));
}
