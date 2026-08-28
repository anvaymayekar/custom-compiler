add_executable(mr_tests
    test_main.cpp
    test_lexer.cpp
    test_parser.cpp
    test_sema.cpp
    test_codegen.cpp
)
target_compile_options(mr_tests PRIVATE
    -Wno-backslash-newline-at-end-of-file
)
target_include_directories(mr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mr_tests PRIVATE mr_core)

add_test(NAME mr_tests COMMAND mr_tests)