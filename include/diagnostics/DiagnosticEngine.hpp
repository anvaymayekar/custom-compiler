#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "diagnostics/Diagnostic.hpp"

namespace mr {

// Central sink for every diagnostic produced anywhere in the pipeline
// (lexer, parser, semantic analysis, codegen, driver). Nothing in the
// compiler calls std::cerr or exit() directly any more - components take a
// DiagnosticEngine& and report through it, then the top-level driver
// decides what to do once a stage finishes.
class DiagnosticEngine final {
   public:
    // `source` is the full text of the file being compiled; it is used to
    // print the offending source line under each diagnostic. It may be
    // left empty if that context isn't available (e.g. in unit tests).
    explicit DiagnosticEngine(std::string source = {}) : _source(std::move(source)) {
    }

    void report(Diagnostic diag) {
        if (diag.severity == Severity::Error) { _hadError = true; }
        _diagnostics.push_back(std::move(diag));
    }

    void error(DiagCategory category, SourceLocation loc, std::string message,
               std::optional<std::string> note = std::nullopt, int length = 1) {
        report(Diagnostic{Severity::Error, category, std::move(message), std::move(loc),
                          std::move(note), length});
    }

    void warning(DiagCategory category, SourceLocation loc, std::string message,
                 std::optional<std::string> note = std::nullopt, int length = 1) {
        report(Diagnostic{Severity::Warning, category, std::move(message), std::move(loc),
                          std::move(note), length});
    }

    [[nodiscard]] bool hasErrors() const noexcept {
        return _hadError;
    }
    [[nodiscard]] const std::vector<Diagnostic> &diagnostics() const noexcept {
        return _diagnostics;
    }
    [[nodiscard]] std::size_t errorCount() const noexcept {
        std::size_t n = 0;
        for (const auto &d : _diagnostics) { n += d.severity == Severity::Error; }
        return n;
    }

    // Pretty-prints every collected diagnostic (in the order reported) to
    // `os`, in a compact "file:line:col: error: message" + source-snippet
    // style similar to gcc/clang/rustc.
    void printAll(std::ostream &os) const;

   private:
    void printOne(std::ostream &os, const Diagnostic &diag) const;

    std::string _source;
    std::vector<Diagnostic> _diagnostics;
    bool _hadError = false;
};

}  // namespace mr
