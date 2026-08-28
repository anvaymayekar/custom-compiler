#pragma once

#include <string>
#include <vector>

#include "diagnostics/DiagnosticEngine.hpp"
#include "lexer/Token.hpp"

namespace mr {

// Turns raw source text into a flat token stream. The lexer never aborts
// the program on bad input: invalid characters are reported through the
// DiagnosticEngine and skipped so the rest of the file can still be
// tokenized.
class Lexer final {
   public:
    Lexer(std::string source, std::string filename, DiagnosticEngine &diags);

    [[nodiscard]] std::vector<Token> tokenize();

   private:
    [[nodiscard]] bool isAtEnd() const;
    [[nodiscard]] char peek(std::size_t ahead = 0) const;
    char advance();
    [[nodiscard]] SourceLocation here() const;

    void skipWhitespaceAndComments();
    Token lexIdentifierOrKeyword();
    Token lexNumber();
    Token lexString();
    Token lexChar();
    Token lexPunctuationOrOperator();

    std::string _source;
    std::string _filename;
    DiagnosticEngine &_diags;

    std::size_t _idx = 0;
    int _line = 1;
    int _col = 1;
};

}  // namespace mr