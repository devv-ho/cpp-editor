#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor::drivers {

// (col, (length, token_type))
using SpanList = std::vector<std::pair<std::size_t, std::pair<std::size_t, std::string>>>;

// Scans C/C++ source text with a single-pass state machine and returns
// per-line span lists keyed by 0-based line number.
//
// Token types emitted:
//   "keyword"      – C++ reserved words
//   "comment"      – // and /* */ regions
//   "string"       – double-quoted string literals
//   "char"         – single-quoted character literals
//   "preprocessor" – # directives
//   "number"       – integer and floating-point literals
//   "syn_operator" – punctuation / operators (distinct from LSP "operator")
//   "bracket"      – ( ) [ ] { }
//
// LSP semantic tokens override syntactic spans during merge (handled in
// FtxuiRenderer). This function is unaware of LSP.
inline std::unordered_map<std::size_t, SpanList> highlight_file(std::string_view source) {
    // ── Keyword set ────────────────────────────────────────────────────────
    static const std::unordered_map<std::string, bool> kKeywords = {
        {"alignas", true},       {"alignof", true},     {"and", true},
        {"and_eq", true},        {"asm", true},         {"auto", true},
        {"bitand", true},        {"bitor", true},       {"bool", true},
        {"break", true},         {"case", true},        {"catch", true},
        {"char", true},          {"char8_t", true},     {"char16_t", true},
        {"char32_t", true},      {"class", true},       {"compl", true},
        {"concept", true},       {"const", true},       {"consteval", true},
        {"constexpr", true},     {"constinit", true},   {"const_cast", true},
        {"continue", true},      {"co_await", true},    {"co_return", true},
        {"co_yield", true},      {"decltype", true},    {"default", true},
        {"delete", true},        {"do", true},          {"double", true},
        {"dynamic_cast", true},  {"else", true},        {"enum", true},
        {"explicit", true},      {"export", true},      {"extern", true},
        {"false", true},         {"float", true},       {"for", true},
        {"friend", true},        {"goto", true},        {"if", true},
        {"inline", true},        {"int", true},         {"long", true},
        {"mutable", true},       {"namespace", true},   {"new", true},
        {"noexcept", true},      {"not", true},         {"not_eq", true},
        {"nullptr", true},       {"operator", true},    {"or", true},
        {"or_eq", true},         {"private", true},     {"protected", true},
        {"public", true},        {"register", true},    {"reinterpret_cast", true},
        {"requires", true},      {"return", true},      {"short", true},
        {"signed", true},        {"sizeof", true},      {"static", true},
        {"static_assert", true}, {"static_cast", true}, {"struct", true},
        {"switch", true},        {"template", true},    {"this", true},
        {"thread_local", true},  {"throw", true},       {"true", true},
        {"try", true},           {"typedef", true},     {"typeid", true},
        {"typename", true},      {"union", true},       {"unsigned", true},
        {"using", true},         {"virtual", true},     {"void", true},
        {"volatile", true},      {"wchar_t", true},     {"while", true},
        {"xor", true},           {"xor_eq", true},      {"override", true},
        {"final", true},         {"import", true},      {"module", true},
    };

    // ── Operator / bracket sets ────────────────────────────────────────────
    static const std::string kBrackets = "()[]{}";

    // Multi-char operators (longest first, important for correct tokenisation).
    static const std::vector<std::string> kMultiOps = {
        "<<=", ">>=", "...", "<=>", "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=",
        "-=",  "*=",  "/=",  "%=",  "&=", "|=", "^=", "<<", ">>", "->", "::", ".*", "->*",
    };
    static const std::string kSingleOps = "+-*/%&|^~!<>=,.:;?";

    // ── Scanner state ──────────────────────────────────────────────────────
    enum class State {
        Code,
        LineComment,
        BlockComment,
        StringLit,
        CharLit,
        Preprocessor,
    };

    std::unordered_map<std::size_t, SpanList> result;

    State state = State::Code;
    std::size_t line = 0;
    std::size_t col = 0;

    // span_start_{line,col} track where the current multi-char span began.
    std::size_t span_line = 0;
    std::size_t span_col = 0;
    bool in_raw_escape = false;  // tracks \\-escape inside string/char

    const std::size_t n = source.size();

    auto emit = [&](std::size_t sl, std::size_t sc, std::size_t el, std::size_t ec,
                    const std::string& type) {
        // A span may cross multiple lines. Emit one SpanList entry per line.
        if (sl == el) {
            if (ec > sc) result[sl].emplace_back(sc, std::make_pair(ec - sc, type));
            return;
        }
        // Multi-line span: emit from start col to EOL on first line, then
        // full lines, then from col 0 to ec on last line.
        // We don't know line lengths here, so emit a very long length;
        // colorize_line already clamps to line.size().
        static constexpr std::size_t kBigLen = 0xFFFF;
        result[sl].emplace_back(sc, std::make_pair(kBigLen, type));
        for (std::size_t l = sl + 1; l < el; ++l)
            result[l].emplace_back(0, std::make_pair(kBigLen, type));
        if (ec > 0) result[el].emplace_back(0, std::make_pair(ec, type));
    };

    auto emit_single = [&](std::size_t l, std::size_t c, std::size_t len, const std::string& type) {
        result[l].emplace_back(c, std::make_pair(len, type));
    };

    std::size_t i = 0;
    while (i < n) {
        char ch = source[i];

        // ── Newline bookkeeping ────────────────────────────────────────────
        if (ch == '\n') {
            if (state == State::LineComment || state == State::Preprocessor) {
                emit(span_line, span_col, line, col,
                     state == State::LineComment ? "comment" : "preprocessor");
                state = State::Code;
            } else if (state == State::StringLit || state == State::CharLit) {
                // Unterminated literal: span to end of line, reset.
                emit(span_line, span_col, line, col, state == State::StringLit ? "string" : "char");
                state = State::Code;
                in_raw_escape = false;
            }
            // BlockComment continues across \n — no state change.
            ++line;
            col = 0;
            ++i;
            continue;
        }

        switch (state) {
            // ────────────────────────────────────────────────────────────────────
            case State::Code: {
                // ── Block comment start ──
                if (i + 1 < n && ch == '/' && source[i + 1] == '*') {
                    span_line = line;
                    span_col = col;
                    state = State::BlockComment;
                    col += 2;
                    i += 2;
                    break;
                }
                // ── Line comment start ──
                if (i + 1 < n && ch == '/' && source[i + 1] == '/') {
                    span_line = line;
                    span_col = col;
                    state = State::LineComment;
                    col += 2;
                    i += 2;
                    break;
                }
                // ── String literal start ──
                if (ch == '"') {
                    span_line = line;
                    span_col = col;
                    state = State::StringLit;
                    in_raw_escape = false;
                    ++col;
                    ++i;
                    break;
                }
                // ── Char literal start ──
                if (ch == '\'') {
                    span_line = line;
                    span_col = col;
                    state = State::CharLit;
                    in_raw_escape = false;
                    ++col;
                    ++i;
                    break;
                }
                // ── Preprocessor directive (#) at column 0 or after only whitespace ──
                if (ch == '#') {
                    // Valid if only whitespace has appeared on this line so far.
                    // We rely on the scanner: if we're in Code state at '#', treat as preprocessor.
                    span_line = line;
                    span_col = col;
                    state = State::Preprocessor;
                    ++col;
                    ++i;
                    break;
                }
                // ── Number literal ──
                if (std::isdigit(static_cast<unsigned char>(ch)) ||
                    (ch == '.' && i + 1 < n &&
                     std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
                    std::size_t start = i;
                    std::size_t sc = col;
                    // Consume: 0x hex, 0b binary, decimal, float with exponent, suffixes.
                    bool is_hex = false;
                    if (ch == '0' && i + 1 < n && (source[i + 1] == 'x' || source[i + 1] == 'X')) {
                        is_hex = true;
                        col += 2;
                        i += 2;
                        while (i < n && (std::isxdigit(static_cast<unsigned char>(source[i])) ||
                                         source[i] == '\'')) {
                            ++col;
                            ++i;
                        }
                    } else if (ch == '0' && i + 1 < n &&
                               (source[i + 1] == 'b' || source[i + 1] == 'B')) {
                        col += 2;
                        i += 2;
                        while (i < n &&
                               (source[i] == '0' || source[i] == '1' || source[i] == '\'')) {
                            ++col;
                            ++i;
                        }
                    } else {
                        while (i < n && (std::isdigit(static_cast<unsigned char>(source[i])) ||
                                         source[i] == '\'' || source[i] == '.')) {
                            ++col;
                            ++i;
                        }
                        // Exponent
                        if (i < n && (source[i] == 'e' || source[i] == 'E')) {
                            ++col;
                            ++i;
                            if (i < n && (source[i] == '+' || source[i] == '-')) {
                                ++col;
                                ++i;
                            }
                            while (i < n && std::isdigit(static_cast<unsigned char>(source[i]))) {
                                ++col;
                                ++i;
                            }
                        }
                    }
                    // Suffixes: u, l, ul, ull, f, etc.
                    while (i < n && (source[i] == 'u' || source[i] == 'U' || source[i] == 'l' ||
                                     source[i] == 'L' || source[i] == 'f' || source[i] == 'F')) {
                        ++col;
                        ++i;
                    }
                    emit_single(line, sc, col - sc, "number");
                    break;
                }
                // ── Identifier or keyword ──
                if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
                    std::size_t sc = col;
                    std::size_t si = i;
                    while (i < n && (std::isalnum(static_cast<unsigned char>(source[i])) ||
                                     source[i] == '_')) {
                        ++col;
                        ++i;
                    }
                    std::string word(source.substr(si, i - si));
                    if (kKeywords.count(word)) emit_single(line, sc, col - sc, "keyword");
                    // Non-keyword identifiers: no syntactic type (LSP covers them).
                    break;
                }
                // ── Bracket ──
                if (kBrackets.find(ch) != std::string::npos) {
                    emit_single(line, col, 1, "bracket");
                    ++col;
                    ++i;
                    break;
                }
                // ── Multi-char operator (longest match) ──
                {
                    bool matched = false;
                    for (const auto& op : kMultiOps) {
                        if (i + op.size() <= n && source.substr(i, op.size()) == op) {
                            emit_single(line, col, op.size(), "syn_operator");
                            col += op.size();
                            i += op.size();
                            matched = true;
                            break;
                        }
                    }
                    if (matched) break;
                }
                // ── Single-char operator ──
                if (kSingleOps.find(ch) != std::string::npos) {
                    emit_single(line, col, 1, "syn_operator");
                    ++col;
                    ++i;
                    break;
                }
                // ── Whitespace / other: skip ──
                ++col;
                ++i;
                break;
            }

            // ────────────────────────────────────────────────────────────────────
            case State::LineComment:
            case State::Preprocessor:
                // Content consumed until \n (handled above).
                ++col;
                ++i;
                break;

            // ────────────────────────────────────────────────────────────────────
            case State::BlockComment:
                if (i + 1 < n && ch == '*' && source[i + 1] == '/') {
                    col += 2;
                    i += 2;
                    emit(span_line, span_col, line, col, "comment");
                    state = State::Code;
                } else {
                    ++col;
                    ++i;
                }
                break;

            // ────────────────────────────────────────────────────────────────────
            case State::StringLit:
                if (in_raw_escape) {
                    in_raw_escape = false;
                    ++col;
                    ++i;
                } else if (ch == '\\') {
                    in_raw_escape = true;
                    ++col;
                    ++i;
                } else if (ch == '"') {
                    ++col;
                    ++i;
                    emit(span_line, span_col, line, col, "string");
                    state = State::Code;
                } else {
                    ++col;
                    ++i;
                }
                break;

            // ────────────────────────────────────────────────────────────────────
            case State::CharLit:
                if (in_raw_escape) {
                    in_raw_escape = false;
                    ++col;
                    ++i;
                } else if (ch == '\\') {
                    in_raw_escape = true;
                    ++col;
                    ++i;
                } else if (ch == '\'') {
                    ++col;
                    ++i;
                    emit(span_line, span_col, line, col, "char");
                    state = State::Code;
                } else {
                    ++col;
                    ++i;
                }
                break;
        }
    }

    // ── Flush open span at EOF ─────────────────────────────────────────────
    if (state == State::LineComment)
        emit(span_line, span_col, line, col, "comment");
    else if (state == State::Preprocessor)
        emit(span_line, span_col, line, col, "preprocessor");
    else if (state == State::BlockComment)
        emit(span_line, span_col, line, col, "comment");
    else if (state == State::StringLit)
        emit(span_line, span_col, line, col, "string");
    else if (state == State::CharLit)
        emit(span_line, span_col, line, col, "char");

    return result;
}

}  // namespace editor::drivers
