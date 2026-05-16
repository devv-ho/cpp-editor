// PtyHarness -- spawns the editor binary inside a pseudo-terminal (PTY).
//
// A PTY makes the editor behave as if it has a real terminal: isatty() returns
// true, FTXUI draws full TUI output, and we can send keystrokes as raw bytes.
//
// FTXUI emits ESC[?25l (cursor hide) at the start of every frame, then writes
// the full screen content. The harness tracks the raw byte stream and extracts
// the latest complete frame on demand.
//
// Preferred usage:
//   PtyHarness h("editor", {"/tmp/test.cpp"});
//   h.write("i");
//   h.expect_screen("hello\n INSERT ", 2000);
//
// expect_screen(expected, timeout_ms):
//   Blocks until the latest rendered frame contains every non-empty line in
//   `expected` as a substring of the corresponding trimmed screen line.
//   Fails immediately (returns false) if any line does not match.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace editor::e2e {

class PtyHarness {
public:
    // Spawns `binary` with `args` inside a PTY.
    // Throws std::runtime_error if forkpty/execvp fails.
    PtyHarness(const std::string& binary, std::vector<std::string> args);

    ~PtyHarness();

    // Non-copyable, non-movable (owns raw fd and pid).
    PtyHarness(const PtyHarness&) = delete;
    PtyHarness& operator=(const PtyHarness&) = delete;

    // Sends raw bytes to the editor's stdin (through the PTY master).
    void write(const std::string& data);

    // Blocks until the latest rendered frame satisfies expected_lines, or
    // timeout_ms elapses. expected is a newline-separated list of substrings;
    // each non-empty expected line must appear as a substring of the
    // corresponding trimmed screen line (by position). Returns true on match.
    bool expect_screen(const std::string& expected, int timeout_ms = 2000);

    // Blocks until the status bar line starts exactly with " {mode}  "
    // (the prefix produced by FtxuiRenderer::render_statusbar). The status
    // bar is the last non-empty line of the frame. Exact prefix comparison
    // prevents buffer content from producing a false positive.
    bool validate_status(std::string_view mode, int timeout_ms = 2000);

    // Blocks until every non-empty line in `expected` (newline-separated)
    // appears as a substring of at least one line in the buffer region
    // (all frame lines except the last non-empty status bar line).
    bool validate_buffer(const std::string& expected, int timeout_ms = 2000);

    // Returns the latest decoded frame as a newline-separated string (for
    // failure messages).
    std::string current_frame() const;

    // Blocks until the child process exits. Returns its exit status.
    int wait_exit(int timeout_ms = 5000);

    // Legacy: blocks until pattern appears anywhere in accumulated output.
    bool wait_for(const std::string& pattern, int timeout_ms = 3000);

    // Returns all accumulated plain text (for debugging).
    std::string screen_text() const;

private:
    int pty_fd_;
    pid_t pid_;

    struct Impl;
    Impl* impl_;

    void start_reader();
    void stop_reader();
};

}  // namespace editor::e2e
