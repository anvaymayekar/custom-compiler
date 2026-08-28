#include "parser/Parser.hpp"

namespace mr {

Parser::Parser(std::vector<Token> tokens, std::string filename,
               DiagnosticEngine &diags, Arena &arena)
    : _tokens(std::move(tokens)),
      _filename(std::move(filename)),
      _diags(diags),
      _arena(arena) {
}

const Token &Parser::peek(std::size_t ahead) const {
    std::size_t i = _idx + ahead;
    if (i >= _tokens.size()) { return _tokens.back(); }
    return _tokens[i];
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

Token Parser::advance() {
    const Token tok = peek();
    if (_idx < _tokens.size() - 1) { _idx++; }
    return tok;
}

std::optional<Token> Parser::match(TokenType type) {
    if (check(type)) { return advance(); }
    return std::nullopt;
}

std::optional<Token> Parser::expect(TokenType type) {
    if (check(type)) { return advance(); }
    const Token &got = peek();
    _diags.error(DiagCategory::Syntax, got.loc,
                 "expected " + tokenTypeName(type) + " but found " +
                     tokenTypeName(got.type));
    return std::nullopt;
}

void Parser::synchronize() {
    while (!check(TokenType::EndOfFile)) {
        if (peek().type == TokenType::Semicolon) {
            advance();
            return;
        }
        if (peek().type == TokenType::CloseCurly) { return; }
        advance();
    }
}

bool Parser::isTypeKeyword(TokenType type) {
    switch (type) {
        case TokenType::KwAnk:
        case TokenType::KwAkshar:
        case TokenType::KwBhagank:
        case TokenType::KwPurnank:
        case TokenType::KwVidhan:
        case TokenType::KwNirank:
        case TokenType::KwAgyat:
            return true;
        default:
            return false;
    }
}

bool Parser::isDeclModifierStart(TokenType type) {
    switch (type) {
        case TokenType::KwHe:
        case TokenType::KwTe:
        case TokenType::KwMaze:
        case TokenType::KwSthir:
        case TokenType::KwSarve:
        case TokenType::KwLahan:
        case TokenType::KwMaha:
        case TokenType::KwUch:
            return true;
        default:
            return false;
    }
}

bool Parser::isCompoundAssignOp(TokenType type) {
    switch (type) {
        case TokenType::Equal:
        case TokenType::PlusEqual:
        case TokenType::MinusEqual:
        case TokenType::StarEqual:
        case TokenType::SlashEqual:
        case TokenType::PercentEqual:
        case TokenType::AmpEqual:
        case TokenType::PipeEqual:
        case TokenType::CaretEqual:
        case TokenType::LessLessEqual:
        case TokenType::GreaterGreaterEqual:
            return true;
        default:
            return false;
    }
}

CompoundOp Parser::toCompoundOp(TokenType type) {
    switch (type) {
        case TokenType::PlusEqual:
            return CompoundOp::AddAssign;
        case TokenType::MinusEqual:
            return CompoundOp::SubAssign;
        case TokenType::StarEqual:
            return CompoundOp::MulAssign;
        case TokenType::SlashEqual:
            return CompoundOp::DivAssign;
        case TokenType::PercentEqual:
            return CompoundOp::ModAssign;
        case TokenType::AmpEqual:
            return CompoundOp::AndAssign;
        case TokenType::PipeEqual:
            return CompoundOp::OrAssign;
        case TokenType::CaretEqual:
            return CompoundOp::XorAssign;
        case TokenType::LessLessEqual:
            return CompoundOp::ShlAssign;
        case TokenType::GreaterGreaterEqual:
            return CompoundOp::ShrAssign;
        default:
            return CompoundOp::Assign;
    }
}

std::optional<int> Parser::binaryPrecedence(TokenType type) {
    switch (type) {
        case TokenType::PipePipe:
        case TokenType::KwVa:
            return 0;
        case TokenType::AmpAmp:
        case TokenType::KwAni:
            return 1;
        case TokenType::Pipe:
            return 2;
        case TokenType::Caret:
            return 3;
        case TokenType::Amp:
            return 4;
        case TokenType::EqualEqual:
        case TokenType::BangEqual:
            return 5;
        case TokenType::Less:
        case TokenType::Greater:
        case TokenType::LessEqual:
        case TokenType::GreaterEqual:
            return 6;
        case TokenType::LessLess:
        case TokenType::GreaterGreater:
            return 7;
        case TokenType::Plus:
        case TokenType::Minus:
            return 8;
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent:
            return 9;
        default:
            return std::nullopt;
    }
}

std::optional<BinaryOp> Parser::toBinaryOp(TokenType type) {
    switch (type) {
        case TokenType::Plus:
            return BinaryOp::Add;
        case TokenType::Minus:
            return BinaryOp::Sub;
        case TokenType::Star:
            return BinaryOp::Mul;
        case TokenType::Slash:
            return BinaryOp::Div;
        case TokenType::Percent:
            return BinaryOp::Mod;
        case TokenType::EqualEqual:
            return BinaryOp::Eq;
        case TokenType::BangEqual:
            return BinaryOp::Ne;
        case TokenType::Less:
            return BinaryOp::Lt;
        case TokenType::Greater:
            return BinaryOp::Gt;
        case TokenType::LessEqual:
            return BinaryOp::Le;
        case TokenType::GreaterEqual:
            return BinaryOp::Ge;
        case TokenType::AmpAmp:
        case TokenType::KwAni:
            return BinaryOp::LogicalAnd;
        case TokenType::PipePipe:
        case TokenType::KwVa:
            return BinaryOp::LogicalOr;
        case TokenType::Amp:
            return BinaryOp::BitAnd;
        case TokenType::Pipe:
            return BinaryOp::BitOr;
        case TokenType::Caret:
            return BinaryOp::BitXor;
        case TokenType::LessLess:
            return BinaryOp::Shl;
        case TokenType::GreaterGreater:
            return BinaryOp::Shr;
        default:
            return std::nullopt;
    }
}

std::vector<NodeExpr *> Parser::parseArgList() {
    std::vector<NodeExpr *> args;
    expect(TokenType::OpenParen);
    if (!check(TokenType::CloseParen)) {
        while (true) {
            auto arg = parseExpr();
            if (arg.has_value()) { args.push_back(arg.value()); }
            if (!match(TokenType::Comma).has_value()) { break; }
        }
    }
    expect(TokenType::CloseParen);
    return args;
}

std::optional<NodeTerm *> Parser::parseTerm() {
    if (auto lit = match(TokenType::IntLiteral)) {
        auto *node = _arena.emplace<NodeTermIntLiteral>(
            NodeTermIntLiteral{lit->lexeme.value_or(""), lit->loc});
        return _arena.emplace<NodeTerm>(NodeTerm{node});
    }
    if (auto lit = match(TokenType::FloatLiteral)) {
        auto *node = _arena.emplace<NodeTermFloatLiteral>(
            NodeTermFloatLiteral{lit->lexeme.value_or(""), lit->loc});
        return _arena.emplace<NodeTerm>(NodeTerm{node});
    }
    if (auto lit = match(TokenType::KwKhare)) {
        auto *node = _arena.emplace<NodeTermBoolLiteral>(
            NodeTermBoolLiteral{true, lit->loc});
        return _arena.emplace<NodeTerm>(NodeTerm{node});
    }
    if (auto lit = match(TokenType::KwKhote)) {
        auto *node = _arena.emplace<NodeTermBoolLiteral>(
            NodeTermBoolLiteral{false, lit->loc});
        return _arena.emplace<NodeTerm>(NodeTerm{node});
    }
    if (auto id = match(TokenType::Identifier)) {
        if (check(TokenType::OpenParen)) {
            auto args = parseArgList();
            auto *call = _arena.emplace<NodeCallExpr>(NodeCallExpr{
                id->lexeme.value_or(""), std::move(args), id->loc});
            return _arena.emplace<NodeTerm>(NodeTerm{call});
        }
        auto *node = _arena.emplace<NodeTermIdentifier>(
            NodeTermIdentifier{id->lexeme.value_or(""), id->loc});
        return _arena.emplace<NodeTerm>(NodeTerm{node});
    }
    if (match(TokenType::OpenParen)) {
        auto inner = parseExpr();
        if (!inner.has_value()) {
            _diags.error(DiagCategory::Syntax, peek().loc,
                         "expected an expression inside '(' ')'");
            return std::nullopt;
        }
        if (!expect(TokenType::CloseParen).has_value()) { return std::nullopt; }
        auto *paren =
            _arena.emplace<NodeTermParen>(NodeTermParen{inner.value()});
        return _arena.emplace<NodeTerm>(NodeTerm{paren});
    }
    return std::nullopt;
}

std::optional<NodeExpr *> Parser::parsePostfix() {
    // Postfix ++/-- only apply to a bare identifier, matching the
    // language's "increment/decrement act on variables" semantics.
    if (check(TokenType::Identifier) &&
        (peek(1).type == TokenType::PlusPlus ||
         peek(1).type == TokenType::MinusMinus)) {
        Token name = advance();
        Token op = advance();
        const IncDecOp kind = (op.type == TokenType::PlusPlus)
                                  ? IncDecOp::PostInc
                                  : IncDecOp::PostDec;
        auto *node = _arena.emplace<NodeIncDecExpr>(
            NodeIncDecExpr{kind, name.lexeme.value_or(""), name.loc});
        return _arena.emplace<NodeExpr>(NodeExpr{node});
    }
    auto term = parseTerm();
    if (!term.has_value()) { return std::nullopt; }
    return _arena.emplace<NodeExpr>(NodeExpr{term.value()});
}

std::optional<NodeExpr *> Parser::parseUnary() {
    if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
        Token op = advance();
        auto nameTok = expect(TokenType::Identifier);
        if (!nameTok.has_value()) { return std::nullopt; }
        const IncDecOp kind = (op.type == TokenType::PlusPlus)
                                  ? IncDecOp::PreInc
                                  : IncDecOp::PreDec;
        auto *node = _arena.emplace<NodeIncDecExpr>(
            NodeIncDecExpr{kind, nameTok->lexeme.value_or(""), op.loc});
        return _arena.emplace<NodeExpr>(NodeExpr{node});
    }
    if (check(TokenType::Bang) || check(TokenType::Tilde) ||
        check(TokenType::Minus) || check(TokenType::Plus)) {
        Token op = advance();
        auto operand = parseUnary();
        if (!operand.has_value()) {
            _diags.error(
                DiagCategory::Syntax, peek().loc,
                "expected an expression after " + tokenTypeName(op.type));
            return std::nullopt;
        }
        UnaryOp kind = UnaryOp::Neg;
        switch (op.type) {
            case TokenType::Minus:
                kind = UnaryOp::Neg;
                break;
            case TokenType::Plus:
                kind = UnaryOp::Plus;
                break;
            case TokenType::Bang:
                kind = UnaryOp::LogicalNot;
                break;
            case TokenType::Tilde:
                kind = UnaryOp::BitNot;
                break;
            default:
                break;
        }
        auto *node = _arena.emplace<NodeUnaryExpr>(
            NodeUnaryExpr{kind, operand.value(), op.loc});
        return _arena.emplace<NodeExpr>(NodeExpr{node});
    }
    return parsePostfix();
}

std::optional<NodeExpr *> Parser::parseExpr(int minPrec) {
    auto lhs = parseUnary();
    if (!lhs.has_value()) { return std::nullopt; }

    while (true) {
        const auto prec = binaryPrecedence(peek().type);
        if (!prec.has_value() || prec.value() < minPrec) { break; }

        const Token op = advance();
        auto rhs = parseExpr(prec.value() + 1);
        if (!rhs.has_value()) {
            _diags.error(
                DiagCategory::Syntax, peek().loc,
                "expected an expression after " + tokenTypeName(op.type));
            return std::nullopt;
        }

        auto binOp = toBinaryOp(op.type);
        auto *bin = _arena.emplace<NodeBinExpr>(
            NodeBinExpr{binOp.value(), lhs.value(), rhs.value(), op.loc});
        lhs = _arena.emplace<NodeExpr>(NodeExpr{bin});
    }

    return lhs;
}

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
        match(TokenType::KwHe);  // optional; also the implicit default
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

    if (match(TokenType::KwAhe)) {
        modifiers.isImmutable = true;
    } else if (!expect(TokenType::Equal).has_value()) {
        return std::nullopt;
    }

    auto expr = parseExpr();
    if (!expr.has_value()) {
        _diags.error(DiagCategory::Syntax, peek().loc,
                     "expected an expression in variable declaration");
        return std::nullopt;
    }
    if (!expect(TokenType::Semicolon).has_value()) { return std::nullopt; }

    auto *decl = _arena.emplace<NodeStmtVarDecl>();
    decl->name = nameTok->lexeme.value_or("");
    decl->expr = expr.value();
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
    if (mods.type.base == BaseType::Inferred) {
        return std::nullopt;
    }  // parseModifiers already reported

    if (match(TokenType::KwKarya)) { return parseFuncDeclTail(mods, start); }
    return parseVarDeclTail(mods, start);
}

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
        auto stmt = parseStmt();
        if (stmt.has_value()) {
            program.stmts.push_back(stmt.value());
        } else {
            synchronize();
        }
    }
    return program;
}

}  // namespace mr