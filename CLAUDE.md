# CLAUDE.md — Editor Project

## What this is
A TUI code editor written in C++23, modeled after Vim. Built with FTXUI for rendering.
This is a **prototype/blueprint** — the human will extend it. Claude's role after the prototype is senior engineer / architect / TDD manager.

## Stack
- **Language**: C++23
- **TUI**: FTXUI
- **Tests**: GoogleTest (unit) + custom pty-based harness (e2e)
- **Build**: CMake 3.20+
- **LSP**: clangd via subprocess, JSON-RPC over stdin/stdout, async via `std::thread` + message queue

## Scope (prototype)
- Vim modal editing: Normal / Insert mode
- Keys: `h j k l gg G $ 0 i a ESC Backspace Return`
- File open/save via CLI arg
- LSP: real clangd subprocess, diagnostics displayed in TUI
- Unit tests for buffer, cursor, LSP message parsing
- E2e test harness (fork + execvp + pty)
- No undo/redo, no tabs, no file tree, no config file

## Principles
- SOLID, OOP, modern C++23
- Design patterns where they genuinely fit — each one called out in docs
- No over-engineering: patterns serve the code, not the other way around
- Strict tests: unit + e2e coverage
- Well-written docs

## Key design decisions
- Async LSP I/O: `std::thread` + `std::mutex` / `std::condition_variable` message queue (no boost/libuv)
- E2e tests: gtest-based, spawn editor binary over a pty, assert on screen state
- Language support: C++ only now, but architecture must not prevent adding more languages later

## Claude's role
- Read this file at the start of every session
- Act as senior engineer + architect + TDD manager
- Do not add features beyond what is asked
- Ask before introducing new dependencies

## Documented Solutions

`docs/solutions/` — documented solutions to past problems (bugs, best practices, workflow patterns), organized by category with YAML frontmatter (`module`, `tags`, `problem_type`). Relevant when implementing or debugging in documented areas.
