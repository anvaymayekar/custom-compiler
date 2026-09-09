#include "codegen/CodeGenerator.hpp"

#include <algorithm>
#include <stdexcept>

namespace mr {

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

}  // namespace mr
