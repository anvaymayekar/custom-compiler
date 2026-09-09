#include "parser/Parser.hpp"

namespace mr {

std::optional<NodeStmtScope *> Parser::parseScope() {
    if (!expect(TokenType::OpenCurly).has_value()) { return std::nullopt; }
    auto *scope = _arena.emplace<NodeStmtScope>();
    while (!check(TokenType::CloseCurly) && !check(TokenType::EndOfFile)) {
        auto stmt = parseStmt();
        if (stmt.has_value()) {
            scope->stmts.push_back(stmt.value());
        } else {
            synchronize();
        }
    }
    if (!expect(TokenType::CloseCurly).has_value()) { return std::nullopt; }
    return scope;
}

std::optional<NodeElseChain *> Parser::parseElseChain() {
    if (auto kw = match(TokenType::KwNahitar)) {
        if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
        auto expr = parseExpr();
        if (!expr.has_value()) {
            _diags.error(DiagCategory::Syntax, peek().loc,
                         "expected a condition expression after 'nahitar ('");
            return std::nullopt;
        }
        if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
        auto scope = parseScope();
        if (!scope.has_value()) { return std::nullopt; }
        auto *elseIf = _arena.emplace<NodeElseIf>();
        elseIf->expr = expr.value();
        elseIf->scope = scope.value();
        elseIf->loc = kw->loc;
        elseIf->next = parseElseChain();
        return _arena.emplace<NodeElseChain>(NodeElseChain{elseIf});
    }
    if (match(TokenType::KwAnyatha)) {
        auto scope = parseScope();
        if (!scope.has_value()) { return std::nullopt; }
        auto *elseNode = _arena.emplace<NodeElse>(NodeElse{scope.value()});
        return _arena.emplace<NodeElseChain>(NodeElseChain{elseNode});
    }
    return std::nullopt;
}

Modifiers Parser::parseModifiers() {
    Modifiers mods;
    if (match(TokenType::KwTe)) {
        mods.type.isCollection = true;
    } else {
        match(TokenType::KwHe);
    }
    if (match(TokenType::KwMaze)) { mods.isPrivate = true; }
    if (match(TokenType::KwSthir)) { mods.isStatic = true; }
    if (match(TokenType::KwSarve)) { mods.isAll = true; }
    if (match(TokenType::KwLahan)) {
        mods.type.size = SizeQualifier::Lahan;
    } else if (match(TokenType::KwMaha)) {
        mods.type.size = SizeQualifier::Maha;
    } else if (match(TokenType::KwUch)) {
        mods.type.size = SizeQualifier::Uch;
    }
    switch (peek().type) {
        case TokenType::KwAnk:
            mods.type.base = BaseType::Ank;
            advance();
            break;
        case TokenType::KwAkshar:
            mods.type.base = BaseType::Akshar;
            advance();
            break;
        case TokenType::KwBhagank:
            mods.type.base = BaseType::Bhagank;
            advance();
            break;
        case TokenType::KwPurnank:
            mods.type.base = BaseType::Purnank;
            advance();
            break;
        case TokenType::KwVidhan:
            mods.type.base = BaseType::Vidhan;
            advance();
            break;
        case TokenType::KwNirank:
            mods.type.base = BaseType::Nirank;
            advance();
            break;
        case TokenType::KwAgyat:
            mods.type.base = BaseType::Agyat;
            advance();
            break;
        default:
            _diags.error(
                DiagCategory::Syntax, peek().loc,
                "expected a type (ank/akshar/bhagank/purnank/vidhan/nirank) "
                "but found " +
                    tokenTypeName(peek().type));
            mods.type.base = BaseType::Inferred;
            break;
    }
    return mods;
}

std::optional<NodeStmt *> Parser::parseVarDeclTail(Modifiers modifiers,
                                                   SourceLocation start) {
    auto nameTok = expect(TokenType::Identifier);
    if (!nameTok.has_value()) { return std::nullopt; }

    std::optional<NodeExpr *> expr;
    if (match(TokenType::KwAhe)) {
        modifiers.isImmutable = true;
        auto e = parseExpr();
        if (!e.has_value()) {
            _diags.error(DiagCategory::Syntax, peek().loc,
                         "expected an expression after 'ahe'");
            return std::nullopt;
        }
        expr = e;
    } else if (match(TokenType::Equal)) {
        auto e = parseExpr();
        if (!e.has_value()) {
            _diags.error(DiagCategory::Syntax, peek().loc,
                         "expected an expression in variable declaration");
            return std::nullopt;
        }
        expr = e;
    } else if (!check(TokenType::Semicolon)) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected '=', 'ahe', or ';' after declaration name");
        return std::nullopt;
    }
    // else: bare `;` -> forward declaration, expr stays nullopt.

    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }

    auto *decl = _arena.emplace<NodeStmtVarDecl>();
    decl->name = nameTok->lexeme.value_or("");
    decl->expr = expr;
    decl->modifiers = modifiers;
    decl->loc = start;
    decl->nameLoc = nameTok->loc;
    return _arena.emplace<NodeStmt>(NodeStmt{decl});
}

std::optional<NodeStmt *> Parser::parseFuncDeclTail(Modifiers modifiers,
                                                    SourceLocation start) {
    auto nameTok = expect(TokenType::Identifier);
    if (!nameTok.has_value()) { return std::nullopt; }
    if (!expect(TokenType::OpenParen).has_value()) { return std::nullopt; }
    std::vector<NodeParam *> params;
    if (!check(TokenType::CloseParen)) {
        while (true) {
            SourceLocation pStart = peek().loc;
            Modifiers pMods = parseModifiers();
            auto pName = expect(TokenType::Identifier);
            if (!pName.has_value()) { return std::nullopt; }
            params.push_back(_arena.emplace<NodeParam>(
                NodeParam{pName->lexeme.value_or(""), pMods, pStart}));
            if (!match(TokenType::Comma).has_value()) { break; }
        }
    }
    if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
    auto body = parseScope();
    if (!body.has_value()) { return std::nullopt; }
    auto *func = _arena.emplace<NodeStmtFuncDecl>();
    func->name = nameTok->lexeme.value_or("");
    func->params = std::move(params);
    func->modifiers = modifiers;
    func->body = body.value();
    func->loc = start;
    return _arena.emplace<NodeStmt>(NodeStmt{func});
}

std::optional<NodeStmt *> Parser::parseDeclOrFunc() {
    const SourceLocation start = peek().loc;
    Modifiers mods = parseModifiers();
    if (mods.type.base == BaseType::Inferred) { return std::nullopt; }
    if (match(TokenType::KwKarya)) { return parseFuncDeclTail(mods, start); }
    return parseVarDeclTail(mods, start);
}

}  // namespace mr
