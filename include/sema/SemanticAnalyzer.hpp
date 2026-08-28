#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ast/Ast.hpp"
#include "diagnostics/DiagnosticEngine.hpp"

namespace mr {

// Walks the AST after parsing and before codegen. Checks:
//  - undeclared / duplicate / out-of-scope identifiers
//  - assignment to an immutable (`ahe`) variable
//  - break/continue used outside a loop
//  - return used outside a function
//  - call to an undeclared function, or with the wrong argument count
//
// Functions are only recognized at top level (no nested function
// declarations) - this keeps the calling convention in CodeGenerator
// simple (flat, non-nested call frames) and matches the spec's examples,
// none of which nest a `karya` inside another.
class SemanticAnalyzer final {
   public:
    explicit SemanticAnalyzer(DiagnosticEngine &diags) : _diags(diags) {
    }

    void analyze(const NodeProgram &program);

   private:
    struct VarInfo {
        std::string name;
        bool isImmutable;
    };
    struct Scope {
        std::vector<VarInfo> vars;
    };
    struct FuncInfo {
        std::size_t arity;
        SourceLocation loc;
    };

    void collectFunctionSignatures(const NodeProgram &program);

    void visitStmt(const NodeStmt &stmt);
    void visitScope(const NodeStmtScope &scope);
    void visitExpr(const NodeExpr &expr);
    void visitTerm(const NodeTerm &term);
    void visitFuncDecl(const NodeStmtFuncDecl &func);

    void declare(const std::string &name, bool isImmutable,
                 const SourceLocation &loc);
    void checkUse(const std::string &name, const SourceLocation &loc);
    void checkAssignable(const std::string &name, const SourceLocation &loc);
    [[nodiscard]] const VarInfo *find(const std::string &name) const;

    DiagnosticEngine &_diags;
    std::vector<Scope> _scopes;
    std::unordered_map<std::string, FuncInfo> _functions;
    int _loopDepth = 0;
    int _funcDepth = 0;
};

}  // namespace mr