#include "lexer/Lexer.hpp"

namespace mr {

Lexer::Lexer(std::string source, std::string filename, DiagnosticEngine &diags)
    : _source(std::move(source)),
      _filename(std::move(filename)),
      _diags(diags) {
}

bool Lexer::isAtEnd() const {
    return _idx >= _source.size();
}

char Lexer::peek(std::size_t ahead) const {
    const std::size_t i = _idx + ahead;
    return i < _source.size() ? _source[i] : '\0';
}

char Lexer::advance() {
    const char c = _source[_idx++];
    if (c == '\n') {
        _line++;
        _col = 1;
    } else {
        _col++;
    }
    return c;
}

SourceLocation Lexer::here() const {
    return SourceLocation{_filename, _line, _col, _idx};
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        const char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') { advance(); }
        } else if (c == '/' && peek(1) == '*') {
            advance();
            advance();
            while (!isAtEnd() && !(peek() == '*' && peek(1) == '/')) {
                advance();
            }
            if (!isAtEnd()) {
                advance();
                advance();
            } else {
                _diags.error(DiagCategory::Lexical, here(),
                             "unterminated block comment");
            }
        } else {
            break;
        }
    }
}


std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            tokens.push_back(Token{TokenType::EndOfFile, here(), std::nullopt});
            break;
        }

        const char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
        } else if (c == '"') {
            tokens.push_back(lexString());
        } else if (c == '\'') {
            tokens.push_back(lexChar());
        } else {
            Token tok = lexPunctuationOrOperator();
            if (tok.type != TokenType::Invalid) { tokens.push_back(tok); }
        }
    }
    return tokens;
}


}  // namespace mr
