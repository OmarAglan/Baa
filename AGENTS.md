# AGENTS.md - Baa Compiler

Baa (باء) is an Arabic-first compiled systems programming language written in C11,
targeting Windows x86-64. Pipeline: Lexer -> Parser -> Semantic Analysis -> IR -> Optimizer -> Codegen (x86-64 asm) -> GCC linker.

## Build Commands

Toolchain: CMake 3.10+ with MinGW-w64 GCC (`-std=gnu11`).

```powershell
# Configure (once, or after CMakeLists.txt changes)
cmake -B build -G "MinGW Makefiles"

# Build the compiler
cmake --build build

# Clean rebuild
cmake --build build --clean-first

# Verbose build (to diagnose errors)
cmake --build build --verbose
```

When adding new source files, add them to the `SOURCES` list in `CMakeLists.txt`.

## Running Tests

There is no formal test framework. Tests are standalone C files in `tests/` with hand-rolled
assertions. Each test has its own `main()` and exits 0 on PASS, 1 on FAIL.

```powershell
# Compile and run a single IR unit test (from project root):
gcc -g -std=gnu11 -o build/ir_constfold_test.exe tests/ir_constfold_test.c src/ir.c src/ir_builder.c src/ir_constfold.c src/ir_pass.c src/ir_analysis.c
build\ir_constfold_test.exe

# Available unit tests (same pattern for each):
#   tests/ir_analysis_test.c   - CFG validation, dominators
#   tests/ir_constfold_test.c  - Constant folding pass
#   tests/ir_dce_test.c        - Dead code elimination
#   tests/ir_copyprop_test.c   - Copy propagation
#   tests/ir_cse_test.c        - Common subexpression elimination

# Integration test: compile a Baa program
build\baa.exe tests/ir_test.baa -o build/test_out.exe
build\test_out.exe
```

Each test links against the IR source files it needs (ir.c, ir_builder.c, ir_pass.c,
ir_analysis.c, plus the specific pass under test). On PASS, tests print
`ir_<name>_test: PASS` to stderr.

## Compiling a Baa Program

```powershell
build\baa.exe program.baa -o program.exe    # compile
build\baa.exe program.baa -o program.exe -v # verbose
build\baa.exe program.baa --dump-ir         # dump IR
build\baa.exe --help                        # show help
```

## Code Style

### Language and Standard

- C11 with GNU extensions (`-std=gnu11`)
- 4-space indentation, no tabs
- K&R brace style (opening brace on same line)
- Max ~100 characters per line (soft limit)

### Arabic-First Documentation (CRITICAL)

ALL code comments and Doxygen documentation MUST be in Arabic. This is the project's
most important style rule.

```c
/**
 * @brief وصف مختصر للدالة
 * @param اسم_المعامل وصف المعامل
 * @return وصف القيمة المرجعة
 */
```

Inline comments in Arabic:
```c
// التحقق من نهاية الملف
if (token.type == TOKEN_EOF) {
    // إرجاع عقدة فارغة
    return NULL;
}
```

Section headers use `// ====...====` separators with Arabic + English:
```c
// ============================================================================
// معالجة المصفوفات (Array Handling)
// ============================================================================
```

### Includes

- Source files use quoted local includes: `#include "baa.h"`, `#include "ir.h"`
- System headers use angle brackets: `#include <stdio.h>`
- Order: own header first, then project headers, then system headers
- Test files use relative paths: `#include "../src/ir.h"`

### Header Guards

Use `#ifndef`/`#define`/`#endif` pattern: `BAA_H`, `BAA_IR_H`, etc.
Newer IR headers also include `extern "C"` guards for C++ compatibility.

### Naming Conventions

- Functions: `snake_case` -- prefixed by module: `ir_builder_emit_add()`, `ir_block_new()`
- Types: `PascalCase` -- `IRModule`, `IRBlock`, `IRInst`, `BaaTokenType`
- Enums: `UPPER_SNAKE_CASE` with module prefix: `IR_OP_ADD`, `TOKEN_IF`, `IR_TYPE_I64`
- Macros/constants: `UPPER_SNAKE_CASE`: `BAA_VERSION`, `ANSI_RESET`
- Local variables: `snake_case`
- File-scoped globals: `snake_case` with `static`: `static int label_counter`
- Files: `snake_case.c/.h` -- module prefix for IR: `ir_constfold.c`, `ir_builder.h`

### Arabic Naming in IR

IR opcodes, block labels, and registers use Arabic names in output:
- Registers: `%م<n>` (virtual registers)
- Block labels: `كتلة_` prefix (basic blocks)
- Opcodes: `جمع` (add), `طرح` (sub), `ضرب` (mul), `قسم` (div), etc.
- Types: `ص٦٤` (i64), `ص٣٢` (i32), `ص٨` (i8), `ص١` (i1)

### Error Handling

- Always validate pointers before dereferencing (NULL checks)
- Use `error()` or `error_at()` from `src/error.c` for user-facing diagnostics
- Free all allocated memory -- every `malloc()`/`strdup()` needs a matching `free()`
- Tests use a hand-rolled `require(cond, msg)` pattern (returns 0/1, prints to stderr)
- Do not use `goto` except for error-cleanup patterns

### Function Style

- Prefer `static` for file-internal functions
- Older code (lexer, parser, codegen) uses global state (e.g., `Parser parser;` global)
- Newer code (IR subsystem) passes context explicitly (`IRBuilder*`, `IRLowerCtx*`)
- New code should follow the newer explicit-context-passing pattern

### Exports

- `baa.h` is the monolithic header for the frontend (lexer, parser, codegen, analysis)
- IR subsystem uses modular headers: `ir.h`, `ir_builder.h`, `ir_lower.h`, `ir_pass.h`, etc.
- Each IR pass has its own `.h`/`.c` pair

## Project Structure

```
src/
  baa.h            -- Main header (tokens, AST, enums, all frontend declarations)
  main.c           -- CLI driver and build orchestration
  lexer.c          -- Tokenization + preprocessor
  parser.c         -- AST construction
  analysis.c       -- Semantic analysis / type checking
  codegen.c        -- AST -> x86-64 assembly generation
  error.c          -- Diagnostic engine (errors + warnings)
  ir.h / ir.c      -- IR data structures (SSA form)
  ir_builder.h/c   -- IR builder pattern API
  ir_lower.h/c     -- AST -> IR lowering
  ir_analysis.h/c  -- CFG validation, dominators
  ir_pass.h/c      -- Pass interface abstraction
  ir_constfold.h/c -- Constant folding optimization
  ir_dce.h/c       -- Dead code elimination
  ir_copyprop.h/c  -- Copy propagation
  ir_cse.h/c       -- Common subexpression elimination
  ir_optimizer.h/c -- Optimization pipeline orchestrator
tests/             -- Standalone C test files (hand-rolled)
docs/              -- Language spec, IR spec, internals, API reference
```

## Development Workflow

Detailed workflows exist in `.agent/workflows/baa-development.md` and
`.trae/BAA_DEV_PROMPT.md`. Key points:

1. Read `ROADMAP.md` to find the current task (look for `IN PROGRESS` / first `[ ]`)
2. Read `docs/INTERNALS.md` and relevant source before implementing
3. Implement with Arabic comments and Doxygen documentation
4. Build with `cmake --build build` and fix all warnings
5. Update `CHANGELOG.md`, `ROADMAP.md`, and docs after each change
6. Never break existing functionality -- preserve backward compatibility

### Common Tasks

**Adding a new IR opcode:** Add enum to `IROp` in `ir.h` -> case in `ir_print_inst()` in
`ir.c` -> emit function in `ir_builder.c` -> lowering in `ir_lower.c` -> update IR spec docs.

**Adding a new AST node:** Add enum to `NodeType` in `baa.h` -> struct in `Node` union ->
parser function in `parser.c` -> analysis in `analysis.c` -> lowering in `ir_lower.c`.

**Adding a new keyword:** Add enum to `TokenType` in `baa.h` -> keyword table in `lexer.c`
-> parsing logic in `parser.c` -> update `docs/LANGUAGE.md`.

## Key Constraints

- Target platform is Windows x86-64 (assembly follows Windows x64 ABI)
- All text encoding is UTF-8 (Arabic source, Arabic keywords, Arabic IR output)
- The compiler itself is compiled with MinGW-w64 GCC on Windows
- No external dependencies beyond the C standard library and Windows APIs (urlmon, wininet)
