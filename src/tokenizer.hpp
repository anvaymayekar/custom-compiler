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
    sub,
    div
};

std::optional<int> binPrec(TokenType type) {
    switch (type) {
        case TokenType::plus:
        case TokenType::sub:
            return 0;
        case TokenType::mul:
        case TokenType::div:
            return 1;
        default:
            return std::nullopt;
    }
}
struct Token {
    TokenType type;
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
                    tokens.push_back({.type = TokenType::shevti});
                    buffer.clear();
                } else if (buffer == "ank") {
                    tokens.push_back({.type = TokenType::_ank});
                    buffer.clear();
                } else {
                    tokens.push_back({.type = TokenType::id, .value = buffer});
                    buffer.clear();
                }
            } else if (std::isdigit(peek().value())) {
                buffer.push_back(consume());
                while (peek().has_value() && std::isdigit(peek().value())) {
                    buffer.push_back(consume());
                }
                tokens.push_back({.type = TokenType::ank, .value = buffer});
                buffer.clear();
            } else if (peek().value() == '(') {
                consume();
                tokens.push_back({.type = TokenType::openParen});
            } else if (peek().value() == ')') {
                consume();
                tokens.push_back({.type = TokenType::closeParen});
            } else if (peek().value() == ';') {
                tokens.push_back({.type = TokenType::semi});
                consume();
            } else if (peek().value() == '=') {
                consume();
                tokens.push_back({.type = TokenType::eq});
            } else if (peek().value() == '+') {
                consume();
                tokens.push_back({.type = TokenType::plus});

            } else if (peek().value() == '*') {
                consume();
                tokens.push_back({.type = TokenType::mul});
            } else if (peek().value() == '-') {
                consume();
                tokens.push_back({.type = TokenType::sub});
            } else if (peek().value() == '/') {
                consume();
                tokens.push_back({.type = TokenType::div});
            } else if (std::isspace(peek().value())) {
                consume();
            } else {
                std::cerr << "Unexpected character: " << peek().value() << "\n";
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
        return _src.at(_idx++);
    }
    const std::string _src;
    size_t _idx = 0;
};