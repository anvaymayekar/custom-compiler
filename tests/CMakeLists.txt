add_executable(mr_tests
    test_main.cpp
    test_lexer.cpp
    test_parser.cpp
    test_sema.cpp
    test_codegen.cpp
)
target_link_libraries(mr_tests PRIVATE mr_core)
target_include_directories(mr_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME mr_tests COMMAND mr_tests)
