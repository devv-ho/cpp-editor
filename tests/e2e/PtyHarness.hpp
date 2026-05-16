// PtyHarness -- spawns the editor binary inside a pseudo-terminal (PTY).
//
// A PTY makes the editor behave as if it has a real terminal: isatty() returns
// true, FTXUI draws full TUI output, and we can send keystrokes as raw bytes.
//
// Usage:
//   PtyHarness h("editor", {"/tmp/test.cpp"});
//   h.write("ihello");          // send keystrokes
//   h.wait_for("hello", 2000);  // block until pattern appears in screen output
//   h.write("\x1b");            // ESC back to normal mode
//
// The harness reads PTY output in a background thread so the main test thread
// never blocks waiting for the editor to flush.

#pragma once

#include <string>
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

    // Returns all bytes received from the PTY so far.
    std::string screen_text() const;

    // Blocks until `pattern` appears in received output or `timeout_ms` elapses.
    // Returns true if pattern found, false on timeout.
    bool wait_for(const std::string& pattern, int timeout_ms = 3000);

    // Blocks until the child process exits. Returns its exit status.
    int wait_exit(int timeout_ms = 5000);

private:
    int pty_fd_;  // PTY master fd (read editor output / write keystrokes)
    pid_t pid_;   // child PID

    // Background reader thread state.
    struct Impl;
    Impl* impl_;

    void start_reader();
    void stop_reader();
};

}  // namespace editor::e2e
