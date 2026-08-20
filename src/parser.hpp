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
struct NodeBinExprAdd {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

// struct NodeBinExprMul {
//     NodeExpr *lhs;
//     NodeExpr *rhs;
// };

// Binary expression node
struct NodeBinExpr {
    NodeBinExprAdd *add;
};

// Basic expression nodes
struct NodeTermAnk {
    Token ank;
};

struct NodeTermId {
    Token id;
};

struct NodeTerm {
    std::variant<NodeTermAnk *, NodeTermId *> var;
};
// Expression node
struct NodeExpr {
    std::variant<NodeTerm *, NodeBinExpr *> var;
};

// Statement nodes
struct NodeStmtShevti {
    NodeExpr *expr;
};

struct NodeStmtAnk {
    Token id;
    NodeExpr *expr;
};

// Statement
struct NodeStmt {
    std::variant<NodeStmtShevti *, NodeStmtAnk *> var;
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
    std::optional<NodeTerm *> parseTerm() {
        if (auto termAnk = tryConsume(TokenType::ank)) {
            auto nodeTermAnk = _allocator.alloc<NodeTermAnk>();
            nodeTermAnk->ank = termAnk.value();
            auto term = _allocator.alloc<NodeTerm>();
            term->var = nodeTermAnk;
            return term;
        } else if (auto termId = tryConsume(TokenType::id)) {
            auto nodeTermId = _allocator.alloc<NodeTermId>();
            nodeTermId->id = termId.value();
            auto term = _allocator.alloc<NodeTerm>();
            term->var = nodeTermId;
            return term;
        }
        return std::nullopt;
    }
    std::optional<NodeExpr *> parseExpr() {
        if (auto term = parseTerm()) {
            if (tryConsume(TokenType::plus).has_value()) {
                auto binExpr = _allocator.alloc<NodeBinExpr>();
                auto binExprAdd = _allocator.alloc<NodeBinExprAdd>();

                auto lhsExpr = _allocator.alloc<NodeExpr>();
                lhsExpr->var = term.value();

                binExprAdd->lhs = lhsExpr;

                if (auto rhs = parseExpr()) {
                    binExprAdd->rhs = rhs.value();
                    binExpr->add = binExprAdd;

                    auto expr = _allocator.alloc<NodeExpr>();
                    expr->var = binExpr;

                    return expr;
                } else {
                    std::cerr << "Expected an expression after '+'"
                              << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            auto expr = _allocator.alloc<NodeExpr>();
            expr->var = term.value();

            return expr;

        } else {
            return std::nullopt;
        }
    }

    std::optional<NodeStmt> parseStmt() {
        if (peek().value().type == TokenType::shevti && peek(1).has_value() &&
            peek(1).value().type == TokenType::openParen) {
            consume(2);
            auto stmtShevti = _allocator.alloc<NodeStmtShevti>();
            if (auto nodeExpr = parseExpr()) {
                stmtShevti->expr = nodeExpr.value();
            } else {
                std::cerr << "Invalid expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            tryConsume(TokenType::closeParen, "Expected ')'");
            tryConsume(TokenType::semi, "Expected ';'");

            return NodeStmt{.var = stmtShevti};
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
            tryConsume(TokenType::semi, "Expected ';'");
            return NodeStmt{.var = stmtAnk};
        } else {
            return std::nullopt;
        }
    }
    std::optional<NodeExit> parseExit() {
        std::optional<NodeExit> exitNode;
        while (peek().has_value()) {
            if (peek().value().type == TokenType::shevti &&
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
    inline Token tryConsume(TokenType type, const std::string errMsg) {
        if (peek().has_value() && peek().value().type == type) {
            return consume();
        } else {
            std::cerr << errMsg << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    inline std::optional<Token> tryConsume(TokenType type) {
        if (peek().has_value() && peek().value().type == type) {
            return consume();
        } else {
            return std::nullopt;
        }
    }
    const std::vector<Token> _tokens;
    size_t _idx = 0;
    ArenaAllocator _allocator;
};