#pragma once
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class TokenType {
    shevti,
    ank,
    semi,
    openParen,
    closeParen,
    id,
    _ank,
    eq,
    plus,
    mul,
    minus,
    slash,
    openCurly,
    closeCurly,
    jar,
    nahitar,
    anyatha
};

inline std::optional<int> binPrec(const TokenType type) {
    switch (type) {
        case TokenType::plus:
        case TokenType::minus:
            return 0;
        case TokenType::mul:
        case TokenType::slash:
            return 1;
        default:
            return std::nullopt;
    }
};
struct Token {
    TokenType type;
    int line;
    int col;
    std::optional<std::string> value;
};

class Tokenizer {
   public:
    inline explicit Tokenizer(std::string src) : _src(std::move(src)) {
    }
    inline std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        std::string buffer;
        while (peek().has_value()) {
            if (std::isalpha(peek().value())) {
                buffer.push_back(consume());
                while (peek().has_value() && std::isalpha(peek().value())) {
                    buffer.push_back(consume());
                }
                if (buffer == "shevti") {
                    tokens.push_back({.type = TokenType::shevti,
                                      .line = lineCount,
                                      .col = colCount});
                    buffer.clear();
                } else if (buffer == "ank") {
                    tokens.push_back({.type = TokenType::_ank,
                                      .line = lineCount,
                                      .col = colCount});
                    buffer.clear();
                } else if (buffer == "jar") {
                    tokens.push_back({.type = TokenType::jar,
                                      .line = lineCount,
                                      .col = colCount});
                    buffer.clear();
                } else if (buffer == "nahitar") {
                    tokens.push_back({.type = TokenType::nahitar,
                                      .line = lineCount,
                                      .col = colCount});
                    buffer.clear();
                } else if (buffer == "anyatha") {
                    tokens.push_back({.type = TokenType::anyatha,
                                      .line = lineCount,
                                      .col = colCount});
                    buffer.clear();
                } else {
                    tokens.push_back({.type = TokenType::id,
                                      .line = lineCount,
                                      .col = colCount,
                                      .value = buffer});
                    buffer.clear();
                }
            } else if (std::isdigit(peek().value())) {
                buffer.push_back(consume());
                while (peek().has_value() && std::isdigit(peek().value())) {
                    buffer.push_back(consume());
                }
                tokens.push_back({.type = TokenType::ank,
                                  .line = lineCount,
                                  .col = colCount,
                                  .value = buffer});
                buffer.clear();
            } else if (peek().value() == '/' && peek(1).has_value() &&
                       peek(1).value() == '/') {
                consume();
                consume();
                while (peek().has_value() && peek().value() != '\n') {
                    consume();
                }
            } else if (peek().value() == '/' && peek(1).has_value() &&
                       peek(1).value() == '*') {
                consume();
                consume();
                while (peek().has_value()) {
                    if (peek().value() == '*' && peek(1).has_value() &&
                        peek(1).value() == '/') {
                        break;
                    }
                    consume();
                }
                if (peek().has_value()) consume();
                if (peek().has_value()) consume();
            } else if (peek().value() == '(') {
                consume();
                tokens.push_back({.type = TokenType::openParen,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == ')') {
                consume();
                tokens.push_back({.type = TokenType::closeParen,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == ';') {
                tokens.push_back({.type = TokenType::semi,
                                  .line = lineCount,
                                  .col = colCount});
                consume();
            } else if (peek().value() == '=') {
                consume();
                tokens.push_back({.type = TokenType::eq,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == '+') {
                consume();
                tokens.push_back({.type = TokenType::plus,
                                  .line = lineCount,
                                  .col = colCount});

            } else if (peek().value() == '*') {
                consume();
                tokens.push_back({.type = TokenType::mul,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == '-') {
                consume();
                tokens.push_back({.type = TokenType::minus,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == '/') {
                consume();
                tokens.push_back({.type = TokenType::slash,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == '{') {
                consume();
                tokens.push_back({.type = TokenType::openCurly,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (peek().value() == '}') {
                consume();
                tokens.push_back({.type = TokenType::closeCurly,
                                  .line = lineCount,
                                  .col = colCount});
            } else if (std::isspace(peek().value())) {
                consume();
            } else {
                std::cerr << "Invalid Token " << peek().value()
                          << " at line: " << lineCount << " & col: " << colCount
                          << "\n";
                exit(EXIT_FAILURE);
            }
        }
        _idx = 0;
        return tokens;
    }

   private:
    [[nodiscard]] std::optional<char> peek(size_t ahead = 0) const {
        if (_idx + ahead >= _src.length()) { return std::nullopt; }
        return _src.at(_idx + ahead);
    }

    inline char consume() {
        if (_src.at(_idx) == '\n') {
            lineCount++;
            colCount = 0;
        } else {
            colCount++;
        }
        return _src.at(_idx++);
    }
    const std::string _src;
    size_t _idx = 0;
    int lineCount = 1;
    int colCount = 0;
};