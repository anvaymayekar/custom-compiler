#pragma once

#include <cstddef>
#include <string>

namespace mr {

// A single point in a source file. Kept intentionally small (it is copied
// into every Token and AST node), but rich enough that diagnostics never
// need to re-derive line/column information after the fact.
struct SourceLocation {
    std::string file;
    int line = 1;
    int col = 1;
    std::size_t offset = 0;  // byte offset into the source buffer

    [[nodiscard]] bool isValid() const noexcept {
        return !file.empty();
    }
};

}  // namespace mr
