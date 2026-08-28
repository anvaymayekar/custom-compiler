#pragma once

#include <optional>
#include <string>

#include "support/SourceLocation.hpp"

namespace mr {

// The complete token vocabulary, covering every keyword and operator in
// the language specification (docs/language-spec.md). See docs/README.md
// for exactly which of these the parser currently builds grammar around
// vs. which are still reserved (Tier 2: structs/classes/exceptions/imports).
enum class TokenType {
    // literals / identifiers
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,
    Identifier,

    // punctuation
    Semicolon,
    Comma,
    Colon,
    Dot,
    OpenParen,
    CloseParen,
    OpenCurly,
    CloseCurly,
    OpenBracket,
    CloseBracket,

    // assignment
    Equal,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,
    AmpEqual,
    PipeEqual,
    CaretEqual,
    LessLessEqual,
    GreaterGreaterEqual,

    // arithmetic
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    PlusPlus,
    MinusMinus,

    // comparison
    EqualEqual,
    BangEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,

    // logical
    AmpAmp,
    PipePipe,
    Bang,

    // bitwise
    Amp,
    Pipe,
    Caret,
    Tilde,
    LessLess,
    GreaterGreater,

    // --- currently implemented keywords ---
    KwAnk,
    KwShevti,
    KwJar,
    KwNahitar,
    KwAnyatha,

    // --- newly wired keywords (loops, functions, types, modifiers) ---
    KwHe,
    KwTe,
    KwAhe,
    KwMaze,
    KwSthir,
    KwSarve,
    KwLahan,
    KwMaha,
    KwUch,
    KwAkshar,
    KwBhagank,
    KwPurnank,
    KwVidhan,
    KwNirank,
    KwAgyat,
    KwJovar,
    KwPratyek,
    KwParyay,
    KwThamba,
    KwPudhe,
    KwPartav,
    KwKarya,
    KwLeeh,
    KwKhare,
    KwKhote,
    KwAni,
    KwVa,

    // --- reserved: structs/classes/exceptions/imports (Tier 2, see docs) ---
    KwVarg,
    KwRachna,
    KwNavin,
    KwVishes,
    KwPrakar,
    KwPrayatna,
    KwApvaad,
    KwAyat,

    EndOfFile,
    Invalid,
};

struct Token {
    TokenType type;
    SourceLocation loc;
    std::optional<std::string> lexeme;  // raw text for identifiers/literals

    [[nodiscard]] bool is(TokenType t) const noexcept {
        return type == t;
    }
};

// Human-readable name for diagnostics (e.g. "';'", "identifier").
[[nodiscard]] std::string tokenTypeName(TokenType type);

}  // namespace mr