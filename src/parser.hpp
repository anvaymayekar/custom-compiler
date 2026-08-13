#pragma once
#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "tokenizer.hpp"

struct NodeExpr {
    Token ank;
};

struct NodeExit {
    NodeExpr expr;
};
class Parser {
   public:
    inline explicit Parser(std::vector<Token> tokens)
        : _tokens(std::move(tokens)) {
    }
    std::optional<NodeExpr> parseExpr() {
        if (peek().has_value() && peek().value().type == TokenType::ank) {
            return NodeExpr{.ank = consume()};
        } else {
            return std::nullopt;
        }
    }
    std::optional<NodeExit> parseExit() {
        std::optional<NodeExit> exitNode;
        while (peek().has_value()) {
            if (peek().value().type == TokenType::nigh && peek(1).has_value() &&
                peek(1).value().type == TokenType::openParen) {
                consume(2);
                if (auto nodeExpr = parseExpr()) {
                    exitNode = NodeExit{.expr = nodeExpr.value()};
                } else {
                    std::cerr << "Invalid expression" << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (peek().has_value() &&
                    peek().value().type == TokenType::closeParen) {
                    consume();
                } else {
                    std::cerr << "Expected ')'" << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (peek().has_value() &&
                    peek().value().type == TokenType::semi) {
                    consume();
                } else {
                    std::cerr << "Expected ';'" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
        _idx = 0;
        return exitNode;
    }

   private:
    [[nodiscard]] std::optional<Token> peek(size_t ahead = 0) const {
        if (_idx + ahead >= _tokens.size()) { return std::nullopt; }
        return _tokens.at(_idx + ahead);
    }

    inline Token consume(size_t count = 1) {
        Token token = _tokens.at(_idx);
        _idx += count;
        return token;
    }

    const std::vector<Token> _tokens;
    size_t _idx = 0;
};