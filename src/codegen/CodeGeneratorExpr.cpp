#include <algorithm>
#include <cstdint>
#include <cstring>

#include "codegen/CodeGenerator.hpp"

namespace mr {

namespace {
const char *kArgRegs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
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

}  // namespace mr
