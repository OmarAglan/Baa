# AGENTS.md - Baa Compiler

Baa (باء) is an Arabic-first compiled systems programming language written in C11 (GNU11).
Pipeline: Lexer -> Parser -> Semantic Analysis -> IR -> Optimizer -> Backend (ISel/RegAlloc/Emit)
-> host GCC/Clang link.

Targets (v0.3.2.8+):
- `x86_64-windows` (COFF + Windows x64 ABI)
- `x86_64-linux` (ELF + SystemV AMD64 ABI)

This file is for coding agents working in this repo.

## Build Commands

Windows (MinGW via CMake):

```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build
cmake --build build --clean-first
cmake --build build --verbose
```

Linux (native build):

```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j
./build-linux/baa --version
```

Notes:
- When adding new source files, update `SOURCES` in `CMakeLists.txt`.
- `src/updater.c` is Windows-only; non-Windows builds use `src/updater_stub.c`.

## Lint / Warnings

No dedicated linter. Treat a warning-free build as the lint signal.
- Preferred: fix warnings, do not silence them unless justified.

## Running Tests

Primary QA runner:

```bash
python scripts/qa_run.py --mode quick
python scripts/qa_run.py --mode full
python scripts/qa_run.py --mode stress
```

Tests are:
- Integration `.baa` programs in `tests/integration/**/*.baa` (compiled with `baa`, then executed).
- Negative diagnostics tests in `tests/neg/*.baa` (expected-fail marker based).
- Stress programs in `tests/stress/*.baa` (used in `--mode stress`).

Test file metadata (where applicable):
- `// RUN:` execution contract (`expect-pass`, `expect-fail`, `runtime`, `compile-only`, `skip`)
- `// EXPECT:` required diagnostic substring(s) for expected-fail tests
- `// FLAGS:` extra compiler flags for that test

### Run A Single Integration Test

Windows:

```powershell
build\baa.exe -O2 --verify-ir --verify-ssa tests\integration\backend\backend_test.baa -o build\backend_test.exe
build\backend_test.exe
```

Linux:

```bash
./build-linux/baa -O2 --verify-ir --verify-ssa tests/integration/backend/backend_test.baa -o build-linux/backend_test
./build-linux/backend_test
```

Target selection:
- Host defaults to host target.
- Cross-target: only `-S` (assembly) is supported for now.

```bash
./build-linux/baa --target=x86_64-windows -S tests/integration/backend/backend_test.baa -o build-linux/out_win.s
```

## Useful Compiler Flags

- Optimization: `-O0` / `-O1` / `-O2`
- IR/SSA validation: `--verify-ir`, `--verify-ssa`, `--verify-gate`
- Target: `--target=x86_64-windows|x86_64-linux`
- Assembly-only / object-only: `-S`, `-c`
- Code model options (v0.3.2.8.3): `-fPIC`, `-fPIE`, `-mcmodel=small`
- Stack protector (ELF only): `-fstack-protector`, `-fstack-protector-all`

## Code Style (C)

### Standard / Formatting
- C11 with GNU extensions (`-std=gnu11`)
- 4-space indentation, no tabs; K&R braces
- Soft limit ~100 columns

### Arabic-First Documentation (CRITICAL)
ALL comments and Doxygen blocks MUST be in Arabic (UTF-8).

```c
/**
 * @brief وصف مختصر
 */
```

### Includes
- Order: own header first -> project headers -> system headers
- Project: quoted includes (`"baa.h"`), system: angle brackets (`<stdio.h>`)
- Tests include via relative paths (e.g. `#include "../src/ir.h"`).

### Naming
- Functions/locals: `snake_case` (module-prefixed)
- Types: `PascalCase`
- Enums/macros: `UPPER_SNAKE_CASE`
- File names: `snake_case.c/.h`

### Error Handling / Ownership
- Validate pointers before dereference.
- User-facing diagnostics: `error()` / `error_at()` in `src/error.c`.
- Free what you allocate (`malloc`/`strdup`); IR core uses arena-style ownership.
- Prefer `static` for file-local helpers; avoid `goto` except cleanup.

### Backend Conventions (x86-64)
- Assembly syntax: AT&T (GAS).
- Machine special vregs:
  - `-1` => RBP, `-2` => ABI return reg (usually RAX), `-3` => RSP
  - `-10..` => ABI integer argument regs (target-dependent)

## Workflow / Repo Rules

- Check `ROADMAP.md` and `docs/INTERNALS.md` before large changes.
- Keep docs + `CHANGELOG.md` + version metadata in sync for releases.
- Cursor/Copilot rules: none found (`.cursor/rules/`, `.cursorrules`,
  `.github/copilot-instructions.md` not present at time of writing).
