#include "diagnostics/DiagnosticEngine.hpp"

#include <sstream>
#include <string_view>

namespace mr {

namespace {

std::string_view lineAt(std::string_view source, int lineNumber) {
    int current = 1;
    std::size_t start = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (current == lineNumber) {
            std::size_t end = source.find('\n', start);
            if (end == std::string_view::npos) { end = source.size(); }
            return source.substr(start, end - start);
        }
        if (source[i] == '\n') {
            current++;
            start = i + 1;
        }
    }
    return {};
}

const char *severityLabel(Severity s) {
    switch (s) {
        case Severity::Error:
            return "error";
        case Severity::Warning:
            return "warning";
        case Severity::Note:
            return "note";
    }
    return "note";
}

const char *categoryLabel(DiagCategory c) {
    switch (c) {
        case DiagCategory::Lexical:
            return "lex";
        case DiagCategory::Syntax:
            return "syntax";
        case DiagCategory::Semantic:
            return "semantic";
        case DiagCategory::CodeGen:
            return "codegen";
        case DiagCategory::Driver:
            return "driver";
    }
    return "";
}

}  // namespace

void DiagnosticEngine::printOne(std::ostream &os, const Diagnostic &diag) const {
    os << (diag.loc.isValid() ? diag.loc.file : std::string("<unknown>")) << ':'
       << diag.loc.line << ':' << diag.loc.col << ": " << severityLabel(diag.severity)
       << "[" << categoryLabel(diag.category) << "]: " << diag.message << '\n';

    if (diag.loc.isValid() && !_source.empty()) {
        std::string_view line = lineAt(_source, diag.loc.line);
        if (!line.empty()) {
            std::ostringstream gutter;
            gutter << diag.loc.line;
            const std::string pad(gutter.str().size(), ' ');

            os << " " << pad << " |\n";
            os << " " << diag.loc.line << " | " << line << '\n';
            os << " " << pad << " | ";
            for (int i = 1; i < diag.loc.col; ++i) { os << ' '; }
            os << '^';
            for (int i = 1; i < diag.length; ++i) { os << '~'; }
            os << '\n';
        }
    }

    if (diag.note.has_value()) { os << "  = note: " << diag.note.value() << '\n'; }
    os << '\n';
}

void DiagnosticEngine::printAll(std::ostream &os) const {
    for (const auto &diag : _diagnostics) { printOne(os, diag); }
    if (!_diagnostics.empty()) {
        os << errorCount() << " error(s), "
           << (_diagnostics.size() - errorCount()) << " warning(s)\n";
    }
}

}  // namespace mr
