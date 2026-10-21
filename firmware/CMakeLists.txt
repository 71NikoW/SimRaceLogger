# Host build of the hardware-independent core + unit tests.
#   cmake -S firmware -B build && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.16)
project(rigdash_core_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(rigdash_core
  lib/core/src/protocol.cpp
  lib/core/src/format.cpp
  lib/core/src/screen_manager.cpp)
target_include_directories(rigdash_core PUBLIC lib/core/include)
target_compile_options(rigdash_core PRIVATE -Wall -Wextra -Werror)

add_executable(core_tests
  test/test_main.cpp
  test/test_protocol.cpp
  test/test_format.cpp
  test/test_screen_manager.cpp
  test/test_vectors.cpp)
target_link_libraries(core_tests rigdash_core)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(core_tests PRIVATE
  VECTORS_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../protocol/vectors.txt")

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
