---
title: "feat: Add regex-based syntactic highlighting layer"
type: feat
status: active
date: 2026-05-17
---

# feat: Add regex-based syntactic highlighting layer

## Overview

clangd's semantic tokens cover only ~86 tokens for a typical C++ file — identifiers
that have semantic meaning (functions, types, variables, macros, operators). It never
emits tokens for C++ keywords (`int`, `const`, `return`, …), preprocessor directives
(`#include`, `#define`, …), comments, or string/character literals. Those gaps render
as the terminal's default colour, making large portions of every file look unhighlighted.

This plan adds a **regex-based syntactic highlighting layer** that runs before LSP
tokens are applied. The syntactic layer fills the gaps; the LSP layer overwrites where
it has more precise information. The result matches the visual quality of editors like
VS Code without introducing any external dependencies.

---

## Problem Frame

The current pipeline in `FtxuiRenderer::render_buffer()` is:

```
LSP semantic tokens → token_map (per-line SpanList) → colorize_line
```

clangd on this project emits ~86 semantic tokens per file. A typical C++ source file
has many more colourable tokens — keywords, preprocessor directives, comments, string
literals, numeric literals, and operators. Those appear as unstyled default-colour text.

The fix is a two-layer pipeline:

```
Syntactic regex pass (whole file, state machine) → regex_map
              ↓ merge (LSP wins on overlap)
         token_map (per-line SpanList)
              ↓
         colorize_line (unchanged)
```

---

## Requirements Trace

- R1. All C++ keywords produce a distinct colour (never default).
- R2. Preprocessor directives (`#include`, `#define`, …) are coloured as a group;
      the included path/header (`<…>` or `"…"`) is coloured as a string.
- R3. Line comments (`// …`) and block comments (`/* … */`) are coloured correctly
      end-to-end, including content that looks like code (operators, keywords, strings).
- R4. String literals (`"…"`) and character literals (`'…'`) are coloured correctly;
      escape sequences inside them do not break the span.
- R5. Numeric literals (integer, float, hex, binary, suffix variants) are coloured.
- R6. Operators (`+`, `-`, `*`, `/`, `%`, `=`, `<`, `>`, `!`, `&`, `|`, `^`, `~`,
      `?`, `:`, `<<`, `>>`, `->`, `::`, `<=`, `>=`, `!=`, `==`, `&&`, `||`, `++`, `--`,
      `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`) are coloured.
- R7. Brackets (`(`, `)`, `{`, `}`, `[`, `]`) are coloured.
- R8. A token already covered by an LSP semantic span is **not** overridden by the
      syntactic layer — LSP wins on any overlap.
- R9. Content inside a comment or string literal is **never** re-interpreted as a
      keyword, operator, or other syntactic token.
- R10. The cursor-injection logic in `colorize_line` is unaffected.
- R11. Unit tests cover every token category and all critical cross-context edge cases.

---

## Scope Boundaries

- Only C++ syntax is handled; language detection is not added.
- Raw string literals (`R"(…)"`) are out of scope for v1; they are treated as
  unrecognised text and fall through to default colour.
- Multi-line string continuation (backslash at end of line) is out of scope.
- Semantic meaning of operators (e.g. overloaded `<<`) is handled by LSP, not here.
- No incremental / differential re-parse; the whole file is scanned on each render call.
- No external dependencies are introduced (`<regex>` is stdlib).

---

## Context & Research

### Relevant Code and Patterns

- `src/drivers/FtxuiRenderer.cpp` — `render_buffer()`, `colorize_line()`, `token_color()`,
  `SpanList` alias.
- `src/core/usecases/LspService.hpp` — `LspSemanticToken` struct (line, col, length, token_type).
- `src/drivers/ScreenFormat.hpp` — pattern for a driver-layer header with no FTXUI dep.
- `tests/unit/test_LspJsonRpc.cpp` — unit test style: flat `TEST()` macros, no fixtures,
  designated initialisers, `EXPECT_EQ` / `ASSERT_EQ`.

### Institutional Learnings

None in `docs/solutions/` cover this area yet. Decisions made here should be captured
afterwards.

---

## Key Technical Decisions

- **State machine over regex-per-category**: A single left-to-right pass with explicit
  states (`CODE`, `LINE_COMMENT`, `BLOCK_COMMENT`, `STRING`, `CHAR`, `PREPROCESSOR`)
  correctly handles nesting/priority (e.g. `"// not a comment"`, `/* "not a string" */`)
  without lookahead or backtracking. Regex applied independently per category cannot do
  this correctly.

- **File-level scan, line-level output**: The state machine runs over the whole
  `std::string` (all lines joined with `\n`) so that block comments spanning multiple
  lines are handled. Output is a `per-line SpanList map` identical in shape to the
  existing `token_map`.

- **LSP wins on overlap**: When a syntactic span and an LSP span cover overlapping
  columns on the same line, the LSP span takes priority. Merge is implemented by
  inserting LSP spans into the regex map and re-sorting/de-overlapping before passing
  to `colorize_line`.

- **Reuse existing type strings and `token_color`**: Syntactic tokens use the same
  string keys already in `token_color` (`"keyword"`, `"comment"`, `"string"`,
  `"number"`, `"operator"`) plus two new ones (`"preprocessor"`, `"bracket"`). Only
  `token_color` needs two new branches; `colorize_line` is untouched.

- **New free-function header `SyntaxHighlighter.hpp`**: Lives in `src/drivers/`,
  compiled as part of `editor_adapters`. No FTXUI include required in this header,
  keeping it independently unit-testable.

- **Unterminated string/char recovery**: If a `"` or `'` is not closed before the
  newline, the state resets to `CODE` at the start of the next line (C++ unterminated
  literals are compile errors; the editor shows the broken line highlighted as a string,
  which is the correct visual signal).

- **Block comment persists across lines**: State is carried through the newline; the
  next line opens already in `BLOCK_COMMENT` state.

- **No new CMake targets or dependencies**: `SyntaxHighlighter.hpp` is included by
  `FtxuiRenderer.cpp`; the test file is picked up by the existing `GLOB_RECURSE` in
  `CMakeLists.txt`.

---

## Open Questions

### Resolved During Planning

- *Should `bracket` get its own colour or reuse an existing one?*
  Resolution: `ftxui::Color::White` (distinct from default, dim enough not to dominate).
- *Should operators reuse `Yellow` (same as LSP functions/methods/operators)?*
  Resolution: `ftxui::Color::Cyan` — visually distinct from functions; operators in
  clangd legend are also typed as `"operator"` so LSP will overwrite where it knows better.
- *Should preprocessor directives reuse `BlueLight` (same as macro/namespace)?*
  Resolution: yes — `BlueLight` for the directive keyword; `Green` for the path/header
  string (reuses the `"string"` colour, consistent with regular string literals).

### Deferred to Implementation

- Exact handling of `\\` escape inside strings: implement as "skip next char" inside
  STRING/CHAR state — straightforward but confirmed at coding time.
- Whether `#pragma`, `#error`, `#line` need separate colours: treat as `"preprocessor"`
  for now; can be split later via LSP macro tokens.

---

## High-Level Technical Design

> *This illustrates the intended approach and is directional guidance for review, not
> implementation specification. The implementing agent should treat it as context, not
> code to reproduce.*

### State machine states and transitions

```
States: CODE | LINE_COMMENT | BLOCK_COMMENT | STRING | CHAR | PREPROCESSOR

Transitions (left-to-right scan, first match wins):

CODE:
  '//'          → emit gap, enter LINE_COMMENT, mark start
  '/*'          → emit gap, enter BLOCK_COMMENT, mark start
  '"'           → emit gap, enter STRING, mark start
  '\''          → emit gap, enter CHAR, mark start
  '#' at col 0  → emit gap, enter PREPROCESSOR, mark start
     (after whitespace-only prefix also counts as col 0 for preprocessor)
  keyword match → emit keyword span
  number match  → emit number span
  operator seq  → emit operator span (longest-match: '>>=', then '>>', then '>')
  bracket       → emit bracket span
  else          → advance one char (part of uncoloured gap)

LINE_COMMENT:
  '\n'          → emit comment span for [start, current), reset to CODE

BLOCK_COMMENT:
  '*/'          → emit comment span for [start, current+2), reset to CODE
  '\n'          → emit comment span for [start, EOL), new line starts in BLOCK_COMMENT
  EOF           → emit remaining as comment

STRING:
  '\\'          → skip next char (escape)
  '"'           → emit string span for [start, current+1), reset to CODE
  '\n'          → emit string span for [start, EOL) (unterminated), reset to CODE

CHAR:
  '\\'          → skip next char (escape)
  '\''          → emit char span for [start, current+1), reset to CODE
  '\n'          → emit char span for [start, EOL) (unterminated), reset to CODE

PREPROCESSOR:
  (space after directive word)
    → emit preprocessor span for directive, then:
    if directive is 'include': scan for '<…>' or '"…"' → emit as string span
    else: remainder of line → emit as preprocessor span
  '\n'          → emit preprocessor span for [start, EOL), reset to CODE
```

### Merge algorithm (LSP wins)

```
Input:  regex_map[line] = sorted SpanList (non-overlapping by construction)
        lsp_map[line]   = sorted SpanList from semantic tokens

For each line:
  merged = regex_map[line]
  For each lsp_span in lsp_map[line]:
    remove or trim any regex spans that overlap lsp_span
    insert lsp_span
  re-sort merged by col
  pass to colorize_line
```

### Colour additions to `token_color`

| Type string     | `ftxui::Color`  | Rationale                              |
|-----------------|-----------------|----------------------------------------|
| `"preprocessor"`| `BlueLight`     | Matches macro/namespace (same layer)   |
| `"bracket"`     | `White`         | Visible but not dominant               |
| `"operator"`    | `Cyan`          | Already mapped — confirms no change needed here (LSP `"operator"` → `Yellow`; syntactic `"operator"` → `Cyan` until LSP overwrites) |

Wait — `"operator"` is already mapped to `Yellow` for LSP operators. For syntactic
operators a separate type string `"syn_operator"` avoids collision: syntactic layer
uses `"syn_operator"` → `Cyan`; LSP layer emits `"operator"` → `Yellow` (kept as-is).

---

## Implementation Units

- U1. **SyntaxHighlighter: state machine scanner**

**Goal:** Implement the file-level state machine that scans the entire source text and
produces a `per-line SpanList map` covering comments, strings, preprocessor, keywords,
numbers, operators, and brackets.

**Requirements:** R1–R7, R9

**Dependencies:** None

**Files:**
- Create: `src/drivers/SyntaxHighlighter.hpp`
- Test: `tests/unit/test_SyntaxHighlighter.cpp`

**Approach:**
- Single public free function: `highlight_file(std::string_view source)` returning
  `std::unordered_map<std::size_t, SpanList>` (same `SpanList` alias shape as renderer).
- Define `SpanList` in the header so tests can use it without including renderer internals.
- State machine iterates `source` character by character, tracking `line` and `col`
  counters that reset on `\n`. Spans are appended to the map as they close.
- Keyword matching: at `CODE` state, when a letter or `_` is seen, read the full
  identifier, then check against a `std::unordered_set<std::string_view>` of C++23
  keywords. If matched, emit `"keyword"` span; otherwise emit nothing (identifier colour
  is handled by LSP semantic tokens).
- Number matching: digit or `.digit` or `0x`/`0b` prefix → read until non-digit/non-suffix.
- Operator matching: longest-match from a fixed table (`>>=`, `<<=`, `->*`, … down to
  single-char `+`, `-`, etc.). Use `"syn_operator"` type string.
- Bracket matching: single char from `({[]})`; use `"bracket"` type string.
- Preprocessor: `#` at start-of-line (after optional whitespace). Read directive word.
  If `include`: emit directive as `"preprocessor"`, then scan for `<…>` / `"…"` and
  emit as `"string"`. Other directives: emit rest of line as `"preprocessor"`.

**Patterns to follow:**
- `src/drivers/ScreenFormat.hpp` for header-only driver-layer utilities.
- `tests/unit/test_LspJsonRpc.cpp` for test style.

**Test scenarios:**

*Happy path:*
- `// this is a comment` → single span col 0, len=full line, type `"comment"`.
- `/* block */` single line → span covering `/* block */`.
- `/* block\n   continues */` two-line → line 0 gets `[0, EOL)` as comment; line 1 gets `[0, col-after-*/)` as comment.
- `"hello world"` → single string span.
- `'x'` → single char span.
- `#include <cstdlib>` → preprocessor span for `#include`, string span for `<cstdlib>`.
- `#include "myheader.hpp"` → preprocessor span for `#include`, string span for `"myheader.hpp"`.
- `#define FOO 42` → single preprocessor span covering the whole line.
- `int`, `const`, `return`, `void`, `auto`, `namespace`, `class`, `struct`, `if`,
  `else`, `for`, `while`, `switch`, `case`, `break`, `continue`, `new`, `delete`,
  `nullptr`, `true`, `false`, `this`, `sizeof`, `static`, `inline`, `virtual`,
  `override`, `final`, `explicit`, `constexpr`, `noexcept`, `template`, `typename`,
  `using`, `typedef`, `enum`, `operator`, `public`, `private`, `protected` → each
  produces a `"keyword"` span at the correct col and length.
- `42`, `3.14`, `0xFF`, `0b1010`, `1ULL`, `1.5f` → each produces a `"number"` span.
- `+`, `-`, `*`, `/`, `%`, `=`, `<`, `>`, `!`, `&`, `|`, `^`, `~`, `?`, `:` →
  single-char `"syn_operator"` spans.
- `<<`, `>>`, `->`, `::`, `<=`, `>=`, `!=`, `==`, `&&`, `||`, `++`, `--` → two-char
  `"syn_operator"` spans (longest-match).
- `>>=` → single three-char `"syn_operator"` span (not two spans `>>` + `=`).
- `(`, `)`, `{`, `}`, `[`, `]` → `"bracket"` spans.

*Edge cases — state machine context isolation:*
- `std::cout << "// not a comment";` → `//` inside string is not a comment span.
- `std::cout << "hello"; // real comment` → string span for `"hello"`, comment span for `// real comment`.
- `/* comment with "string inside" */` → single comment span; no string span inside.
- `"escape: \""` → string span covers full `"escape: \""` (escaped quote does not close string).
- `'\\' + x` → char span for `'\\'` only; `+` is an operator after.
- `/* unclosed` (no `*/`) → comment span to EOF.
- `"unterminated` (no closing `"`) → string span to EOL; next line starts in CODE.
- Empty line → empty SpanList (no crash).
- Line with only whitespace → empty SpanList.
- Identifier that is a prefix of a keyword (`integer`, `constexpr_value`) → not a keyword span.
- `// keyword int const` → comment span; no keyword spans inside.
- `#include` with no path → preprocessor span for directive; no string span.
- `0xFF + 0b1010` → two number spans, one operator span between them.

*Error/boundary:*
- Source string empty → returns empty map (no crash).
- Single-char source `"x"` → no span if not a special character.
- `\n` only → empty map.

**Verification:**
- `ctest -R SyntaxHighlighter` passes all test scenarios.
- No FTXUI or LspService headers are included in `SyntaxHighlighter.hpp`.

---

- U2. **`token_color`: add `"preprocessor"`, `"bracket"`, `"syn_operator"` branches**

**Goal:** Extend the colour mapping so the three new syntactic type strings produce
distinct, visually appropriate colours.

**Requirements:** R1–R7

**Dependencies:** U1 (defines the new type strings)

**Files:**
- Modify: `src/drivers/FtxuiRenderer.cpp`

**Approach:**
- Add three branches to `token_color()` before the final `return Default`:
  - `"preprocessor"` → `ftxui::Color::BlueLight`
  - `"bracket"` → `ftxui::Color::White`
  - `"syn_operator"` → `ftxui::Color::Cyan`
- No other changes to `token_color` or `colorize_line`.

**Patterns to follow:**
- Existing `if (type == "keyword")` pattern in `token_color`.

**Test scenarios:**

Test expectation: none — colour mapping is a one-liner lookup; covered visually by
running the editor and indirectly by the merge integration test (U3).

**Verification:**
- `cmake --build build` succeeds with no warnings on the modified file.

---

- U3. **`render_buffer`: merge syntactic and LSP span maps (LSP wins)**

**Goal:** Wire `highlight_file` into `render_buffer()`, merge the two span maps so LSP
spans overwrite overlapping syntactic spans, and pass the merged map to `colorize_line`.

**Requirements:** R8, R10

**Dependencies:** U1, U2

**Files:**
- Modify: `src/drivers/FtxuiRenderer.cpp`
- Test: `tests/unit/test_SyntaxHighlighter.cpp` (add merge tests)

**Approach:**
- Call `highlight_file(full_source)` once per `render_buffer()` call, where
  `full_source = buf.to_string()` (already available via `Buffer::to_string()`).
- For each line `i`:
  1. Start with `merged = regex_map[i]` (copy or move from syntactic output).
  2. For each LSP span on line `i`, remove or trim any `merged` spans that overlap it,
     then insert the LSP span.
  3. Re-sort `merged` by col.
  4. Pass `merged` (not the original `token_map[i]`) to `colorize_line`.
- The merge loop runs only on lines that have at least one LSP span; lines with only
  syntactic spans pass through unchanged.
- `colorize_line` signature and implementation are **not** changed.

**Patterns to follow:**
- Existing `token_map` construction loop in `render_buffer()`.

**Test scenarios (merge logic — tested via free functions, not the renderer):**

Add a `merge_spans` free function (or test the merged output directly):
- LSP span `[5, 8)` fully covers syntactic span `[5, 8)` → only LSP span in output.
- LSP span `[5, 8)` partially overlaps syntactic span `[3, 7)` → syntactic trimmed to
  `[3, 5)`, LSP span `[5, 8)` inserted.
- LSP span `[5, 8)` partially overlaps syntactic span `[6, 10)` → syntactic trimmed to
  `[8, 10)`, LSP span `[5, 8)` inserted.
- LSP span `[5, 8)` fully contains syntactic span `[6, 7)` → syntactic span removed.
- No overlap → both spans preserved in col order.
- Multiple LSP spans on same line, each overlapping different syntactic spans → all
  resolved correctly; output is sorted by col with no overlaps.
- Line has only syntactic spans (no LSP) → syntactic spans passed through unchanged.
- Line has only LSP spans (no syntactic) → LSP spans passed through unchanged.

**Verification:**
- `ctest -R SyntaxHighlighter` passes all merge tests.
- `cmake --build build --target editor` succeeds.
- Running `./build/editor src/main.cpp` shows keywords, comments, preprocessor
  directives, and string literals in colour; `::` operators coloured; LSP-identified
  identifiers retain their LSP colour.

---

- U4. **Commit and push to PR**

**Goal:** Clean state, 200/200 tests passing, pushed to remote.

**Requirements:** All R1–R11

**Dependencies:** U1, U2, U3

**Files:** (no new files; verification only)

**Approach:**
- Remove the temporary legend-dump `#include <fstream>` and `ofstream` block from
  `src/core/usecases/LspService.cpp` if still present.
- Run `ctest --output-on-failure` — confirm 200+N tests pass (N = new syntactic tests).
- Commit with a message following the `[FEAT][HIGHLIGHT]` prefix convention used in
  recent commits on this branch.
- Push to `feat/lsp-e2e-tests`.

**Test scenarios:**

Test expectation: none — this is a verification and commit unit, not a behaviour unit.

**Verification:**
- `git push` succeeds.
- PR #15 reflects the new commits.

---

## System-Wide Impact

- **Interaction graph:** `render_buffer()` gains one extra call per render frame
  (`highlight_file`). The FTXUI render loop calls this on every keypress and on every
  LSP async update. For a ~40-line file the overhead is negligible; for a large file
  the state machine scan cost should be profiled if render latency is noticed.
- **Error propagation:** `highlight_file` returns an empty map on empty input; no
  exceptions are thrown. `render_buffer` falls back gracefully (LSP-only highlighting)
  if the syntactic map is empty.
- **State lifecycle risks:** `highlight_file` is a pure function with no shared state.
  No mutex or locking is required.
- **API surface parity:** `colorize_line` and `token_color` are in an anonymous
  namespace; no external callers affected.
- **Integration coverage:** The merge unit test (U3) exercises the interaction between
  syntactic and LSP spans without requiring a live renderer.
- **Unchanged invariants:** `colorize_line` signature, cursor-injection logic, and all
  existing LSP overlay rendering (diagnostics, hover, completion, locations, symbols,
  inlay hints) are untouched.

---

## Risks & Dependencies

| Risk | Mitigation |
|------|------------|
| State machine mis-handles `\\` escape inside strings causing remainder of line to be wrongly coloured | Unit test: `"escape: \""` and `'\\'`; fix is localised to STRING/CHAR state skip logic |
| Block comment spanning many lines causes O(n²) span-list growth | Each line gets its own span entry; cost is O(lines-in-comment), not quadratic |
| Merge logic trims LSP spans instead of syntactic spans (inverted priority) | Merge unit tests in U3 explicitly assert LSP span survives; syntactic span is trimmed |
| Render latency increases noticeably for large files | State machine is a single O(n) pass; benchmark only if the user reports lag |
| `highlight_file` called with `\r\n` line endings | Treat `\r` as whitespace inside `CODE`; `\n` is always the line boundary |

---

## Sources & References

- Related code: `src/drivers/FtxuiRenderer.cpp`
- Related code: `src/core/usecases/LspService.hpp` (`LspSemanticToken`)
- Related PRs: #15 (`feat/lsp-e2e-tests`)
- clangd semantic token legend (runtime): 25 types, no `keyword` or `comment` emitted
