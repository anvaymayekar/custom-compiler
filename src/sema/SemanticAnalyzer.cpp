#include "sema/SemanticAnalyzer.hpp"

#include <algorithm>

namespace mr {

const SemanticAnalyzer::VarInfo *SemanticAnalyzer::find(
    const std::string &name) const {
    for (auto it = _scopes.rbegin(); it != _scopes.rend(); ++it) {
        auto found =
            std::find_if(it->vars.begin(), it->vars.end(),
                         [&](const VarInfo &v) { return v.name == name; });
        if (found != it->vars.end()) { return &(*found); }
    }
    return nullptr;
}

void SemanticAnalyzer::declare(const std::string &name, bool isImmutable,
                               const SourceLocation &loc) {
    Scope &current = _scopes.back();
    if (std::find_if(current.vars.begin(), current.vars.end(),
                     [&](const VarInfo &v) { return v.name == name; }) !=
        current.vars.end()) {
        _diags.error(
            DiagCategory::Semantic, loc,
            "identifier '" + name + "' is already declared in this scope",
            "remove the duplicate declaration or rename one of the variables",
            static_cast<int>(name.size()));
        return;
    }
    current.vars.push_back({name, isImmutable});
}

void SemanticAnalyzer::checkUse(const std::string &name,
                                const SourceLocation &loc) {
    if (find(name) == nullptr) {
        _diags.error(DiagCategory::Semantic, loc,
                     "undeclared identifier '" + name + "'",
                     "'" + name + "' was not declared in this scope",
                     static_cast<int>(name.size()));
    }
}

void SemanticAnalyzer::checkAssignable(const std::string &name,
                                       const SourceLocation &loc) {
    const VarInfo *info = find(name);
    if (info == nullptr) {
        _diags.error(DiagCategory::Semantic, loc,
                     "undeclared identifier '" + name + "'",
                     "'" + name + "' was not declared in this scope",
                     static_cast<int>(name.size()));
        return;
    }
    if (info->isImmutable) {
        _diags.error(DiagCategory::Semantic, loc,
                     "cannot assign to '" + name +
                         "': it was declared immutable with 'ahe'",
                     std::nullopt, static_cast<int>(name.size()));
    }
}

void SemanticAnalyzer::visitTerm(const NodeTerm &term) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTermIdentifier>) {
                checkUse(node->name, node->loc);
            } else if constexpr (std::is_same_v<T, NodeTermParen>) {
                visitExpr(*node->inner);
            } else if constexpr (std::is_same_v<T, NodeCallExpr>) {
                auto it = _functions.find(node->callee);
                if (it == _functions.end()) {
                    _diags.error(
                        DiagCategory::Semantic, node->loc,
                        "call to undeclared function '" + node->callee + "'");
                } else if (it->second.arity != node->args.size()) {
                    _diags.error(DiagCategory::Semantic, node->loc,
                                 "'" + node->callee + "' expects " +
                                     std::to_string(it->second.arity) +
                                     " argument(s) but got " +
                                     std::to_string(node->args.size()));
                }
                for (NodeExpr *arg : node->args) { visitExpr(*arg); }
            }
            // Int/Float/Bool literals need no semantic check.
        },
        term.var);
}

void SemanticAnalyzer::visitExpr(const NodeExpr &expr) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTerm>) {
                visitTerm(*node);
            } else if constexpr (std::is_same_v<T, NodeBinExpr>) {
                visitExpr(*node->lhs);
                visitExpr(*node->rhs);
            } else if constexpr (std::is_same_v<T, NodeUnaryExpr>) {
                visitExpr(*node->operand);
            } else if constexpr (std::is_same_v<T, NodeIncDecExpr>) {
                checkAssignable(node->name, node->loc);
            }
        },
        expr.var);
}

void SemanticAnalyzer::visitScope(const NodeStmtScope &scope) {
    _scopes.push_back(Scope{});
    for (const NodeStmt *stmt : scope.stmts) { visitStmt(*stmt); }
    _scopes.pop_back();
}

void SemanticAnalyzer::visitFuncDecl(const NodeStmtFuncDecl &func) {
    _scopes.push_back(Scope{});
    for (const NodeParam *param : func.params) {
        declare(param->name, false, param->loc);
    }
    _funcDepth++;
    for (const NodeStmt *stmt : func.body->stmts) { visitStmt(*stmt); }
    _funcDepth--;
    _scopes.pop_back();
}

void SemanticAnalyzer::visitStmt(const NodeStmt &stmt) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeStmtExit>) {
                visitExpr(*node->expr);
            } else if constexpr (std::is_same_v<T, NodeStmtPrint>) {
                visitExpr(*node->expr);
            } else if constexpr (std::is_same_v<T, NodeStmtVarDecl>) {
                if (node->modifiers.isImmutable && !node->expr.has_value()) {
                    _diags.error(
                        DiagCategory::Semantic, node->nameLoc,
                        "'" + node->name +
                            "' is declared 'ahe' but has no initializer");
                }
                if (node->expr.has_value()) { visitExpr(*node->expr.value()); }
                declare(node->name, node->modifiers.isImmutable, node->nameLoc);
            } else if constexpr (std::is_same_v<T, NodeStmtScope>) {
                visitScope(*node);
            } else if constexpr (std::is_same_v<T, NodeStmtIf>) {
                visitExpr(*node->expr);
                visitScope(*node->scope);
                std::optional<NodeElseChain *> chain = node->elseChain;
                while (chain.has_value()) {
                    NodeElseChain *link = chain.value();
                    if (std::holds_alternative<NodeElseIf *>(link->var)) {
                        NodeElseIf *elseIf = std::get<NodeElseIf *>(link->var);
                        visitExpr(*elseIf->expr);
                        visitScope(*elseIf->scope);
                        chain = elseIf->next;
                    } else {
                        NodeElse *elseNode = std::get<NodeElse *>(link->var);
                        visitScope(*elseNode->scope);
                        chain = std::nullopt;
                    }
                }
            } else if constexpr (std::is_same_v<T, NodeStmtAssign>) {
                checkAssignable(node->name, node->nameLoc);
                visitExpr(*node->expr);
            } else if constexpr (std::is_same_v<T, NodeStmtWhile>) {
                visitExpr(*node->expr);
                _loopDepth++;
                visitScope(*node->scope);
                _loopDepth--;
            } else if constexpr (std::is_same_v<T, NodeStmtFor>) {
                _scopes.push_back(Scope{});
                visitStmt(*node->init);
                _loopDepth++;
                visitExpr(*node->cond);
                visitStmt(*node->step);
                visitScope(*node->scope);
                _loopDepth--;
                _scopes.pop_back();
            } else if constexpr (std::is_same_v<T, NodeStmtBreak>) {
                if (_loopDepth == 0) {
                    _diags.error(DiagCategory::Semantic, node->loc,
                                 "'thamba' used outside a loop");
                }
            } else if constexpr (std::is_same_v<T, NodeStmtContinue>) {
                if (_loopDepth == 0) {
                    _diags.error(DiagCategory::Semantic, node->loc,
                                 "'pudhe' used outside a loop");
                }
            } else if constexpr (std::is_same_v<T, NodeStmtSwitch>) {
                visitExpr(*node->expr);
                for (NodeSwitchCase *c : node->cases) {
                    if (c->value.has_value()) { visitExpr(*c->value.value()); }
                    _scopes.push_back(Scope{});
                    for (const NodeStmt *s : c->stmts) { visitStmt(*s); }
                    _scopes.pop_back();
                }
            } else if constexpr (std::is_same_v<T, NodeStmtFuncDecl>) {
                if (_funcDepth > 0 || _scopes.size() > 1) {
                    _diags.error(DiagCategory::Semantic, node->loc,
                                 "nested function declarations are not "
                                 "supported; declare '" +
                                     node->name + "' at the top level");
                    return;
                }
                visitFuncDecl(*node);
            } else if constexpr (std::is_same_v<T, NodeStmtReturn>) {
                if (_funcDepth == 0) {
                    _diags.error(DiagCategory::Semantic, node->loc,
                                 "'partav' used outside a function");
                }
                if (node->expr.has_value()) { visitExpr(*node->expr.value()); }
            } else if constexpr (std::is_same_v<T, NodeStmtExprStmt>) {
                visitExpr(*node->expr);
            }
        },
        stmt.var);
}

void SemanticAnalyzer::collectFunctionSignatures(const NodeProgram &program) {
    for (const NodeStmt *stmt : program.stmts) {
        if (!std::holds_alternative<NodeStmtFuncDecl *>(stmt->var)) {
            continue;
        }
        const NodeStmtFuncDecl *func = std::get<NodeStmtFuncDecl *>(stmt->var);
        if (_functions.contains(func->name)) {
            _diags.error(DiagCategory::Semantic, func->loc,
                         "function '" + func->name + "' is already declared");
            continue;
        }
        _functions[func->name] = FuncInfo{func->params.size(), func->loc};
    }
}

void SemanticAnalyzer::analyze(const NodeProgram &program) {
    _scopes.clear();
    _functions.clear();
    _loopDepth = 0;
    _funcDepth = 0;
    _scopes.push_back(Scope{});
    collectFunctionSignatures(program);
    for (const NodeStmt *stmt : program.stmts) { visitStmt(*stmt); }
    _scopes.pop_back();
}

}  // namespace mr