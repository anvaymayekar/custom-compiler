#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <format>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "parser.hpp"

class Generator {
   public:
    inline explicit Generator(NodeProg prog) : _prog(std::move(prog)) {
    }

    void genTerm(const NodeTerm *term) {
        struct TermVisitor {
            Generator &_gen;
            void operator()(const NodeTermAnk *nodeTermAnk) const {
                _gen._output << "    mov rax," << nodeTermAnk->ank.value.value()
                             << "\n";
                _gen.push("rax");
            }
            void operator()(const NodeTermId *nodeTermId) const {
                auto it = std::find_if(_gen._vars.cbegin(), _gen._vars.cend(),
                                       [&](const Var &var) {
                                           return var.name ==
                                                  nodeTermId->id.value.value();
                                       });
                if (it == _gen._vars.cend()) {
                    std::cerr << "Undeclared identifier: "
                              << nodeTermId->id.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }
                std::stringstream offset;
                offset << "QWORD [rsp + "
                       << (_gen._stackSize - (*it).stackLoc - 1) * 8 << "]";
                _gen.push(offset.str());
            }
            void operator()(const NodeTermParen *nodeTermParen) const {
                _gen.genExpr(nodeTermParen->expr);
            }
        };
        TermVisitor visitor({._gen = *this});
        std::visit(visitor, term->var);
    }
    void genBinExpr(const NodeBinExpr *binExpr) {
        struct BinExprVisitor {
            Generator &_gen;
            void operator()(const NodeBinExprAdd *add) const {
                _gen.genExpr(add->rhs);
                _gen.genExpr(add->lhs);
                _gen.pop("rax");
                _gen.pop("rbx");
                _gen._output << "    add rax, rbx\n";
                _gen.push("rax");
            }
            void operator()(const NodeBinExprSub *sub) const {
                _gen.genExpr(sub->rhs);
                _gen.genExpr(sub->lhs);
                _gen.pop("rax");
                _gen.pop("rbx");
                _gen._output << "    sub rax, rbx\n";
                _gen.push("rax");
            }

            void operator()(const NodeBinExprMul *mul) const {
                _gen.genExpr(mul->rhs);
                _gen.genExpr(mul->lhs);
                _gen.pop("rax");
                _gen.pop("rbx");
                _gen._output << "    mul rbx\n";
                _gen.push("rax");
            }
            void operator()(const NodeBinExprDiv *div) const {
                _gen.genExpr(div->rhs);
                _gen.genExpr(div->lhs);
                _gen.pop("rax");
                _gen.pop("rbx");
                _gen._output << "    xor rdx, rdx\n";
                _gen._output << "    div rbx\n";

                _gen.push("rax");
            }
        };
        BinExprVisitor visitor{._gen = *this};
        std::visit(visitor, binExpr->var);
    }
    void genExpr(const NodeExpr *expr) {
        struct ExprVisitor {
            Generator &_gen;
            void operator()(const NodeTerm *term) {
                _gen.genTerm(term);
            }
            void operator()(const NodeBinExpr *binExpr) const {
                _gen.genBinExpr(binExpr);
            }
        };
        ExprVisitor visitor{._gen = *this};
        std::visit(visitor, expr->var);
    }

    void genScope(const NodeStmtScope *scope) {
        beginScope();
        for (const NodeStmt *stmt : scope->stmts) { genStmt(*stmt); }
        endScope();
    }

    void genJarPred(const NodeJarPred *pred, const std::string &endLabel) {
        struct PredVisitor {
            Generator &_gen;
            const std::string &endLabel;
            void operator()(const NodeNahitar *nahitar) const {
                _gen.genExpr(nahitar->expr);
                _gen.pop("rax");
                const std::string label = _gen.createLabel();
                _gen._output << "     test rax, rax\n";
                _gen._output << "     jz " << label << "\n";
                _gen.genScope(nahitar->scope);
                _gen._output << "     jmp " << endLabel << "\n";
                if (nahitar->pred.has_value()) {
                    _gen._output << label << ":\n";
                    _gen.genJarPred(nahitar->pred.value(), endLabel);
                }
            }
            void operator()(const NodeAnyatha *anyatha) const {
                _gen.genScope(anyatha->scope);
            }
        };
        PredVisitor visitor{._gen = *this, .endLabel = endLabel};
        std::visit(visitor, pred->var);
    }
    void genStmt(const NodeStmt &stmt) {
        struct StmtVisitor {
            Generator &_gen;
            void operator()(const NodeStmtShevti *stmtShevti) const {
                _gen.genExpr(stmtShevti->expr);
                _gen._output << "    mov rax, 60\n";
                _gen.pop("rdi");
                _gen._output << "    syscall\n";
            }
            void operator()(const NodeStmtAnk *stmtAnk) {
                auto temp = stmtAnk->id.value.value();
                auto it = std::find_if(_gen._vars.cbegin(), _gen._vars.cend(),
                                       [&](const Var &var) {
                                           return var.name ==
                                                  stmtAnk->id.value.value();
                                       });

                if (it != _gen._vars.cend()) {
                    std::cerr << "Identifier already declared: "
                              << stmtAnk->id.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }
                _gen._vars.push_back({.name = stmtAnk->id.value.value(),
                                      .stackLoc = _gen._stackSize});
                _gen.genExpr(stmtAnk->expr);
            }
            void operator()(const NodeStmtScope *scope) const {
                _gen.genScope(scope);
            }
            void operator()(const NodeStmtJar *stmtJar) const {
                _gen.genExpr(stmtJar->expr);
                _gen.pop("rax");
                const std::string label = _gen.createLabel();
                _gen._output << "     test rax, rax\n";
                _gen._output << "     jz " << label << "\n";
                _gen.genScope(stmtJar->scope);
                _gen._output << label << ":\n";

                if (stmtJar->pred.has_value()) {
                    const std::string endLabel = _gen.createLabel();
                    _gen.genJarPred(stmtJar->pred.value(), endLabel);
                    _gen._output << endLabel << ": \n";
                }
            }
        };
        StmtVisitor visitor{._gen = *this};
        std::visit(visitor, stmt.var);
    }
    [[nodiscard]] std::string generate() {
        _output << "global _start\n";
        _output << "_start:\n";
        for (const NodeStmt *stmt : _prog.stmts) { genStmt(*stmt); }
        _output << "    mov rax, 60\n";
        _output << "    mov rdi, 0\n";
        _output << "    syscall\n";
        return _output.str();
    }

   private:
    void push(const std::string &reg) {
        _output << "    push " << reg << "\n";
        _stackSize++;
    }
    void pop(const std::string &reg) {
        _output << "    pop " << reg << "\n";
        _stackSize--;
    }
    void beginScope() {
        _scopes.push_back(_vars.size());
    }
    void endScope() {
        size_t popCount = _vars.size() - _scopes.back();
        _output << "    add rsp, " << popCount * 8 << "\n";
        _stackSize -= popCount;
        for (int i = 0; i < popCount; i++) { _vars.pop_back(); }
        _scopes.pop_back();
    }

    std::string createLabel() {
        std::stringstream ss;
        ss << "label" << _labelCount++;
        return ss.str();
    }
    struct Var {
        std::string name;
        size_t stackLoc;
    };

    const NodeProg _prog;
    std::stringstream _output;
    size_t _stackSize = 0;
    std::vector<Var> _vars{};
    std::vector<size_t> _scopes{};
    int _labelCount = 0;
};