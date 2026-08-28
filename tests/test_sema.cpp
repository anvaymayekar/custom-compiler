#include "MiniTest.hpp"
#include "TestSupport.hpp"

MT_TEST(undeclared_identifier_is_reported) {
    mrtest::Pipeline p("shevti(x);");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(declared_identifier_is_accepted) {
    mrtest::Pipeline p("ank x = 5; shevti(x);");
    MT_CHECK(!p.diags.hasErrors());
}

MT_TEST(duplicate_declaration_in_same_scope_is_an_error) {
    mrtest::Pipeline p("ank x = 5; ank x = 6;");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(shadowing_in_a_nested_scope_is_allowed) {
    mrtest::Pipeline p("ank x = 5; { ank x = 6; shevti(x); }");
    MT_CHECK(!p.diags.hasErrors());
}

MT_TEST(variable_out_of_scope_after_block_ends_is_undeclared) {
    mrtest::Pipeline p("{ ank x = 5; } shevti(x);");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(assignment_to_undeclared_variable_is_an_error) {
    mrtest::Pipeline p("x = 5;");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(self_referential_initializer_is_an_error) {
    mrtest::Pipeline p("ank x = x;");
    MT_CHECK(p.diags.hasErrors());
}

MT_TEST(condition_and_branch_variables_are_checked_in_if_chain) {
    mrtest::Pipeline p(
        "ank a = 1;\n"
        "jar (a) { shevti(a); } nahitar (b) { shevti(a); } anyatha { shevti(a); }\n");
    // 'b' in the nahitar condition is never declared.
    MT_CHECK(p.diags.hasErrors());
}
