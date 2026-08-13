#pragma once
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class TokenType { nigh, _int, semi };

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
                if (buffer == "nigh") {
                    tokens.push_back({.type = TokenType::nigh});
                    buffer.clear();
                    continue;
                } else {
                    std::cerr << "Unknown identifier: " << buffer << "\n";
                    exit(EXIT_FAILURE);
                }
            } else if (std::isdigit(peek().value())) {
                buffer.push_back(consume());
                while (peek().has_value() && std::isdigit(peek().value())) {
                    buffer.push_back(consume());
                }
                tokens.push_back({.type = TokenType::_int, .value = buffer});
                buffer.clear();
                continue;
            } else if (peek().value() == ';') {
                tokens.push_back({.type = TokenType::semi});
                consume();
                continue;
            } else if (std::isspace(peek().value())) {
                consume();
                continue;
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