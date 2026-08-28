cmake_minimum_required(VERSION 3.20)
project(mr_compiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_compile_options(-Wall -Wextra -Wpedantic)

# --- Core compiler library (everything except main.cpp) ---
# Kept as a library so the test suite can link against the same lexer /
# parser / sema / codegen code the CLI uses, instead of re-declaring it or
# only being able to test the binary as a black box.
add_library(mr_core STATIC
    src/diagnostics/DiagnosticEngine.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/sema/SemanticAnalyzer.cpp
    src/codegen/CodeGenerator.cpp
)
target_include_directories(mr_core PUBLIC include)

# --- CLI driver ---
add_executable(compiler src/main.cpp)
target_link_libraries(compiler PRIVATE mr_core)

# --- Tests ---
enable_testing()
add_subdirectory(tests)
