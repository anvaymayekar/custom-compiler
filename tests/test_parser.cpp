#include "MiniTest.hpp"
#include "TestSupport.hpp"

using namespace mr;

MT_TEST(parses_the_canonical_test_program) {
    mrtest::Pipeline p(
        "ank a = 10 + 90 * 5;\n"
        "ank b = 1;\n"
        "shevti(a + b);\n");
    MT_CHECK(!p.diags.hasErrors());
    MT_CHECK(p.program.has_value());
    MT_CHECK_EQ(p.program->stmts.size(), static_cast<size_t>(3));
}

MT_TEST(multiplication_binds_tighter_than_addition) {
    mrtest::Pipeline p("ank a = 10 + 90 * 5;");
    MT_CHECK(!p.diags.hasErrors());
    auto *decl = std::get<NodeStmtVarDecl *>(p.program->stmts[0]->var);
    auto *add = std::get<NodeBinExpr *>(decl->expr->var);
    MT_CHECK(add->op == BinaryOp::Add);
    // rhs of the top-level '+' must itself be the '*' subexpression.
    auto *rhsBin = std::get<NodeBinExpr *>(add->rhs->var);
    MT_CHECK(rhsBin->op == BinaryOp::Mul);
}

MT_TEST(parentheses_override_precedence) {
    mrtest::Pipeline p("ank a = (10 + 90) * 5;");
    MT_CHECK(!p.diags.hasErrors());
    auto *decl = std::get<NodeStmtVarDecl *>(p.program->stmts[0]->var);
    auto *mul = std::get<NodeBinExpr *>(decl->expr->var);
    MT_CHECK(mul->op == BinaryOp::Mul);
}

MT_TEST(if_else_if_else_chain_parses) {
    mrtest::Pipeline p(
        "ank a = 1;\n"
        "jar (a) { shevti(1); } nahitar (a) { shevti(2); } anyatha { shevti(3); }\n");
    MT_CHECK(!p.diags.hasErrors());
    MT_CHECK_EQ(p.program->stmts.size(), static_cast<size_t>(2));
}

MT_TEST(missing_semicolon_is_a_syntax_error_not_a_crash) {
    mrtest::Pipeline p("ank a = 10\n");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(multiple_independent_errors_are_all_reported) {
    // Two unrelated broken statements; a naive parser that bails on the
    // first error would only ever report one of these.
    mrtest::Pipeline p(
        "ank a = ;\n"
        "ank b = 1\n"
        "ank c = 3;\n");
    MT_CHECK(p.diags.errorCount() >= 2);
}

MT_TEST(unterminated_paren_does_not_throw_bad_optional_access) {
    // Historically this shape of input crashed the compiler with
    // std::bad_optional_access; it must now fail gracefully.
    mrtest::Pipeline p("ank a = (1 + 2;\n");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(empty_program_parses_to_zero_statements) {
    mrtest::Pipeline p("");
    MT_CHECK(!p.diags.hasErrors());
    MT_CHECK(p.program.has_value());
    MT_CHECK_EQ(p.program->stmts.size(), static_cast<size_t>(0));
}
