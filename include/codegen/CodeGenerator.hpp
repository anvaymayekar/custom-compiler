#pragma once

#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>
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
//
// Type handling: the backend tracks a coarse StorageKind (Int/Char/Str/
// Float/Bool) per variable and per function signature (see
// collectFunctionSignatures()) so it knows when a value needs converting
// between the integer and SSE register classes - e.g. passing an `ank`
// literal where a `bhagank` parameter is expected, or printing a
// function's result with the right print_* routine. Every place a value
// crosses a "declared kind" boundary (assignment, call argument, return)
// goes through emitKindConversion() so mismatched int/float kinds don't
// silently reinterpret each other's bit patterns.
//
// Scoping: every top-level declaration (outside any `karya` body) is
// given real global (.bss) storage, regardless of whether `sthir` was
// written - there's no other coherent meaning for "global" when each
// function is generated as its own isolated stack frame (genFuncDecl()
// clears _vars per function), so a non-static top-level variable would
// otherwise be a dangling reference to a frame the function can't
// address. These are tracked separately in _globals so they survive that
// per-function _vars reset; see the `_inFunction` flag and findVar().
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
        StorageKind kind = StorageKind::Int;
    };

    struct FuncSig {
        std::vector<StorageKind> paramKinds;
        StorageKind returnKind = StorageKind::Int;
    };

    void collectFunctionSignatures();

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
    void emitStrConcatRoutine();
    void emitCharToStrRoutine();
    void emitIntToStrRoutine();

    // Infers the StorageKind an expression/term will evaluate to, using
    // declared variable/function kinds - does not emit any code.
    [[nodiscard]] StorageKind inferKind(const NodeTerm &term) const;
    [[nodiscard]] StorageKind inferKind(const NodeExpr &expr) const;

    // If `from` and `to` disagree about being StorageKind::Float, emits
    // the int<->double conversion (cvtsi2sd / cvttsd2si) needed so the
    // 64-bit value sitting in `gpReg` is reinterpreted correctly on the
    // other side of the boundary. No-op when the kinds already agree, or
    // when neither/both sides are float-incompatible kinds (Str is never
    // auto-converted). Clobbers xmm0.
    void emitKindConversion(const std::string &gpReg, StorageKind from,
                            StorageKind to);

    void push(const std::string &reg, const std::string &comment = {});
    void pop(const std::string &reg, const std::string &comment = {});
    void beginScope();
    void endScope();
    [[nodiscard]] std::string newLabel(const std::string &hint);
    [[nodiscard]] std::size_t stackOffsetOf(const std::string &name) const;
    [[nodiscard]] const Var *findVar(const std::string &name) const;
    void declareVar(const std::string &name, bool isStatic, StorageKind kind);
    [[nodiscard]] std::string internString(const std::string &text);

    NodeProgram _program;
    std::ostringstream _out;         // _start + function bodies
    std::ostringstream _staticData;  // .bss slots for `sthir` variables
    std::ostringstream _rodata;      // .rodata slots for string literals
    std::size_t _stackSize = 0;
    std::vector<Var> _vars;
    // Top-level (global) declarations, tracked separately from _vars so
    // they remain resolvable from inside a function body even though
    // genFuncDecl() clears _vars per function - see the class comment.
    std::vector<Var> _globals;
    bool _inFunction = false;
    std::vector<std::size_t> _scopeMarks;
    std::vector<std::pair<std::string, std::string>>
        _loopLabels;  // {continue, break}
    std::unordered_map<std::string, FuncSig> _functionSigs;
    StorageKind _currentFuncReturnKind = StorageKind::Int;
    int _labelCount = 0;
    int _staticCount = 0;
    int _stringCount = 0;
};

}  // namespace mr
