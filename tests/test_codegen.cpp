#include <sys/wait.h>

#include <cstdlib>
#include <fstream>

#include "MiniTest.hpp"
#include "TestSupport.hpp"
#include "codegen/CodeGenerator.hpp"

namespace {

bool toolchainAvailable() {
    static const bool available =
        (std::system("which nasm > /dev/null 2>&1") == 0) &&
        (std::system("which ld > /dev/null 2>&1") == 0);
    return available;
}

int compileAndRun(const std::string &src, const std::string &tag) {
    mrtest::Pipeline p(src);
    if (p.diags.hasErrors() || !p.program.has_value()) {
        throw std::runtime_error("test program failed to compile: " + tag);
    }
    mr::CodeGenerator gen(std::move(*p.program));
    const std::string asmText = gen.generate();

    const std::string asmPath = "codegen_test_" + tag + ".asm";
    const std::string objPath = "codegen_test_" + tag + ".o";
    const std::string binPath = "./codegen_test_" + tag;

    {
        std::ofstream f(asmPath);
        f << asmText;
    }
    if (std::system(("nasm -f elf64 " + asmPath + " -o " + objPath).c_str()) !=
        0) {
        throw std::runtime_error("nasm failed for " + tag);
    }
    if (std::system(("ld -o " + binPath + " " + objPath).c_str()) != 0) {
        throw std::runtime_error("ld failed for " + tag);
    }
    const int status = std::system(binPath.c_str());
    return WEXITSTATUS(status);
}

}  // namespace

MT_TEST(assembly_contains_no_forbidden_8bit_multiply) {
    mrtest::Pipeline p("ank a = 90 * 50; shevti(a);");
    MT_CHECK(!p.diags.hasErrors());
    mr::CodeGenerator gen(std::move(*p.program));
    const std::string asmText = gen.generate();
    MT_CHECK(asmText.find("imul rax, rbx") != std::string::npos);
    MT_CHECK(asmText.find("mul bl") == std::string::npos);
    MT_CHECK(asmText.find("mul al") == std::string::npos);
}

MT_TEST(canonical_test_program_exits_461) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun(
        "ank a = 10 + 90 * 5;\n"
        "ank b = 1;\n"
        "shevti(a + b);\n",
        "canonical");
    MT_CHECK_EQ(code, 461 % 256);
}

MT_TEST(large_multiplication_is_correct) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun("shevti(90 * 50);\n", "bigmul");
    MT_CHECK_EQ(code, 4500 % 256);
}

MT_TEST(parentheses_change_the_result_vs_default_precedence) {
    if (!toolchainAvailable()) { return; }
    const int withoutParens =
        compileAndRun("shevti(10 + 90 * 5);\n", "noparen");
    const int withParens =
        compileAndRun("shevti((10 + 90) * 5);\n", "withparen");
    MT_CHECK_EQ(withoutParens, 460 % 256);
    MT_CHECK_EQ(withParens, 500 % 256);
    MT_CHECK(withoutParens != withParens);
}

MT_TEST(variables_and_arithmetic_across_statements) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun(
        "ank a = 10;\n"
        "ank b = 20;\n"
        "shevti(a + b);\n",
        "vars");
    MT_CHECK_EQ(code, 30);
}

MT_TEST(if_true_branch_executes) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun(
        "ank a = 1;\n"
        "jar (a) { shevti(42); }\n"
        "shevti(0);\n",
        "if_true");
    MT_CHECK_EQ(code, 42);
}

MT_TEST(if_false_falls_through_to_anyatha) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun(
        "ank a = 0;\n"
        "jar (a) { shevti(1); } anyatha { shevti(2); }\n",
        "if_else");
    MT_CHECK_EQ(code, 2);
}

MT_TEST(nahitar_branch_selected_when_first_condition_false) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun(
        "ank a = 0;\n"
        "ank b = 1;\n"
        "jar (a) { shevti(1); } nahitar (b) { shevti(7); } anyatha { "
        "shevti(9); }\n",
        "nahitar");
    MT_CHECK_EQ(code, 7);
}

MT_TEST(reassignment_updates_the_variable) {
    if (!toolchainAvailable()) { return; }
    const int code = compileAndRun(
        "ank a = 1;\n"
        "a = 99;\n"
        "shevti(a);\n",
        "reassign");
    MT_CHECK_EQ(code, 99);
}