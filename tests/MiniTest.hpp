#pragma once
// A deliberately tiny test harness. The project has no internet access
// guarantee at build time, so rather than vendor or fetch a third-party
// framework (Catch2/GTest), tests are plain functions registered here and
// run from test_main.cpp. This is enough for a compiler's needs: each
// test is a function that makes assertions and the runner reports
// pass/fail counts with a non-zero exit code on any failure (so `ctest`
// and CI both work normally).

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace minitest {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase> &registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string &name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int failures = 0;
inline std::string currentTest;

inline void fail(const std::string &expr, const std::string &file, int line) {
    std::cerr << "  FAILED: " << expr << " (" << file << ":" << line << ")\n";
    failures++;
}

}  // namespace minitest

#define MT_TEST(name)                                                             \
    static void mt_test_##name();                                                 \
    static ::minitest::Registrar mt_reg_##name(#name, &mt_test_##name);           \
    static void mt_test_##name()

#define MT_CHECK(cond)                                                            \
    do {                                                                          \
        if (!(cond)) { ::minitest::fail(#cond, __FILE__, __LINE__); }             \
    } while (0)

#define MT_CHECK_EQ(a, b)                                                         \
    do {                                                                          \
        if (!((a) == (b))) { ::minitest::fail(#a " == " #b, __FILE__, __LINE__); } \
    } while (0)
