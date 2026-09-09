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

}  // namespace mr
