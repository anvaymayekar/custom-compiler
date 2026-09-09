#include "lexer/Lexer.hpp"

namespace mr {

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
}  // namespace mr
