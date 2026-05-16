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

// Strips ANSI/VT100 escape sequences so pattern matching works on plain text.
// Covers: CSI sequences (ESC [ ... letter), lone ESC + one char, and DCS (ESC P).
static std::string strip_ansi(const std::string& s) {
    static const std::regex kAnsi("\x1b(\\[[^a-zA-Z]*[a-zA-Z]|[^\\[])");
    return std::regex_replace(s, kAnsi, "");
}

struct PtyHarness::Impl {
    std::thread reader_thread;
    std::mutex mtx;
    std::condition_variable cv;
    std::string raw_buf;    // raw bytes from PTY (for debug)
    std::string plain_buf;  // ANSI-stripped, for pattern matching
    std::atomic<bool> running{true};
};

PtyHarness::PtyHarness(const std::string& binary, std::vector<std::string> args)
    : pty_fd_(-1), pid_(-1), impl_(new Impl) {
    // Build argv for execvp: [binary, args..., nullptr]
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(binary.c_str()));
    for (auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    struct winsize ws{};
    ws.ws_col = 220;
    ws.ws_row = 50;

    pid_ = forkpty(&pty_fd_, nullptr, nullptr, &ws);
    if (pid_ < 0) {
        throw std::runtime_error(std::string("forkpty failed: ") + strerror(errno));
    }

    if (pid_ == 0) {
        // Child: exec the editor binary.
        execvp(binary.c_str(), argv.data());
        // execvp only returns on failure.
        std::perror("execvp");
        _exit(127);
    }

    start_reader();
}

PtyHarness::~PtyHarness() {
    // Kill the child first so clangd's subprocess pipe closes, then close the
    // PTY master fd so the reader thread's ::read() unblocks and can be joined.
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        // Give it 500ms to exit gracefully, then force-kill.
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
            if (n <= 0) {
                break;  // PTY closed (editor exited)
            }
            {
                std::string chunk_str(chunk, static_cast<std::size_t>(n));
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
    if (impl_->reader_thread.joinable()) {
        impl_->reader_thread.join();
    }
}

}  // namespace editor::e2e
