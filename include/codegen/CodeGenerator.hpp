#pragma once

#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ast/Ast.hpp"

namespace mr {

class CodeGenerator final {
   public:
    explicit CodeGenerator(NodeProgram program) : _program(std::move(program)) {
    }

    [[nodiscard]] std::string generate();

   private:
    struct Var {
        std::string name;
        bool isStatic = false;
        std::size_t stackLoc = 0;
        std::string staticLabel;
        StorageKind kind = StorageKind::Int;
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
    void emitPrintStrRoutine();
    void emitPrintCharRoutine();
    void emitPrintFloatRoutine();

    void push(const std::string &reg, const std::string &comment = {});
    void pop(const std::string &reg, const std::string &comment = {});
    void beginScope();
    void endScope();
    [[nodiscard]] std::string newLabel(const std::string &hint);
    [[nodiscard]] std::size_t stackOffsetOf(const std::string &name) const;
    [[nodiscard]] const Var *findVar(const std::string &name) const;
    void declareVar(const std::string &name, bool isStatic, StorageKind kind);

    // Best-effort static type inference over an already-parsed expression,
    // used only to pick codegen strategy (push width / SSE vs GP / which
    // print_* routine) - not a real type checker and not wired into
    // SemanticAnalyzer. Falls back to Int for anything ambiguous (e.g. a
    // call, whose return kind we don't track), which preserves today's
    // int-only behavior for everything that isn't explicitly float/char/str.
    [[nodiscard]] StorageKind inferKind(const NodeExpr &expr) const;
    [[nodiscard]] StorageKind inferKind(const NodeTerm &term) const;

    // Registers a string literal in .rodata (length-prefixed: an 8-byte
    // length followed by the raw bytes, no NUL terminator needed) and
    // returns its label. Identical literals are not deduplicated - kept
    // simple since programs are small.
    [[nodiscard]] std::string internString(const std::string &text);

    NodeProgram _program;
    std::ostringstream _out;
    std::ostringstream _staticData;  // .bss slots for `sthir` variables
    std::ostringstream _rodata;      // string literal storage
    std::size_t _stackSize = 0;
    std::vector<Var> _vars;
    std::vector<std::size_t> _scopeMarks;
    std::vector<std::pair<std::string, std::string>> _loopLabels;
    int _labelCount = 0;
    int _staticCount = 0;
    int _stringCount = 0;
};

}  // namespace mr