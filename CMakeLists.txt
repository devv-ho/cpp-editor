cmake_minimum_required(VERSION 3.20)
project(Editor LANGUAGES CXX)

# Generate compile_commands.json for IDE IntelliSense (clangd, VS Code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Language standard ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ───────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG        main
)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest ftxui nlohmann_json)

# ── Layer targets ──────────────────────────────────────────────────────────────
# Dependencies point inward only: drivers -> adapters -> core.

add_library(editor_core INTERFACE)
target_include_directories(editor_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(editor_core INTERFACE cxx_std_23)

add_library(editor_adapters STATIC
  src/adapters/lsp/ClangdProcess.cpp
  src/core/usecases/LspService.cpp
  src/drivers/FtxuiRenderer.cpp
  src/drivers/EditorApp.cpp
)
target_include_directories(editor_adapters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(editor_adapters PUBLIC cxx_std_23)
target_link_libraries(editor_adapters PUBLIC editor_core ftxui::component nlohmann_json::nlohmann_json)

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

# ── Unit tests ─────────────────────────────────────────────────────────────────
enable_testing()
include(GoogleTest)

file(GLOB_RECURSE UNIT_TEST_SOURCES tests/unit/*.cpp)
if(UNIT_TEST_SOURCES)
  add_executable(unit_tests ${UNIT_TEST_SOURCES})
  target_link_libraries(unit_tests PRIVATE
    editor_adapters
    ftxui::component
    editor_core
    GTest::gtest_main
    GTest::gmock_main
  )
  gtest_discover_tests(unit_tests)
endif()

# ── E2e tests ──────────────────────────────────────────────────────────────────
file(GLOB_RECURSE E2E_TEST_SOURCES tests/e2e/*.cpp)
if(E2E_TEST_SOURCES)
  add_executable(e2e_tests ${E2E_TEST_SOURCES})
  target_include_directories(e2e_tests PRIVATE tests/e2e ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(e2e_tests PRIVATE GTest::gtest_main)
  target_compile_definitions(e2e_tests PRIVATE
    EDITOR_BINARY="$<TARGET_FILE:editor>"
  )
  add_dependencies(e2e_tests editor)
  gtest_discover_tests(e2e_tests)
endif()
