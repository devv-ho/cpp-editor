#include "adapters/lsp/ClangdProcess.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include "adapters/lsp/LspDecoder.hpp"

namespace editor::adapters::lsp {

// pipe() returns -1 on failure and sets errno -- no exception type, C convention.
constexpr int kReadEnd = 0;
constexpr int kWriteEnd = 1;
// fork() returns 0 to the child, the child's PID to the parent.
constexpr pid_t kChildPid = 0;

ClangdProcess::ClangdProcess(std::string_view clangd_path) {
    std::array<int, 2> to_child{};
    std::array<int, 2> from_child{};

    if (pipe(to_child.data()) != 0 || pipe(from_child.data()) != 0) {
        throw std::runtime_error(std::string("pipe: ") + strerror(errno));
    }

    pid_ = fork();
    if (pid_ < 0) {
        throw std::runtime_error(std::string("fork: ") + strerror(errno));
    }

    if (pid_ == kChildPid) {
        // ── Child only ────────────────────────────────────────────────────────
        // Rewire fd 0/1 so clangd's stdin/stdout go through the pipes,
        // then exec clangd. Everything below this block is parent-only because
        // execvp replaces this process entirely; _exit(1) is the fallback if
        // execvp fails -- execution never falls through to the parent code.
        dup2(to_child[kReadEnd], STDIN_FILENO);
        dup2(from_child[kWriteEnd], STDOUT_FILENO);

        close(to_child[kReadEnd]);
        close(to_child[kWriteEnd]);
        close(from_child[kReadEnd]);
        close(from_child[kWriteEnd]);

        std::string path(clangd_path);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        char* argv[] = {const_cast<char*>(path.c_str()), nullptr};
        execvp(argv[0], argv);
        _exit(1);
    } else {
        // ── Parent only ───────────────────────────────────────────────────────
        // Close the ends the parent handed to the child -- keeping them open
        // would prevent the pipe from reaching EOF when clangd exits.
        close(to_child[kReadEnd]);
        close(from_child[kWriteEnd]);

        clangd_stdin_fd_ = to_child[kWriteEnd];
        clangd_stdout_fd_ = from_child[kReadEnd];

        reader_thread_ = std::thread(&ClangdProcess::reader_loop, this);
    }
}

ClangdProcess::~ClangdProcess() {
    running_ = false;
    close(clangd_stdin_fd_);
    close(clangd_stdout_fd_);  // unblocks the reader_loop read()

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }

    kill(pid_, SIGTERM);  // ask clangd to shut down cleanly
    waitpid(pid_, nullptr, 0);
}

void ClangdProcess::send(const std::string& message) {
    const char* ptr = message.data();
    std::size_t remaining = message.size();
    while (remaining > 0) {
        // write() may write fewer bytes than requested -- loop until all sent.
        // Returns -1 on error (e.g. clangd exited and closed the pipe).
        ssize_t written = write(clangd_stdin_fd_, ptr, remaining);
        if (written <= 0) {
            break;
        }
        ptr += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

std::optional<LspMessage> ClangdProcess::receive() {
    // unique_lock required here (not lock_guard) because condition_variable::wait
    // needs to temporarily release the lock while the thread sleeps.
    std::unique_lock lock(queue_mutex_);
    queue_cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

    if (queue_.empty()) {
        return std::nullopt;
    }

    LspMessage msg = std::move(queue_.front());
    queue_.pop_front();
    return msg;
}

void ClangdProcess::reader_loop() {
    LspDecoder decoder;
    std::array<char, 4096> buf{};

    while (running_) {
        // read() blocks here until clangd writes data or the pipe closes.
        // Returns 0 on EOF (clangd exited), -1 on error.
        ssize_t n = read(clangd_stdout_fd_, buf.data(), buf.size());
        if (n <= 0) {
            break;
        }

        decoder.feed(std::string_view(buf.data(), static_cast<std::size_t>(n)));

        {
            std::lock_guard lock(queue_mutex_);
            while (auto msg = decoder.next_message()) {
                // std::move transfers ownership into the queue without copying
                // the json payload -- cheaper than a deep copy for large messages.
                queue_.push_back(std::move(*msg));
            }
        }
        // Wake any thread blocked in receive() so it can pop the new messages.
        queue_cv_.notify_all();
    }

    // Clangd exited or pipe errored -- signal receive() to return nullopt.
    running_ = false;
    queue_cv_.notify_all();
}

}  // namespace editor::adapters::lsp
