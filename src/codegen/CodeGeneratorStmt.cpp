#include "codegen/CodeGenerator.hpp"

namespace mr {

void CodeGenerator::genScope(const NodeStmtScope &scope) {
    beginScope();
    for (const NodeStmt *stmt : scope.stmts) { genStmt(*stmt); }
    endScope();
}

void CodeGenerator::genIfChain(const NodeElseChain &chain,
                               const std::string &endLabel) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeElseIf>) {
                genExpr(*node->expr);
                pop("rax");
                const std::string nextLabel = newLabel("nahitar_next");
                _out << "    test rax, rax\n    jz " << nextLabel << "\n";
                genScope(*node->scope);
                _out << "    jmp " << endLabel << "\n" << nextLabel << ":\n";
                if (node->next.has_value()) {
                    genIfChain(*node->next.value(), endLabel);
                }
            } else if constexpr (std::is_same_v<T, NodeElse>) {
                genScope(*node->scope);
            }
        },
        chain.var);
}

void CodeGenerator::genCompoundAssign(const NodeStmtAssign &assign) {
    const Var *v = findVar(assign.name);
    const bool isStatic = v != nullptr && v->isStatic;
    const StorageKind targetKind = v != nullptr ? v->kind : StorageKind::Int;
    const std::string loc =
        isStatic
            ? ("[" + v->staticLabel + "]")
            : ("[rsp + " + std::to_string(stackOffsetOf(assign.name)) + "]");

    if (assign.op == CompoundOp::Assign) {
        const StorageKind exprKind = inferKind(*assign.expr);
        genExpr(*assign.expr);
        pop("rax");
        emitKindConversion("rax", exprKind, targetKind);
        _out << "    mov QWORD " << loc << ", rax    ; assign " << assign.name
             << "\n";
        return;
    }

    const bool isArith = (assign.op == CompoundOp::AddAssign ||
                          assign.op == CompoundOp::SubAssign ||
                          assign.op == CompoundOp::MulAssign ||
                          assign.op == CompoundOp::DivAssign);

    if (targetKind == StorageKind::Float && isArith) {
        // Route arithmetic compound-assignment through SSE when the
        // variable is `bhagank`, converting the right-hand side to a
        // double first if it isn't already one (e.g. `x += 2;`).
        const StorageKind exprKind = inferKind(*assign.expr);
        genExpr(*assign.expr);
        pop("rbx");
        emitKindConversion("rbx", exprKind, StorageKind::Float);
        _out << "    mov rax, QWORD " << loc << "    ; load current "
             << assign.name << "\n";
        _out << "    movq xmm0, rax\n    movq xmm1, rbx\n";
        switch (assign.op) {
            case CompoundOp::AddAssign:
                _out << "    addsd xmm0, xmm1\n";
                break;
            case CompoundOp::SubAssign:
                _out << "    subsd xmm0, xmm1\n";
                break;
            case CompoundOp::MulAssign:
                _out << "    mulsd xmm0, xmm1\n";
                break;
            case CompoundOp::DivAssign:
                _out << "    divsd xmm0, xmm1\n";
                break;
            default:
                break;
        }
        _out << "    movq rax, xmm0\n";
        _out << "    mov QWORD " << loc << ", rax    ; store " << assign.name
             << "\n";
        return;
    }

    const StorageKind exprKind = inferKind(*assign.expr);
    genExpr(*assign.expr);
    pop("rbx");
    // If the variable is int-like but the rhs happens to be a bhagank
    // expression (e.g. `intVar += 2.5;`), truncate it rather than
    // reinterpreting its bits as an integer.
    emitKindConversion("rbx", exprKind, targetKind);
    _out << "    mov rax, QWORD " << loc << "    ; load current " << assign.name
         << "\n";
    switch (assign.op) {
        case CompoundOp::AddAssign:
            _out << "    add rax, rbx\n";
            break;
        case CompoundOp::SubAssign:
            _out << "    sub rax, rbx\n";
            break;
        case CompoundOp::MulAssign:
            _out << "    imul rax, rbx\n";
            break;
        case CompoundOp::DivAssign:
            _out << "    cqo\n    idiv rbx\n";
            break;
        case CompoundOp::ModAssign:
            _out << "    cqo\n    idiv rbx\n    mov rax, rdx\n";
            break;
        case CompoundOp::AndAssign:
            _out << "    and rax, rbx\n";
            break;
        case CompoundOp::OrAssign:
            _out << "    or rax, rbx\n";
            break;
        case CompoundOp::XorAssign:
            _out << "    xor rax, rbx\n";
            break;
        case CompoundOp::ShlAssign:
            _out << "    mov rcx, rbx\n    shl rax, cl\n";
            break;
        case CompoundOp::ShrAssign:
            _out << "    mov rcx, rbx\n    sar rax, cl\n";
            break;
        case CompoundOp::Assign:
            break;
    }
    _out << "    mov QWORD " << loc << ", rax    ; store " << assign.name
         << "\n";
}

void CodeGenerator::genStmt(const NodeStmt &stmt) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeStmtExit>) {
                _out << "    ; shevti(...)\n";
                genExpr(*node->expr);
                pop("rdi");
                _out << "    mov rax, 60\n    syscall\n";
            } else if constexpr (std::is_same_v<T, NodeStmtPrint>) {
                _out << "    ; leeh(...)\n";
                const StorageKind k = inferKind(*node->expr);
                genExpr(*node->expr);
                pop("rdi");
                switch (k) {
                    case StorageKind::Str:
                        _out << "    call print_str\n";
                        break;
                    case StorageKind::Char:
                        _out << "    call print_char\n";
                        break;
                    case StorageKind::Float:
                        _out << "    call print_float\n";
                        break;
                    default:
                        _out << "    call print_int\n";
                        break;
                }
            } else if constexpr (std::is_same_v<T, NodeStmtVarDecl>) {
                // Every declaration outside a function body is a real
                // global (see the CodeGenerator class comment), regardless
                // of whether `sthir` was written.
                const bool storeAsStatic =
                    node->modifiers.isStatic || !_inFunction;
                _out << "    ; declare " << node->name
                     << (node->modifiers.isStatic
                             ? " (sthir)"
                             : (!_inFunction ? " (global)" : ""))
                     << "\n";
                const StorageKind k = resolveStorageKind(node->modifiers.type);
                declareVar(node->name, storeAsStatic, k);
                if (!_inFunction) {
                    // Keep a persistent lookup entry that survives
                    // genFuncDecl()'s per-function `_vars.clear()`, so a
                    // function body can still resolve this name. A
                    // `sthir` declared *inside* a function is
                    // intentionally NOT mirrored here - it keeps the same
                    // persistent .bss storage, but the name itself stays
                    // local to that one function (matching "static
                    // local" semantics), since `_inFunction` is true at
                    // that declaration site.
                    _globals.push_back(*findVar(node->name));
                }
                if (storeAsStatic) {
                    if (node->expr.has_value()) {
                        const StorageKind exprKind =
                            inferKind(*node->expr.value());
                        genExpr(*node->expr.value());
                        pop("rax");
                        emitKindConversion("rax", exprKind, k);
                        const Var *v = findVar(node->name);
                        _out << "    mov QWORD [" << v->staticLabel
                             << "], rax\n";
                    }
                    // else: .bss is zero-initialized already.
                } else {
                    if (node->expr.has_value()) {
                        const StorageKind exprKind =
                            inferKind(*node->expr.value());
                        genExpr(*node->expr.value());
                        if (exprKind != k) {
                            // The initializer's value is already the new
                            // variable's stack slot (no extra push needed
                            // for the common case) - but converting it in
                            // place means popping it, fixing it up, and
                            // pushing it back.
                            pop("rax");
                            emitKindConversion("rax", exprKind, k);
                            push("rax");
                        }
                    } else {
                        push("0", "uninitialized " + node->name);
                    }
                }
            } else if constexpr (std::is_same_v<T, NodeStmtAssign>) {
                genCompoundAssign(*node);
            } else if constexpr (std::is_same_v<T, NodeStmtScope>) {
                _out << "    ; scope\n";
                genScope(*node);
                _out << "    ; /scope\n";
            } else if constexpr (std::is_same_v<T, NodeStmtIf>) {
                _out << "    ; jar (...)\n";
                genExpr(*node->expr);
                pop("rax");
                _out << "    test rax, rax\n";
                if (node->elseChain.has_value()) {
                    const std::string falseLabel = newLabel("jar_false");
                    const std::string endLabel = newLabel("jar_end");
                    _out << "    jz " << falseLabel << "\n";
                    genScope(*node->scope);
                    _out << "    jmp " << endLabel << "\n"
                         << falseLabel << ":\n";
                    genIfChain(*node->elseChain.value(), endLabel);
                    _out << endLabel << ":\n";
                } else {
                    const std::string endLabel = newLabel("jar_end");
                    _out << "    jz " << endLabel << "\n";
                    genScope(*node->scope);
                    _out << endLabel << ":\n";
                }
            } else if constexpr (std::is_same_v<T, NodeStmtWhile>) {
                const std::string condLabel = newLabel("jovar_cond");
                const std::string endLabel = newLabel("jovar_end");
                _loopLabels.push_back({condLabel, endLabel});
                _out << condLabel << ":\n";
                genExpr(*node->expr);
                pop("rax");
                _out << "    test rax, rax\n    jz " << endLabel << "\n";
                genScope(*node->scope);
                _out << "    jmp " << condLabel << "\n" << endLabel << ":\n";
                _loopLabels.pop_back();
            } else if constexpr (std::is_same_v<T, NodeStmtFor>) {
                beginScope();
                genStmt(*node->init);
                const std::string condLabel = newLabel("pratyek_cond");
                const std::string stepLabel = newLabel("pratyek_step");
                const std::string endLabel = newLabel("pratyek_end");
                _loopLabels.push_back({stepLabel, endLabel});
                _out << condLabel << ":\n";
                genExpr(*node->cond);
                pop("rax");
                _out << "    test rax, rax\n    jz " << endLabel << "\n";
                genScope(*node->scope);
                _out << stepLabel << ":\n";
                genStmt(*node->step);
                _out << "    jmp " << condLabel << "\n" << endLabel << ":\n";
                _loopLabels.pop_back();
                endScope();
            } else if constexpr (std::is_same_v<T, NodeStmtBreak>) {
                _out << "    jmp " << _loopLabels.back().second
                     << "    ; thamba\n";
            } else if constexpr (std::is_same_v<T, NodeStmtContinue>) {
                _out << "    jmp " << _loopLabels.back().first
                     << "    ; pudhe\n";
            } else if constexpr (std::is_same_v<T, NodeStmtSwitch>) {
                genExpr(*node->expr);
                const std::size_t baseline = _stackSize;
                const std::string endLabel = newLabel("paryay_end");
                std::vector<std::string> caseLabels;
                std::string defaultLabel;
                for (std::size_t i = 0; i < node->cases.size(); ++i) {
                    caseLabels.push_back(newLabel("paryay_case"));
                }
                for (std::size_t i = 0; i < node->cases.size(); ++i) {
                    NodeSwitchCase *c = node->cases[i];
                    if (!c->value.has_value()) {
                        defaultLabel = caseLabels[i];
                        continue;
                    }
                    const std::size_t offset = (_stackSize - baseline) * 8;
                    push("QWORD [rsp + " + std::to_string(offset) + "]",
                         "duplicate switch scrutinee for comparison");
                    genExpr(*c->value.value());
                    pop("rbx");
                    pop("rax");
                    _out << "    cmp rax, rbx\n    je " << caseLabels[i]
                         << "\n";
                }
                _out << "    jmp "
                     << (defaultLabel.empty() ? endLabel : defaultLabel)
                     << "\n";
                for (std::size_t i = 0; i < node->cases.size(); ++i) {
                    _out << caseLabels[i] << ":\n";
                    beginScope();
                    for (const NodeStmt *s : node->cases[i]->stmts) {
                        genStmt(*s);
                    }
                    endScope();
                    _out << "    jmp " << endLabel << "\n";
                }
                _out << endLabel << ":\n";
                pop("rax", "discard switch scrutinee");
            } else if constexpr (std::is_same_v<T, NodeStmtReturn>) {
                if (node->expr.has_value()) {
                    const StorageKind exprKind = inferKind(*node->expr.value());
                    genExpr(*node->expr.value());
                    pop("rax");
                    emitKindConversion("rax", exprKind, _currentFuncReturnKind);
                }
                _out << "    mov rsp, rbp\n    pop rbp\n    ret    ; partav\n";
            } else if constexpr (std::is_same_v<T, NodeStmtExprStmt>) {
                genExpr(*node->expr);
                pop("rax", "discard unused expression statement result");
            } else if constexpr (std::is_same_v<T, NodeStmtFuncDecl>) {
                // emitted separately by genFunctions()
            }
        },
        stmt.var);
}

}  // namespace mr
