#include "PtyHarness.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <util.h>  // forkpty on macOS (pty.h on Linux)

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace editor::e2e {

// Strips ANSI/VT100 escape sequences from a raw PTY chunk.
static std::string strip_ansi(const std::string& s) {
    static const std::regex kAnsi("\x1b(\\[[^a-zA-Z]*[a-zA-Z]|[^\\[])");
    return std::regex_replace(s, kAnsi, "");
}

// Extracts the latest complete frame from raw_buf.
// FTXUI emits ESC[?25l (cursor hide) at the start of every frame, then writes
// the full screen content line by line. The last occurrence of ESC[?25l marks
// the most recently rendered frame.
static std::vector<std::string> extract_latest_frame(const std::string& raw_buf) {
    static const std::string kFrameStart = "\x1b[?25l";

    // Find the last frame boundary.
    auto pos = raw_buf.rfind(kFrameStart);
    if (pos == std::string::npos) return {};

    std::string frame_raw = raw_buf.substr(pos + kFrameStart.size());
    std::string plain = strip_ansi(frame_raw);

    // Split on \r\n or \n, trim trailing whitespace from each line.
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < plain.size()) {
        auto end = plain.find('\n', start);
        if (end == std::string::npos) end = plain.size();
        std::string line = plain.substr(start, end - start);
        // Strip trailing \r and spaces.
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

// Returns true if every non-empty expected line appears as a substring of at
// least one screen line. Order is not required; each expected line is matched
// independently against the full set of screen lines.
static bool frame_matches(const std::vector<std::string>& screen,
                          const std::vector<std::string>& expected_lines) {
    for (const auto& exp : expected_lines) {
        if (exp.empty()) continue;
        bool found = false;
        for (const auto& sline : screen) {
            if (sline.find(exp) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

struct PtyHarness::Impl {
    std::thread reader_thread;
    std::mutex mtx;
    std::condition_variable cv;
    std::string raw_buf;    // raw bytes including ANSI sequences
    std::string plain_buf;  // accumulated plain text (legacy wait_for)
    std::atomic<bool> running{true};
};

PtyHarness::PtyHarness(const std::string& binary, std::vector<std::string> args)
    : pty_fd_(-1), pid_(-1), impl_(new Impl) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(binary.c_str()));
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    struct winsize ws{};
    ws.ws_col = 220;
    ws.ws_row = 50;

    pid_ = forkpty(&pty_fd_, nullptr, nullptr, &ws);
    if (pid_ < 0) throw std::runtime_error(std::string("forkpty failed: ") + strerror(errno));

    if (pid_ == 0) {
        execvp(binary.c_str(), argv.data());
        std::perror("execvp");
        _exit(127);
    }

    start_reader();
}

PtyHarness::~PtyHarness() {
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        for (int i = 0; i < 10; ++i) {
            int status;
            if (waitpid(pid_, &status, WNOHANG) == pid_) {
                pid_ = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (pid_ > 0) {
            kill(pid_, SIGKILL);
            int status;
            waitpid(pid_, &status, 0);
            pid_ = -1;
        }
    }
    if (pty_fd_ >= 0) {
        close(pty_fd_);
        pty_fd_ = -1;
    }
    stop_reader();
    delete impl_;
}

void PtyHarness::write(const std::string& data) {
    const char* p = data.data();
    std::size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t n = ::write(pty_fd_, p, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("PTY write failed: ") + strerror(errno));
        }
        p += n;
        remaining -= static_cast<std::size_t>(n);
    }
}

bool PtyHarness::expect_screen(const std::string& expected, int timeout_ms) {
    // Parse expected into lines.
    std::vector<std::string> expected_lines;
    std::size_t start = 0;
    while (start <= expected.size()) {
        auto end = expected.find('\n', start);
        if (end == std::string::npos) end = expected.size();
        expected_lines.push_back(expected.substr(start, end - start));
        if (end == expected.size()) break;
        start = end + 1;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::unique_lock lock(impl_->mtx);
    return impl_->cv.wait_until(lock, deadline, [&] {
        auto frame = extract_latest_frame(impl_->raw_buf);
        return frame_matches(frame, expected_lines);
    });
}

std::string PtyHarness::current_frame() const {
    std::lock_guard lock(impl_->mtx);
    auto lines = extract_latest_frame(impl_->raw_buf);
    std::string result;
    for (auto& l : lines) {
        if (!l.empty()) {
            result += l;
            result += '\n';
        }
    }
    return result;
}

std::string PtyHarness::screen_text() const {
    std::lock_guard lock(impl_->mtx);
    return impl_->plain_buf;
}

bool PtyHarness::wait_for(const std::string& pattern, int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::unique_lock lock(impl_->mtx);
    return impl_->cv.wait_until(
        lock, deadline, [&] { return impl_->plain_buf.find(pattern) != std::string::npos; });
}

int PtyHarness::wait_exit(int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        int status;
        pid_t result = waitpid(pid_, &status, WNOHANG);
        if (result == pid_) {
            pid_ = -1;
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return -1;
}

void PtyHarness::start_reader() {
    impl_->reader_thread = std::thread([this] {
        char chunk[4096];
        while (impl_->running.load()) {
            ssize_t n = ::read(pty_fd_, chunk, sizeof(chunk));
            if (n <= 0) break;
            std::string chunk_str(chunk, static_cast<std::size_t>(n));
            {
                std::lock_guard lock(impl_->mtx);
                impl_->raw_buf += chunk_str;
                impl_->plain_buf += strip_ansi(chunk_str);
            }
            impl_->cv.notify_all();
        }
    });
}

void PtyHarness::stop_reader() {
    impl_->running.store(false);
    if (impl_->reader_thread.joinable()) impl_->reader_thread.join();
}

}  // namespace editor::e2e
