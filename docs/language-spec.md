# Marathi Programming Language — Full Grammar & Rules (source of truth)

This is the complete intended grammar for the language, as supplied by the
project owner. `docs/README.md` describes the *subset* of this that the
compiler in this repository currently implements — treat any gap between
the two as a roadmap, not a bug.

## Core design

Roman-script Marathi keywords for language concepts; conventional symbols
for operators and punctuation. File extension: `.mr`.

## Declaration ordering

```text
he/te [maze] [sthir] [lahan|maha|uch] type name = expr;
```

- `he` / `te` — singular / collection
- `maze` — private
- `sthir` — static
- `lahan` / `maha` / `uch` — size (small / large / ultra); no keyword = default
- type — `ank`, `akshar`, `bhagank`, `purnank`, `vidhan`, `nirank`, `agyat`

Examples: `he ank x = 5;`, `te ank numbers = [1,2,3];`,
`he maze sthir maha ank count = 0;`.

## Constants

`ahe` makes a declaration immutable: `he ank x ahe 5;` vs. mutable
`he ank x = 5;` (which can later be reassigned with `x = 10;`).

## Functions

```text
[return type] karya(parameters) {
    ...
}
```

`partav` returns a value. Parameters use the same `he`/`te` system as
variables, e.g. `ank karya(he ank a, te bhagank b) { ... }`.

## Control flow

- `jar (cond) { }` — if (parentheses mandatory)
- `nahitar (cond) { }` — else-if
- `anyatha { }` — else
- `jovar (cond) { }` — while
- `pratyek init; cond; step { }` — C-style for
- `thamba;` — break
- `pudhe;` — continue
- `paryay (x) { 1: ...; anyatha: ...; }` — switch

## Operators

Arithmetic `+ - * / %`; comparison `== != < > <= >=`; logical
`&& || !` (word forms `ani`/`va`); bitwise `& | ^ ~ << >>`; assignment
`= += -= *= /= %= &= |= ^= <<= >>=`; increment/decrement `++ --` (prefix
and postfix).

## OOP / modules

`varg` (class), `rachna` (struct), `navin` (new), `vishes` (override),
`prakar` (typeof), `prayatna`/`apvaad` (try/catch), `ayat` (import).

## Print / exit

`leeh(value);` prints. `shevti(value);` exits with that value as the
process exit code.

## Full current keyword set

Declaration/modifiers: `he te ahe maze sthir sarve lahan maha uch`
Types: `ank akshar bhagank purnank vidhan nirank agyat`
Control flow: `jar nahitar anyatha jovar pratyek paryay thamba pudhe partav`
Functions: `karya leeh shevti`
Boolean/logical: `khare khote ani va`
OOP/type-system: `varg rachna navin vishes prakar`
Exceptions/modules: `prayatna apvaad ayat`

## Potential future keywords/features (not yet part of the vocabulary)

Inheritance (e.g. `darja`), enums, `throw`/`finally`, interfaces,
generics, namespaces, pointers/references, pattern matching, additional
access modifiers, operator/function overloading.
