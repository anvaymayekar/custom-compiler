#include "codegen/CodeGenerator.hpp"

namespace mr {

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

}  // namespace mr
