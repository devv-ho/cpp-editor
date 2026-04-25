cmake_minimum_required(VERSION 3.20)
project(Editor LANGUAGES CXX)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ───────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG        main
)
FetchContent_MakeAvailable(ftxui)

# ── Layer targets ──────────────────────────────────────────────────────────────
# Dependencies point inward only: drivers -> adapters -> core.

add_library(editor_core INTERFACE)
target_include_directories(editor_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(editor_core INTERFACE cxx_std_23)

add_library(editor_adapters INTERFACE)
target_link_libraries(editor_adapters INTERFACE editor_core)

add_library(editor_drivers INTERFACE)
target_link_libraries(editor_drivers INTERFACE
  editor_adapters
  ftxui::component
  ftxui::dom
  ftxui::screen
)

# ── Main executable ────────────────────────────────────────────────────────────
add_executable(editor src/main.cpp)
target_link_libraries(editor PRIVATE editor_drivers)
