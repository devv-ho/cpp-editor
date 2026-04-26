# Architecture

## Overview

This editor follows Clean Architecture. Dependencies point inward — outer layers know about inner layers, never the reverse.

```
┌─────────────────────────────────────────────┐
│  Frameworks & Drivers  (editor_drivers)      │
│  FtxuiRenderer  ClangdProcess               │
│                                             │
│  ┌───────────────────────────────────────┐  │
│  │  Interface Adapters  (editor_adapters) │  │
│  │  InputTranslator  LspJsonRpc          │  │
│  │                                       │  │
│  │  ┌─────────────────────────────────┐  │  │
│  │  │  Use Cases  (editor_core)        │  │  │
│  │  │  EditorMode  EditorCommands      │  │  │
│  │  │  LspService  FileService         │  │  │
│  │  │                                  │  │  │
│  │  │  ┌────────────────────────────┐  │  │  │
│  │  │  │  Entities  (editor_core)   │  │  │  │
│  │  │  │  Position  Buffer          │  │  │  │
│  │  │  │  Cursor    Document        │  │  │  │
│  │  │  └────────────────────────────┘  │  │  │
│  │  └─────────────────────────────────┘  │  │
│  └───────────────────────────────────────┘  │
└─────────────────────────────────────────────┘
```

## Layers

### Entities (`src/core/entities/`)

Pure C++23. Zero external dependencies. These types define what the editor *is*.

| Type | Responsibility |
|---|---|
| `Position` | Zero-based (line, col) coordinate. Value type. |
| `Buffer` | Owns file text as a vector of lines. All mutations return `std::expected`. |
| `Cursor` | Owns caret position. Enforces buffer boundary invariants on every motion. |
| `Document` | Composes `Buffer` + `Cursor`. The single unit of state the editor operates on. |

### Use Cases (`src/core/usecases/`)

Orchestration logic. Depend only on entities and interfaces. No framework code.

| Type | Responsibility |
|---|---|
| `EditorMode` | Normal/Insert mode state machine. Dispatches key input to commands. |
| `EditorCommands` | Command objects for each editor action. Operate on `Document`. |
| `LspService` | LSP lifecycle: initialize, didOpen, didChange, publishDiagnostics. |
| `FileService` | Load file into `Buffer`. Save `Buffer` to file. |

### Interface Adapters (`src/adapters/`)

Translation only. Convert between the core's types and the outside world's types.

| Type | Responsibility |
|---|---|
| `InputTranslator` | FTXUI key event → our internal `Key` type. |
| `LspJsonRpc` | JSON-RPC wire bytes ↔ typed C++ LSP message structs. |

### Frameworks & Drivers (`src/drivers/`)

Talks to external systems. Only this layer imports FTXUI, POSIX, or clangd.

| Type | Responsibility |
|---|---|
| `FtxuiRenderer` | Renders `Document` state to the terminal via FTXUI. |
| `ClangdProcess` | Spawns clangd subprocess, manages stdin/stdout pipes. |

## CMake targets

| Target | Layer | External deps |
|---|---|---|
| `editor_core` | Entities + Use Cases | none |
| `editor_adapters` | Interface Adapters | `nlohmann_json` (LSP) |
| `editor_drivers` | Frameworks & Drivers | `ftxui` |
| `editor` | Binary / composition root | all of the above |
| `unit_tests` | — | `gtest`, `gmock` |
| `e2e_tests` | — | `gtest` |

## Design patterns

| Pattern | Where | Why |
|---|---|---|
| **Command** | `EditorCommands` | Each vim action is an object. Enables sequencing and future undo/redo. |
| **State** | `EditorMode` | Normal/Insert are explicit states with their own dispatch tables. |
| **Strategy** | `LanguageMode` (interface) | Swappable per-language behaviour without touching the core. |
| **Observer** | `LspService` → `FtxuiRenderer` | Renderer subscribes to diagnostic updates without polling. |
| **Factory** | `ClangdProcess::create()` | Returns `std::unique_ptr` — ownership is explicit, construction is encapsulated. |
| **Adapter** | `InputTranslator`, `LspJsonRpc` | Translate external representations to internal types at the boundary. |
| **Composite** | `Document` | Composes `Buffer` + `Cursor` into a single coherent unit. |

## Key invariants

- **No exceptions cross layer boundaries.** Fallible operations return `std::expected<T, E>`.
- **`Buffer` never imports FTXUI.** `FtxuiRenderer` never imports `Buffer` directly — it goes through `Document`.
- **`Cursor` always holds a valid position.** Every motion method clamps silently.
- **`main.cpp` is composition root only.** No business logic lives there.

## LSP async model

```
main thread                      reader thread (std::jthread)
──────────────────               ──────────────────────────────
LspService::send() ─write──────► clangd stdin
                                 clangd stdout ─read──► LspJsonRpc::parse()
                                                              │
                                                    ThreadSafeQueue<LspMessage>
                                                              │
LspService::poll() ◄─────────────────────────────────────────┘
     │
     ▼
Observer callbacks → FtxuiRenderer (diagnostics)
```

`std::jthread` auto-joins on destruction. `std::stop_token` signals cooperative shutdown.
