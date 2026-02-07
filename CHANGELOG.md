# Changelog

All notable changes to the Baa programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

## [0.3.1.6] - 2026-02-07

### Added
- **IR optimization pipeline** — unified pass orchestration:
  - Ordered pass execution: ConstFold → CopyProp → CSE → DCE.
  - Fixpoint iteration: runs passes until no changes (max 10 iterations).
  - CLI flags: `-O0` (no optimization), `-O1` (basic, default), `-O2` (full + CSE).
  - `--dump-ir-opt` flag: prints IR after optimization.
  - Entry point: [`src/ir_optimizer.c`](src/ir_optimizer.c).

## [0.3.1.5] - 2026-02-07

### Added
- **IR common subexpression elimination pass (حذف_المكرر)** — new optimization pass for Baa IR:
  - Hashes expressions (opcode + operand signatures) to detect duplicates.
  - Replaces uses of duplicate expressions with the original result register.
  - Removes redundant instructions after propagation.
  - Eligible operations: arithmetic, comparisons, logical (pure operations only).
  - Pass entry point: [`src/ir_cse.c`](src/ir_cse.c), API header: [`src/ir_cse.h`](src/ir_cse.h).
- **Tests:** Added [`tests/ir_cse_test.c`](tests/ir_cse_test.c) for pass verification.

### Changed
- **Build system:** Added `src/ir_cse.c` to [`CMakeLists.txt`](CMakeLists.txt).

## [0.3.1.4] - 2026-01-27

### Added
- **IR copy propagation pass (نشر_النسخ)** — new optimization pass for Baa IR:
  - Replaces uses of `نسخ`-defined SSA registers with their original source value.
  - Canonicalizes copy chains (`%م٢ = نسخ %م١`, `%م١ = نسخ %م٠`) so later passes see fewer intermediates.
  - Updates operands in normal instructions, call arguments, and phi incoming values.
  - Pass entry point: [`src/ir_copyprop.c`](src/ir_copyprop.c:1), API header: [`src/ir_copyprop.h`](src/ir_copyprop.h:1).
- **Tests:** Added [`tests/ir_copyprop_test.c`](tests/ir_copyprop_test.c:1) for pass verification.

### Changed
- **Build system:** Added [`src/ir_copyprop.c`](src/ir_copyprop.c:1) to [`CMakeLists.txt`](CMakeLists.txt:1).
- **Docs:** Extended IR optimizer API reference with copy propagation entry points in [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md:1).

## [0.3.1.3] - 2026-01-27

### Added
- **IR dead code elimination pass (حذف_الميت)** — new optimization pass for Baa IR:
  - Removes dead SSA instructions whose destination register is unused (for instructions with no side effects).
  - Removes unreachable basic blocks (not reachable from function entry).
  - Conservative correctness: `نداء` (calls), `خزن` (stores), and terminators are always kept.
  - Pass entry point: [`src/ir_dce.c`](src/ir_dce.c), API header: [`src/ir_dce.h`](src/ir_dce.h).
- **Tests:** Added [`tests/ir_dce_test.c`](tests/ir_dce_test.c) for pass verification.

### Changed
- **Build system:** Added `src/ir_dce.c` to [`CMakeLists.txt`](CMakeLists.txt).

### Technical Details
- DCE is function-local and safe for the current IR model (virtual registers scoped per function).
- Unreachable-block removal rebuilds CFG edges using the analysis utilities before/after pruning.
- Phi nodes are pruned of incoming edges from removed predecessor blocks to avoid dangling references.

### Testing
- [`tests/ir_dce_test.c`](tests/ir_dce_test.c): Verifies dead-instruction removal, cascade behavior, unreachable-block elimination, and conservative side-effect preservation.

## [0.3.1.2] - 2026-01-22

### Added
- **IR constant folding pass (طي_الثوابت)** — new optimization pass for Baa IR:
  - Implements arithmetic and comparison folding when both operands are immediate constants.
  - Removes folded instructions and replaces register uses with constant values.
  - Pass entry point: [`ir_constfold_run()`](src/ir_constfold.c), pass descriptor: [`IR_PASS_CONSTFOLD`](src/ir_constfold.c).
  - API header: [`src/ir_constfold.h`](src/ir_constfold.h).
  - Integrated into optimizer pipeline via [`IRPass`](src/ir_pass.h).
- **Tests:** Added [`tests/ir_constfold_test.c`](tests/ir_constfold_test.c) for pass verification.

### Changed
- **Build system:** Added `src/ir_constfold.c` to [CMakeLists.txt](CMakeLists.txt).

### Technical Details
- Constant folding supports: جمع/طرح/ضرب/قسم/باقي (add/sub/mul/div/mod) and قارن <pred> (comparisons).
- Folded instructions are removed from IR; all uses of their destination register are replaced with constant immediates.
- Pass is function-local; virtual registers are scoped per function.

### Testing
- [`tests/ir_constfold_test.c`](tests/ir_constfold_test.c): Verifies folding, instruction removal, and register replacement.

## [0.3.1.1] - 2026-01-21

### Added
- **IR analysis infrastructure (CFG + dominance)** — foundational analysis utilities for the upcoming optimizer:
  - CFG validation (`ir_func_validate_cfg()`, `ir_module_validate_cfg()`) in [src/ir_analysis.c](src/ir_analysis.c) / [src/ir_analysis.h](src/ir_analysis.h)
  - Predecessor rebuild (`ir_func_rebuild_preds()`, `ir_module_rebuild_preds()`) in [src/ir_analysis.c](src/ir_analysis.c) / [src/ir_analysis.h](src/ir_analysis.h)
  - Dominator tree + dominance frontier (`ir_func_compute_dominators()`, `ir_module_compute_dominators()`) in [src/ir_analysis.c](src/ir_analysis.c) / [src/ir_analysis.h](src/ir_analysis.h)

- **IRPass interface** — a minimal pass abstraction for the optimization pipeline in [src/ir_pass.c](src/ir_pass.c) / [src/ir_pass.h](src/ir_pass.h)

### Changed
- **Build system** — added IR analysis + pass sources to [CMakeLists.txt](CMakeLists.txt)

### Testing
- Added IR analysis smoke test: [tests/ir_analysis_test.c](tests/ir_analysis_test.c)

## [0.3.0.7] - 2026-01-17

### Added
- **IR phase integrated into the driver pipeline** — the compiler now builds IR for each translation unit after semantic analysis.
  - New program-level lowering entry point: [`ir_lower_program()`](src/ir_lower.c:855) declared in [`src/ir_lower.h`](src/ir_lower.h:104).
  - Driver now runs: Parse → Analyze → **Lower IR** → (current) AST→Assembly codegen (IR→backend is still pending) in [`src/main.c`](src/main.c:325).

- **`--emit-ir` CLI flag** — writes Arabic IR to `<input>.ir` next to the source file using [`ir_module_dump()`](src/ir.c:1693).
  - Implemented in [`src/main.c`](src/main.c:158).

### Changed
- **`--dump-ir` now uses the integrated IR phase** (instead of a separate ad-hoc lowering path), printing the same IR module produced by [`ir_lower_program()`](src/ir_lower.c:855).

### Fixed
- **Global variable references during IR lowering** — expression/assignment lowering now resolves module-scope globals (e.g. `@حد`) when no local binding exists:
  - [`lower_expr()`](src/ir_lower.c:209) resolves globals in `NODE_VAR_REF`
  - [`lower_assign()`](src/ir_lower.c:383) supports stores to globals

### Testing
- Added integration IR test program: [`tests/ir_test.baa`](tests/ir_test.baa:1)

## [0.3.0.6] - 2026-01-17

### Added
- **IR Printer (Arabic-first)** — Canonical IR text output aligned with the IR text grammar in [`docs/BAA_IR_SPECIFICATION.md`](docs/BAA_IR_SPECIFICATION.md:398).
  - Implemented/updated IR printing in [`src/ir.c`](src/ir.c:1146):
    - [`ir_func_print()`](src/ir.c:1554) — prints function header + blocks using `دالة @... -> ... {}` format.
    - [`ir_block_print()`](src/ir.c:1527) — prints block labels with Arabic-Indic digits for label suffixes.
    - [`ir_inst_print()`](src/ir.c:1355) — prints instructions with Arabic opcodes and Arabic comma `،`.
    - [`ir_value_print()`](src/ir.c:1348) — prints `%م<n>` registers and immediates using Arabic-Indic numerals.
  - Added compatibility wrappers for the roadmap task names:
    - [`ir_print_func()`](src/ir.c:1715), [`ir_print_block()`](src/ir.c:1711), [`ir_print_inst()`](src/ir.c:1707)

- **CLI flag `--dump-ir`** — Dumps IR to stdout after semantic analysis (does not replace the main AST→assembly pipeline yet).
  - Implemented in [`src/main.c`](src/main.c:1) via a lightweight AST→IR build using [`IRBuilder`](src/ir_builder.h:43) + lowering helpers from [`src/ir_lower.c`](src/ir_lower.c:1), then printed with [`ir_module_print()`](src/ir.c:1641).

### Note
- IR is currently generated for `--dump-ir` output only; full pipeline integration remains scheduled for v0.3.0.7.

## [0.3.0.5] - 2026-01-16

### Added
- **AST → IR Lowering (Control Flow)** — Implemented CFG-based lowering for control-flow nodes using Arabic block labels and IR branches.
  - Updated lowering context [`IRLowerCtx`](src/ir_lower.h:34) with:
    - Label counter for unique block labels
    - Break/continue target stacks for nested loops/switch
  - Implemented lowering in [`src/ir_lower.c`](src/ir_lower.c:1) for:
    - `NODE_IF` → `قفز_شرط` + then/else blocks + merge block
    - `NODE_WHILE` → header/body/exit blocks with back edge `قفز`
    - `NODE_FOR` → init + header/body/increment/exit blocks (`استمر` targets increment)
    - `NODE_SWITCH` → comparison chain via `قارن يساوي` + case blocks + default + end (with C-style fallthrough)
    - `NODE_BREAK` / `NODE_CONTINUE` → `قفز` to active targets

### Note
- IR lowering is still not integrated into the CLI pipeline (`src/main.c`) yet; this will land in v0.3.0.7.

---

## [0.3.0.4] - 2026-01-16

### Added
- **AST → IR Lowering (Statements)** — Implemented initial statement lowering on top of the existing expression lowering.
  - Extended the IR lowering module: [`src/ir_lower.h`](src/ir_lower.h) + [`src/ir_lower.c`](src/ir_lower.c)
  - Added [`lower_stmt()`](src/ir_lower.c:377) and [`lower_stmt_list()`](src/ir_lower.c:369) to lower:
    - `NODE_VAR_DECL` → `حجز` (alloca) + `خزن` (store) + bind local name for `NODE_VAR_REF`
    - `NODE_ASSIGN` → `خزن` (store) to an existing local binding
    - `NODE_RETURN` → `رجوع` (return)
    - `NODE_PRINT` → `نداء @اطبع(...)` (call)
    - `NODE_READ` → `نداء @اقرأ(%ptr)` (call)

### Note
- IR lowering is still not integrated into the driver pipeline (`src/main.c`) yet; this work prepares v0.3.0.7 integration.

---

## [0.3.0.3] - 2026-01-16

### Added
- **AST → IR Lowering (Expressions)** — Initial expression lowering layer built on top of the IR Builder.
  - New module: [`src/ir_lower.h`](src/ir_lower.h) + [`src/ir_lower.c`](src/ir_lower.c)
  - Implemented [`lower_expr()`](src/ir_lower.c:146) for:
    - `NODE_INT` → constant immediate
    - `NODE_VAR_REF` → `حمل` (load) from local binding table
    - `NODE_BIN_OP` → arithmetic (`جمع`/`طرح`/`ضرب`/`قسم`/`باقي`) + comparisons (`قارن`) + boolean ops (`و`/`أو`)
    - `NODE_UNARY_OP` → `سالب` (neg) and boolean `نفي` (not)
    - `NODE_CALL_EXPR` → `نداء` (call) with argument lowering
  - Added lightweight lowering context + local bindings API:
    - [`IRLowerCtx`](src/ir_lower.h:36)
    - [`ir_lower_bind_local()`](src/ir_lower.c:35)

### Changed
- **Build System** — Added [`src/ir_lower.c`](CMakeLists.txt:7) to CMake sources.

---

## [0.3.0.2] - 2026-01-15

### Added
- **IR Builder Pattern API** — Convenient builder API for IR construction (`src/ir_builder.h`, `src/ir_builder.c`).
  - **`IRBuilder` context struct**: Tracks current function, block, and source location for instruction emission.
  - **Function creation**: `ir_builder_create_func()`, `ir_builder_add_param()`.
  - **Block creation**: `ir_builder_create_block()`, `ir_builder_set_insert_point()`.
  - **Register allocation**: `ir_builder_alloc_reg()`, `ir_builder_reg_value()`.
  - **Emit functions**: Comprehensive set of `ir_builder_emit_*()` functions for all instruction types:
    - Arithmetic: `emit_add`, `emit_sub`, `emit_mul`, `emit_div`, `emit_mod`, `emit_neg`
    - Memory: `emit_alloca`, `emit_load`, `emit_store`
    - Comparison: `emit_cmp`, `emit_cmp_eq`, `emit_cmp_ne`, `emit_cmp_gt`, `emit_cmp_lt`, `emit_cmp_ge`, `emit_cmp_le`
    - Logical: `emit_and`, `emit_or`, `emit_not`
    - Control flow: `emit_br`, `emit_br_cond`, `emit_ret`, `emit_ret_void`, `emit_ret_int`
    - Function calls: `emit_call`, `emit_call_void`
    - SSA: `emit_phi`, `phi_add_incoming`, `emit_copy`
    - Type conversion: `emit_cast`
  - **Constant helpers**: `ir_builder_const_int()`, `const_i64()`, `const_i32()`, `const_bool()`, `const_string()`.
  - **Control flow structure helpers**: `ir_builder_create_if_then()`, `create_if_else()`, `create_while()`.
  - **Global variables**: `ir_builder_create_global()`, `create_global_init()`, `get_global()`.
  - **Statistics**: `ir_builder_get_inst_count()`, `get_block_count()`, `print_stats()`.

### Changed
- **CMakeLists.txt**: Added `src/ir_builder.c` to build.
- **ROADMAP.md**: Marked v0.3.0.2 as completed.

---

## [0.3.0.1] - 2026-01-15

### Added
- **Intermediate Representation (IR)** — Phase 3 begins with the introduction of Baa's Arabic-first IR.
  - **IR Module** (`src/ir.h`, `src/ir.c`): Complete SSA-form IR infrastructure.
  - **Arabic Opcodes**: `جمع` (add), `طرح` (sub), `ضرب` (mul), `قسم` (div), `حمل` (load), `خزن` (store), `قفز` (br), `رجوع` (ret), `نداء` (call), `فاي` (phi).
  - **IR Type System**: `ص٦٤` (i64), `ص٣٢` (i32), `ص٨` (i8), `ص١` (bool), `مؤشر` (ptr), `فراغ` (void).
  - **Arabic Numerals**: Register names use Arabic-Indic numerals (`%م٠`, `%م١`, `%م٢`).
  - **SSA Form**: Phi nodes for control flow merging, single assignment per register.
  - **IR Printing**: `ir_module_print()` for debugging with `--dump-ir` flag (Arabic or English output).

### Technical Details
- **Data Structures**:
  - `IRModule`: Top-level container for functions, globals, and string table.
  - `IRFunc`: Function with parameters, basic blocks, and virtual register allocation.
  - `IRBlock`: Basic block with label, instruction list, and CFG edges (predecessors/successors).
  - `IRInst`: Individual instruction with opcode, type, destination register, and operands.
  - `IRValue`: Operand types (register, constant int, constant string, block label, global/function reference).
  - `IRType`: Type kinds (void, i1, i8, i16, i32, i64, pointer, array, function).
- **Helper Functions**: Constructors and destructors for all IR structures.
- **Comparison Predicates**: `يساوي` (eq), `لا_يساوي` (ne), `أكبر` (gt), `أصغر` (lt), `أكبر_أو_يساوي` (ge), `أصغر_أو_يساوي` (le).
- **IR Specification Document**: Full specification in `docs/BAA_IR_SPECIFICATION.md`.

### Changed
- **CMakeLists.txt**: Updated to version 0.3.0, added `src/ir.c` to build.
- **ROADMAP.md**: Added detailed sub-versions (v0.3.0.1 through v0.3.2.9) for Phase 3.

### Note
- This release introduces the IR infrastructure only. AST-to-IR lowering will be added in v0.3.0.3.
- Current compilation still uses direct AST-to-assembly code generation.

---

## [0.2.9] - 2026-01-14

### Added
- **Input Statement**: `اقرأ س.` (scanf) for reading user input.
- **Boolean Type**: `منطقي` type with `صواب`/`خطأ` literals.
- **Compile Timing**: Show compilation time with `-v`.

---

## [0.2.8] - 2026-01-13

### Added
- **Warning System** — Non-fatal diagnostic messages for potential code issues.
  - **Unused Variable Warning** (`-Wunused-variable`): Detects variables declared but never used.
  - **Dead Code Warning** (`-Wdead-code`): Detects unreachable code after `إرجع` (return) or `توقف` (break).
  - **Shadow Variable Warning**: Warns when local variable shadows a global variable.
- **Warning Flags** — Command-line options to control warning behavior:
  - `-Wall`: Enable all warnings.
  - `-Werror`: Treat warnings as errors (compilation fails).
  - `-Wunused-variable`: Enable only unused variable warning.
  - `-Wdead-code`: Enable only dead code warning.
  - `-Wno-<warning>`: Disable specific warning.
- **Colored Output** — ANSI color support for terminal output:
  - Errors displayed in red.
  - Warnings displayed in yellow.
  - Line numbers displayed in cyan.
  - Automatic detection of terminal capability (Windows 10+, Unix TTY).
  - `-Wcolor` to force colors, `-Wno-color` to disable.

### Changed
- **Symbol Table** — Extended with usage tracking fields:
  - `is_used`: Tracks whether variable is referenced.
  - `decl_line`, `decl_col`, `decl_file`: Stores declaration location for accurate warning messages.
- **Semantic Analysis** — Enhanced to:
  - Track variable usage during AST traversal.
  - Detect code after terminating statements (return/break/continue).
  - Check for local-global variable shadowing.
- **Diagnostic Engine** — Upgraded from error-only to full error+warning system.

### Technical Details
- Warnings are disabled by default (must use `-Wall` or specific `-W<type>`).
- Warning configuration stored in global `g_warning_config` structure.
- ANSI escape codes used for colors: `\033[31m` (red), `\033[33m` (yellow), `\033[36m` (cyan).
- Windows: Uses `SetConsoleMode()` to enable `ENABLE_VIRTUAL_TERMINAL_PROCESSING`.

---

## [0.2.7] - 2026-01-12

### Added
- **Constant Keyword (`ثابت`)** — Declare immutable variables that cannot be reassigned after initialization.
  - Syntax: `ثابت صحيح حد = ١٠٠.` (const int limit = 100)
  - Works for both global and local variables.
- **Constant Arrays** — Support for immutable arrays: `ثابت صحيح قائمة[٥].`
- **Const Checking** — Semantic analysis now detects and reports:
  - Reassignment to constant variables.
  - Modification of constant array elements.
  - Constants declared without initialization.

### Changed
- **Symbol Table** — Added `is_const` flag to track constant status.
- **Parser** — Updated to recognize `ثابت` keyword before type declarations.
- **Semantic Analysis** — Enhanced to enforce immutability rules.

### Technical Details
- Constants must be initialized at declaration time.
- Functions cannot be declared as const.
- Const checking happens during semantic analysis phase (before code generation).

---
## [0.2.6] - 2026-01-11

### Added
- **Preprocessor Engine** – Fully integrated into Lexer (`lexer.c`) to handle directives before tokenization.
- **Macro Definitions** – Implemented `#تعريف <name> <value>` to define compile-time constants.
- **Conditional Compilation** – Implemented `#إذا_عرف`, `#وإلا`, and `#نهاية` to conditionally include or exclude code blocks.
- **Undefine Directive** – Implemented `#الغاء_تعريف` to remove macro definitions.
- **Macro Substitution** – Identifiers are automatically replaced with macro values during lexing.

### Fixed
- **Codegen State Leak** – Fixed critical bug where symbol tables, label counters, and loop stacks were not reset between compilation units.
- Added `reset_codegen()` function called at start of each `NODE_PROGRAM` in `codegen.c`.
- Prevents symbol collisions and invalid assembly when compiling multiple files.

### Technical Details
- Preprocessor directives are handled in `lexer_next_token()` before any tokenization.
- Include stack depth limited to 10 levels to prevent infinite recursion.
- Macro table size limited to 100 entries.
- File state (source, line, col, filename) properly tracked through include stack.

## [0.2.5] - 2026-01-04
### Added
- **Multi-File Compilation**: Full support for compiling multiple `.baa` files into a single executable.
  - Each file compiled to `.o` independently, then linked together.
  - Proper handling of compilation errors per file.
- **Function Prototypes**: New syntax `صحيح دالة().` to declare functions without definition (for cross-file usage).
- **Include Directive**: Implemented `#تضمين "file"` (include) for header files.
  - Nested includes supported up to 10 levels deep.
  - Proper filename tracking for error reporting.
- **File Extension Convention**: Standardized `.baa` for source, `.baahd` for headers.

### Changed
- **Documentation**: Updated all examples to use `.baa` extension.
- **CLI**: Multi-file compilation now primary workflow.

### Fixed
- **CLI Arguments**: Fixed crash when running `baa` without arguments.
- **Lowercase `-s` Flag**: Added support for `-s` as alias for `-S`.
- **Updater Version Logic**: Fixed semantic version comparison (now properly compares major.minor.patch).
- **Update Command Parsing**: `baa update` must now be used alone (no filename arguments).

## [0.2.4] - 2026-01-04

### Added
- **Semantic Analysis Pass** — Implemented a dedicated validation phase (`src/analysis.c`) that runs before code generation. It checks for:
    - **Symbol Resolution**: Ensures variables are declared before use.
    - **Type Checking**: Strictly enforces `TYPE_INT` vs `TYPE_STRING` compatibility.
    - **Scope Validation**: Tracks global vs local variable declarations.
    - **Control Flow Validation**: Ensures `break`/`continue` only used in valid contexts.
- **Shared Symbol Definitions** — Moved `Symbol` and `ScopeType` to `src/baa.h` to allow sharing between Analysis and Codegen modules.

### Changed
- **Compiler Pipeline** — Updated `src/main.c` to invoke the analyzer after parsing. Compilation now aborts immediately if semantic errors are found.
- **Code Generator** — Refactored `src/codegen.c` to rely on the shared symbol definitions.


### Technical Details
- Analyzer maintains separate symbol tables from codegen (isolation).
- Type inference implemented for all expression types.
- Nested scope tracking (global/local only - no block-level yet).

---

## [0.2.3] - 2025-12-29

### Added
- **Windows Installer** — Created a professional `setup.exe` using Inno Setup. It installs the compiler, documentation, and creates Start Menu shortcuts.
- **PATH Integration** – Installer automatically adds Baa to system `PATH` environment variable.
  - Enables `baa` command from any directory.
- **Auto Updater** — Added `baa update` command. It checks a remote server for new versions and downloads/installs updates automatically.
- **Update System** – Implemented `src/updater.c` using native Windows APIs.
  - Uses `URLDownloadToFileA` from `urlmon.lib`.
  - Downloads installer to temp directory.
  - Launches installer and exits current process.

### Technical Details
- Version checking via HTTP GET to `version.txt`.
- Semantic version comparison (major.minor.patch).
- Cache clearing to ensure fresh version data.

---

## [0.2.2] - 2025-12-29

### Added
- **Diagnostic Engine** (`src/error.c`) – Professional error reporting with source context.
  - Displays: `[Error] filename:line:col: message`
  - Shows actual source line with `^` pointer to error position.
  - Printf-style formatting support.
- **Panic Mode Recovery** – Parser continues after syntax errors to find multiple issues.
  - Synchronizes at statement boundaries (`;`, `.`, `}`, keywords).
  - Prevents cascading error messages.
- **Enhanced Token Tracking** – All tokens now store `filename`, `line`, and `col`.

### Fixed
- **Global String Initialization** – Fixed bug where `نص س = "text".` would print `(null)`.
  - Now correctly emits `.quad .Lstr_N` in data section.

---

## [0.2.1] - 2025-12-29

### Added
- **Executable Branding** — Added a custom icon (`.ico`) and version metadata to `baa.exe`.
    - Resource file: `src/baa.rc`
    - Displays in Windows Explorer properties.
- **Windows Integration** — File properties now show "Baa Programming Language" and version info.

---

## [0.2.0] - 2025-12-29

### Added
- **CLI Driver** – Complete rewrite of `main.c` as professional build system.
  - Argument parsing with flag support.
  - Multi-stage compilation pipeline.
  - Automatic invocation of GCC for assembly/linking.
- **Flags** — Added support for standard compiler flags:
    - `-o <file>`: Specify output filename.
    - `-S`: Compile to assembly only (skip assembler/linker).
    - `-c`: Compile to object file (skip linker).
    - `-v`: Verbose output.
    - `--help`: Print usage information.
    - `--version`: Print version and build date.
- **GCC Integration** – Automatic invocation via `system()` calls.

### Changed
- **Architecture** – Transformed from simple transpiler to full compilation toolchain.

---

## [0.1.3] - 2025-12-27

### Added
- **Extended If** — Added support for `وإلا` (Else) and `وإلا إذا` (Else If) blocks.
- **Switch Statement** — Implemented `اختر` (Switch), `حالة` (Case), and `افتراضي` (Default) for clean multi-way branching.
- **Constant Folding** — Compiler now optimizes arithmetic expressions with constant operands at compile-time (e.g., `2 * 3 + 4` generates `10` directly).

### Changed
- **Parser** — Enhanced expression parsing to support immediate evaluation of constant binary operations.
- **Codegen** — Improved label management for nested control structures (`if`, `switch`, `loops`).

---

## [0.1.2] - 2025-12-27

### Added
- **Recursion** — Functions can now call themselves recursively (e.g., Fibonacci, Factorial).
- **String Variables** — Introduced `نص` keyword to declare string variables (behaves like `char*`).
- **Loop Control** — Added `توقف` (break) to exit loops immediately.
- **Loop Control** — Added `استمر` (continue) to skip to the next iteration.
- **Type System** — Internal symbol table now strictly tracks `TYPE_INT` vs `TYPE_STRING`.

### Fixed
- **Stack Alignment** — Enforced 16-byte stack alignment in generated x64 assembly to prevent crashes during external API calls and deep recursion.
- **Register Names** — Fixed double-percent typo in register names in `codegen.c`.

---

## [0.1.1] - 2025-12-26 - Structured Data

### Added
- **Arrays** — Declaration (`صحيح قائمة[٥]`), access (`قائمة[٠]`), and assignment (`قائمة[٠] = ١`)
- **For Loop** — `لكل` with Arabic semicolon `؛` separator
- **Postfix Operators** — Increment (`++`) and decrement (`--`)
- **Logic Operators** — `&&` (AND), `||` (OR), `!` (NOT) with short-circuit evaluation

---

## [0.1.0] - 2025-12-26 - Text & Unary

### Added
- **String Literals** — `"text"` support
- **Character Literals** — `'x'` support
- **Unary Minus** — Negative numbers (`-5`)

### Changed
- Updated `اطبع` to support printing strings via `%s`
- Implemented string table generation in codegen

---

## [0.0.9] - 2025-12-26 - Advanced Math

### Added
- **Multiplication** (`*`), **Division** (`/`), and **Modulo** (`%`)
- **Relational Operators** — `<`, `>`, `<=`, `>=`

### Changed
- **Parser** — Implemented operator precedence (PEMDAS) for correct expression evaluation

---

## [0.0.8] - 2025-12-26 - Functions

### Added
- **Function Definitions** — `صحيح func(...) {...}` syntax
- **Function Calls** — `func(...)` syntax
- **Scoping** — Global vs local variable scope
- **Entry Point** — Detection of `الرئيسية` (Main) as mandatory entry point
- **Windows x64 ABI** — Stack frames, register argument passing (RCX, RDX, R8, R9), shadow space

### Fixed
- Global variables now correctly use their initializers

### Changed
- **Architecture** — Program structure changed from linear script to list of declarations

---

## [0.0.7] - 2025-12-25 - Loops

### Added
- **While Loop** — `طالما` statement
- **Variable Reassignment** — `x = 5.` syntax for updating existing variables

### Changed
- Implemented loop code generation using label jumps

---

## [0.0.6] - 2025-12-25 - Control Flow

### Added
- **If Statement** — `إذا` conditional
- **Block Scopes** — `{ ... }` grouping
- **Comparison Operators** — `==`, `!=`

### Changed
- Implemented label generation and conditional jumps in codegen
- Comprehensive documentation update (Internals & API)

---

## [0.0.5] - 2025-12-25 - Type System

### Changed
- **Breaking:** Renamed `رقم` to `صحيح` (int) to align with C types

### Added
- Single line comments (`//`)

---

## [0.0.4] - 2025-12-24 - Variables

### Added
- `رقم` (Int) type keyword
- Variable declaration (`رقم name = val.`)
- Variable usage in expressions

---

## [0.0.3] - 2025-12-24 - I/O

### Added
- `اطبع` (Print) statement
- Support for multiple statements in a program
- Integration with C Standard Library (`printf`)

---

## [0.0.2] - 2025-12-24 - Math

### Added
- Arabic numeral support (٠-٩)
- Addition (`+`) and subtraction (`-`)

---

## [0.0.1] - 2025-12-24 - Initial Release

### Added
- Initial compiler implementation
- Compiles `إرجع <number>.` to executable
- Basic pipeline: Lexer → Parser → Codegen → GCC