#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ast/Ast.hpp"
#include "codegen/CodeGenerator.hpp"
#include "diagnostics/DiagnosticEngine.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "sema/SemanticAnalyzer.hpp"
#include "support/Arena.hpp"

namespace {

// Every internal stage reports through DiagnosticEngine and never calls
// exit() itself; this is the one place that turns "did the pipeline
// succeed" into a process exit code, per the "no exit() throughout the
// compiler" requirement.
enum class ExitCode : int {
    Success = 0,
    UsageError = 1,
    FileError = 2,
    CompileError = 3,
    AssembleOrLinkError = 4,
};

std::optional<std::string> readFile(const std::string &path) {
    std::ifstream input(path);
    if (!input) { return std::nullopt; }
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

bool writeFile(const std::string &path, const std::string &content) {
    std::ofstream out(path);
    if (!out) { return false; }
    out << content;
    return static_cast<bool>(out);
}

// Runs a shell command and returns true iff it exited with status 0.
bool run(const std::string &command) {
    return std::system(command.c_str()) == 0;
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: compiler <file.mr>\n";
        return static_cast<int>(ExitCode::UsageError);
    }

    const std::string inputPath = argv[1];
    const auto source = readFile(inputPath);
    if (!source.has_value()) {
        std::cerr << "error: failed to open file: " << inputPath << '\n';
        return static_cast<int>(ExitCode::FileError);
    }

    mr::DiagnosticEngine diags(*source);

    // --- Lexing ---
    mr::Lexer lexer(*source, inputPath, diags);
    std::vector<mr::Token> tokens = lexer.tokenize();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return static_cast<int>(ExitCode::CompileError);
    }

    // --- Parsing ---
    mr::Arena arena;
    mr::Parser parser(std::move(tokens), inputPath, diags, arena);
    std::optional<mr::NodeProgram> program = parser.parseProgram();
    if (!program.has_value() || diags.hasErrors()) {
        diags.printAll(std::cerr);
        return static_cast<int>(ExitCode::CompileError);
    }

    // --- Semantic analysis ---
    mr::SemanticAnalyzer sema(diags);
    sema.analyze(*program);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return static_cast<int>(ExitCode::CompileError);
    }
    // Warnings (if any were reported by an earlier stage) are still
    // surfaced even on a successful compile.
    if (!diags.diagnostics().empty()) { diags.printAll(std::cerr); }

    // --- Code generation ---
    mr::CodeGenerator generator(std::move(*program));
    const std::string assembly = generator.generate();

    const std::string asmPath = "out.asm";
    const std::string objPath = "out.o";
    const std::string binPath = "out";

    if (!writeFile(asmPath, assembly)) {
        std::cerr << "error: failed to write " << asmPath << '\n';
        return static_cast<int>(ExitCode::FileError);
    }

    if (!run("nasm -f elf64 " + asmPath + " -o " + objPath)) {
        std::cerr << "error: assembling " << asmPath << " with nasm failed\n";
        return static_cast<int>(ExitCode::AssembleOrLinkError);
    }
    if (!run("ld -o " + binPath + " " + objPath)) {
        std::cerr << "error: linking " << objPath << " failed\n";
        return static_cast<int>(ExitCode::AssembleOrLinkError);
    }

    std::cout << "Successfully assembled -> ./" << binPath << '\n';
    return static_cast<int>(ExitCode::Success);
}
