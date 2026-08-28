#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "support/SourceLocation.hpp"

// AST layout
// ----------
// Every node owns a SourceLocation so diagnostics can point at the right
// place in the source at any pipeline stage. Nodes are allocated out of an
// Arena and referenced by raw pointer; the arena owns their lifetime.
//
// See docs/README.md for the Tier 1 / Tier 2 split: this AST supports
// declarations (with full modifier chains), the full operator set,
// if/nahitar/anyatha, while/for loops with break/continue, switch
// (paryay), functions with return, and scopes. Structs/classes,
// arrays/strings, and try/catch are Tier 2 and documented separately.
namespace mr {

struct NodeExpr;

// ---- Types ----
// A very small type system: a base keyword plus an optional size
// qualifier. Real width/format differences (int8 vs int64, double vs
// int64 bit pattern) are handled in codegen; sema uses this purely to
// check declared-type vs. usage in the limited ways currently enforced
// (e.g. function argument counts, return-type presence).
enum class BaseType {
    Ank,
    Akshar,
    Bhagank,
    Purnank,
    Vidhan,
    Nirank,
    Agyat,
    Inferred
};
enum class SizeQualifier { Default, Lahan, Maha, Uch };

struct TypeInfo {
    BaseType base = BaseType::Inferred;
    SizeQualifier size = SizeQualifier::Default;
    bool isCollection = false;  // `te` vs `he`
};

// Declaration modifiers shared by variables and functions.
struct Modifiers {
    bool isPrivate = false;  // maze
    bool isStatic = false;   // sthir
    bool isAll =
        false;  // sarve (spec does not define semantics; accepted as a no-op)
    bool isImmutable = false;  // ahe
    TypeInfo type;
};

// ---- Expressions ----

struct NodeTermIntLiteral {
    std::string text;
    SourceLocation loc;
};

struct NodeTermFloatLiteral {
    std::string text;
    SourceLocation loc;
};

struct NodeTermBoolLiteral {  // khare / khote
    bool value;
    SourceLocation loc;
};

struct NodeTermIdentifier {
    std::string name;
    SourceLocation loc;
};

struct NodeTermParen {
    NodeExpr *inner;
};

struct NodeCallExpr {  // name(args...)
    std::string callee;
    std::vector<NodeExpr *> args;
    SourceLocation loc;
};

struct NodeTerm {
    std::variant<NodeTermIntLiteral *, NodeTermFloatLiteral *,
                 NodeTermBoolLiteral *, NodeTermIdentifier *, NodeTermParen *,
                 NodeCallExpr *>
        var;
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    LogicalAnd,
    LogicalOr,
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
};

struct NodeBinExpr {
    BinaryOp op;
    NodeExpr *lhs;
    NodeExpr *rhs;
    SourceLocation loc;
};

enum class UnaryOp { Neg, Plus, LogicalNot, BitNot };

struct NodeUnaryExpr {
    UnaryOp op;
    NodeExpr *operand;
    SourceLocation loc;
};

enum class IncDecOp { PreInc, PreDec, PostInc, PostDec };

struct NodeIncDecExpr {
    IncDecOp op;
    std::string name;
    SourceLocation loc;
};

struct NodeExpr {
    std::variant<NodeTerm *, NodeBinExpr *, NodeUnaryExpr *, NodeIncDecExpr *>
        var;
};

// ---- Statements ----

struct NodeStmt;

struct NodeStmtScope {
    std::vector<NodeStmt *> stmts;
};

struct NodeStmtExit {  // shevti(expr);
    NodeExpr *expr;
    SourceLocation loc;
};

struct NodeStmtPrint {  // leeh(expr);
    NodeExpr *expr;
    SourceLocation loc;
};

struct NodeStmtVarDecl {  // [modifiers] type name = expr;  (or `ahe` for
                          // immutable)
    std::string name;
    std::optional<NodeExpr *> expr;
    Modifiers modifiers;
    SourceLocation loc;
    SourceLocation nameLoc;
};

enum class CompoundOp {
    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,
    ModAssign,
    AndAssign,
    OrAssign,
    XorAssign,
    ShlAssign,
    ShrAssign
};

struct NodeStmtAssign {  // name (op)= expr;
    std::string name;
    CompoundOp op;
    NodeExpr *expr;
    SourceLocation nameLoc;
};

struct NodeElseIf;
struct NodeElse;

struct NodeElseChain {
    std::variant<NodeElseIf *, NodeElse *> var;
};

struct NodeElseIf {
    NodeExpr *expr;
    NodeStmtScope *scope;
    std::optional<NodeElseChain *> next;
    SourceLocation loc;
};

struct NodeElse {
    NodeStmtScope *scope;
};

struct NodeStmtIf {
    NodeExpr *expr;
    NodeStmtScope *scope;
    std::optional<NodeElseChain *> elseChain;
    SourceLocation loc;
};

struct NodeStmtWhile {  // jovar (expr) { ... }
    NodeExpr *expr;
    NodeStmtScope *scope;
    SourceLocation loc;
};

struct NodeStmtFor {  // pratyek init; cond; step { ... }
    NodeStmt *init;  // NodeStmtVarDecl* or NodeStmtAssign*, wrapped in NodeStmt
    NodeExpr *cond;
    NodeStmt *step;  // NodeStmtAssign* (incl. ++/--), wrapped in NodeStmt
    NodeStmtScope *scope;
    SourceLocation loc;
};

struct NodeStmtBreak {
    SourceLocation loc;
};
struct NodeStmtContinue {
    SourceLocation loc;
};

struct NodeSwitchCase {
    std::optional<NodeExpr *>
        value;  // nullopt for the `anyatha` (default) case
    std::vector<NodeStmt *> stmts;
    SourceLocation loc;
};

struct NodeStmtSwitch {  // paryay (expr) { 1: ...; anyatha: ...; }
    NodeExpr *expr;
    std::vector<NodeSwitchCase *> cases;
    SourceLocation loc;
};

struct NodeParam {
    std::string name;
    Modifiers modifiers;
    SourceLocation loc;
};

struct NodeStmtFuncDecl {  // [modifiers] returnType karya name(params) { ... }
    std::string name;
    std::vector<NodeParam *> params;
    Modifiers modifiers;  // modifiers.type is the return type
    NodeStmtScope *body;
    SourceLocation loc;
};

struct NodeStmtReturn {  // partav [expr];
    std::optional<NodeExpr *> expr;
    SourceLocation loc;
};

struct NodeStmtExprStmt {  // a bare call used as a statement: foo();
    NodeExpr *expr;
    SourceLocation loc;
};

struct NodeStmt {
    std::variant<NodeStmtExit *, NodeStmtPrint *, NodeStmtVarDecl *,
                 NodeStmtScope *, NodeStmtIf *, NodeStmtAssign *,
                 NodeStmtWhile *, NodeStmtFor *, NodeStmtBreak *,
                 NodeStmtContinue *, NodeStmtSwitch *, NodeStmtFuncDecl *,
                 NodeStmtReturn *, NodeStmtExprStmt *>
        var;
};

struct NodeProgram {
    std::vector<NodeStmt *> stmts;
};

}  // namespace mr