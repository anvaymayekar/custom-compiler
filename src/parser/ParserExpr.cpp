#include "parser/Parser.hpp"

namespace mr {

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
    if (auto lit = match(TokenType::StringLiteral)) {
        auto *node = _arena.emplace<NodeTermStringLiteral>(
            NodeTermStringLiteral{lit->lexeme.value_or(""), lit->loc});
        return _arena.emplace<NodeTerm>(NodeTerm{node});
    }
    if (auto lit = match(TokenType::CharLiteral)) {
        auto *node = _arena.emplace<NodeTermCharLiteral>(NodeTermCharLiteral{
            lit->lexeme.value_or(std::string(1, '\0')), lit->loc});
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

}  // namespace mr
