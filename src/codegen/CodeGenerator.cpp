#include "codegen/CodeGenerator.hpp"

#include <algorithm>
#include <stdexcept>

namespace mr {

namespace {
const char *kArgRegs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
}

void CodeGenerator::push(const std::string &reg, const std::string &comment) {
    _out << "    push " << reg;
    if (!comment.empty()) { _out << "    ; " << comment; }
    _out << "\n";
    _stackSize++;
}

void CodeGenerator::pop(const std::string &reg, const std::string &comment) {
    _out << "    pop " << reg;
    if (!comment.empty()) { _out << "    ; " << comment; }
    _out << "\n";
    _stackSize--;
}

void CodeGenerator::beginScope() {
    _scopeMarks.push_back(_vars.size());
}

void CodeGenerator::endScope() {
    const std::size_t mark = _scopeMarks.back();
    std::size_t popCount = 0;
    for (std::size_t i = mark; i < _vars.size(); ++i) {
        if (!_vars[i].isStatic) { popCount++; }
    }
    if (popCount > 0) {
        _out << "    add rsp, " << (popCount * 8) << "    ; drop " << popCount
             << " local(s) leaving scope\n";
    }
    _stackSize -= popCount;
    _vars.resize(mark);
    _scopeMarks.pop_back();
}

std::string CodeGenerator::newLabel(const std::string &hint) {
    return "." + hint + std::to_string(_labelCount++);
}

const CodeGenerator::Var *CodeGenerator::findVar(
    const std::string &name) const {
    auto it = std::find_if(_vars.crbegin(), _vars.crend(),
                           [&](const Var &v) { return v.name == name; });
    return it == _vars.crend() ? nullptr : &(*it);
}

std::size_t CodeGenerator::stackOffsetOf(const std::string &name) const {
    const Var *v = findVar(name);
    if (v == nullptr || v->isStatic) {
        throw std::logic_error(
            "codegen: stack offset requested for non-stack variable '" + name +
            "'");
    }
    return (_stackSize - v->stackLoc - 1) * 8;
}

void CodeGenerator::declareVar(const std::string &name, bool isStatic) {
    if (isStatic) {
        const std::string label = "static_" + std::to_string(_staticCount++);
        _staticData << "    " << label << ": resq 1\n";
        _vars.push_back(Var{name, true, 0, label});
    } else {
        _vars.push_back(Var{name, false, _stackSize, {}});
    }
}

// ---- Expressions ----

void CodeGenerator::genTerm(const NodeTerm &term) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTermIntLiteral>) {
                _out << "    mov rax, " << node->text << "\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermFloatLiteral>) {
                // Tier 2 limitation (see docs/README.md): no IEEE-754
                // backend yet, so float literals are truncated to their
                // integer part.
                const std::string intPart =
                    node->text.substr(0, node->text.find('.'));
                _out << "    mov rax, " << intPart
                     << "    ; bhagank literal truncated to int (Tier 2 "
                        "limitation)\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermBoolLiteral>) {
                _out << "    mov rax, " << (node->value ? 1 : 0) << "\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermIdentifier>) {
                const Var *v = findVar(node->name);
                if (v != nullptr && v->isStatic) {
                    push("QWORD [" + v->staticLabel + "]",
                         "load " + node->name);
                } else {
                    const std::size_t offset = stackOffsetOf(node->name);
                    push("QWORD [rsp + " + std::to_string(offset) + "]",
                         "load " + node->name);
                }
            } else if constexpr (std::is_same_v<T, NodeTermParen>) {
                genExpr(*node->inner);
            } else if constexpr (std::is_same_v<T, NodeCallExpr>) {
                genCall(*node);
            }
        },
        term.var);
}

void CodeGenerator::genCall(const NodeCallExpr &call) {
    for (NodeExpr *arg : call.args) { genExpr(*arg); }
    // Args were pushed left-to-right; pop right-to-left into their
    // registers so the first argument ends up in the first register.
    for (std::size_t i = call.args.size(); i-- > 0;) {
        pop(kArgRegs[i],
            "arg " + std::to_string(i) + " for call to " + call.callee);
    }
    _out << "    call func_" << call.callee << "\n";
    push("rax", "result of " + call.callee + "(...)");
}

void CodeGenerator::genUnaryExpr(const NodeUnaryExpr &un) {
    genExpr(*un.operand);
    pop("rax");
    switch (un.op) {
        case UnaryOp::Neg:
            _out << "    neg rax\n";
            break;
        case UnaryOp::Plus:
            break;  // no-op
        case UnaryOp::LogicalNot:
            _out << "    cmp rax, 0\n    sete al\n    movzx rax, al\n";
            break;
        case UnaryOp::BitNot:
            _out << "    not rax\n";
            break;
    }
    push("rax");
}

void CodeGenerator::genIncDecExpr(const NodeIncDecExpr &incDec) {
    const Var *v = findVar(incDec.name);
    const bool isStatic = v != nullptr && v->isStatic;
    const std::string loc =
        isStatic
            ? ("[" + v->staticLabel + "]")
            : ("[rsp + " + std::to_string(stackOffsetOf(incDec.name)) + "]");
    const bool isInc =
        (incDec.op == IncDecOp::PreInc || incDec.op == IncDecOp::PostInc);
    const bool isPre =
        (incDec.op == IncDecOp::PreInc || incDec.op == IncDecOp::PreDec);

    if (isPre) {
        _out << "    " << (isInc ? "add" : "sub") << " QWORD " << loc
             << ", 1\n";
        push("QWORD " + loc,
             (isInc ? "pre-increment " : "pre-decrement ") + incDec.name);
    } else {
        push("QWORD " + loc, "old value of " + incDec.name);
        // Recompute the location: pushing the old value shifted rsp, so a
        // stack-resident variable's offset grows by 8; a static variable's
        // absolute label is unaffected.
        const std::string postLoc =
            isStatic ? loc
                     : ("[rsp + " + std::to_string(stackOffsetOf(incDec.name)) +
                        "]");
        _out << "    " << (isInc ? "add" : "sub") << " QWORD " << postLoc
             << ", 1\n";
    }
}

void CodeGenerator::genBinExpr(const NodeBinExpr &bin) {
    if (bin.op == BinaryOp::LogicalAnd || bin.op == BinaryOp::LogicalOr) {
        const std::string shortCircuit =
            newLabel(bin.op == BinaryOp::LogicalAnd ? "and_false" : "or_true");
        const std::string end = newLabel("logical_end");
        genExpr(*bin.lhs);
        pop("rax");
        _out << "    test rax, rax\n";
        _out << "    " << (bin.op == BinaryOp::LogicalAnd ? "jz " : "jnz ")
             << shortCircuit << "\n";
        genExpr(*bin.rhs);
        pop("rax");
        _out << "    test rax, rax\n";
        _out << "    " << (bin.op == BinaryOp::LogicalAnd ? "jz " : "jnz ")
             << shortCircuit << "\n";
        _out << "    mov rax, " << (bin.op == BinaryOp::LogicalAnd ? 1 : 0)
             << "\n";
        _out << "    jmp " << end << "\n";
        _out << shortCircuit << ":\n";
        _out << "    mov rax, " << (bin.op == BinaryOp::LogicalAnd ? 0 : 1)
             << "\n";
        _out << end << ":\n";
        push("rax");
        return;
    }

    genExpr(*bin.rhs);
    genExpr(*bin.lhs);
    pop("rax");  // lhs
    pop("rbx");  // rhs

    switch (bin.op) {
        case BinaryOp::Add:
            _out << "    add rax, rbx\n";
            break;
        case BinaryOp::Sub:
            _out << "    sub rax, rbx\n";
            break;
        case BinaryOp::Mul:
            // Signed multiply: `ank` is a signed integer, so this must be
            // `imul`, not the unsigned `mul` (which would misinterpret a
            // negative operand as a huge unsigned value).
            _out << "    imul rax, rbx\n";
            break;
        case BinaryOp::Div:
            _out << "    cqo    ; sign-extend rax into rdx:rax for signed "
                    "division\n";
            _out << "    idiv rbx\n";
            break;
        case BinaryOp::Mod:
            _out << "    cqo\n    idiv rbx\n    mov rax, rdx    ; remainder\n";
            break;
        case BinaryOp::Eq:
            _out << "    cmp rax, rbx\n    sete al\n    movzx rax, al\n";
            break;
        case BinaryOp::Ne:
            _out << "    cmp rax, rbx\n    setne al\n    movzx rax, al\n";
            break;
        case BinaryOp::Lt:
            _out << "    cmp rax, rbx\n    setl al\n    movzx rax, al\n";
            break;
        case BinaryOp::Gt:
            _out << "    cmp rax, rbx\n    setg al\n    movzx rax, al\n";
            break;
        case BinaryOp::Le:
            _out << "    cmp rax, rbx\n    setle al\n    movzx rax, al\n";
            break;
        case BinaryOp::Ge:
            _out << "    cmp rax, rbx\n    setge al\n    movzx rax, al\n";
            break;
        case BinaryOp::BitAnd:
            _out << "    and rax, rbx\n";
            break;
        case BinaryOp::BitOr:
            _out << "    or rax, rbx\n";
            break;
        case BinaryOp::BitXor:
            _out << "    xor rax, rbx\n";
            break;
        case BinaryOp::Shl:
            _out << "    mov rcx, rbx\n    shl rax, cl\n";
            break;
        case BinaryOp::Shr:
            _out << "    mov rcx, rbx\n    sar rax, cl    ; arithmetic "
                    "(signed) shift\n";
            break;
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            break;  // handled above with short-circuiting
    }
    push("rax");
}

void CodeGenerator::genExpr(const NodeExpr &expr) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTerm>) {
                genTerm(*node);
            } else if constexpr (std::is_same_v<T, NodeBinExpr>) {
                genBinExpr(*node);
            } else if constexpr (std::is_same_v<T, NodeUnaryExpr>) {
                genUnaryExpr(*node);
            } else if constexpr (std::is_same_v<T, NodeIncDecExpr>) {
                genIncDecExpr(*node);
            }
        },
        expr.var);
}

// ---- Statements ----

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

    if (assign.op == CompoundOp::Assign) {
        genExpr(*assign.expr);
        pop("rax");
        const std::string loc =
            isStatic ? ("[" + v->staticLabel + "]")
                     : ("[rsp + " + std::to_string(stackOffsetOf(assign.name)) +
                        "]");
        _out << "    mov QWORD " << loc << ", rax    ; assign " << assign.name
             << "\n";
        return;
    }

    genExpr(*assign.expr);
    pop("rbx");  // rhs value
    const std::string loc =
        isStatic
            ? ("[" + v->staticLabel + "]")
            : ("[rsp + " + std::to_string(stackOffsetOf(assign.name)) + "]");
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
            break;  // unreachable, handled above
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
                genExpr(*node->expr);
                pop("rdi");
                _out << "    call print_int\n";
            } else if constexpr (std::is_same_v<T, NodeStmtVarDecl>) {
                _out << "    ; declare " << node->name
                     << (node->modifiers.isStatic ? " (sthir)" : "") << "\n";
                if (node->modifiers.isStatic) {
                    // Statics are initialized once, at first declaration,
                    // into their .bss slot rather than the runtime stack.
                    declareVar(node->name, true);
                    genExpr(*node->expr);
                    pop("rax");
                    const Var *v = findVar(node->name);
                    _out << "    mov QWORD [" << v->staticLabel << "], rax\n";
                } else {
                    declareVar(node->name, false);
                    genExpr(*node->expr);
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
                genExpr(*node->expr);  // scrutinee stays on the stack for the
                                       // whole switch
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
                    // Push a *copy* of the scrutinee first, then evaluate
                    // the case value: genExpr uses rax as scratch space, so
                    // loading the scrutinee into rax before calling genExpr
                    // (as this used to do) got it clobbered by the case
                    // value's own codegen, making the comparison always
                    // compare the case value against itself.
                    push("QWORD [rsp + " + std::to_string(offset) + "]",
                         "duplicate switch scrutinee for comparison");
                    genExpr(*c->value.value());
                    pop("rbx");  // case value
                    pop("rax");  // scrutinee copy
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
                    genExpr(*node->expr.value());
                    pop("rax");
                }
                _out << "    mov rsp, rbp\n    pop rbp\n    ret    ; partav\n";
            } else if constexpr (std::is_same_v<T, NodeStmtExprStmt>) {
                genExpr(*node->expr);
                pop("rax", "discard unused expression statement result");
            } else if constexpr (std::is_same_v<T, NodeStmtFuncDecl>) {
                // Function bodies are emitted separately by genFunctions();
                // nothing to do at the point of declaration itself.
            }
        },
        stmt.var);
}

void CodeGenerator::genFuncDecl(const NodeStmtFuncDecl &func) {
    // Save and reset the "current frame" bookkeeping so the function body
    // computes its stack offsets relative to its own frame, not whatever
    // scope was generating code when we got here.
    const auto savedVars = _vars;
    const auto savedMarks = _scopeMarks;
    const auto savedStackSize = _stackSize;
    _vars.clear();
    _scopeMarks.clear();
    _stackSize = 0;

    _out << "func_" << func.name << ":\n";
    _out << "    push rbp\n    mov rbp, rsp\n";

    beginScope();
    for (std::size_t i = 0; i < func.params.size(); ++i) {
        push(kArgRegs[i], "param " + func.params[i]->name);
        _vars.push_back(Var{func.params[i]->name, false, i, {}});
    }
    for (const NodeStmt *stmt : func.body->stmts) { genStmt(*stmt); }
    endScope();

    // Safety net for a function that falls off the end without an
    // explicit `partav` (return value in rax is then whatever was last in
    // it - documented in docs/README.md as a limitation of the current,
    // no-control-flow-analysis backend).
    _out << "    mov rsp, rbp\n    pop rbp\n    ret\n\n";

    _vars = savedVars;
    _scopeMarks = savedMarks;
    _stackSize = savedStackSize;
}

void CodeGenerator::genFunctions() {
    for (const NodeStmt *stmt : _program.stmts) {
        if (std::holds_alternative<NodeStmtFuncDecl *>(stmt->var)) {
            genFuncDecl(*std::get<NodeStmtFuncDecl *>(stmt->var));
        }
    }
}

void CodeGenerator::genEntryPoint() {
    _out << "_start:\n";
    for (const NodeStmt *stmt : _program.stmts) {
        if (!std::holds_alternative<NodeStmtFuncDecl *>(stmt->var)) {
            genStmt(*stmt);
        }
    }
    _out << "    ; implicit program exit (status 0) if execution falls "
            "through\n";
    _out << "    mov rax, 60\n    mov rdi, 0\n    syscall\n\n";
}

void CodeGenerator::emitPrintIntRoutine() {
    _out << "; print_int: writes the signed 64-bit value in rdi to stdout as\n";
    _out << "; decimal ASCII followed by a trailing newline. Clobbers "
            "rax/rbx/rcx/rdx/r8/r9.\n";
    _out << "print_int:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 32\n";
    // Reserve [rbp-1] for the trailing newline byte up front, so the
    // digit/sign buffer below it is contiguous with it in memory: after
    // the digits are written we can print [digit_start .. rbp-1] in one
    // syscall instead of needing a second write for the newline.
    _out << "    mov byte [rbp - 1], 10    ; trailing newline\n";
    _out << "    mov rax, rdi\n";
    _out << "    xor r8, r8\n";
    _out << "    cmp rax, 0\n";
    _out << "    jge .print_int_conv\n";
    _out << "    mov r8, 1\n";
    _out << "    neg rax\n";
    _out << ".print_int_conv:\n";
    _out << "    mov rbx, 10\n";
    _out << "    xor rcx, rcx\n";
    _out << "    lea r9, [rbp - 2]\n";
    _out << ".print_int_digit_loop:\n";
    _out << "    xor rdx, rdx\n";
    _out << "    div rbx\n";
    _out << "    add dl, '0'\n";
    _out << "    mov [r9], dl\n";
    _out << "    dec r9\n";
    _out << "    inc rcx\n";
    _out << "    test rax, rax\n";
    _out << "    jnz .print_int_digit_loop\n";
    _out << "    cmp r8, 0\n";
    _out << "    je .print_int_no_sign\n";
    _out << "    mov byte [r9], '-'\n";
    _out << "    dec r9\n";
    _out << "    inc rcx\n";
    _out << ".print_int_no_sign:\n";
    _out << "    inc r9\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, r9\n";
    _out << "    mov rdx, rcx\n";
    _out << "    inc rdx    ; include the trailing newline byte\n";
    _out << "    syscall\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n";
}

std::string CodeGenerator::generate() {
    _out << "; Generated by the mr compiler - do not edit by hand.\n";
    _out << "global _start\n";
    _out << "section .text\n";
    genEntryPoint();
    genFunctions();
    emitPrintIntRoutine();

    if (!_staticData.str().empty()) {
        _out << "\nsection .bss\n" << _staticData.str();
    }
    return _out.str();
}

}  // namespace mr