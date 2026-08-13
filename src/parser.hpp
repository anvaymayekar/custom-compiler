#pragma once
#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "tokenizer.hpp"

struct NodeExpr {
    Token _int;
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
        if (peek().has_value() && peek().value().type == TokenType::_int) {
            return NodeExpr{._int = consume()};
        } else {
            return std::nullopt;
        }
    }
    std::optional<NodeExit> parse() {
        std::optional<NodeExit> exitNode;
        while (peek().has_value()) {
            if (peek().value().type == TokenType::_exit) {
                consume();
                if (auto nodeExpr = parseExpr()) {
                    exitNode = NodeExit{.expr = nodeExpr.value()};
                } else {
                    std::cerr << "Invalid expression" << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (peek().has_value() &&
                    peek().value().type == TokenType::semi) {
                    consume();
                } else {
                    std::cerr << "Need semicolon" << std::endl;
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

    inline Token consume() {
        return _tokens.at(_idx++);
    }

    const std::vector<Token> _tokens;
    size_t _idx = 0;
};