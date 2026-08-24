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

struct NodeBinExprMul {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprSub {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprDiv {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

// Binary expression node
struct NodeBinExpr {
    std::variant<NodeBinExprAdd *, NodeBinExprMul *, NodeBinExprSub *,
                 NodeBinExprDiv *>
        var;
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
    std::optional<NodeExpr *> parseExpr(int minPrec = 0) {
        auto termLHS = parseTerm();

        if (!termLHS.has_value()) { return std::nullopt; }

        auto exprLHS = _allocator.alloc<NodeExpr>();
        exprLHS->var = termLHS.value();

        while (true) {
            auto current = peek();

            if (!current.has_value()) { break; }

            auto prec = binPrec(current->type);

            if (!prec.has_value() || prec.value() < minPrec) { break; }

            Token op = consume();

            auto exprRHS = parseExpr(prec.value() + 1);

            if (!exprRHS.has_value()) {
                std::cerr << "Unable to parse expression\n";
                exit(EXIT_FAILURE);
            }

            auto expr = _allocator.alloc<NodeBinExpr>();
            auto exprLeft = _allocator.alloc<NodeExpr>();

            exprLeft->var = exprLHS->var;

            if (op.type == TokenType::plus) {
                auto add = _allocator.alloc<NodeBinExprAdd>();

                add->lhs = exprLeft;
                add->rhs = exprRHS.value();

                expr->var = add;
            } else if (op.type == TokenType::mul) {
                auto mul = _allocator.alloc<NodeBinExprMul>();

                mul->lhs = exprLeft;
                mul->rhs = exprRHS.value();

                expr->var = mul;
            } else if (op.type == TokenType::sub) {
                auto sub = _allocator.alloc<NodeBinExprSub>();

                sub->lhs = exprLeft;
                sub->rhs = exprRHS.value();

                expr->var = sub;
            } else if (op.type == TokenType::div) {
                auto div = _allocator.alloc<NodeBinExprDiv>();

                div->lhs = exprLeft;
                div->rhs = exprRHS.value();

                expr->var = div;
            } else {
                assert(false);
            }

            exprLHS->var = expr;
        }

        return exprLHS;
    }
    std::optional<NodeStmt> parseStmt() {
        if (peek().has_value() && peek().value().type == TokenType::shevti) {
            if (!peek(1).has_value() ||
                peek(1).value().type != TokenType::openParen) {
                std::cerr << "Expected '('\n";
                exit(EXIT_FAILURE);
            }

            consume(2);

            auto stmtShevti = _allocator.alloc<NodeStmtShevti>();

            if (auto nodeExpr = parseExpr()) {
                stmtShevti->expr = nodeExpr.value();
            } else {
                std::cerr << "Invalid expression\n";
                exit(EXIT_FAILURE);
            }

            tryConsume(TokenType::closeParen, "Expected ')'");
            tryConsume(TokenType::semi, "Expected ';'");

            return NodeStmt{.var = stmtShevti};
        }

        if (peek().has_value() && peek().value().type == TokenType::_ank) {
            if (!peek(1).has_value() || peek(1).value().type != TokenType::id) {
                std::cerr << "Expected identifier after 'ank'\n";
                exit(EXIT_FAILURE);
            }

            if (!peek(2).has_value() || peek(2).value().type != TokenType::eq) {
                std::cerr << "Expected '=' after identifier\n";
                exit(EXIT_FAILURE);
            }

            consume();

            auto stmtAnk = _allocator.alloc<NodeStmtAnk>();

            stmtAnk->id = consume();
            consume();

            if (auto expr = parseExpr()) {
                stmtAnk->expr = expr.value();
            } else {
                std::cerr << "Invalid expression\n";
                exit(EXIT_FAILURE);
            }

            tryConsume(TokenType::semi, "Expected ';'");

            return NodeStmt{.var = stmtAnk};
        }

        return std::nullopt;
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
                std::cerr << "Invalid Statement";

                if (peek().has_value()) {
                    std::cerr << " at token type: "
                              << static_cast<int>(peek().value().type);

                    if (peek().value().value.has_value()) {
                        std::cerr << " value: " << peek().value().value.value();
                    }
                }

                std::cerr << '\n';
                return std::nullopt;
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