# mr — a Marathi programming language compiler

A small compiler for a Roman-script Marathi programming language, targeting
Linux x86-64 (NASM). This is a refactor of an earlier single-file
prototype into a proper multi-stage compiler with real diagnostics.

## Building

Requires a C++20 compiler, CMake ≥ 3.20, and — to actually assemble/link
output — `nasm` and `ld` on your `PATH`.

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

This produces:
- `build/compiler` — the CLI
- `build/tests/mr_tests` — the test suite (also runnable via `ctest`)

Debug build: `cmake .. -DCMAKE_BUILD_TYPE=Debug`.

## Running

```sh
./build/compiler examples/canonical.mr
./out
echo $?
```

`compiler <file.mr>` lexes, parses, and semantically analyzes the file,
reporting every diagnostic it finds. If the program is valid, it emits
`out.asm`, assembles it with `nasm -f elf64`, and links it with `ld`,
producing an executable named `out` in the current directory.

Exit codes from the driver itself (not the compiled program):
| Code | Meaning |
|------|---------|
| 0 | success |
| 1 | usage error (wrong number of CLI args) |
| 2 | couldn't read the input file / write `out.asm` |
| 3 | lexical, syntax, or semantic errors in the `.mr` source |
| 4 | `nasm` or `ld` failed |

## Testing

```sh
cd build
ctest --output-on-failure
# or directly:
./tests/mr_tests
```

The suite has no external test-framework dependency (see
`tests/MiniTest.hpp`) so it builds anywhere the compiler does. Codegen
tests that need `nasm`/`ld` skip themselves (rather than fail) if the
toolchain isn't on `PATH`.

## The language, currently

This is the subset of the full language specification
(`docs/language-spec.md`) that the compiler accepts today. Everything else
in the spec is *reserved* — the lexer already recognises those keywords so
they can't be reused as identifiers by accident, but the parser doesn't
build grammar around them yet.

### Variable declaration

```text
ank name = expr;
```

Declares an integer variable and initializes it. Re-declaring a name
already visible in the same scope, or referencing an undeclared name, is a
semantic error.

> **Note on the spec vs. today's grammar:** `docs/language-spec.md`
> describes declarations as `he ank x = 5;` (with the `he`/`te`
> singular/collection modifier always present). The compiler this project
> started from only ever implemented the shorter `ank x = 5;` form, and
> that is what's preserved here to avoid breaking the existing test
> program and any `.mr` files written against it. Wiring up `he`/`te`,
> `maze`, `sthir`, `lahan`/`maha`/`uch`, and `ahe` (immutability) is
> tracked as a deliberate, minimal-diff follow-up — see "Extending the
> grammar" below — rather than done here as a silent, unannounced
> grammar change.

### Assignment

```text
name = expr;
```

### Exit

```text
shevti(expr);
```

Evaluates `expr` and exits the process with that value as its exit code
(truncated to a byte by the OS, as usual for Unix exit codes).

### Conditionals

```text
jar (cond) { ... }
jar (cond) { ... } anyatha { ... }
jar (cond) { ... } nahitar (cond2) { ... } anyatha { ... }
```

`nahitar` chains may repeat any number of times before an optional
trailing `anyatha`. Parentheses around the condition are mandatory.

### Scopes

```text
{ ... }
```

A bare `{ }` block introduces a new lexical scope; variables declared
inside go out of scope at the closing `}`.

### Expressions

Integer literals, identifiers, parenthesized subexpressions, and the four
arithmetic operators `+ - * /` with conventional precedence
(`*`/`/` bind tighter than `+`/`-`; use `()` to override).

### Comments

`// line comment` and `/* block comment */`.

## Compiler architecture

| Stage | Files | Responsibility |
|---|---|---|
| Lexing | `include/lexer`, `src/lexer` | source text → token stream |
| Parsing | `include/parser`, `src/parser` | tokens → AST, syntax diagnostics, error recovery |
| AST | `include/ast/Ast.hpp` | node definitions (arena-allocated) |
| Semantic analysis | `include/sema`, `src/sema` | scoping / declaration checks |
| Code generation | `include/codegen`, `src/codegen` | AST → NASM x86-64 assembly |
| Diagnostics | `include/diagnostics`, `src/diagnostics` | structured errors/warnings, pretty printing |
| Driver | `src/main.cpp` | wires the stages together, owns exit codes |

Each stage takes a `DiagnosticEngine&` and reports through it instead of
calling `std::cerr` + `exit()` directly; only `main.cpp` decides what a
failed stage means for the process exit code. Every diagnostic carries a
`SourceLocation` (file/line/column) that survives all the way from the
lexer to codegen, which is what lets errors from any stage point at the
right place in the original source.

### Codegen notes

Expressions evaluate onto a runtime stack (matching how the original
implementation worked): each subexpression pushes its single result, and
binary operators pop both operands, compute, and push the result back.
Variables live at a fixed offset from the current stack pointer, tracked
in `CodeGenerator::_vars`.

Multiplication and division use the full 64-bit registers (`mul rbx`,
`div rbx`), not an 8-bit form — this was checked specifically because an
earlier version of this project was suspected of truncating large
products; `tests/test_codegen.cpp` has a standing regression test
(`large_multiplication_is_correct`, 90 × 50 = 4500) and a static check that
the emitted assembly never contains a narrow (`mul al`/`mul bl`) multiply.

### Arena allocator

`support/Arena.hpp` is a bump-pointer allocator that grows by adding new
blocks on demand rather than throwing once a fixed byte budget runs out.
AST nodes are allocated from it and never individually freed; the whole
arena is released when the `Parser` that owns it goes out of scope. This
keeps node lifetime reasoning simple (valid as long as the arena is alive)
without per-node `new`/`delete` overhead.

## Extending the grammar

The AST, parser, and semantic analyzer are structured so that adding a
construct is additive rather than a redesign:

1. Add the AST node(s) to `include/ast/Ast.hpp` (a struct + a new
   alternative in the relevant `std::variant`).
2. Add a `parseX()` method to `Parser` and a case in `parseStmt()` (or
   wherever the new construct is reachable from).
3. Add a `visitX()` case to `SemanticAnalyzer` if the construct has scoping
   or type implications.
4. Add a `genX()` case to `CodeGenerator`.
5. Add lexer/parser/sema/codegen tests under `tests/`.

The full keyword set the lexer already recognises (see
`include/lexer/Token.hpp`) is the natural starting point for what to wire
up next — declaration modifiers (`he`/`te`/`maze`/`sthir`/`lahan`/`maha`/
`uch`/`ahe`), functions (`karya`/`partav`/`leeh`), loops (`jovar`/
`pratyek`/`thamba`/`pudhe`), and so on — in roughly that order, since later
features (classes, exceptions) tend to depend on functions existing first.

## Editor support

`tools/vscode-mr/` is a VS Code language extension for `.mr` files
(syntax highlighting, bracket matching, snippets), kept in sync with the
lexer's actual keyword set. Reserved-but-not-yet-parseable keywords are
still highlighted (so future code doesn't look broken), but snippets for
not-yet-compilable syntax are clearly labeled as such.

## Known limitations

- No functions, loops, arrays, classes, structs, or exceptions yet — see
  "Extending the grammar" above.
- Only integer arithmetic; no `bhagank` (float), `akshar` (char), or
  string support in codegen yet, even though those types tokenize.
- Codegen assumes throughput-simplicity (stack-machine evaluation) over
  register allocation; this is fine for the current program sizes but is
  the first thing to revisit if generated code performance ever matters.
