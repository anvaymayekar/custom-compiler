#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "support/SourceLocation.hpp"

namespace mr {

struct NodeExpr;

// ---- Types ----
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

struct Modifiers {
    bool isPrivate = false;    // maze
    bool isStatic = false;     // sthir
    bool isAll = false;        // sarve
    bool isImmutable = false;  // ahe
    TypeInfo type;
};

// Resolved storage kind for a declared variable or a literal - derived
// once from a TypeInfo (base keyword x he/te) rather than re-derived ad
// hoc at every use site. CodeGenerator uses this to pick push width,
// arithmetic class (integer vs SSE), and print routine; SemanticAnalyzer
// will grow real type-mismatch checks against it over time.
enum class StorageKind { Int, Char, Str, Float, Bool, Inferred };

[[nodiscard]] inline StorageKind resolveStorageKind(const TypeInfo &t) {
    switch (t.base) {
        case BaseType::Ank:
            return StorageKind::Int;
        case BaseType::Akshar:
            // `te akshar` (collection of chars) is a string; `he akshar`
            // (singular) is one character.
            return t.isCollection ? StorageKind::Str : StorageKind::Char;
        case BaseType::Bhagank:
            return StorageKind::Float;
        default:
            // purnank/vidhan/nirank/agyat: reserved, not yet given real
            // codegen semantics - treated as Int so declarations still
            // compile rather than hard-failing.
            return StorageKind::Int;
    }
}

// ---- Expressions ----

struct NodeTermIntLiteral {
    std::string text;
    SourceLocation loc;
};

struct NodeTermFloatLiteral {
    std::string text;
    SourceLocation loc;
};

struct NodeTermBoolLiteral {
    bool value;
    SourceLocation loc;
};

struct NodeTermStringLiteral {  // "..."
    std::string text;           // already escape-processed by the lexer
    SourceLocation loc;
};

struct NodeTermCharLiteral {  // '.'
    std::string text;         // one (possibly escape-processed) byte
    SourceLocation loc;
};

struct NodeTermIdentifier {
    std::string name;
    SourceLocation loc;
};

struct NodeTermParen {
    NodeExpr *inner;
};

struct NodeCallExpr {
    std::string callee;
    std::vector<NodeExpr *> args;
    SourceLocation loc;
};

struct NodeTermTypeOf {  // prakar(expr) - compile-time "what type is this".
    NodeExpr *operand;   // NOT evaluated at runtime, only kind-inspected.
    SourceLocation loc;
};

struct NodeTerm {
    std::variant<NodeTermIntLiteral *, NodeTermFloatLiteral *,
                 NodeTermBoolLiteral *, NodeTermStringLiteral *,
                 NodeTermCharLiteral *, NodeTermIdentifier *, NodeTermParen *,
                 NodeCallExpr *, NodeTermTypeOf *>
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

struct NodeStmtExit {
    NodeExpr *expr;
    SourceLocation loc;
};

struct NodeStmtPrint {
    NodeExpr *expr;
    SourceLocation loc;
};

struct NodeStmtVarDecl {
    std::string name;
    std::optional<NodeExpr *> expr;  // nullopt = forward declaration
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

struct NodeStmtAssign {
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

struct NodeStmtWhile {
    NodeExpr *expr;
    NodeStmtScope *scope;
    SourceLocation loc;
};

struct NodeStmtFor {
    NodeStmt *init;
    NodeExpr *cond;
    NodeStmt *step;
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
    std::optional<NodeExpr *> value;
    std::vector<NodeStmt *> stmts;
    SourceLocation loc;
};

struct NodeStmtSwitch {
    NodeExpr *expr;
    std::vector<NodeSwitchCase *> cases;
    SourceLocation loc;
};

struct NodeParam {
    std::string name;
    Modifiers modifiers;
    SourceLocation loc;
};

struct NodeStmtFuncDecl {
    std::string name;
    std::vector<NodeParam *> params;
    Modifiers modifiers;
    NodeStmtScope *body;
    SourceLocation loc;
};

struct NodeStmtReturn {
    std::optional<NodeExpr *> expr;
    SourceLocation loc;
};

struct NodeStmtExprStmt {
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