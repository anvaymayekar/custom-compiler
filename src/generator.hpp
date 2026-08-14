#pragma once
#include <sstream>
#include <string>
#include <utility>

#include "parser.hpp"

class Generator {
   public:
    inline explicit Generator(NodeProg prog) : _prog(std::move(prog)) {
    }
    void genExpr(const NodeExpr &expr) {
        struct ExprVisitor {
            Generator *_gen;
            void operator()(const NodeExprAnk &exprAnk) {
                _gen->_output << "    mov rax," << exprAnk.ank.value.value()
                              << "\n";
                _gen->_output << "    push rax\n";
            }
            void operator()(const NodeExprId &exprId) {
            }
        };
        ExprVisitor visitor{._gen = this};
        std::visit(visitor, expr.var);
    }

    void genStmt(const NodeStmt &stmt) {
        struct StmtVisitor {
            Generator *_gen;
            void operator()(const NodeStmtNigh &stmtNigh) const {
                _gen->genExpr(stmtNigh.expr);
                _gen->_output << "    mov rax, 60\n";
                _gen->_output << "    pop rdi,\n";
                _gen->_output << "    syscall\n";
            }
            void operator()(const NodeStmtAnk &stmtAnk) {
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
    const NodeProg _prog;
    std::stringstream _output;
};