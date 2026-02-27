# AGENTS.md - Baa Compiler Agent Guide
This guide is for coding agents working in this repository.
Baa (باء) is an Arabic-first compiled systems language written in C11 (GNU11).
Pipeline: `Lexer -> Parser -> Semantic Analysis -> IR -> Optimizer -> Backend (ISel/RegAlloc/Emit) -> host GCC/Clang link`
Targets:
- `x86_64-windows` (COFF + Windows x64 ABI)
- `x86_64-linux` (ELF + SystemV AMD64 ABI)

## 1) Repo Layout
- Compiler sources: `src/*.c`
- Public declarations: `src/baa.h`
- Integration tests: `tests/integration/**/*.baa`
- Negative tests: `tests/neg/*.baa`
- Stress tests: `tests/stress/*.baa`
- Keep docs synchronized with behavior changes:
  - `README.md`
  - `docs/LANGUAGE.md`
  - `ROADMAP.md`
  - `CHANGELOG.md`

## 2) Build Commands
### Windows (MinGW + CMake)
```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build
cmake --build build --clean-first
cmake --build build --verbose
```
### Linux (native)
```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j
./build-linux/baa --version
```
Build notes:
- If you add a new source file, update `SOURCES` in `CMakeLists.txt`.
- `src/updater.c` is Windows-only; non-Windows builds use `src/updater_stub.c`.
- Warnings options:
  - `BAA_ENABLE_WARNINGS` (default ON)
  - `BAA_WARNINGS_AS_ERRORS` (default OFF)

## 3) Lint / Quality Gate
There is no separate linter; use a warning-clean build.
### Strict warnings build (Windows)
```powershell
cmake -B build -G "MinGW Makefiles" -DBAA_WARNINGS_AS_ERRORS=ON
cmake --build build
```
### Strict warnings build (Linux)
```bash
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release -DBAA_WARNINGS_AS_ERRORS=ON
cmake --build build-linux -j
```

## 4) Test Commands
### Main QA runner
```bash
python scripts/qa_run.py --mode quick
python scripts/qa_run.py --mode full
python scripts/qa_run.py --mode stress
```
Mode summary:
- `quick`: integration suite (`tests/test.py`)
- `full`: quick + regression/negative + verify smoke + multifile smoke
- `stress`: full + stress suite + fuzz-lite
### Direct runners
```bash
python tests/test.py
python tests/regress.py
```

### Run a single integration test (manual, recommended)
Windows:
```powershell
build\baa.exe -O2 --verify tests\integration\backend\backend_test.baa -o build\backend_test.exe
build\backend_test.exe
```
Linux:
```bash
./build-linux/baa -O2 --verify tests/integration/backend/backend_test.baa -o build-linux/backend_test
./build-linux/backend_test
```

### Run a single compile-only integration test
```bash
./build-linux/baa -O2 --verify tests/integration/ir/ir_test.baa -o build-linux/ir_test
```

### Run a single negative test (expect compiler failure)
Windows:
```powershell
build\baa.exe -O1 tests\neg\semantic_deref_non_pointer.baa -o build\neg_tmp.exe
```
Linux:
```bash
./build-linux/baa -O1 tests/neg/semantic_deref_non_pointer.baa -o build-linux/neg_tmp
```
Then confirm stderr includes the test `// EXPECT:` marker text.

### Test metadata conventions
- `// RUN:` contract (`expect-pass`, `expect-fail`, `runtime`, `compile-only`, `skip`)
- `// EXPECT:` required diagnostic substring(s)
- `// FLAGS:` extra flags for that specific test file

## 5) Useful Compiler Flags
- Optimization: `-O0`, `-O1`, `-O2`
- Validation: `--verify`, `--verify-ir`, `--verify-ssa`, `--verify-gate`
- Target: `--target=x86_64-windows|x86_64-linux`
- Output: `-S` (assembly), `-c` (object)
- Code model: `-fPIC`, `-fPIE`, `-mcmodel=small`
- Stack protector (ELF): `-fstack-protector`, `-fstack-protector-all`
Cross-target note: currently supported for assembly generation (`-S`) only.

## 6) Code Style (C)
### Formatting
- C11 + GNU extensions (`-std=gnu11`)
- 4-space indentation, no tabs
- K&R brace style
- Keep lines near 100 columns when practical
- Prefer small focused helpers over very large functions

### Arabic-first comments/diagnostics (critical)
- Write comments and Doxygen blocks in Arabic (UTF-8)
- Keep user-facing diagnostics Arabic-first
Example:
```c
/**
 * @brief وصف مختصر
 */
```

### Includes/imports
- Own module header first (if present)
- Then project headers (`"..."`)
- Then system headers (`<...>`)
- Keep include blocks stable and minimal

### Naming
- Functions/local vars: `snake_case` (often module-prefixed)
- Types/typedef structs: `PascalCase`
- Enums/macros/constants: `UPPER_SNAKE_CASE`
- Filenames: `snake_case.c` / `snake_case.h`

### Types and conversions
- Use fixed-width integers (`int64_t`, `uint32_t`, etc.) for width-sensitive logic
- Use `bool` for predicates
- Be explicit about signed/unsigned behavior and casts
- Keep pointer-type metadata consistent across parser/semantic/IR

### Error handling
- Validate pointers before dereference
- Prefer early returns for invalid states
- Do not silently swallow parser/semantic/IR errors
- Use centralized diagnostics APIs where applicable:
  - `error_report(...)`
  - `warning_report(...)`

### Memory ownership
- Free what you allocate (`malloc`, `strdup`, `realloc` paths)
- Preserve existing ownership patterns and cleanup flow
- IR core uses arena allocation; avoid ad-hoc frees of arena-owned objects

### Backend conventions
- Assembly syntax is AT&T (GAS)
- Special machine vregs:
  - `-1` => RBP
  - `-2` => ABI return register (usually RAX)
  - `-3` => RSP
  - `-10..` => ABI integer argument registers (target-dependent)

## 7) Agent Workflow Expectations
- Before large changes, read `ROADMAP.md` and `docs/INTERNALS.md`
- Keep docs/changelog/roadmap synchronized with behavior changes
- Add/update tests with each semantic/backend feature change
- Prefer minimal, surgical edits; avoid unrelated refactors
- Never revert unrelated local changes in a dirty worktree

## 8) Cursor / Copilot Rules Status
Checked paths:
- `.cursor/rules/`
- `.cursorrules`
- `.github/copilot-instructions.md`
Current status: no Cursor/Copilot rule files were found in this repository.
