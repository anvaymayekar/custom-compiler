#include "codegen/CodeGenerator.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
    if (it != _vars.crend()) { return &(*it); }
    // Fall back to top-level globals - see the CodeGenerator class
    // comment. Not reached for a name that also exists in _vars (a local
    // or parameter correctly shadows a same-named global, matching
    // SemanticAnalyzer's innermost-scope-first lookup).
    auto git = std::find_if(_globals.crbegin(), _globals.crend(),
                            [&](const Var &v) { return v.name == name; });
    return git != _globals.crend() ? &(*git) : nullptr;
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

void CodeGenerator::declareVar(const std::string &name, bool isStatic,
                               StorageKind kind) {
    if (isStatic) {
        const std::string label = "static_" + std::to_string(_staticCount++);
        _staticData << "    " << label << ": resq 1\n";
        _vars.push_back(Var{name, true, 0, label, kind});
    } else {
        _vars.push_back(Var{name, false, _stackSize, {}, kind});
    }
}

void CodeGenerator::collectFunctionSignatures() {
    for (const NodeStmt *stmt : _program.stmts) {
        if (!std::holds_alternative<NodeStmtFuncDecl *>(stmt->var)) {
            continue;
        }
        const NodeStmtFuncDecl *func = std::get<NodeStmtFuncDecl *>(stmt->var);
        FuncSig sig;
        sig.returnKind = resolveStorageKind(func->modifiers.type);
        for (const NodeParam *param : func->params) {
            sig.paramKinds.push_back(resolveStorageKind(param->modifiers.type));
        }
        _functionSigs[func->name] = std::move(sig);
    }
}

void CodeGenerator::emitKindConversion(const std::string &gpReg,
                                       StorageKind from, StorageKind to) {
    if (from == to) { return; }
    // Only Int/Char/Bool are treated as float-convertible; Str is a
    // pointer and is never auto-converted to/from a float bit pattern.
    auto isFloatConvertible = [](StorageKind k) {
        return k == StorageKind::Int || k == StorageKind::Char ||
               k == StorageKind::Bool;
    };
    if (to == StorageKind::Float && isFloatConvertible(from)) {
        _out << "    cvtsi2sd xmm0, " << gpReg << "    ; int -> bhagank\n";
        _out << "    movq " << gpReg << ", xmm0\n";
    } else if (from == StorageKind::Float && isFloatConvertible(to)) {
        _out << "    movq xmm0, " << gpReg << "\n";
        _out << "    cvttsd2si " << gpReg
             << ", xmm0    ; bhagank -> int (truncated)\n";
    }
}

std::string CodeGenerator::internString(const std::string &text) {
    const std::string label = "str_" + std::to_string(_stringCount++);
    _rodata << "    " << label << ": dq " << text.size() << "\n";
    if (!text.empty()) {
        _rodata << "    db ";
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (i) { _rodata << ", "; }
            _rodata << static_cast<int>(static_cast<unsigned char>(text[i]));
        }
        _rodata << "\n";
    }
    return label;
}

StorageKind CodeGenerator::inferKind(const NodeTerm &term) const {
    StorageKind result = StorageKind::Int;
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTermIntLiteral>) {
                result = StorageKind::Int;
            } else if constexpr (std::is_same_v<T, NodeTermFloatLiteral>) {
                result = StorageKind::Float;
            } else if constexpr (std::is_same_v<T, NodeTermBoolLiteral>) {
                result = StorageKind::Bool;
            } else if constexpr (std::is_same_v<T, NodeTermStringLiteral>) {
                result = StorageKind::Str;
            } else if constexpr (std::is_same_v<T, NodeTermCharLiteral>) {
                result = StorageKind::Char;
            } else if constexpr (std::is_same_v<T, NodeTermIdentifier>) {
                const Var *v = findVar(node->name);
                result = v != nullptr ? v->kind : StorageKind::Int;
            } else if constexpr (std::is_same_v<T, NodeTermParen>) {
                result = inferKind(*node->inner);
            } else if constexpr (std::is_same_v<T, NodeCallExpr>) {
                auto it = _functionSigs.find(node->callee);
                result = (it != _functionSigs.end()) ? it->second.returnKind
                                                     : StorageKind::Int;
            } else if constexpr (std::is_same_v<T, NodeTermTypeOf>) {
                // prakar(...) always evaluates to a printable type-name
                // string.
                result = StorageKind::Str;
            }
        },
        term.var);
    return result;
}

StorageKind CodeGenerator::inferKind(const NodeExpr &expr) const {
    StorageKind result = StorageKind::Int;
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTerm>) {
                result = inferKind(*node);
            } else if constexpr (std::is_same_v<T, NodeBinExpr>) {
                switch (node->op) {
                    case BinaryOp::Add: {
                        const StorageKind lk = inferKind(*node->lhs);
                        const StorageKind rk = inferKind(*node->rhs);
                        if (lk == StorageKind::Str || rk == StorageKind::Str) {
                            // String concatenation - see genBinExpr(). May
                            // mix Str with Char/Int/Bool (converted at
                            // runtime); SemanticAnalyzer rejects anything
                            // else (e.g. Str + Float).
                            result = StorageKind::Str;
                        } else if (lk == StorageKind::Float ||
                                   rk == StorageKind::Float) {
                            result = StorageKind::Float;
                        } else {
                            result = StorageKind::Int;
                        }
                        break;
                    }
                    case BinaryOp::Sub:
                    case BinaryOp::Mul:
                    case BinaryOp::Div:
                    case BinaryOp::Mod: {
                        const StorageKind lk = inferKind(*node->lhs);
                        const StorageKind rk = inferKind(*node->rhs);
                        result = (lk == StorageKind::Float ||
                                  rk == StorageKind::Float)
                                     ? StorageKind::Float
                                     : StorageKind::Int;
                        break;
                    }
                    default:
                        // comparisons/logical/bitwise always yield an
                        // int-like 0/1 result regardless of operand kind.
                        result = StorageKind::Int;
                        break;
                }
            } else if constexpr (std::is_same_v<T, NodeUnaryExpr>) {
                result = (node->op == UnaryOp::Neg || node->op == UnaryOp::Plus)
                             ? inferKind(*node->operand)
                             : StorageKind::Int;
            } else if constexpr (std::is_same_v<T, NodeIncDecExpr>) {
                const Var *v = findVar(node->name);
                result = v != nullptr ? v->kind : StorageKind::Int;
            }
        },
        expr.var);
    return result;
}

void CodeGenerator::genTerm(const NodeTerm &term) {
    std::visit(
        [&](auto *node) {
            using T = std::decay_t<decltype(*node)>;
            if constexpr (std::is_same_v<T, NodeTermIntLiteral>) {
                _out << "    mov rax, " << node->text << "\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermFloatLiteral>) {
                // Encode the literal as the exact IEEE-754 bit pattern of
                // the double, loaded as a 64-bit immediate. The runtime
                // "stack" is just raw 8-byte slots, so pushing these bits
                // is indistinguishable from pushing an int slot - float
                // ops later reinterpret them via `movq` into an xmm reg.
                const double value = std::strtod(node->text.c_str(), nullptr);
                std::uint64_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                _out << "    mov rax, " << bits << "    ; bhagank literal "
                     << node->text << "\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermBoolLiteral>) {
                _out << "    mov rax, " << (node->value ? 1 : 0) << "\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermStringLiteral>) {
                const std::string label = internString(node->text);
                _out << "    mov rax, " << label << "    ; string literal\n";
                push("rax");
            } else if constexpr (std::is_same_v<T, NodeTermCharLiteral>) {
                const unsigned char c =
                    node->text.empty()
                        ? 0
                        : static_cast<unsigned char>(node->text[0]);
                _out << "    mov rax, " << static_cast<int>(c)
                     << "    ; char literal\n";
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
            } else if constexpr (std::is_same_v<T, NodeTermTypeOf>) {
                // prakar(expr): purely a compile-time query over the
                // operand's inferred StorageKind - the operand itself is
                // NOT evaluated at runtime (matching e.g. C++ typeid's
                // treatment of a non-polymorphic operand), so a call with
                // side effects inside prakar(...) will not execute them.
                const StorageKind k = inferKind(*node->operand);
                std::string typeName;
                switch (k) {
                    case StorageKind::Int:
                        typeName = "ank";
                        break;
                    case StorageKind::Float:
                        typeName = "bhagank";
                        break;
                    case StorageKind::Char:
                        typeName = "akshar";
                        break;
                    case StorageKind::Str:
                        typeName = "akshar (te)";
                        break;
                    case StorageKind::Bool:
                        typeName = "khare/khote";
                        break;
                    default:
                        typeName = "agyat";
                        break;
                }
                const std::string label = internString(typeName);
                _out << "    mov rax, " << label << "    ; prakar(...)\n";
                push("rax");
            }
        },
        term.var);
}

void CodeGenerator::genCall(const NodeCallExpr &call) {
    auto sigIt = _functionSigs.find(call.callee);
    const bool haveSig = sigIt != _functionSigs.end();

    for (NodeExpr *arg : call.args) { genExpr(*arg); }
    for (std::size_t i = call.args.size(); i-- > 0;) {
        pop(kArgRegs[i],
            "arg " + std::to_string(i) + " for call to " + call.callee);
        // Convert the argument's actual kind to match the callee's
        // declared parameter kind, e.g. an `ank` literal passed where a
        // `bhagank` parameter is expected - otherwise the callee would
        // reinterpret the raw integer bits as if they were already an
        // IEEE-754 double, producing garbage.
        if (haveSig && i < sigIt->second.paramKinds.size()) {
            const StorageKind argKind = inferKind(*call.args[i]);
            emitKindConversion(kArgRegs[i], argKind,
                               sigIt->second.paramKinds[i]);
        }
    }
    _out << "    call func_" << call.callee << "\n";
    push("rax", "result of " + call.callee + "(...)");
}

void CodeGenerator::genUnaryExpr(const NodeUnaryExpr &un) {
    const StorageKind operandKind = inferKind(*un.operand);
    genExpr(*un.operand);
    pop("rax");
    if (operandKind == StorageKind::Float && un.op == UnaryOp::Neg) {
        // Flip the IEEE-754 sign bit (bit 63) rather than a 2's-complement
        // `neg`, which would corrupt the float's bit pattern.
        _out << "    btc rax, 63    ; negate bhagank (flip sign bit)\n";
    } else {
        switch (un.op) {
            case UnaryOp::Neg:
                _out << "    neg rax\n";
                break;
            case UnaryOp::Plus:
                break;
            case UnaryOp::LogicalNot:
                _out << "    cmp rax, 0\n    sete al\n    movzx rax, al\n";
                break;
            case UnaryOp::BitNot:
                _out << "    not rax\n";
                break;
        }
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

    const bool isArith = (bin.op == BinaryOp::Add || bin.op == BinaryOp::Sub ||
                          bin.op == BinaryOp::Mul || bin.op == BinaryOp::Div);
    const bool isCompare = (bin.op == BinaryOp::Eq || bin.op == BinaryOp::Ne ||
                            bin.op == BinaryOp::Lt || bin.op == BinaryOp::Gt ||
                            bin.op == BinaryOp::Le || bin.op == BinaryOp::Ge);
    const StorageKind lk = inferKind(*bin.lhs);
    const StorageKind rk = inferKind(*bin.rhs);

    if (bin.op == BinaryOp::Add &&
        (lk == StorageKind::Str || rk == StorageKind::Str)) {
        // String concatenation. Either side may be a non-Str value (a
        // char or an int/bool), in which case it's first converted to a
        // fresh length-prefixed string on the runtime str_heap via
        // char_to_str/int_to_str before both sides go to str_concat.
        // SemanticAnalyzer rejects anything else mixed with a string
        // (e.g. Str + bhagank), so only those kinds reach here.
        genExpr(*bin.rhs);
        genExpr(*bin.lhs);
        pop("rax", "lhs value");
        pop("rbx", "rhs value");
        // char_to_str/int_to_str preserve rbx (and r12-r15) internally,
        // so it's safe to hold the still-unconverted rhs value in rbx
        // across the lhs conversion call below, and vice versa.
        if (lk != StorageKind::Str) {
            _out << "    mov rdi, rax\n";
            _out << "    call "
                 << (lk == StorageKind::Char ? "char_to_str" : "int_to_str")
                 << "    ; convert lhs for string concatenation\n";
            _out << "    mov rax, rax\n";
        }
        if (rk != StorageKind::Str) {
            _out << "    mov rdi, rbx\n";
            _out << "    call "
                 << (rk == StorageKind::Char ? "char_to_str" : "int_to_str")
                 << "    ; convert rhs for string concatenation\n";
            _out << "    mov rbx, rax\n";
        }
        _out << "    mov rdi, rax\n";
        _out << "    mov rsi, rbx\n";
        _out << "    call str_concat\n";
        push("rax", "concatenated string");
        return;
    }

    const bool useFloat = (isArith || isCompare) && (lk == StorageKind::Float ||
                                                     rk == StorageKind::Float);

    genExpr(*bin.rhs);
    genExpr(*bin.lhs);
    pop("rax");  // lhs bits
    pop("rbx");  // rhs bits

    if (useFloat) {
        // Reinterpret each side's raw bits as a double if it's already
        // `bhagank`, or convert the integer value if it isn't (int/float
        // mixing, e.g. `2 * 1.5`).
        _out << "    "
             << (lk == StorageKind::Float ? "movq xmm0, rax"
                                          : "cvtsi2sd xmm0, rax")
             << "\n";
        _out << "    "
             << (rk == StorageKind::Float ? "movq xmm1, rbx"
                                          : "cvtsi2sd xmm1, rbx")
             << "\n";
        switch (bin.op) {
            case BinaryOp::Add:
                _out << "    addsd xmm0, xmm1\n";
                break;
            case BinaryOp::Sub:
                _out << "    subsd xmm0, xmm1\n";
                break;
            case BinaryOp::Mul:
                _out << "    mulsd xmm0, xmm1\n";
                break;
            case BinaryOp::Div:
                _out << "    divsd xmm0, xmm1\n";
                break;
            case BinaryOp::Eq:
                // Ordered-equal: NaN must compare unequal to everything,
                // including itself, so a bare `sete` (which ComISD alone
                // doesn't distinguish from "unordered") isn't enough.
                _out << "    comisd xmm0, xmm1\n    sete al\n    setnp dl\n"
                        "    and al, dl\n    movzx rax, al\n";
                break;
            case BinaryOp::Ne:
                _out << "    comisd xmm0, xmm1\n    setne al\n    setp dl\n"
                        "    or al, dl\n    movzx rax, al\n";
                break;
            case BinaryOp::Lt:
                _out << "    comisd xmm1, xmm0\n    seta al\n    movzx rax, "
                        "al\n";
                break;
            case BinaryOp::Gt:
                _out << "    comisd xmm0, xmm1\n    seta al\n    movzx rax, "
                        "al\n";
                break;
            case BinaryOp::Le:
                _out << "    comisd xmm1, xmm0\n    setae al\n    movzx rax, "
                        "al\n";
                break;
            case BinaryOp::Ge:
                _out << "    comisd xmm0, xmm1\n    setae al\n    movzx rax, "
                        "al\n";
                break;
            default:
                break;
        }
        if (isArith) { _out << "    movq rax, xmm0\n"; }
        push("rax");
        return;
    }

    switch (bin.op) {
        case BinaryOp::Add:
            _out << "    add rax, rbx\n";
            break;
        case BinaryOp::Sub:
            _out << "    sub rax, rbx\n";
            break;
        case BinaryOp::Mul:
            _out << "    imul rax, rbx\n";
            break;
        case BinaryOp::Div:
            _out << "    cqo\n    idiv rbx\n";
            break;
        case BinaryOp::Mod:
            _out << "    cqo\n    idiv rbx\n    mov rax, rdx\n";
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
            _out << "    mov rcx, rbx\n    sar rax, cl\n";
            break;
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            break;
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

void CodeGenerator::genFuncDecl(const NodeStmtFuncDecl &func) {
    const auto savedVars = _vars;
    const auto savedMarks = _scopeMarks;
    const auto savedStackSize = _stackSize;
    const StorageKind savedReturnKind = _currentFuncReturnKind;
    _vars.clear();
    _scopeMarks.clear();
    _stackSize = 0;
    _currentFuncReturnKind = resolveStorageKind(func.modifiers.type);
    _inFunction = true;

    _out << "func_" << func.name << ":\n";
    _out << "    push rbp\n    mov rbp, rsp\n";
    beginScope();
    for (std::size_t i = 0; i < func.params.size(); ++i) {
        push(kArgRegs[i], "param " + func.params[i]->name);
        _vars.push_back(
            Var{func.params[i]->name,
                false,
                i,
                {},
                resolveStorageKind(func.params[i]->modifiers.type)});
    }
    for (const NodeStmt *stmt : func.body->stmts) { genStmt(*stmt); }
    endScope();
    _out << "    mov rsp, rbp\n    pop rbp\n    ret\n\n";

    _inFunction = false;
    _vars = savedVars;
    _scopeMarks = savedMarks;
    _stackSize = savedStackSize;
    _currentFuncReturnKind = savedReturnKind;
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
    _out << "; decimal ASCII followed by a trailing newline.\n";
    _out << "print_int:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 32\n";
    _out << "    mov byte [rbp - 1], 10\n";
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
    _out << "    inc rdx\n";
    _out << "    syscall\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n";
}

void CodeGenerator::emitPrintStrRoutine() {
    _out
        << "; print_str: writes the length-prefixed string pointed to by rdi\n";
    _out
        << "; ([rdi] = 8-byte length, rdi+8.. = raw bytes) to stdout, then a\n";
    _out << "; trailing newline.\n";
    _out << "print_str:\n";
    _out << "    mov rdx, [rdi]\n";
    _out << "    lea rsi, [rdi + 8]\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    syscall\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, nl_byte\n";
    _out << "    mov rdx, 1\n";
    _out << "    syscall\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitPrintCharRoutine() {
    _out << "; print_char: writes the single byte in the low 8 bits of rdi\n";
    _out << "; to stdout, then a trailing newline.\n";
    _out << "print_char:\n";
    _out << "    push rdi\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, rsp\n";
    _out << "    mov rdx, 1\n";
    _out << "    syscall\n";
    _out << "    add rsp, 8\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, nl_byte\n";
    _out << "    mov rdx, 1\n";
    _out << "    syscall\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitPrintFloatRoutine() {
    _out << "; print_float: writes the IEEE-754 double whose bit pattern is\n";
    _out << "; in rdi to stdout as \"[-]digits.dddddd\" (fixed 6 fractional\n";
    _out
        << "; digits, like printf \"%.6f\") followed by a newline. This is a\n";
    _out << "; fixed-precision formatter, not a general dtoa; assumes the\n";
    _out << "; magnitude fits in an int64 (matches the rest of this "
            "int64-only\n";
    _out << "; backend) and does not special-case NaN/Infinity.\n";
    _out << "print_float:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 96\n";
    _out << "    lea r12, [rbp - 96]\n";
    _out << "    mov r15, r12\n";
    _out << "    mov rax, rdi\n";
    _out << "    movq xmm0, rax\n";
    _out << "    xorpd xmm1, xmm1\n";
    _out << "    xor r8, r8\n";
    _out << "    comisd xmm0, xmm1\n";
    _out << "    jae .pf_pos\n";
    _out << "    mov r8, 1\n";
    _out << "    btc rax, 63\n";
    _out << "    movq xmm0, rax\n";
    _out << ".pf_pos:\n";
    _out << "    cvttsd2si r10, xmm0\n";
    _out << "    cvtsi2sd xmm2, r10\n";
    _out << "    subsd xmm0, xmm2\n";
    _out << "    mov rax, 1000000\n";
    _out << "    cvtsi2sd xmm3, rax\n";
    _out << "    mulsd xmm0, xmm3\n";
    _out << "    mov rax, 0x3FE0000000000000\n";
    _out << "    movq xmm4, rax\n";
    _out << "    addsd xmm0, xmm4\n";
    _out << "    cvttsd2si r13, xmm0\n";
    _out << "    cmp r13, 1000000\n";
    _out << "    jl .pf_no_carry\n";
    _out << "    sub r13, 1000000\n";
    _out << "    inc r10\n";
    _out << ".pf_no_carry:\n";
    _out << "    cmp r8, 0\n";
    _out << "    je .pf_write_int\n";
    _out << "    mov byte [r12], '-'\n";
    _out << "    inc r12\n";
    _out << ".pf_write_int:\n";
    _out << "    lea r9, [rbp - 32]\n";
    _out << "    mov rax, r10\n";
    _out << "    mov rbx, 10\n";
    _out << "    xor rcx, rcx\n";
    _out << ".pf_int_digit_loop:\n";
    _out << "    xor rdx, rdx\n";
    _out << "    div rbx\n";
    _out << "    add dl, '0'\n";
    _out << "    dec r9\n";
    _out << "    mov [r9], dl\n";
    _out << "    inc rcx\n";
    _out << "    test rax, rax\n";
    _out << "    jnz .pf_int_digit_loop\n";
    _out << ".pf_copy_int:\n";
    _out << "    test rcx, rcx\n";
    _out << "    jz .pf_copy_int_done\n";
    _out << "    mov al, [r9]\n";
    _out << "    mov [r12], al\n";
    _out << "    inc r9\n";
    _out << "    inc r12\n";
    _out << "    dec rcx\n";
    _out << "    jmp .pf_copy_int\n";
    _out << ".pf_copy_int_done:\n";
    _out << "    mov byte [r12], '.'\n";
    _out << "    inc r12\n";
    for (const char *divisor : {"100000", "10000", "1000", "100", "10"}) {
        _out << "    mov rax, r13\n";
        _out << "    xor rdx, rdx\n";
        _out << "    mov rbx, " << divisor << "\n";
        _out << "    div rbx\n";
        _out << "    add al, '0'\n";
        _out << "    mov [r12], al\n";
        _out << "    inc r12\n";
        _out << "    mov r13, rdx\n";
    }
    _out << "    mov rax, r13\n";
    _out << "    add al, '0'\n";
    _out << "    mov [r12], al\n";
    _out << "    inc r12\n";
    _out << "    mov byte [r12], 10\n";
    _out << "    inc r12\n";
    _out << "    mov rdx, r12\n";
    _out << "    sub rdx, r15\n";
    _out << "    mov rax, 1\n";
    _out << "    mov rdi, 1\n";
    _out << "    mov rsi, r15\n";
    _out << "    syscall\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitStrConcatRoutine() {
    _out
        << "; str_concat: concatenates two length-prefixed strings (rdi = A,\n";
    _out << "; rsi = B, each pointer -> 8-byte length then raw bytes) into a\n";
    _out << "; fresh region of the bump-allocated str_heap and returns a\n";
    _out << "; pointer to the new length-prefixed string in rax. The heap is\n";
    _out << "; never freed - fine for short-lived programs, not for anything\n";
    _out << "; long-running or concatenation-heavy.\n";
    _out << "str_concat:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    push rbx\n";
    _out << "    push r12\n";
    _out << "    push r13\n";
    _out << "    push r14\n";
    _out << "    mov r12, rdi\n";
    _out << "    mov r13, rsi\n";
    _out << "    mov r8, [r12]\n";
    _out << "    mov r9, [r13]\n";
    _out << "    lea r10, [r8 + r9]\n";
    _out << "    mov rax, [str_heap_offset]\n";
    _out << "    lea r14, [str_heap + rax]\n";
    _out << "    mov [r14], r10\n";
    _out << "    lea rbx, [r10 + 8 + 7]\n";
    _out << "    and rbx, -8\n";
    _out << "    add rax, rbx\n";
    _out << "    mov [str_heap_offset], rax\n";
    _out << "    lea rsi, [r12 + 8]\n";
    _out << "    lea rdi, [r14 + 8]\n";
    _out << "    mov rcx, r8\n";
    _out << "    rep movsb\n";
    _out << "    lea rsi, [r13 + 8]\n";
    _out << "    lea rdi, [r14 + r8 + 8]\n";
    _out << "    mov rcx, r9\n";
    _out << "    rep movsb\n";
    _out << "    mov rax, r14\n";
    _out << "    pop r14\n";
    _out << "    pop r13\n";
    _out << "    pop r12\n";
    _out << "    pop rbx\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitCharToStrRoutine() {
    _out << "; char_to_str: allocates a fresh 1-byte length-prefixed string\n";
    _out << "; on str_heap holding the single byte in the low 8 bits of rdi,\n";
    _out << "; and returns a pointer to it in rax. Does not touch rbx or\n";
    _out << "; r12-r15, so a caller may hold a pending value in one of those\n";
    _out << "; across this call (see genBinExpr's string-concat handling).\n";
    _out << "char_to_str:\n";
    _out << "    mov rax, [str_heap_offset]\n";
    _out << "    lea rdx, [str_heap + rax]\n";
    _out << "    mov qword [rdx], 1\n";
    _out << "    mov [rdx + 8], dil\n";
    _out << "    add rax, 16\n";
    _out << "    mov [str_heap_offset], rax\n";
    _out << "    mov rax, rdx\n";
    _out << "    ret\n\n";
}

void CodeGenerator::emitIntToStrRoutine() {
    _out << "; int_to_str: allocates a fresh length-prefixed decimal string\n";
    _out << "; on str_heap representing the signed 64-bit value in rdi, and\n";
    _out << "; returns a pointer to it in rax. Preserves rbx (uses r12/r13\n";
    _out << "; internally but saves/restores them), so a caller may hold a\n";
    _out << "; pending value in rbx across this call.\n";
    _out << "int_to_str:\n";
    _out << "    push rbp\n";
    _out << "    mov rbp, rsp\n";
    _out << "    sub rsp, 32\n";
    _out << "    push r12\n";
    _out << "    push r13\n";
    _out << "    mov rax, rdi\n";
    _out << "    xor r8, r8\n";
    _out << "    cmp rax, 0\n";
    _out << "    jge .its_conv\n";
    _out << "    mov r8, 1\n";
    _out << "    neg rax\n";
    _out << ".its_conv:\n";
    _out << "    mov r9, 10\n";
    _out << "    xor r12, r12\n";
    _out << "    lea r13, [rbp - 1]\n";
    _out << ".its_digit_loop:\n";
    _out << "    xor rdx, rdx\n";
    _out << "    div r9\n";
    _out << "    add dl, '0'\n";
    _out << "    dec r13\n";
    _out << "    mov [r13], dl\n";
    _out << "    inc r12\n";
    _out << "    test rax, rax\n";
    _out << "    jnz .its_digit_loop\n";
    _out << "    cmp r8, 0\n";
    _out << "    je .its_no_sign\n";
    _out << "    dec r13\n";
    _out << "    mov byte [r13], '-'\n";
    _out << "    inc r12\n";
    _out << ".its_no_sign:\n";
    _out << "    mov rax, [str_heap_offset]\n";
    _out << "    lea r10, [str_heap + rax]\n";
    _out << "    mov [r10], r12\n";
    _out << "    lea rdi, [r10 + 8]\n";
    _out << "    mov rsi, r13\n";
    _out << "    mov rcx, r12\n";
    _out << "    rep movsb\n";
    _out << "    lea rax, [r12 + 8 + 7]\n";
    _out << "    and rax, -8\n";
    _out << "    add rax, [str_heap_offset]\n";
    _out << "    mov [str_heap_offset], rax\n";
    _out << "    mov rax, r10\n";
    _out << "    pop r13\n";
    _out << "    pop r12\n";
    _out << "    mov rsp, rbp\n";
    _out << "    pop rbp\n";
    _out << "    ret\n\n";
}

std::string CodeGenerator::generate() {
    collectFunctionSignatures();
    _out << "default abs    ; silences NASM's implicit-default-ABS deprecation "
            "warning\n";
    _out << "; Generated by the mr compiler - do not edit by hand.\n";
    _out << "global _start\n";
    _out << "section .text\n";
    genEntryPoint();
    genFunctions();
    emitPrintIntRoutine();
    emitPrintStrRoutine();
    emitPrintCharRoutine();
    emitPrintFloatRoutine();
    emitStrConcatRoutine();
    emitCharToStrRoutine();
    emitIntToStrRoutine();

    _out << "\nsection .rodata\n";
    _out << "    nl_byte: db 10\n";
    _out << _rodata.str();

    _out << "\nsection .bss\n";
    _out << "    str_heap: resb 65536    ; bump-allocated runtime string "
            "arena, see str_concat\n";
    _out << "    str_heap_offset: resq 1\n";
    _out << _staticData.str();
    return _out.str();
}

}  // namespace mr