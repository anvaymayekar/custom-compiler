#pragma once

#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ast/Ast.hpp"

namespace mr {

// Emits Linux x86-64 NASM assembly for a semantically-valid program.
// Evaluation model: every expression pushes its single result onto the
// runtime stack; operators pop operands and push results back. Variables
// live at a fixed offset from the current stack pointer (or, for `sthir`
// statics, in a dedicated .bss slot). See docs/README.md "Codegen notes"
// for the calling convention used for `karya` functions.
class CodeGenerator final {
   public:
    explicit CodeGenerator(NodeProgram program) : _program(std::move(program)) {
    }

    [[nodiscard]] std::string generate();

   private:
    struct Var {
        std::string name;
        bool isStatic = false;
        std::size_t stackLoc = 0;  // valid when !isStatic
        std::string staticLabel;   // valid when isStatic
    };

    void genEntryPoint();
    void genFunctions();
    void genFuncDecl(const NodeStmtFuncDecl &func);
    void genStmt(const NodeStmt &stmt);
    void genScope(const NodeStmtScope &scope);
    void genExpr(const NodeExpr &expr);
    void genTerm(const NodeTerm &term);
    void genBinExpr(const NodeBinExpr &bin);
    void genUnaryExpr(const NodeUnaryExpr &un);
    void genIncDecExpr(const NodeIncDecExpr &incDec);
    void genCall(const NodeCallExpr &call);
    void genIfChain(const NodeElseChain &chain, const std::string &endLabel);
    void genCompoundAssign(const NodeStmtAssign &assign);
    void emitPrintIntRoutine();

    void push(const std::string &reg, const std::string &comment = {});
    void pop(const std::string &reg, const std::string &comment = {});
    void beginScope();
    void endScope();
    [[nodiscard]] std::string newLabel(const std::string &hint);
    [[nodiscard]] std::size_t stackOffsetOf(const std::string &name) const;
    [[nodiscard]] const Var *findVar(const std::string &name) const;
    void declareVar(const std::string &name, bool isStatic);

    NodeProgram _program;
    std::ostringstream _out;         // _start + function bodies
    std::ostringstream _staticData;  // .bss slots for `sthir` variables
    std::size_t _stackSize = 0;
    std::vector<Var> _vars;
    std::vector<std::size_t> _scopeMarks;
    std::vector<std::pair<std::string, std::string>>
        _loopLabels;  // {continue, break}
    int _labelCount = 0;
    int _staticCount = 0;
};

}  // namespace mr