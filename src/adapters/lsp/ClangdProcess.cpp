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

ClangdProcess::ClangdProcess(std::string_view clangd_path) {
    // pipe[0] = read end, pipe[1] = write end.
    std::array<int, 2> to_child{};
    std::array<int, 2> from_child{};

    if (pipe(to_child.data()) != 0 || pipe(from_child.data()) != 0) {
        throw std::runtime_error(std::string("pipe: ") + strerror(errno));
    }

    pid_ = fork();
    if (pid_ < 0) {
        throw std::runtime_error(std::string("fork: ") + strerror(errno));
    }

    if (pid_ == 0) {
        // Child: wire pipes to stdin/stdout, then exec clangd.
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);

        close(to_child[0]);
        close(to_child[1]);
        close(from_child[0]);
        close(from_child[1]);

        std::string path(clangd_path);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        char* argv[] = {const_cast<char*>(path.c_str()), nullptr};
        execvp(argv[0], argv);
        // execvp only returns on failure.
        _exit(1);
    }

    // Parent: close the ends the parent doesn't use.
    close(to_child[0]);
    close(from_child[1]);

    stdin_fd_ = to_child[1];
    stdout_fd_ = from_child[0];

    reader_thread_ = std::thread(&ClangdProcess::reader_loop, this);
}

ClangdProcess::~ClangdProcess() {
    running_ = false;
    close(stdin_fd_);
    close(stdout_fd_);  // unblocks the reader_loop read()

    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }

    kill(pid_, SIGTERM);
    waitpid(pid_, nullptr, 0);
}

void ClangdProcess::send(const std::string& message) {
    const char* ptr = message.data();
    std::size_t remaining = message.size();
    while (remaining > 0) {
        ssize_t written = write(stdin_fd_, ptr, remaining);
        if (written <= 0) {
            break;
        }
        ptr += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

std::optional<LspMessage> ClangdProcess::receive() {
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
        ssize_t n = read(stdout_fd_, buf.data(), buf.size());
        if (n <= 0) {
            break;
        }

        decoder.feed(std::string_view(buf.data(), static_cast<std::size_t>(n)));

        {
            std::lock_guard lock(queue_mutex_);
            while (auto msg = decoder.next_message()) {
                queue_.push_back(std::move(*msg));
            }
        }
        queue_cv_.notify_all();
    }

    running_ = false;
    queue_cv_.notify_all();  // wake any blocked receive() so it can return nullopt
}

}  // namespace editor::adapters::lsp
