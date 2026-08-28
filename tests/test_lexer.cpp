#include "MiniTest.hpp"
#include "TestSupport.hpp"

using mr::TokenType;

MT_TEST(lexes_numbers_identifiers_and_keywords) {
    mr::DiagnosticEngine diags;
    auto tokens = mrtest::lex("ank a = 10 + 90 * 5;", diags);
    MT_CHECK(!diags.hasErrors());

    std::vector<TokenType> types;
    for (const auto &t : tokens) { types.push_back(t.type); }

    const std::vector<TokenType> expected = {
        TokenType::KwAnk,      TokenType::Identifier, TokenType::Equal,
        TokenType::IntLiteral, TokenType::Plus,       TokenType::IntLiteral,
        TokenType::Star,       TokenType::IntLiteral, TokenType::Semicolon,
        TokenType::EndOfFile,
    };
    MT_CHECK_EQ(types.size(), expected.size());
    for (std::size_t i = 0; i < types.size() && i < expected.size(); ++i) {
        MT_CHECK(types[i] == expected[i]);
    }
}

MT_TEST(reserved_keywords_tokenize_distinctly_from_identifiers) {
    mr::DiagnosticEngine diags;
    auto tokens = mrtest::lex("karya leeh varg karyaX", diags);
    MT_CHECK(!diags.hasErrors());
    MT_CHECK(tokens[0].type == TokenType::KwKarya);
    MT_CHECK(tokens[1].type == TokenType::KwLeeh);
    MT_CHECK(tokens[2].type == TokenType::KwVarg);
    MT_CHECK(tokens[3].type == TokenType::Identifier);
    MT_CHECK_EQ(tokens[3].lexeme.value_or(""), std::string("karyaX"));
}

MT_TEST(comments_are_skipped) {
    mr::DiagnosticEngine diags;
    auto tokens = mrtest::lex("// a comment\nank a = 1; /* block */ ", diags);
    MT_CHECK(!diags.hasErrors());
    MT_CHECK(tokens.front().type == TokenType::KwAnk);
}

MT_TEST(invalid_character_reports_lexical_error_and_recovers) {
    mr::DiagnosticEngine diags;
    auto tokens = mrtest::lex("ank a = 1 @ 2;", diags);
    MT_CHECK(diags.hasErrors());
    bool sawSemicolon = false;
    for (const auto &t : tokens) {
        sawSemicolon |= (t.type == TokenType::Semicolon);
    }
    MT_CHECK(sawSemicolon);
}

MT_TEST(source_locations_track_line_and_column) {
    mr::DiagnosticEngine diags;
    auto tokens = mrtest::lex("ank a = 1;\nank b = 2;", diags);
    bool found = false;
    for (const auto &t : tokens) {
        if (t.type == TokenType::Identifier && t.lexeme == "b") {
            MT_CHECK_EQ(t.loc.line, 2);
            found = true;
        }
    }
    MT_CHECK(found);
}