# Coding Rules

Rules that apply to all code in this project. Enforced in code review.

---

## Naming

| Thing | Convention | Example |
|---|---|---|
| Private member fields | `trailing_underscore_` | `lines_`, `pos_`, `buffer_` |
| Types / classes | `PascalCase` | `Buffer`, `EditorMode` |
| Functions / methods | `snake_case` | `insert_char`, `line_count` |
| Constants / enumerators | `PascalCase` | `BufferError::LineOutOfRange` |
| Namespaces | `snake_case` | `editor::core` |
| Files | `PascalCase.hpp / .cpp` | `Buffer.hpp`, `Buffer.cpp` |

---

## Error handling

- Use `std::expected<T, E>` for all fallible operations. No exceptions across
  layer boundaries.
- Use project-defined error enums (e.g. `BufferError`). Never return raw `int`
  error codes or `-1` sentinels.
- Mark all `std::expected`-returning functions `[[nodiscard]]`. Ignoring an
  error is a deliberate opt-out, not a silent default.

---

## Attributes

| Attribute | When to use |
|---|---|
| `[[nodiscard]]` | Any function whose return value must not be silently discarded — all `std::expected`, `std::optional`, factory functions |
| `noexcept` | Any function that genuinely cannot throw — value-type operations, move ops, destructors, clamping helpers |
| `[[maybe_unused]]` | Parameters that exist for interface compliance but are unused in this translation unit |

---

## Modern C++ preferences

- Prefer `std::ranges::` and `std::views::` over raw index loops.
- Use `emplace_back` over `push_back` when constructing in-place — avoids a
  temporary copy.
- Use `std::string_view` for read-only string parameters. Use `std::string` only
  when ownership or mutation is needed.
- Use `std::move` when transferring ownership of a local into a container.
- Never use `while (true)` for iteration that has a natural range — use ranges
  or a `for` loop with a clear termination condition.

---

## Line endings

- The `Buffer` constructor normalises `\r\n` (CRLF) to `\n` (LF) on load.
- All internal storage uses LF only.
- Tests must cover both LF-only and CRLF input.

---

## Comments

- Write comments only when the **why** is non-obvious.
- Do not write comments that explain what the code does — well-named identifiers
  do that.
- Do not write tutorial or study notes in source code — those belong in
  `docs/`.

---

## Tests

- Every new type gets a dedicated test file: `tests/unit/test_<Type>.cpp`.
- Every new method gets at least one happy-path test and one error-path test.
- Test names follow `WhatItDoes` pattern: `InsertCharAtEndOfLine`,
  `DeleteCharOnEmptyLineReturnsError`.
- Copy construction and copy assignment must be tested for all value types.
