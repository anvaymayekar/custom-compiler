#pragma once

#include <optional>
#include <string>

#include "support/SourceLocation.hpp"

namespace mr {

enum class Severity { Error, Warning, Note };

enum class DiagCategory {
    Lexical,    // e.g. invalid character, malformed literal
    Syntax,     // e.g. missing ';', unexpected token
    Semantic,   // e.g. undeclared identifier, duplicate declaration
    CodeGen,    // internal backend errors (should be rare/never in practice)
    Driver,     // file I/O, assembler/linker invocation, etc.
};

struct Diagnostic {
    Severity severity;
    DiagCategory category;
    std::string message;
    SourceLocation loc;
    std::optional<std::string> note;   // extra "= note: ..." context
    int length = 1;                    // how many columns to underline
};

}  // namespace mr
