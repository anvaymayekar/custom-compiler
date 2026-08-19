#pragma once

#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "arena.hpp"
#include "tokenizer.hpp"

// Forward declarations
struct NodeExpr;
struct NodeBinExpr;

// Binary expressions
struct BinExprAdd {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct BinExprMul {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

// Binary expression node
struct NodeBinExpr {
    std::variant<BinExprAdd *, BinExprMul *> var;
};

// Basic expression nodes
struct NodeExprAnk {
    Token ank;
};

struct NodeExprId {
    Token id;
};

// Expression node
struct NodeExpr {
    std::variant<NodeExprAnk *, NodeExprId *, NodeBinExpr *> var;
};

// Statement nodes
struct NodeStmtShevat {
    NodeExpr *expr;
};

struct NodeStmtAnk {
    Token id;
    NodeExpr *expr;
};

// Statement
struct NodeStmt {
    std::variant<NodeStmtShevat *, NodeStmtAnk *> var;
};

// Program
struct NodeProg {
    std::vector<NodeStmt> stmts;
};

// Exit node
struct NodeExit {
    NodeExpr *expr;
};
class Parser {
   public:
    inline explicit Parser(std::vector<Token> tokens)
        : _tokens(std::move(tokens)), _allocator(1024 * 1024 * 4) {
    }
    std::optional<NodeExpr *> parseExpr() {
        if (peek().has_value() && peek().value().type == TokenType::ank) {
            auto nodeExprAnk = _allocator.alloc<NodeExprAnk>();
            nodeExprAnk->ank = consume();
            auto expr = _allocator.alloc<NodeExpr>();
            expr->var = nodeExprAnk;
            return expr;
        } else if (peek().has_value() && peek().value().type == TokenType::id) {
            auto nodeExprId = _allocator.alloc<NodeExprId>();
            nodeExprId->id = consume();
            auto expr = _allocator.alloc<NodeExpr>();
            expr->var = nodeExprId;
            return expr;

        } else {
            return std::nullopt;
        }
    }

    std::optional<NodeStmt> parseStmt() {
        if (peek().value().type == TokenType::shevat && peek(1).has_value() &&
            peek(1).value().type == TokenType::openParen) {
            consume(2);
            auto stmtShevat = _allocator.alloc<NodeStmtShevat>();
            if (auto nodeExpr = parseExpr()) {
                stmtShevat->expr = nodeExpr.value();
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
            if (peek().has_value() && peek().value().type == TokenType::semi) {
                consume();
            } else {
                std::cerr << "Expected ';'" << std::endl;
                exit(EXIT_FAILURE);
            }
            return NodeStmt{.var = stmtShevat};
        } else if (peek().has_value() &&
                   peek().value().type == TokenType::_ank &&
                   peek(1).has_value() &&
                   peek(1).value().type == TokenType::id &&
                   peek(2).has_value() &&
                   peek(2).value().type == TokenType::eq) {
            consume();
            auto stmtAnk = _allocator.alloc<NodeStmtAnk>();
            stmtAnk->id = consume();
            consume();
            if (auto expr = parseExpr()) {
                stmtAnk->expr = expr.value();
            } else {
                std::cerr << "Invalid expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            if (peek().has_value() && peek().value().type == TokenType::semi) {
                consume();
            } else {
                std::cerr << "Expected ';'" << std::endl;
                exit(EXIT_FAILURE);
            }
            return NodeStmt{.var = stmtAnk};
        } else {
            return std::nullopt;
        }
    }
    std::optional<NodeExit> parseExit() {
        std::optional<NodeExit> exitNode;
        while (peek().has_value()) {
            if (peek().value().type == TokenType::shevat &&
                peek(1).has_value() &&
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

    std::optional<NodeProg> parseProg() {
        NodeProg prog;
        while (peek().has_value()) {
            if (auto stmt = parseStmt()) {
                prog.stmts.push_back(stmt.value());
            } else {
                std::cerr << "Invalid Statement" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        return prog;
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
    ArenaAllocator _allocator;
};