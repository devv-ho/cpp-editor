// Defines ClangdProcess -- owns the clangd subprocess and its I/O threads.
//
// Spawns clangd via fork/execvp. A background reader thread feeds raw bytes
// from clangd's stdout into an LspDecoder and pushes complete LspMessages onto
// an internal queue. Callers write via send() and block-read via receive().

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "adapters/lsp/LspMessage.hpp"

namespace editor::adapters::lsp {

class ClangdProcess {
public:
    explicit ClangdProcess(std::string_view clangd_path = "clangd");
    ~ClangdProcess();

    ClangdProcess(const ClangdProcess&) = delete;
    ClangdProcess& operator=(const ClangdProcess&) = delete;

    // Writes a wire-format message to clangd's stdin.
    void send(const std::string& message);

    // Blocks until a message is available or the process exits.
    // Returns std::nullopt when clangd has exited and the queue is drained.
    std::optional<LspMessage> receive();

private:
    pid_t pid_;
    int clangd_stdin_fd_;
    int clangd_stdout_fd_;

    std::thread reader_thread_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<LspMessage> queue_;
    std::atomic<bool> running_{true};

    void reader_loop();
};

}  // namespace editor::adapters::lsp
