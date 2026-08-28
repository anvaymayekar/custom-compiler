#include "lexer/Lexer.hpp"

#include <cctype>
#include <unordered_map>

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

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::IntLiteral:
            return "integer literal";
        case TokenType::FloatLiteral:
            return "float literal";
        case TokenType::StringLiteral:
            return "string literal";
        case TokenType::CharLiteral:
            return "character literal";
        case TokenType::Identifier:
            return "identifier";

        case TokenType::Semicolon:
            return "';'";
        case TokenType::Comma:
            return "','";
        case TokenType::Colon:
            return "':'";
        case TokenType::Dot:
            return "'.'";
        case TokenType::OpenParen:
            return "'('";
        case TokenType::CloseParen:
            return "')'";
        case TokenType::OpenCurly:
            return "'{'";
        case TokenType::CloseCurly:
            return "'}'";
        case TokenType::OpenBracket:
            return "'['";
        case TokenType::CloseBracket:
            return "']'";

        case TokenType::Equal:
            return "'='";
        case TokenType::PlusEqual:
            return "'+='";
        case TokenType::MinusEqual:
            return "'-='";
        case TokenType::StarEqual:
            return "'*='";
        case TokenType::SlashEqual:
            return "'/='";
        case TokenType::PercentEqual:
            return "'%='";
        case TokenType::AmpEqual:
            return "'&='";
        case TokenType::PipeEqual:
            return "'|='";
        case TokenType::CaretEqual:
            return "'^='";
        case TokenType::LessLessEqual:
            return "'<<='";
        case TokenType::GreaterGreaterEqual:
            return "'>>='";

        case TokenType::Plus:
            return "'+'";
        case TokenType::Minus:
            return "'-'";
        case TokenType::Star:
            return "'*'";
        case TokenType::Slash:
            return "'/'";
        case TokenType::Percent:
            return "'%'";
        case TokenType::PlusPlus:
            return "'++'";
        case TokenType::MinusMinus:
            return "'--'";

        case TokenType::EqualEqual:
            return "'=='";
        case TokenType::BangEqual:
            return "'!='";
        case TokenType::Less:
            return "'<'";
        case TokenType::Greater:
            return "'>'";
        case TokenType::LessEqual:
            return "'<='";
        case TokenType::GreaterEqual:
            return "'>='";

        case TokenType::AmpAmp:
            return "'&&'";
        case TokenType::PipePipe:
            return "'||'";
        case TokenType::Bang:
            return "'!'";

        case TokenType::Amp:
            return "'&'";
        case TokenType::Pipe:
            return "'|'";
        case TokenType::Caret:
            return "'^'";
        case TokenType::Tilde:
            return "'~'";
        case TokenType::LessLess:
            return "'<<'";
        case TokenType::GreaterGreater:
            return "'>>'";

        case TokenType::KwAnk:
            return "'ank'";
        case TokenType::KwShevti:
            return "'shevti'";
        case TokenType::KwJar:
            return "'jar'";
        case TokenType::KwNahitar:
            return "'nahitar'";
        case TokenType::KwAnyatha:
            return "'anyatha'";

        case TokenType::KwHe:
            return "'he'";
        case TokenType::KwTe:
            return "'te'";
        case TokenType::KwAhe:
            return "'ahe'";
        case TokenType::KwMaze:
            return "'maze'";
        case TokenType::KwSthir:
            return "'sthir'";
        case TokenType::KwSarve:
            return "'sarve'";
        case TokenType::KwLahan:
            return "'lahan'";
        case TokenType::KwMaha:
            return "'maha'";
        case TokenType::KwUch:
            return "'uch'";
        case TokenType::KwAkshar:
            return "'akshar'";
        case TokenType::KwBhagank:
            return "'bhagank'";
        case TokenType::KwPurnank:
            return "'purnank'";
        case TokenType::KwVidhan:
            return "'vidhan'";
        case TokenType::KwNirank:
            return "'nirank'";
        case TokenType::KwAgyat:
            return "'agyat'";
        case TokenType::KwJovar:
            return "'jovar'";
        case TokenType::KwPratyek:
            return "'pratyek'";
        case TokenType::KwParyay:
            return "'paryay'";
        case TokenType::KwThamba:
            return "'thamba'";
        case TokenType::KwPudhe:
            return "'pudhe'";
        case TokenType::KwPartav:
            return "'partav'";
        case TokenType::KwKarya:
            return "'karya'";
        case TokenType::KwLeeh:
            return "'leeh'";
        case TokenType::KwKhare:
            return "'khare'";
        case TokenType::KwKhote:
            return "'khote'";
        case TokenType::KwAni:
            return "'ani'";
        case TokenType::KwVa:
            return "'va'";
        case TokenType::KwVarg:
            return "'varg'";
        case TokenType::KwRachna:
            return "'rachna'";
        case TokenType::KwNavin:
            return "'navin'";
        case TokenType::KwVishes:
            return "'vishes'";
        case TokenType::KwPrakar:
            return "'prakar'";
        case TokenType::KwPrayatna:
            return "'prayatna'";
        case TokenType::KwApvaad:
            return "'apvaad'";
        case TokenType::KwAyat:
            return "'ayat'";

        case TokenType::EndOfFile:
            return "end of file";
        case TokenType::Invalid:
            return "invalid token";
    }
    return "unknown token";
}

Lexer::Lexer(std::string source, std::string filename, DiagnosticEngine &diags)
    : _source(std::move(source)),
      _filename(std::move(filename)),
      _diags(diags) {
}

bool Lexer::isAtEnd() const {
    return _idx >= _source.size();
}

char Lexer::peek(std::size_t ahead) const {
    const std::size_t i = _idx + ahead;
    return i < _source.size() ? _source[i] : '\0';
}

char Lexer::advance() {
    const char c = _source[_idx++];
    if (c == '\n') {
        _line++;
        _col = 1;
    } else {
        _col++;
    }
    return c;
}

SourceLocation Lexer::here() const {
    return SourceLocation{_filename, _line, _col, _idx};
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        const char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!isAtEnd() && peek() != '\n') { advance(); }
        } else if (c == '/' && peek(1) == '*') {
            advance();
            advance();
            while (!isAtEnd() && !(peek() == '*' && peek(1) == '/')) {
                advance();
            }
            if (!isAtEnd()) {
                advance();
                advance();
            } else {
                _diags.error(DiagCategory::Lexical, here(),
                             "unterminated block comment");
            }
        } else {
            break;
        }
    }
}

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

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            tokens.push_back(Token{TokenType::EndOfFile, here(), std::nullopt});
            break;
        }

        const char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
        } else if (c == '"') {
            tokens.push_back(lexString());
        } else if (c == '\'') {
            tokens.push_back(lexChar());
        } else {
            Token tok = lexPunctuationOrOperator();
            if (tok.type != TokenType::Invalid) { tokens.push_back(tok); }
        }
    }
    return tokens;
}

}  // namespace mr