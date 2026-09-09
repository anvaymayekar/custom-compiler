#include "lexer/Lexer.hpp"

#include <cctype>

namespace mr {

namespace {
const std::unordered_map<std::string, TokenType> &keywordTable() {
    static const std::unordered_map<std::string, TokenType> table = {
        {"ank", TokenType::KwAnk},
        {"shevti", TokenType::KwShevti},
        {"jar", TokenType::KwJar},
        {"nahitar", TokenType::KwNahitar},
        {"anyatha", TokenType::KwAnyatha},

        {"he", TokenType::KwHe},
        {"te", TokenType::KwTe},
        {"ahe", TokenType::KwAhe},
        {"maze", TokenType::KwMaze},
        {"sthir", TokenType::KwSthir},
        {"sarve", TokenType::KwSarve},
        {"lahan", TokenType::KwLahan},
        {"maha", TokenType::KwMaha},
        {"uch", TokenType::KwUch},

        {"akshar", TokenType::KwAkshar},
        {"bhagank", TokenType::KwBhagank},
        {"purnank", TokenType::KwPurnank},
        {"vidhan", TokenType::KwVidhan},
        {"nirank", TokenType::KwNirank},
        {"agyat", TokenType::KwAgyat},

        {"jovar", TokenType::KwJovar},
        {"pratyek", TokenType::KwPratyek},
        {"paryay", TokenType::KwParyay},
        {"thamba", TokenType::KwThamba},
        {"pudhe", TokenType::KwPudhe},
        {"partav", TokenType::KwPartav},

        {"karya", TokenType::KwKarya},
        {"leeh", TokenType::KwLeeh},

        {"khare", TokenType::KwKhare},
        {"khote", TokenType::KwKhote},
        {"ani", TokenType::KwAni},
        {"va", TokenType::KwVa},

        {"varg", TokenType::KwVarg},
        {"rachna", TokenType::KwRachna},
        {"navin", TokenType::KwNavin},
        {"vishes", TokenType::KwVishes},
        {"prakar", TokenType::KwPrakar},

        {"prayatna", TokenType::KwPrayatna},
        {"apvaad", TokenType::KwApvaad},
        {"ayat", TokenType::KwAyat},
    };
    return table;
}
}  // namespace

Token Lexer::lexIdentifierOrKeyword() {
    const SourceLocation start = here();
    std::string text;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) ||
                          peek() == '_')) {
        text.push_back(advance());
    }

    const auto &keywords = keywordTable();
    if (auto it = keywords.find(text); it != keywords.end()) {
        return Token{it->second, start, std::nullopt};
    }
    return Token{TokenType::Identifier, start, text};
}

Token Lexer::lexNumber() {
    const SourceLocation start = here();
    std::string text;
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        text.push_back(advance());
    }
    if (!isAtEnd() && peek() == '.' &&
        std::isdigit(static_cast<unsigned char>(peek(1)))) {
        text.push_back(advance());
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            text.push_back(advance());
        }
        return Token{TokenType::FloatLiteral, start, text};
    }
    return Token{TokenType::IntLiteral, start, text};
}

Token Lexer::lexString() {
    const SourceLocation start = here();
    advance();  // opening '"'
    std::string text;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\' && !isAtEnd()) {
            advance();
            const char esc = advance();
            switch (esc) {
                case 'n':
                    text.push_back('\n');
                    break;
                case 't':
                    text.push_back('\t');
                    break;
                case '\\':
                    text.push_back('\\');
                    break;
                case '"':
                    text.push_back('"');
                    break;
                case '0':
                    text.push_back('\0');
                    break;
                default:
                    text.push_back(esc);
                    break;
            }
        } else {
            text.push_back(advance());
        }
    }
    if (isAtEnd()) {
        _diags.error(DiagCategory::Lexical, start,
                     "unterminated string literal");
    } else {
        advance();  // closing '"'
    }
    return Token{TokenType::StringLiteral, start, text};
}

Token Lexer::lexChar() {
    const SourceLocation start = here();
    advance();  // opening '\''
    std::string text;
    if (!isAtEnd() && peek() == '\\') {
        advance();
        const char esc = advance();
        switch (esc) {
            case 'n':
                text.push_back('\n');
                break;
            case 't':
                text.push_back('\t');
                break;
            case '\\':
                text.push_back('\\');
                break;
            case '\'':
                text.push_back('\'');
                break;
            case '0':
                text.push_back('\0');
                break;
            default:
                text.push_back(esc);
                break;
        }
    } else if (!isAtEnd()) {
        text.push_back(advance());
    }
    if (!isAtEnd() && peek() == '\'') {
        advance();
    } else {
        _diags.error(DiagCategory::Lexical, start,
                     "unterminated character literal");
    }
    return Token{TokenType::CharLiteral, start, text};
}

Token Lexer::lexPunctuationOrOperator() {
    const SourceLocation start = here();
    const char c = advance();

    auto two = [&](char second, TokenType twoType, TokenType oneType) {
        if (peek() == second) {
            advance();
            return Token{twoType, start, std::nullopt};
        }
        return Token{oneType, start, std::nullopt};
    };

    switch (c) {
        case ';':
            return Token{TokenType::Semicolon, start, std::nullopt};
        case ',':
            return Token{TokenType::Comma, start, std::nullopt};
        case ':':
            return Token{TokenType::Colon, start, std::nullopt};
        case '.':
            return Token{TokenType::Dot, start, std::nullopt};
        case '(':
            return Token{TokenType::OpenParen, start, std::nullopt};
        case ')':
            return Token{TokenType::CloseParen, start, std::nullopt};
        case '{':
            return Token{TokenType::OpenCurly, start, std::nullopt};
        case '}':
            return Token{TokenType::CloseCurly, start, std::nullopt};
        case '[':
            return Token{TokenType::OpenBracket, start, std::nullopt};
        case ']':
            return Token{TokenType::CloseBracket, start, std::nullopt};

        case '+':
            if (peek() == '+') {
                advance();
                return Token{TokenType::PlusPlus, start, std::nullopt};
            }
            return two('=', TokenType::PlusEqual, TokenType::Plus);
        case '-':
            if (peek() == '-') {
                advance();
                return Token{TokenType::MinusMinus, start, std::nullopt};
            }
            return two('=', TokenType::MinusEqual, TokenType::Minus);
        case '*':
            return two('=', TokenType::StarEqual, TokenType::Star);
        case '/':
            return two('=', TokenType::SlashEqual, TokenType::Slash);
        case '%':
            return two('=', TokenType::PercentEqual, TokenType::Percent);

        case '=':
            return two('=', TokenType::EqualEqual, TokenType::Equal);
        case '!':
            return two('=', TokenType::BangEqual, TokenType::Bang);
        case '<':
            if (peek() == '<') {
                advance();
                return two('=', TokenType::LessLessEqual, TokenType::LessLess);
            }
            return two('=', TokenType::LessEqual, TokenType::Less);
        case '>':
            if (peek() == '>') {
                advance();
                return two('=', TokenType::GreaterGreaterEqual,
                           TokenType::GreaterGreater);
            }
            return two('=', TokenType::GreaterEqual, TokenType::Greater);

        case '&':
            if (peek() == '&') {
                advance();
                return Token{TokenType::AmpAmp, start, std::nullopt};
            }
            return two('=', TokenType::AmpEqual, TokenType::Amp);
        case '|':
            if (peek() == '|') {
                advance();
                return Token{TokenType::PipePipe, start, std::nullopt};
            }
            return two('=', TokenType::PipeEqual, TokenType::Pipe);
        case '^':
            return two('=', TokenType::CaretEqual, TokenType::Caret);
        case '~':
            return Token{TokenType::Tilde, start, std::nullopt};

        default:
            _diags.error(DiagCategory::Lexical, start,
                         std::string("unexpected character '") + c + "'");
            return Token{TokenType::Invalid, start, std::string(1, c)};
    }
}


}  // namespace mr
