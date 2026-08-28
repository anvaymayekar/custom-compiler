#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ast/Ast.hpp"
#include "diagnostics/DiagnosticEngine.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "sema/SemanticAnalyzer.hpp"
#include "support/Arena.hpp"

namespace mrtest {

inline std::vector<mr::Token> lex(const std::string &src, mr::DiagnosticEngine &diags) {
    mr::Lexer lexer(src, "<test>", diags);
    return lexer.tokenize();
}

// Owns everything a parsed-and-analyzed program needs so tests can hold a
// single object instead of juggling lifetimes.
struct Pipeline {
    mr::DiagnosticEngine diags;
    mr::Arena arena;
    std::optional<mr::NodeProgram> program;

    explicit Pipeline(const std::string &src) : diags(src) {
        auto tokens = lex(src, diags);
        mr::Parser parser(std::move(tokens), "<test>", diags, arena);
        program = parser.parseProgram();
        if (program.has_value() && !diags.hasErrors()) {
            mr::SemanticAnalyzer sema(diags);
            sema.analyze(*program);
        }
    }
};

}  // namespace mrtest
