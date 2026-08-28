#pragma once

#include <optional>
#include <vector>

#include "ast/Ast.hpp"
#include "diagnostics/DiagnosticEngine.hpp"
#include "lexer/Token.hpp"
#include "support/Arena.hpp"

namespace mr {

// Recursive-descent parser with precedence climbing for expressions.
// Builds an AST and reports syntax errors; does not perform semantic
// checks (see sema/SemanticAnalyzer.hpp).
//
// Error recovery: on a syntax error inside a statement, the parser
// reports a diagnostic and synchronizes to the next statement boundary.
class Parser final {
   public:
    Parser(std::vector<Token> tokens, std::string filename,
           DiagnosticEngine &diags, Arena &arena);

    [[nodiscard]] std::optional<NodeProgram> parseProgram();

   private:
    [[nodiscard]] const Token &peek(std::size_t ahead = 0) const;
    [[nodiscard]] bool check(TokenType type) const;
    Token advance();
    std::optional<Token> match(TokenType type);
    std::optional<Token> expect(TokenType type);
    void synchronize();

    [[nodiscard]] static bool isTypeKeyword(TokenType type);
    [[nodiscard]] static bool isDeclModifierStart(TokenType type);
    [[nodiscard]] static bool isCompoundAssignOp(TokenType type);
    [[nodiscard]] static CompoundOp toCompoundOp(TokenType type);
    [[nodiscard]] static std::optional<int> binaryPrecedence(TokenType type);
    [[nodiscard]] static std::optional<BinaryOp> toBinaryOp(TokenType type);

    // Expressions (lowest to highest precedence via one climbing function)
    std::optional<NodeExpr *> parseExpr(int minPrec = 0);
    std::optional<NodeExpr *> parseUnary();
    std::optional<NodeTerm *> parseTerm();
    std::optional<NodeExpr *> parsePostfix();

    // Statements
    std::optional<NodeStmtScope *> parseScope();
    std::optional<NodeElseChain *> parseElseChain();
    std::optional<NodeStmt *> parseStmt();
    Modifiers parseModifiers();  // consumes he/te/maze/sthir/lahan/maha/uch +
                                 // type keyword
    std::optional<NodeStmt *> parseDeclOrFunc();
    std::optional<NodeStmt *> parseVarDeclTail(Modifiers modifiers,
                                               SourceLocation start);
    std::optional<NodeStmt *> parseFuncDeclTail(Modifiers modifiers,
                                                SourceLocation start);
    std::optional<NodeStmt *> parseExitStmt();
    std::optional<NodeStmt *> parsePrintStmt();
    std::optional<NodeStmt *> parseIfStmt();
    std::optional<NodeStmt *> parseWhileStmt();
    std::optional<NodeStmt *> parseForStmt();
    std::optional<NodeStmt *> parseSwitchStmt();
    std::optional<NodeStmt *> parseReturnStmt();
    std::optional<NodeStmt *>
    parseIdentifierLeadStmt();  // assignment / call / incdec
    std::optional<NodeStmt *> parseAssignTail(Token nameTok);
    std::optional<NodeStmt *> parseForInit();
    std::optional<NodeStmt *> parseForStep();
    std::vector<NodeExpr *> parseArgList();  // consumes '(' args ')'

    std::vector<Token> _tokens;
    std::string _filename;
    DiagnosticEngine &_diags;
    Arena &_arena;
    std::size_t _idx = 0;
};

}  // namespace mr