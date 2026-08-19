#pragma once
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "parser.hpp"

class Generator {
   public:
    inline explicit Generator(NodeProg prog) : _prog(std::move(prog)) {
    }

    void genTerm(const NodeTerm *term) {
        struct TermVisitor {
            Generator *_gen;
            void operator()(const NodeTermAnk *nodeTermAnk) const {
                _gen->_output << "    mov rax,"
                              << nodeTermAnk->ank.value.value() << "\n";
                _gen->push("rax");
            }
            void operator()(const NodeTermId *nodeTermId) const {
                auto temp = nodeTermId->id.value.value();
                if (!_gen->_vars.contains(temp)) {
                    std::cerr << "Undeclared identifier: " << temp << std::endl;
                    exit(EXIT_FAILURE);
                }
                const auto &var = _gen->_vars.at(temp);
                std::stringstream offset;
                offset << "QWORD [rsp + "
                       << (_gen->_stackSize - var.stackLoc - 1) * 8 << "]\n";
                _gen->push(offset.str());
            }
        };
        TermVisitor visitor({._gen = this});
        std::visit(visitor, term->var);
    }
    void genExpr(const NodeExpr *expr) {
        struct ExprVisitor {
            Generator *_gen;
            void operator()(const NodeTerm *term) {
                _gen->genTerm(term);
            }
            void operator()(const NodeBinExpr *binExpr) const {
                assert(false);  // not implemented
            }
        };
        ExprVisitor visitor{._gen = this};
        std::visit(visitor, expr->var);
    }

    void genStmt(const NodeStmt &stmt) {
        struct StmtVisitor {
            Generator *_gen;
            void operator()(const NodeStmtShevti *stmtShevti) const {
                _gen->genExpr(stmtShevti->expr);
                _gen->_output << "    mov rax, 60\n";
                _gen->pop("rdi");
                _gen->_output << "    syscall\n";
            }
            void operator()(const NodeStmtAnk *stmtAnk) {
                auto temp = stmtAnk->id.value.value();
                if (_gen->_vars.contains(temp)) {
                    std::cerr << "Identifier already declared: " << temp
                              << std::endl;
                    exit(EXIT_FAILURE);
                }
                _gen->_vars.insert({stmtAnk->id.value.value(),
                                    Var{.stackLoc = _gen->_stackSize}});
                _gen->genExpr(stmtAnk->expr);
            }
        };
        StmtVisitor visitor{._gen = this};
        std::visit(visitor, stmt.var);
    }
    [[nodiscard]] std::string generate() {
        _output << "global _start\n";
        _output << "_start:\n";
        for (const NodeStmt &stmt : _prog.stmts) { genStmt(stmt); }
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

    struct Var {
        size_t stackLoc;
    };

    const NodeProg _prog;
    std::stringstream _output;
    size_t _stackSize = 0;
    std::unordered_map<std::string, Var> _vars{};
};