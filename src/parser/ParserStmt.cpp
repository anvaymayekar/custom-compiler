#include "parser/Parser.hpp"

namespace mr {

std::optional<NodeStmt *> Parser::parseExitStmt() {
    const Token kw = advance();
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected an expression inside 'shevti(...)'");
        return std::nullopt;
    }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
    auto *node =
        _arena.emplace<NodeStmtExit>(NodeStmtExit{expr.value(), kw.loc});
    return _arena.emplace<NodeStmt>(NodeStmt{node});
}

std::optional<NodeStmt *> Parser::parsePrintStmt() {
    const Token kw = advance();
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected an expression inside 'leeh(...)'");
        return std::nullopt;
    }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
    auto *node =
        _arena.emplace<NodeStmtPrint>(NodeStmtPrint{expr.value(), kw.loc});
    return _arena.emplace<NodeStmt>(NodeStmt{node});
}

std::optional<NodeStmt *> Parser::parseIfStmt() {
    const Token kw = advance();
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected a condition expression after 'jar ('");
        return std::nullopt;
    }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    auto scope = parseScope();
    if (!scope.has_value()) { return std::nullopt; }
    auto *ifStmt = _arena.emplace<NodeStmtIf>();
    ifStmt->expr = expr.value();
    ifStmt->scope = scope.value();
    ifStmt->loc = kw.loc;
    ifStmt->elseChain = parseElseChain();
    return _arena.emplace<NodeStmt>(NodeStmt{ifStmt});
}

std::optional<NodeStmt *> Parser::parseWhileStmt() {
    const Token kw = advance();
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected a condition expression after 'jovar ('");
        return std::nullopt;
    }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    auto scope = parseScope();
    if (!scope.has_value()) { return std::nullopt; }
    auto *node = _arena.emplace<NodeStmtWhile>(
        NodeStmtWhile{expr.value(), scope.value(), kw.loc});
    return _arena.emplace<NodeStmt>(NodeStmt{node});
}

std::optional<NodeStmt *> Parser::parseAssignTail(Token nameTok) {
    if (!isCompoundAssignOp(peek().type)) {
        _diags.error(
            DiagCategory::Syntax, peek().loc,
            "expected an assignment operator after identifier but found " +
                tokenTypeName(peek().type));
        return std::nullopt;
    }
    Token opTok = advance();
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected an expression after '='");
        return std::nullopt;
    }
    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
    auto *assign = _arena.emplace<NodeStmtAssign>();
    assign->name = nameTok.lexeme.value_or("");
    assign->op = toCompoundOp(opTok.type);
    assign->expr = expr.value();
    assign->nameLoc = nameTok.loc;
    return _arena.emplace<NodeStmt>(NodeStmt{assign});
}

std::optional<NodeStmt *> Parser::parseIdentifierLeadStmt() {
    Token nameTok = advance();
    if (check(TokenType::OpenParen)) {
        auto args = parseArgList();
        if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
        auto *call = _arena.emplace<NodeCallExpr>(NodeCallExpr{
            nameTok.lexeme.value_or(""), std::move(args), nameTok.loc});
        auto *term = _arena.emplace<NodeTerm>(NodeTerm{call});
        auto *expr = _arena.emplace<NodeExpr>(NodeExpr{term});
        auto *stmt = _arena.emplace<NodeStmtExprStmt>(
            NodeStmtExprStmt{expr, nameTok.loc});
        return _arena.emplace<NodeStmt>(NodeStmt{stmt});
    }
    if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
        Token op = advance();
        if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
        const IncDecOp kind = (op.type == TokenType::PlusPlus)
                                  ? IncDecOp::PostInc
                                  : IncDecOp::PostDec;
        auto *node = _arena.emplace<NodeIncDecExpr>(
            NodeIncDecExpr{kind, nameTok.lexeme.value_or(""), nameTok.loc});
        auto *expr = _arena.emplace<NodeExpr>(NodeExpr{node});
        auto *stmt = _arena.emplace<NodeStmtExprStmt>(
            NodeStmtExprStmt{expr, nameTok.loc});
        return _arena.emplace<NodeStmt>(NodeStmt{stmt});
    }
    return parseAssignTail(nameTok);
}

std::optional<NodeStmt *> Parser::parseForInit() {
    if (isDeclModifierStart(peek().type) || isTypeKeyword(peek().type)) {
        return parseDeclOrFunc();
    }
    if (check(TokenType::Identifier)) { return parseIdentifierLeadStmt(); }
    _diags.error(
        DiagCategory::Syntax, peek().loc,
        "expected a variable declaration or assignment in 'pratyek' init");
    return std::nullopt;
}

std::optional<NodeStmt *> Parser::parseForStep() {
    if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
        // Prefix ++i / --i as a 'pratyek' step, e.g. pratyek(...; ...; ++i).
        Token op = advance();
        auto nameTok = expect(TokenType::Identifier);
        if (!nameTok.has_value()) { return std::nullopt; }
        const IncDecOp kind = (op.type == TokenType::PlusPlus)
                                  ? IncDecOp::PreInc
                                  : IncDecOp::PreDec;
        auto *node = _arena.emplace<NodeIncDecExpr>(
            NodeIncDecExpr{kind, nameTok->lexeme.value_or(""), op.loc});
        auto *expr = _arena.emplace<NodeExpr>(NodeExpr{node});
        auto *stmt =
            _arena.emplace<NodeStmtExprStmt>(NodeStmtExprStmt{expr, op.loc});
        return _arena.emplace<NodeStmt>(NodeStmt{stmt});
    }
    auto nameTok = expect(TokenType::Identifier);
    if (!nameTok.has_value()) { return std::nullopt; }
    if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
        Token op = advance();
        const IncDecOp kind = (op.type == TokenType::PlusPlus)
                                  ? IncDecOp::PostInc
                                  : IncDecOp::PostDec;
        auto *node = _arena.emplace<NodeIncDecExpr>(
            NodeIncDecExpr{kind, nameTok->lexeme.value_or(""), nameTok->loc});
        auto *expr = _arena.emplace<NodeExpr>(NodeExpr{node});
        auto *stmt = _arena.emplace<NodeStmtExprStmt>(
            NodeStmtExprStmt{expr, nameTok->loc});
        return _arena.emplace<NodeStmt>(NodeStmt{stmt});
    }
    if (!isCompoundAssignOp(peek().type)) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected '++', '--', or an assignment in 'pratyek' step");
        return std::nullopt;
    }
    Token opTok = advance();
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected an expression in 'pratyek' step");
        return std::nullopt;
    }
    auto *assign = _arena.emplace<NodeStmtAssign>();
    assign->name = nameTok->lexeme.value_or("");
    assign->op = toCompoundOp(opTok.type);
    assign->expr = expr.value();
    assign->nameLoc = nameTok->loc;
    return _arena.emplace<NodeStmt>(NodeStmt{assign});
}

std::optional<NodeStmt *> Parser::parseForStmt() {
    const Token kw = advance();
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    auto init = parseForInit();
    if (!init.has_value()) { return std::nullopt; }
    auto cond = parseExpr();
    if (!cond.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected a condition expression in 'pratyek'");
        return std::nullopt;
    }
    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
    auto step = parseForStep();
    if (!step.has_value()) { return std::nullopt; }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    auto scope = parseScope();
    if (!scope.has_value()) { return std::nullopt; }
    auto *node = _arena.emplace<NodeStmtFor>();
    node->init = init.value();
    node->cond = cond.value();
    node->step = step.value();
    node->scope = scope.value();
    node->loc = kw.loc;
    return _arena.emplace<NodeStmt>(NodeStmt{node});
}

std::optional<NodeStmt *> Parser::parseSwitchStmt() {
    const Token kw = advance();
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected an expression after 'paryay ('");
        return std::nullopt;
    }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    if (!expect(TokenType::OpenCurly).has_value()) { return std::nullopt; }
    std::vector<NodeSwitchCase *> cases;
    while (!check(TokenType::CloseCurly) && !check(TokenType::EndOfFile)) {
        const SourceLocation caseLoc = peek().loc;
        std::optional<NodeExpr *> value;
        if (match(TokenType::KwAnyatha)) {
            value = std::nullopt;
        } else {
            auto v = parseExpr();
            if (!v.has_value()) {
                _diags.error(DiagCategory::Syntax, peek().loc,
                             "expected a case value or 'anyatha'");
                synchronize();
                continue;
            }
            value = v;
        }
        if (!expect(TokenType::Colon).has_value()) {
            synchronize();
            continue;
        }
        std::vector<NodeStmt *> stmts;
        while (!check(TokenType::CloseCurly) && !check(TokenType::EndOfFile) &&
               !check(TokenType::KwAnyatha) &&
               !(check(TokenType::IntLiteral) &&
                 peek(1).type == TokenType::Colon)) {
            auto stmt = parseStmt();
            if (stmt.has_value()) {
                stmts.push_back(stmt.value());
            } else {
                synchronize();
            }
        }
        cases.push_back(_arena.emplace<NodeSwitchCase>(
            NodeSwitchCase{value, std::move(stmts), caseLoc}));
    }
    if (!expect(TokenType::CloseCurly).has_value()) { return std::nullopt; }
    auto *node = _arena.emplace<NodeStmtSwitch>(
        NodeStmtSwitch{expr.value(), std::move(cases), kw.loc});
    return _arena.emplace<NodeStmt>(NodeStmt{node});
}

std::optional<NodeStmt *> Parser::parseReturnStmt() {
    const Token kw = advance();
    std::optional<NodeExpr *> expr;
    if (!check(TokenType::Semicolon)) {
        auto e = parseExpr();
        if (!e.has_value()) {
            _diags.error(DiagCategory::Syntax, peek().loc,
                         "expected an expression after 'partav'");
            return std::nullopt;
        }
        expr = e;
    }
    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }
    auto *node = _arena.emplace<NodeStmtReturn>(NodeStmtReturn{expr, kw.loc});
    return _arena.emplace<NodeStmt>(NodeStmt{node});
}

std::optional<NodeStmt *> Parser::parseStmt() {
    if (isDeclModifierStart(peek().type) || isTypeKeyword(peek().type)) {
        return parseDeclOrFunc();
    }
    switch (peek().type) {
        case TokenType::KwShevti:
            return parseExitStmt();
        case TokenType::KwLeeh:
            return parsePrintStmt();
        case TokenType::KwJar:
            return parseIfStmt();
        case TokenType::KwJovar:
            return parseWhileStmt();
        case TokenType::KwPratyek:
            return parseForStmt();
        case TokenType::KwParyay:
            return parseSwitchStmt();
        case TokenType::KwPartav:
            return parseReturnStmt();
        case TokenType::KwThamba: {
            const Token kw = advance();
            if (!expect(TokenType::Semicolon).has_value()) {
                return std::nullopt;
            }
            auto *node = _arena.emplace<NodeStmtBreak>(NodeStmtBreak{kw.loc});
            return _arena.emplace<NodeStmt>(NodeStmt{node});
        }
        case TokenType::KwPudhe: {
            const Token kw = advance();
            if (!expect(TokenType::Semicolon).has_value()) {
                return std::nullopt;
            }
            auto *node =
                _arena.emplace<NodeStmtContinue>(NodeStmtContinue{kw.loc});
            return _arena.emplace<NodeStmt>(NodeStmt{node});
        }
        case TokenType::OpenCurly: {
            auto scope = parseScope();
            if (!scope.has_value()) { return std::nullopt; }
            return _arena.emplace<NodeStmt>(NodeStmt{scope.value()});
        }
        case TokenType::Identifier:
            return parseIdentifierLeadStmt();
        case TokenType::PlusPlus:
        case TokenType::MinusMinus: {
            // Prefix ++i; / --i; used as a standalone statement. Postfix
            // i++; / i--; is handled inside parseIdentifierLeadStmt since
            // it starts with the identifier instead.
            Token op = advance();
            auto nameTok = expect(TokenType::Identifier);
            if (!nameTok.has_value()) { return std::nullopt; }
            if (!expect(TokenType::Semicolon).has_value()) {
                return std::nullopt;
            }
            const IncDecOp kind = (op.type == TokenType::PlusPlus)
                                      ? IncDecOp::PreInc
                                      : IncDecOp::PreDec;
            auto *node = _arena.emplace<NodeIncDecExpr>(
                NodeIncDecExpr{kind, nameTok->lexeme.value_or(""), op.loc});
            auto *expr = _arena.emplace<NodeExpr>(NodeExpr{node});
            auto *stmt = _arena.emplace<NodeStmtExprStmt>(
                NodeStmtExprStmt{expr, op.loc});
            return _arena.emplace<NodeStmt>(NodeStmt{stmt});
        }
        default:
            _diags.error(
                DiagCategory::Syntax, peek().loc,
                "expected a statement but found " + tokenTypeName(peek().type));
            return std::nullopt;
    }
}

std::optional<NodeProgram> Parser::parseProgram() {
    if (_tokens.empty()) { return std::nullopt; }
    NodeProgram program;
    while (!check(TokenType::EndOfFile)) {
        const std::size_t before = _idx;
        auto stmt = parseStmt();
        if (stmt.has_value()) {
            program.stmts.push_back(stmt.value());
        } else {
            synchronize();
            // synchronize() deliberately stops *without* consuming a
            // CloseCurly, since a nested parseScope() call relies on that
            // to see it and terminate its own loop. At top level there is
            // no such terminator, so a stray '}' here (e.g. from a
            // misparsed construct like a bare `karya` with no leading
            // type) would otherwise leave the cursor stuck and this loop
            // spinning forever, calling _diags.error() until memory is
            // exhausted. Guarantee forward progress unconditionally.
            if (_idx == before) { advance(); }
        }
    }
    return program;
}

}  // namespace mr
