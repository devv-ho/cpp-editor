cmake_minimum_required(VERSION 3.20)
project(MyApp)

# ── Language standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
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

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest ftxui)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE
  ftxui::component
  ftxui::dom
  ftxui::screen
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
include(GoogleTest)

add_executable(basic_test basic_test.cpp)
target_link_libraries(basic_test PRIVATE
  GTest::gtest_main
)
gtest_discover_tests(basic_test)
