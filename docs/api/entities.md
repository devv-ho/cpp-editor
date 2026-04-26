# API — Core Entities

All entities live in namespace `editor::core`. Headers are in `src/core/entities/`.

---

## Position

**Header:** `core/entities/Position.hpp`

Zero-based (line, column) coordinate. Plain aggregate — no invariants to enforce.

```cpp
struct Position {
    std::size_t line{0};
    std::size_t col{0};
};
```

### Members

| Member | Type | Description |
|---|---|---|
| `line` | `std::size_t` | Zero-based line index |
| `col` | `std::size_t` | Zero-based column index |

### Operators

| Operator | Description |
|---|---|
| `<=>` (defaulted) | Full ordering: line-major, then col. Generates `<`, `>`, `<=`, `>=`, `==`, `!=`. |

### Methods

```cpp
std::string to_string() const;
// Returns "(line, col)" via std::format. For diagnostics and logging.
```

---

## BufferError

**Header:** `core/entities/Buffer.hpp`

Error codes returned by `Buffer` mutations.

```cpp
enum class BufferError {
    LineOutOfRange,  // line index >= line_count()
    ColOutOfRange,   // col index > line length (insert) or >= line length (delete)
};
```

---

## Buffer

**Header:** `core/entities/Buffer.hpp`

Owns the file text as a sequence of lines. Newline characters are not stored — they are implied between elements.

```cpp
class Buffer;
```

### Constructors

```cpp
Buffer();
// Single empty line "".

explicit Buffer(std::string_view text);
// Splits text on '\n'. Always produces at least one line.
```

### Queries

```cpp
std::size_t line_count() const noexcept;
// Number of lines. Always >= 1.

std::expected<std::string_view, BufferError> line(std::size_t idx) const noexcept;
// View of line at idx. Error: LineOutOfRange.

std::expected<std::size_t, BufferError> line_length(std::size_t idx) const noexcept;
// Character count of line at idx. Error: LineOutOfRange.

bool is_empty() const noexcept;
// True iff line_count() == 1 && line(0) == "".

std::string to_string() const;
// Joins all lines with '\n'. Inverse of the constructor.
```

### Mutations

All mutations return `std::expected<void, BufferError>`. Check `.has_value()` before assuming success.

```cpp
std::expected<void, BufferError> insert_char(Position pos, char ch);
// Insert ch at pos. pos.col may equal line length (append).
// Errors: LineOutOfRange, ColOutOfRange.

std::expected<void, BufferError> delete_char(Position pos);
// Delete character at pos. pos.col must be < line length.
// Errors: LineOutOfRange, ColOutOfRange.

std::expected<void, BufferError> insert_newline(Position pos);
// Split line at pos into two lines.
// Errors: LineOutOfRange, ColOutOfRange.
```

---

## Cursor

**Header:** `core/entities/Cursor.hpp`

Owns the caret `Position` and enforces buffer boundary invariants on every motion. All methods clamp silently — callers never receive an out-of-range position.

```cpp
class Cursor;
```

### Constructor

```cpp
explicit Cursor(const Buffer& buffer);
// Holds a reference to buffer — buffer must outlive cursor.
// Initial position: {0, 0}.
```

### Queries

```cpp
Position    position() const noexcept;
std::size_t line()     const noexcept;
std::size_t col()      const noexcept;
```

### Vim motions (normal mode)

All clamp to valid buffer range.

```cpp
void move_left()   noexcept;  // h — col--, clamps at 0
void move_right()  noexcept;  // l — col++, clamps at len-1
void move_up()     noexcept;  // k — line--, clamps col to new line length
void move_down()   noexcept;  // j — line++, clamps col to new line length
void move_top()    noexcept;  // gg — {0, 0}
void move_bottom() noexcept;  // G  — {last_line, 0}
void move_sol()    noexcept;  // 0  — col = 0
void move_eol()    noexcept;  // $  — col = len-1 (0 if empty)
```

### Insert-mode advance

```cpp
void advance_col() noexcept;
// Increment col without clamping. Used by Document::insert_char —
// insert mode allows col == len (past last character).
```

### Direct positioning

```cpp
void set_position(Position pos) noexcept;
// Set position, clamping both line and col to valid range.
```

---

## Document

**Header:** `core/entities/Document.hpp`

Composes `Buffer` and `Cursor` into the single unit of state the editor operates on. Use cases and commands receive a `Document&` — they never touch `Buffer` or `Cursor` directly.

```cpp
class Document;
```

### Constructor

```cpp
explicit Document(std::string_view text = "");
// Constructs Buffer from text, Cursor from Buffer.
```

### Buffer access

```cpp
const Buffer& buffer()                                          const noexcept;
std::size_t   line_count()                                      const noexcept;
std::expected<std::string_view, BufferError> line(std::size_t) const noexcept;
```

### Cursor access

```cpp
Position  position() const noexcept;
Cursor&   cursor()         noexcept;
// Direct cursor access for vim motions.
```

### Mutations

```cpp
std::expected<void, BufferError> insert_char(char ch);
// Insert ch at cursor position, then advance_col().

std::expected<void, BufferError> delete_char();
// Delete character at cursor position. Cursor does not move.

std::expected<void, BufferError> insert_newline();
// Split line at cursor, move cursor to {line+1, 0}.
```

### Serialisation

```cpp
std::string to_string() const;
// Full buffer text as newline-separated string.
```

### File path

```cpp
void set_path(std::filesystem::path path) noexcept;
const std::filesystem::path& path() const noexcept;
// Optional metadata — the file this document was loaded from.
```
