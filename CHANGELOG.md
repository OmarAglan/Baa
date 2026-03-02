# Changelog

All notable changes to the Baa programming language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

---

## [0.4.1.0] - 2026-03-02

### Added

- **Standard Library modules (v0.4.1)** via compiler-backed builtins:
  - **Math:** `جذر_تربيعي(عشري)`, `أس(عشري، عشري)`, `مطلق(صحيح)`, `عشوائي()`
  - **System:** `متغير_بيئة(نص)`, `نفذ_أمر(نص)`
  - **Time:** `وقت_حالي()`, `وقت_كنص(صحيح)`
- **Stdlib header/API sync** in `stdlib/baalib.baahd` for new Math/System/Time declarations.
- **New test coverage**:
  - `tests/integration/backend/backend_stdlib_v041_test.baa`
  - `tests/neg/semantic_math_sqrt_bad_type.baa`
  - `tests/neg/semantic_system_env_bad_arity.baa`
  - `tests/neg/semantic_time_now_bad_arity.baa`

### Changed

- **IR lowering** now recognizes v0.4.1 Math/System/Time APIs as builtins and lowers them directly to libc/libm calls (`sqrt`, `pow`, `llabs`, `rand`, `getenv`, `system`, `time`, `ctime`) with Arabic-first diagnostics and shadowing rules.
- **Semantic analysis** now validates argument count/types for the new v0.4.1 APIs before codegen.
- **Driver linker flags** now include `-lm` to ensure math runtime symbols are resolved during final link.

### Fixed

- **Link-time compatibility** for math stdlib calls on hosts requiring explicit libm linkage.

## [0.4.0.6] - 2026-03-02

### Added

- **Inline assembly statement** via keyword `مجمع`:
  - basic block form: `مجمع { "nop" }`
  - constrained operands form: `مجمع { "rdtsc" : "=a"(منخفض)، "=d"(مرتفع) }`
- **Lexer/Parser support**:
  - `TOKEN_ASM` for `مجمع`
  - new AST nodes: `NODE_INLINE_ASM`, `NODE_ASM_OPERAND`
- **Backend emission path** for inline assembly lines inside generated AT&T assembly.
- **Test coverage for inline assembly**:
  - `tests/integration/ir/ir_inline_asm_test.baa`
  - `tests/neg/semantic_inline_asm_constraint_unsupported.baa`
  - `tests/neg/semantic_inline_asm_output_not_lvalue.baa`

### Changed

- **IR→Backend lowering contract**: inline asm is lowered as a dedicated pseudo-call metadata packet and consumed in ISel to emit raw assembly lines with fixed-register operand moves.
- **Register allocation call barriers** now treat inline asm emission points as call-like barriers to preserve correctness for caller-saved registers across `مجمع`.

### Fixed

- **Constraint validation diagnostics** now reject unsupported inline-asm constraints early in semantic analysis with explicit Arabic diagnostics.

## [0.4.0.5] - 2026-03-02

### Added

- **Variadic function declarations** using `...` in normal function signatures and function-pointer signatures (`دالة(...)->...`).
- **Variadic runtime builtins** (compiler intrinsics):
  - `بدء_معاملات(قائمة، آخر_ثابت) -> عدم`
  - `معامل_تالي(قائمة، نوع) -> <النوع_المطلوب>`
  - `نهاية_معاملات(قائمة) -> عدم`
- **Variadic IR metadata/text support**:
  - `IRFunc.is_variadic` flag
  - IR text/function printer support for trailing `...` in function signatures.
- **New test coverage for variadics**:
  - `tests/integration/backend/backend_variadic_functions_test.baa`
  - `tests/neg/semantic_variadic_builtin_outside_function.baa`
  - `tests/neg/semantic_variadic_start_anchor_mismatch.baa`
  - `tests/neg/semantic_variadic_next_type_unsupported.baa`
  - `tests/neg/semantic_variadic_funcptr_call_unsupported.baa`

### Changed

- **Variadic lowering model (v0.4.0.5):** direct variadic calls now package extra arguments into an internal slot buffer and pass a hidden pointer parameter (`__baa_va_base`) to the callee.
- **Semantic checks:** variadic calls now enforce "at least fixed params" for variadic signatures and type-check supported variadic extra-argument kinds.
- **Stdlib header sync:** `stdlib/baalib.baahd` now declares `قائمة_معاملات` and variadic runtime intrinsics.

### Fixed

- **IR lowering predeclaration path:** variadic functions declared before use now consistently receive the hidden variadic base parameter during IR signature setup (fixes false "outside variadic function" lowering errors).

## [0.4.0.0] - 2026-03-02

### Added

- **Formatted console I/O** (v0.4.0.0) عبر دوال مدمجة (Builtins):
  - `اطبع_منسق(نص تنسيق_ثابت، ...) -> عدم`
  - `نسق(نص تنسيق_ثابت، ...) -> نص` (يعيد نصاً مخصصاً على heap ويجب تحريره عبر `حرر_نص`)
  - `اقرأ_منسق(نص تنسيق_ثابت، ...) -> صحيح` (يعيد عدد العناصر المقروءة مثل `scanf`)
  - `اقرأ_سطر() -> نص` (stdin: يعيد `عدم` عند EOF قبل قراءة أي بايت)
  - `اقرأ_رقم() -> صحيح` (stdin)
- **Arabic format specifiers** (v0.4.0.0): دعم مواصفات عربية مثل `%ص/%ط/%س/%ن/%ح/%ع/%م` مع أعلام/عرض/دقة على نمط C ضمن النص.
- **Test runner stdin feeding:** `tests/test.py` يدعم أسطر `// STDIN:` لاختبارات runtime.

### Changed

- **تنسيق مبدئي محافظ:** نص التنسيق يجب أن يكون ثابتاً (literal) في هذا الإصدار لضمان التحقق الساكن.
- **أمان إدخال النص في `اقرأ_منسق`:** في الإدخال، `%ن` يتطلب عرضاً رقمياً (مثل `%64ن`) لتحديد حد أقصى للقراءة.

## [0.3.12.5] - 2026-03-02

### Added

- **Command line arguments for `الرئيسية`** (v0.3.12.5): allow `صحيح الرئيسية(صحيح عدد، نص[] معاملات)` in addition to `صحيح الرئيسية()`.
- **Pointer indexing** (v0.3.12.5): allow `p[i]` (and chained forms like `argv[i][j]`) as sugar for `*(p + i)` where valid.
- **Custom startup entrypoint** (v0.3.12.5): `--startup=custom` links with entry symbol `__baa_start` and injects a small startup stub while still keeping CRT/libc initialization.
- **Test runner args marker:** `tests/test.py` now supports `// ARGS:` lines for runtime integration tests.
- **Test runner assembly expectations:** `tests/test.py` now supports `// EXPECT-ASM:` lines for `-S` assembly substring checks.
- **Coverage for main args:**
  - `tests/integration/backend/backend_main_args_test.baa`
  - `tests/integration/backend/backend_custom_startup_test.baa`
  - `tests/neg/semantic_main_args_bad_signature.baa`
  - `tests/integration/ir/ir_custom_startup_linux_asm_test.baa`

## [0.3.12.0] - 2026-03-02

### Added

- **File I/O APIs** (v0.3.12):
  - `فتح_ملف(مسار، وضع) -> عدم*` (FILE* opaque handle via `fopen`)
  - `اغلق_ملف(ملف) -> صحيح` (fclose)
  - `اقرأ_حرف(ملف) -> حرف` (fgetc + UTF-8 packing)
  - `اكتب_حرف(ملف، حرف) -> صحيح` (fputc)
  - `اقرأ_ملف(ملف، مخزن، عدد_بايتات) -> صحيح` (fread)
  - `اكتب_ملف(ملف، بيانات، عدد_بايتات) -> صحيح` (fwrite)
  - `نهاية_ملف(ملف) -> منطقي` (feof)
  - `موقع_ملف(ملف) -> صحيح` (Linux: ftello / Windows: _ftelli64)
  - `اذهب_لموقع(ملف، موقع) -> صحيح` (Linux: fseeko / Windows: _fseeki64)
  - `اقرأ_سطر(ملف) -> نص` (يعيد `عدم` عند EOF قبل قراءة أي بايت)
  - `اكتب_سطر(ملف، سطر) -> صحيح` (fputs + '\n')
- **Coverage for file I/O**:
  - `tests/integration/backend/backend_file_io_test.baa`
  - `tests/integration/backend/backend_file_io_noheader_test.baa`
  - `tests/neg/semantic_file_open_bad_type.baa`
  - `tests/neg/semantic_file_read_bad_types.baa`
  - `tests/neg/semantic_file_line_bad_arity.baa`

### Changed

- **`نص` nullable support (v0.3.12):** the compiler allows assigning/returning `عدم` as a `نص` value for EOF-style patterns, and allows `==/!=` comparisons between `نص` and `عدم`, as well as `نص == نص` / `نص != نص` (pointer equality).
- **IR lowering:** file I/O builtins are lowered directly to libc stdio calls, respecting shadowing rules.

### Fixed

- **Backend emitter:** handle true `imm64` materialization in AT&T output (`movabsq` + scratch register) for moves/stores and arithmetic/logical ops when an immediate does not fit `imm32`.

## [0.3.11.0] - 2026-03-01

### Added

- **Dynamic memory APIs** (v0.3.11):
  - `حجز_ذاكرة(حجم) -> عدم*` (malloc)
  - `تحرير_ذاكرة(مؤشر) -> عدم` (free)
  - `إعادة_حجز(مؤشر، حجم) -> عدم*` (realloc)
  - `نسخ_ذاكرة(وجهة، مصدر، حجم) -> عدم*` (memcpy)
  - `تعيين_ذاكرة(مؤشر، قيمة، حجم) -> عدم*` (memset)
- **Test coverage for dynamic memory**:
  - `tests/integration/backend/backend_dynamic_memory_test.baa`
  - `tests/integration/backend/backend_dynamic_memory_noheader_test.baa`
  - `tests/neg/semantic_malloc_bad_type.baa`
  - `tests/neg/semantic_free_bad_type.baa`
  - `tests/neg/semantic_voidpp_to_intp_invalid.baa`

### Changed

- **Pointer compatibility** now supports C-like implicit conversions between `عدم*` (void*) and any object pointer type (depth >= 1). `عدم**` is not treated as a universal pointer.
- **IR lowering** recognizes the v0.3.11 memory APIs as builtins and lowers them directly to `malloc/free/realloc/memcpy/memset`, respecting shadowing rules.

## [0.3.10.6] - 2026-02-28

### Added

- **Function pointers (`دالة(...) -> ...`)** end-to-end:
  - variables and parameters of function-pointer type,
  - assign function references to function-pointer variables,
  - indirect call through a function pointer identifier (backend emits `call *...` in AT&T).
- **IR support for indirect calls**:
  - `IRInst.call_callee` for indirect targets,
  - `ir_inst_call_indirect(...)` + `ir_builder_emit_call_indirect(...)`.
- **Coverage for function pointers**:
  - `tests/integration/backend/backend_func_ptr_test.baa`
  - `tests/neg/semantic_funcptr_assign_mismatch.baa`
  - `tests/neg/semantic_funcptr_call_non_funcptr.baa`
  - `tests/neg/semantic_funcptr_compare_order_invalid.baa`

### Changed

- **IR verifier + IR text format** now accept `call` with either a direct `@name` target or an indirect callee value.
- **Global data emission** now supports function pointer initializers as `.quad <symbol>`.

### Fixed

- **IR lowering:** comparisons of function pointers against `عدم` now unify the IR operand types so `--verify-ir` does not fail on signature-mismatched zero constants.
- **Semantic analysis / AST metadata:** `Node.inferred_func_sig` is now stored as a cloned (owned) signature instead of borrowing a local-scope symbol signature that can be freed at `scope_pop()`.
- **Parser:** prevented aliases that resolve to a function-pointer type from being used inside another function-pointer signature (blocks unsupported higher-order signatures at parse time).
- **IR text:** indirect calls are now printed/parsed as `call <ret> <callee>(...)` (with `callee` inside `<>`) to disambiguate from direct calls and to support round-tripping when the callee becomes a function value like `@name`.
- **Semantic calls:** name resolution for `name(...)` now prefers scoped symbols (function-pointer variables) before global functions, so shadowed function-pointer calls behave correctly.

## [0.3.10.5] - 2026-02-27

### Added

- **Explicit cast syntax (`كـ<...>(...)`)** end-to-end in parser/semantic/IR lowering.
- **Cast AST/token plumbing**:
  - `TOKEN_CAST` for keyword `كـ`.
  - `NODE_CAST` with target-type metadata (including pointer base/depth).
- **Semantic cast validation** for:
  - numeric conversions (`int/float/char`),
  - pointer-to-pointer casts,
  - pointer-to-integer and integer-to-pointer casts,
  - guarded diagnostics for unsupported targets (`عدم`, `هيكل`, `اتحاد`) and invalid source/target pairs.
- **Pointer difference semantics (`pointer - pointer`)** returning element-distance (`صحيح`) for compatible pointer types.
- **Coverage expansion for casts + pointer difference**:
  - `tests/integration/backend/backend_cast_pointer_diff_test.baa`
  - `tests/neg/semantic_cast_to_void_unsupported.baa`
  - `tests/neg/semantic_cast_float_to_pointer_invalid.baa`
  - `tests/neg/semantic_cast_pointer_to_float_invalid.baa`
  - `tests/neg/semantic_pointer_diff_incompatible.baa`

### Changed

- **IR verifier cast rules** now accept pointer/integer cast forms alongside existing numeric conversions.
- **Language docs/roadmap status** synchronized to mark v0.3.10.5 as implemented.

## [0.3.10] - 2026-02-25

### Added

- **Pointer core support (v0.3.10)** across parser, semantic analysis, and IR lowering:
  - Pointer type declarations using suffix stars (e.g. `صحيح*`, `هيكل نوع*`, alias-based pointers).
  - Address-of operator `&` and dereference operator `*` in expression context.
  - Dereference assignment statement form: `*ptr = expr.`
  - Null pointer literal via `عدم` in expression context.
  - Core pointer arithmetic/comparison validation (`pointer +/- int`, pointer comparisons).
- **AST/Symbol pointer metadata plumbing** for declarations, aliases, members, arrays, inferred expression types, and function signatures.
- **Pointer test coverage**:
  - `tests/integration/backend/backend_pointer_core_test.baa`
  - `tests/neg/semantic_deref_non_pointer.baa`
  - `tests/neg/semantic_addr_of_non_lvalue.baa`
  - `tests/neg/semantic_pointer_arith_non_integer.baa`
  - `tests/neg/semantic_pointer_compare_invalid.baa`

### Changed

- **`عدم` contextual parsing**: remains a type keyword in type position and is now also accepted as a null-pointer literal in expression position.
- **IR lowering type mapping**: `TYPE_POINTER` now lowers to IR pointer types and participates in pointer-aware unary/binary lowering paths.

## [0.3.9] - 2026-02-25

### Added

- **Multi-dimensional arrays** — parser, semantic analysis, and IR lowering now support fixed-size multi-dimensional declarations and index chains (row-major linearization) for local/global/static storage.
- **Optional debug bounds checks for array indices** — IR lowering can emit runtime bounds-guard paths in debug-oriented lowering mode.
- **String operations API (C-like semantics, Arabic names)**:
  - `طول_نص(نص) -> صحيح`
  - `قارن_نص(نص، نص) -> صحيح`
  - `نسخ_نص(نص) -> نص` (heap-backed)
  - `دمج_نص(نص، نص) -> نص` (heap-backed)
  - `حرر_نص(نص) -> عدم`
- **Standard library artifacts (v0.3.9)**:
  - `stdlib/baalib.baahd`
  - `stdlib/baalib.baa`
- **Array coverage expansion tests**:
  - `tests/integration/backend/backend_multidim_array_test.baa`
  - `tests/neg/semantic_array_rank_mismatch.baa`
- **String/array-operator coverage tests**:
  - `tests/integration/backend/backend_string_ops_test.baa`
  - `tests/integration/backend/backend_array_length_operator_test.baa`
  - `tests/neg/semantic_strlen_bad_arity.baa`
  - `tests/neg/semantic_strlen_bad_type.baa`
  - `tests/neg/semantic_free_bad_type.baa`

### Changed

- **Array AST/Symbol metadata** — array nodes/symbols now track rank + per-dimension sizes + total element count instead of a single fixed size.
- **Array type support** — fixed-size arrays are no longer restricted to `صحيح`; declarations now carry element-type metadata and validate compound element layouts.
- **`حجم(المصفوفة)` array-length idiom formalized** — `حجم(قائمة) / حجم(نوع_العنصر)` is now the documented C-like pattern for fixed-size array length.
- **IR lowering API** — `ir_lower_program` now accepts `enable_bounds_checks` to control optional debug bounds instrumentation:
  - `IRModule* ir_lower_program(Node* program, const char* module_name, bool enable_bounds_checks);`

### Fixed

- **Array index rank diagnostics** — semantic analysis now reports index-count mismatch against declared array rank (e.g., wrong number of `[]` indices).
- **Global pointer-array emission** — emitter now correctly serializes string pointer initializers in global arrays (`.quad .Lstr_N` / `.quad .Lbs_N` as applicable).
- **Indexed struct-member array access path** — constant-index member access on array elements (e.g., `مصفوفة_هيكل[i]:حقل`) now resolves to the correct byte offset in semantic analysis.
- **Builtin string-call diagnostics** — semantic analysis now type-checks and arity-checks `طول_نص/قارن_نص/نسخ_نص/دمج_نص/حرر_نص` even without explicit user function declarations.

## [0.3.8.1] - 2026-02-25

### Changed

- **Test framework cleanup** — reorganized integration tests under:
  - `tests/integration/backend/*.baa`
  - `tests/integration/ir/*.baa`
- **Baa-only QA flow** — `scripts/qa_run.py --mode full` now runs integration + regression + verify smoke + multi-file smoke (removed legacy C-unit stage).
- **README test documentation** — synchronized test commands and directory layout with the new organized framework.

### Removed

- **Legacy C-unit test framework** — removed standalone `tests/*.c` unit tests from the active repository test suite.
- **Tracked generated IR artifact** — removed `tests/ir_test.ir` from versioned test inputs.

## [0.3.8] - 2026-02-25

### Added

- **Unified QA runner** — added `scripts/qa_run.py` with tiered modes:
  - `--mode quick`: integration smoke (`tests/test.py`)
  - `--mode full`: integration + regression + selected C unit tests + verify smoke + multi-file smoke
  - `--mode stress`: full mode + `tests/stress/*.baa` + seeded fuzz-lite
- **Stress test suite**:
  - `tests/stress/stress_deep_scopes.baa`
  - `tests/stress/stress_symbol_volume.baa`
  - `tests/stress/stress_utf8_identifiers.baa`
- **Multi-file regression fixtures**:
  - `tests/fixtures/multifile_counter_main.baa`
  - `tests/fixtures/multifile_counter_lib.baa`
- **New negative diagnostics coverage**:
  - `tests/neg/lexer_bad_escape_in_string.baa`
  - `tests/neg/lexer_bad_escape_in_char.baa`
  - `tests/neg/parser_decl_qualifier_missing_type.baa`
- **CI/CD workflows**:
  - `.github/workflows/ci.yml` (Windows + Linux staged gates: build -> quick -> full)
  - `.github/workflows/stress-nightly.yml` (nightly Linux stress)

### Changed

- **Test metadata contract** — `tests/test.py` and `tests/regress.py` now support `// RUN:` markers (e.g., `expect-pass`, `expect-fail`, `runtime`, `compile-only`, `skip`).
- **Integration runner flexibility** — `tests/test.py` now honors per-test `// FLAGS:` and runtime intent through `// RUN:`.
- **Regression runner semantics** — `tests/regress.py` now honors `// RUN:` in negative tests while preserving `// EXPECT:` marker checks for expected-fail cases.

### Fixed

- **QA stability for stress fuzzing** — fuzz-lite templates were constrained to bounded-failure patterns to prevent parser-hang style cases during nightly QA.

## [0.3.7.5] - 2026-02-25

### Added

- **`ساكن` keyword support** — added `TOKEN_STATIC` with parser support for declaration qualifiers in both orders: `ثابت ساكن` and `ساكن ثابت`.
- **Static local storage lowering** — local `ساكن` variables/arrays now lower to internal IR globals with unique labels (`__baa_static_<func>_<name>_<id>`), preserving values across calls.
- **IR global linkage metadata** — added `IRGlobal.is_internal` and IR text support for `internal` globals.
- **Coverage for static storage**:
  - Integration test: `tests/backend_static_storage_test.baa`
  - Negative tests: `tests/neg/semantic_static_local_nonconst_init.baa`, `tests/neg/parser_static_function_unsupported.baa`

### Changed

- **Semantic constant-initializer rules** — static-storage objects (global or `ساكن` local) now require compile-time-constant initializers when provided.
- **Const initialization policy** — `ثابت` with static storage no longer requires explicit initializer and defaults to zero (C-like static storage semantics).
- **Emitter linkage directives** — globals with internal linkage are no longer emitted as global exports; ELF output now emits `.local` for internal symbols.
- **IR text grammar** — accepts and emits `internal const global` / `internal global` forms.
- **Regression expectation update** — `tests/neg/semantic_global_array_nonconst_init.baa` marker updated to the static-storage diagnostic wording.

### Fixed

- **Static local memory model** — assignments/loads/member and array accesses now correctly address static-backed locals through global-symbol storage instead of stack allocas.

## [0.3.7] - 2026-02-25

### Added

- **Assembly readability flag** — added `--asm-comments` to emit explanatory Arabic comments in generated assembly (function entry, block labels, prologue/epilogue details).
- **Parser recovery modes** — added context-aware synchronization modes for statement/declaration/switch parsing to reduce cascading syntax diagnostics.
- **Lexer UTF-8 validator** — added strict UTF-8 sequence validation for string literals, character literals, and identifiers.
- **Semantic lookup acceleration** — added hash-indexed lookup paths for local/global symbol resolution in `analysis.c` while preserving deterministic behavior.

### Changed

- **Diagnostics normalization** — parser and lexer diagnostics were migrated toward centralized `error_report(...)` style behavior with Arabic-first messaging.
- **Parser initialization order** — parsing now initializes diagnostic source context before consuming initial tokens.
- **Negative diagnostics coverage** — updated affected negative tests under `tests/neg/` to match normalized semantic/parser diagnostic text.

### Fixed

- **Parser hard-abort paths** — replaced parser `exit(1)` array-type mismatch aborts with recoverable diagnostics and synchronization.
- **UTF-8 literal edge handling** — corrected multi-byte UTF-8 copy/advance flow in literal lexing paths and reject invalid sequences consistently.

## [0.3.6.5] - 2026-02-24

### Added

- **Type aliases (`نوع`) (v0.3.6.5)** — global type aliases are now supported with C-like "declare before use" behavior:
  - Syntax: `نوع <name> = <existing_type>.`
  - Supported targets in this milestone: existing implemented types (primitive + `تعداد`/`هيكل`/`اتحاد`).
  - Alias chaining is supported when prior aliases are already declared.
- **Type-alias diagnostics** — strict collision checks between alias names and symbols/functions, with dedicated semantic errors.
- **Numeric semantic diagnostics completion (v0.3.5.5)**:
  - `-Wimplicit-narrowing` for potentially lossy implicit numeric conversions (declarations, assignments, returns, call arguments, member/array assignments).
  - `-Wsigned-unsigned-compare` for mixed signed/unsigned comparisons after integer promotions.
- **Coverage** — new integration and negative tests for type aliases:
  - `tests/backend_type_alias_test.baa`
  - `tests/neg/type_alias_*.baa`
- **Warning regression coverage** — new negative diagnostics tests:
  - `tests/neg/semantic_warn_implicit_narrowing_decl.baa`
  - `tests/neg/semantic_warn_implicit_narrowing_return.baa`
  - `tests/neg/semantic_warn_signed_unsigned_compare.baa`

### Changed

- **`نوع` handling is contextual in the parser** — `نوع` can still appear as an identifier/member name in expressions (e.g. `س:نوع`), while being recognized as alias declaration syntax at top-level declarations.
- **Negative test runner** — `tests/regress.py` now accepts per-test compile flags via `// FLAGS:` markers (used for warning-as-error diagnostics tests).

## [0.3.6] - 2026-02-24

### Added

- **Low-level bitwise operators** — frontend/IR/backend support for `&`, `|`, `^`, `~`, `<<`, `>>` across semantic analysis, IR lowering, ISel, regalloc, and emission.
- **`حجم(...)` operator (`sizeof`)** — supports both forms:
  - `حجم(type)` for compile-time type size queries.
  - `حجم(expr)` for expression/type-based size queries when size is semantically known.
- **`عدم` (void) semantic checks**:
  - reject variable declarations of type `عدم` (local/global).
  - reject `عدم` as parameter type.
  - enforce return-shape correctness (`return` with/without value by function return type).
- **IR opcode coverage for low-level ops**:
  - `IR_OP_XOR`, `IR_OP_SHL`, `IR_OP_SHR` + verifier/text/canonicalization support.
- **Integration and negative tests** for low-level semantics:
  - `tests/backend_low_level_ops_test.baa`
  - `tests/neg/semantic_bitwise_non_integer.baa`
  - `tests/neg/semantic_sizeof_void.baa`
  - `tests/neg/semantic_void_var_decl.baa`
  - `tests/neg/semantic_void_return_value.baa`
  - `tests/neg/semantic_nonvoid_empty_return.baa`

### Changed

- **Parser precedence fix** — logical-AND operands now parse from bitwise-or precedence tier (prevents precedence regressions between logical and bitwise operators).
- **Logical unary NOT lowering** — `!` now lowers as compare-to-zero (boolean), while `~` remains bitwise NOT.

### Fixed

- **SCCP fold semantics** — `IR_OP_NOT` constant folding now matches bitwise semantics (`~x`) instead of logical negation.
- **SCCP low-level support** — added constant evaluation for `IR_OP_XOR`, `IR_OP_SHL`, and `IR_OP_SHR` (signed/unsigned-aware right shift behavior).

## [0.3.5.5] - 2026-02-21

### Added

- **Sized integers** — `ص٨/ص١٦/ص٣٢/ص٦٤` and `ط٨/ط١٦/ط٣٢/ط٦٤` with C-like integer promotions and usual arithmetic conversions.
- **Integer literals** — unsuffixed decimal integer literals default to `ص٣٢` when the value fits, otherwise `صحيح/ص٦٤`.
- **Unsigned correctness** — correct signed/unsigned comparisons, casts, and `div/mod` semantics across IR, optimizer, and backend.
- **Float (`عشري`) enhancements** — `+ - * /`, comparisons, and `اطبع` (f64) with working SysV AMD64 + Windows x64 ABI lowering (XMM regs + SysV varargs AL).

### Fixed

- **NaN comparisons** — optimizer no longer folds `cmp x,x` for `f64` (NaN breaks reflexive equality).
- **Global initializers** — typed integer globals are truncated/extended to their declared width during IR lowering.

## [0.3.5] - 2026-02-20

### Added

- **Character type (`حرف`)** — a distinct scalar type representing a single UTF-8 character (1..4 bytes) with a packed internal representation.
- **UTF-8 char literals** — `'أ'` and other multi-byte UTF-8 characters are accepted as a single char literal.
- **String-char relationship** — `نص` is represented as a null-terminated `حرف[]` and supports indexing with `اسم[٠]`.
- **Float type (`عشري`) (storage-only)** — decimal literals and `عشري` variables are supported as 64-bit storage; FP ops/ABI remain deferred.

### Changed

- **Print lowering** — `اطبع <حرف>.` prints the character as UTF-8 (not a numeric code).
- **Type checking** — `حرف` is compatible with `صحيح` for assignments/comparisons (C-like integer promotion behavior).
- **String literals** — string literals lower to a `حرف[]` constant table (not a C `char*`).

### Fixed

- **Backend emission** — `MACH_LOAD`/`MACH_STORE` now use size-correct `mov` suffixes (enables byte/word/dword loads/stores).
- **Zero-extension emission** — `MACH_MOVZX` now emits the correct instruction form for 32→64 (uses `movl`), preventing assembler errors.
- **MOVZX spilling** — `MACH_MOVZX` now handles spilled (memory) destinations by extending into a temp register then storing.
- **Global data emission** — scalar globals use `.byte/.word/.long/.quad` based on IR type (not always `.quad`).

## [0.3.4.5] - 2026-02-19

### Added

- **Union types (اتحاد)** — union declarations, nested unions, and member access via `:` with all members at offset 0.
- **Tagged union pattern** — can be expressed manually using `تعداد` + `هيكل` + `اتحاد`.

### Changed

- **Optimizer inliner** — fixes register mapping on first-use to keep CFG valid under `-O2 --verify` for functions with out-of-order block lists.

## [0.3.4] - 2026-02-18

### Added

- **Enumerations (تعداد)** — enum type definitions with auto-assigned integer values and qualified access `Enum:Member`.
- **Structures (هيكل)** — struct type definitions with nested struct fields and enum fields.
- **Member access and assignment (`:`)** — read/write struct fields via `<var>:<field>` and nested access `<var>:<field>:<subfield>`.
- **Linux packaging** — CPack-based `TGZ` + `DEB` artifacts (see `scripts/package_linux.sh`).

### Changed

- **Type system** — adds `TYPE_ENUM` and `TYPE_STRUCT` to support compound types.

### Fixed

- **Switch parsing** — prevents `case <value>:` from being mis-tokenized as member access when `:` is enabled in expressions.

## [0.3.3] - 2026-02-18

### Added

- **Array initialization (v0.3.3)** — supports initializer lists for fixed-size `صحيح` arrays:
  - Syntax: `صحيح قائمة[٥] = {١، ٢، ٣}.` (accepts Arabic comma `،` and ASCII comma `,`).
  - C-style semantics: partial initialization is allowed and the remaining elements are zero-filled; `{}` zero-initializes the whole array.
  - Global `صحيح` arrays are now supported (emitted in `.data` with correct zero-fill).

### Changed

- **Lexer delimiter support** — Arabic comma `،` is now tokenized as `TOKEN_COMMA` and treated like `,` across parsing.
- **IR globals** — global array initializers are represented and serialized as initializer lists, and the emitter can output array data.

### Fixed

- **Global array element addressing** — avoids mis-lowering global array bases through `cast` (which would read memory instead of taking the address), preventing runtime crashes.

## [0.3.2.9.4] - 2026-02-17

### Added

- **Documentation & cleanup (v0.3.2.9.4)**
  - IR contributor guide: `docs/IR_DEVELOPER_GUIDE.md`.
  - Code review checklist: `docs/CODE_REVIEW_CHECKLIST.md`.

- **Driver refactor modules (v0.3.2.9.4)**
  - CLI parsing + help/version: `src/driver_cli.c`, `src/driver_cli.h`.
  - Per-file compile pipeline: `src/driver_pipeline.c`, `src/driver_pipeline.h`.
  - Toolchain runner (GCC resolve/assemble/link): `src/driver_toolchain.c`, `src/driver_toolchain.h`.
  - Timing helper: `src/driver_time.c`, `src/driver_time.h`.
  - Shared file reader used by the lexer include system: `src/read_file.c`.

### Changed

- **Driver sanity pass** — split the monolithic driver into focused modules (CLI/toolchain/pipeline), removed fixed-size input/argv limits, and centralized `--time-phases` printing.
- **Semantic diagnostics** — semantic analysis now reports errors via `error_report(...)` with `file:line:col` + source caret context.

- **Repo hygiene** — enforce LF line endings via `.gitattributes` to eliminate CRLF/LF diff churn.

- **Cleanup** — removed deprecated legacy AST codegen paths and backend-compare mode.

## [0.3.2.9.3] - 2026-02-17

### Added

- **Regression testing (v0.3.2.9.3)**
  - Regression runner: `tests/regress.py` (runs `tests/test.py` on all hosts).
  - Docs-derived v0.2.x corpus: `scripts/extract_docs_corpus.py` + `tests/corpus_v2x_docs/**/*.baa`.
  - Negative tests: `tests/neg/*.baa` with `// EXPECT:` markers validated by `tests/regress.py`.

## [0.3.2.9.2] - 2026-02-17

### Added

- **Performance benchmarking (v0.3.2.9.2)**
  - Benchmark suite: `bench/compile_*.baa` + `bench/runtime_*.baa`.
  - Benchmark runner: `scripts/bench.py` (compile-only, end-to-end, runtime, and Linux memory mode).
  - Driver flag: `--time-phases` prints per-phase timings and IR arena memory stats (`[TIME]`/`[MEM]`).

## [0.3.2.9.1] - 2026-02-17

### Added

- **Comprehensive IR verification (v0.3.2.9.1)**
  - `--verify` — runs `--verify-ir` + `--verify-ssa` together (requires `-O1`/`-O2`).
  - Test runner: `tests/test.py` compiles + runs all `tests/*.baa` under `-O2 --verify`.
  - New verifier regression: `tests/ir_verify_ir_test.c` rejects cross-function branch targets.

### Changed

- **SSA verifier CFG hygiene** — `ir_func_verify_ssa()` now rebuilds `succ/pred` before dominance/SSA checks to avoid stale CFG metadata.

### Fixed

- **CFG integrity enforcement** — IR verifier now rejects cross-function block references (`br`/`br_cond`/`phi`) before CFG rebuild, preventing illegal CFG mutation across functions.

## [0.3.2.8.6] - 2026-02-17

### Added

- **Aggressive IR optimizations (v0.3.2.8.6)**
  - **InstCombine** — local algebraic/cmp simplifications to `COPY`/const: `src/ir_instcombine.c` (tests: `tests/ir_instcombine_test.c`).
  - **SCCP** — sparse conditional constant propagation + `br_cond` folding: `src/ir_sccp.c` (tests: `tests/ir_sccp_test.c`).
  - **GVN** — dominator-scoped global value numbering to remove redundant pure expressions: `src/ir_gvn.c` (tests: `tests/ir_gvn_test.c`).

### Changed

- **Optimizer pipeline ordering** — `InstCombine` + `SCCP` now run early (after `Canon`) and `GVN` runs at `-O2` (before `CSE`).
- **Mem2Reg promotability** — replaced the “init store must be in the alloca block” rule with a must-def initialization check across the CFG (test: `tests/ir_mem2reg_promote_test.c`).

### Fixed

- **RegAlloc spilled pointer bases** — when a pointer vreg is spilled and later used as the base of a memory operand, the allocator now reloads it into a reserved scratch register before `MACH_LOAD`/`MACH_STORE`, preventing invalid addressing and runtime crashes (covers `tests/backend_regalloc_stress.baa`).

## [0.3.2.8.5] - 2026-02-17

### Added

- **Windows + SysV stack arguments (v0.3.2.8.5)** — calls now support arguments beyond the register-arg limit on both targets.
  - Outgoing call frames are built in ISel (shadow/home space on Windows; stack args on both).
  - Stack-passed parameters are loaded at function entry.
  - Tail calls now support stack arguments conservatively when the current function’s incoming stack-arg area is large enough.
  - Tests: `tests/backend_stackargs_test.baa` (8 args), updated `tests/backend_tailcall_test.baa`.

### Fixed

- **Emitter load/store spill correctness** — `MACH_LOAD`/`MACH_STORE` now avoid illegal mem-to-mem forms after spilling by using a scratch register.

## [0.3.2.8.4] - 2026-02-17

### Added

- **Linux x86-64 target (v0.3.2.8.4)**
  - Native Linux build of the compiler (`baa`) with CMake + GCC/Clang.
  - ELF output support: `.rodata`/`.data`/`.text` + `.note.GNU-stack`.
  - SystemV AMD64 ABI support for register arguments (up to 6).
  - Links using host `gcc` for now.

### Changed

- **No cross-link/cross-assemble yet** — when the selected `--target` does not match the host object format, compilation is allowed only with `-S` (assembly output). Linking/`-c` cross-target is deferred.

## [0.3.2.8.3] - 2026-02-17

### Added

- **Code model options (v0.3.2.8.3)**
  - Flags: `-fPIC`, `-fPIE` (Linux/ELF), `-mcmodel=small` (only supported model)
  - Stack protection on ELF: `-fstack-protector`, `-fstack-protector-all`, `-fno-stack-protector`
  - Test: `tests/emit_stack_protector_test.c`

### Fixed

- **Stack protector label collisions** — canary epilogues now use unique labels for functions with multiple returns.

## [0.3.2.8.2] - 2026-02-17

### Added

- **Calling convention abstraction (v0.3.2.8.2)**
  - Explicit `BaaCallingConv` contract: caller/callee-saved masks, stack alignment, ABI special-vreg mapping (`abi_arg_vreg0`, `abi_ret_vreg`).
  - ABI-aware lowering/alloc/emission entry points are now used end-to-end for `--target`.
  - Tests: `tests/backend_sysv_6args_test.baa`, `tests/isel_callargs_reject_test.c`

### Changed

- **No stack arguments yet** — backend rejects calls that exceed the register-arg count for the selected target (deferred to v0.3.2.8.5).

### Fixed

- **TCO max-args check** — tail-call detection now respects the selected target register-arg count (Windows=4, SysV=6).

## [0.3.2.8.1] - 2026-02-17

### Added

- **Multi-target groundwork (v0.3.2.8.1)**
  - Target descriptor + calling convention model: `src/target.c`, `src/target.h`
  - CLI target selection: `--target=x86_64-windows|x86_64-linux`
  - Target-aware backend entry points: `isel_run_ex(..., target)`, `regalloc_run_ex(..., target)`, `emit_module_ex(..., target)`
  - Native Linux build support (compiler builds as `baa` and links with host `gcc`)
  - Non-Windows updater stub: `src/updater_stub.c`

### Fixed

- **ELF executable-stack warning** — emitter now outputs `.note.GNU-stack` section when emitting ELF.

## [0.3.2.7.3] - 2026-02-17

### Added

- **Tail call optimization (v0.3.2.7.3)** — at `-O2`, recognizes `call` immediately followed by `ret` (<= 4 args) and lowers it to a tail jump to reuse the caller stack frame.
  - Tests: `tests/backend_tailcall_test.baa`, `tests/isel_tailcall_test.c`

## [0.3.2.7.2] - 2026-02-16

### Added

- **IR inlining (v0.3.2.7.2)** — inlines small internal functions with a single call site at `-O2` before Mem2Reg.
  - Implementation: `src/ir_inline.c`, `src/ir_inline.h`
  - Tests: `tests/ir_inline_test.c`, `tests/backend_inline_test.baa`

### Changed

- `-O2` optimizer now runs inlining before Mem2Reg; existing passes act as post-inline cleanup.

### Fixed

- **IR lowering typed pointer operands** — local load/store pointer operands now carry pointer types, preventing `--verify-ir` failures when Mem2Reg cannot promote the slot (exposed by inlining).
- **Inliner unique block labels** — inlined continuation/cloned blocks now use unique labels to avoid duplicate-label collisions in IR dumps and backend lowering.

## [0.3.2.7.1] - 2026-02-16

### Added

- **Loop optimizations (v0.3.2.7.1)**
  - **Loop detection (natural loops)** — back-edge detection via dominance + natural loop set construction: `src/ir_loop.c`, `src/ir_loop.h`.
  - **LICM** — conservative hoisting of pure, non-trapping loop-invariant instructions to preheaders: `src/ir_licm.c`, `src/ir_licm.h`.
  - **Strength reduction** — instruction selection reduces `ضرب` by power-of-two constants inside loops to `shl`.
  - **Loop unrolling (optional)** — `-funroll-loops` performs conservative full unrolling after Out-of-SSA: `src/ir_unroll.c`, `src/ir_unroll.h`.

### Testing

- Added loop analysis unit test: `tests/ir_loop_test.c`.
- Added LICM unit test: `tests/ir_licm_test.c`.
- Added unroll unit test: `tests/ir_unroll_test.c`.

### Fixed

- **Emitter constraint fix for comparisons** — `cmp`/`test` now avoid illegal memory-to-memory forms by using a scratch register during emission.

## [0.3.2.6.7] - 2026-02-14

### Fixed

- **SSA verification failure in switch statements** — Fixed a bug where CSE replacement did not respect dominance, causing "value not dominated by definition" errors when optimizing `switch` with `default` cases.
  - Added dominance check to `ir_cse.c` before replacing expressions.

## [0.3.2.6.6] - 2026-02-14

### Added

- **IR Data Layout Module (`src/ir_data_layout.h/c`)** — Implements type size and alignment queries for `Target` abstraction (currently Windows x86-64).
  - `ir_type_size_bytes()`, `ir_type_alignment()`, `ir_type_store_size()`.
  - Type predicates: `ir_type_is_integer()`, `ir_type_is_pointer()`.

### Changed

- **Hardened Arithmetic Semantics** — Formalized IR behavior to eliminate undefined behavior in C backend:
  - **Two's complement wrap**: Overflow/underflow is now strictly defined as wrapping.
  - **Safe Division**: `INT64_MIN / -1` wraps to `INT64_MIN` (cpu-safe).
  - **Safe Modulo**: `INT64_MIN % -1` results in `0`.
  - **Use of `uint64_t` internally** to guarantee C standard compliance for signed operations.

### Testing

- Added **Data Layout Unit Test** (`tests/ir_data_layout_test.c`) verifying all type sizes, alignments, and arithmetic edge cases (44/44 pass).

## [0.3.2.6.5] - 2026-02-13

### Added

- **IR well-formedness verification (`--verify-ir`)** — validates operand counts, type consistency, terminator rules, phi placement, and intra-module call signatures.
- **Verifier gate (`--verify-gate`)** — debug mode to run IR/SSA verification after each optimizer iteration to catch pass bugs early (**requires `-O1`/`-O2`**).
- **Canonicalization pass** — normalizes commutative operand ordering, constant placement, and comparison forms to improve downstream optimizations.
- **CFG simplification pass** — removes redundant conditional branches, merges trivial blocks (conservatively around phi), and exposes a reusable critical-edge splitting helper.

### Testing

- Added IR verifier + canonicalization + CFG simplify unit test: [`tests/ir_verify_ir_test.c`](tests/ir_verify_ir_test.c)

## [0.3.2.6.4] - 2026-02-12

### Fixed

- **Register allocation liveness across loops** — Machine CFG successors are now linked during ISel so liveness propagates across loop back-edges correctly.
- **Assembly emission for spilled copies** — `mov` memory-to-memory is now lowered via `%rax` scratch during emission to avoid invalid `movq mem, mem`.
- **Block-level scoping in semantic analysis** — local symbols are now scoped to blocks/branches/loops so for-init variables and inner-block declarations don't leak into the whole function.

### Testing

- Added regalloc stress integration test: [`tests/backend_regalloc_stress.baa`](tests/backend_regalloc_stress.baa)
- Added semantic scoping integration test: [`tests/backend_scope_test.baa`](tests/backend_scope_test.baa)

## [0.3.2.6.3] - 2026-02-11

### Added

- **IR text serialization (write/read)** — new machine-readable IR writer + reader:
  - Writer: [`ir_text_write_module()`](src/ir_text.c:1) / [`ir_text_dump_module()`](src/ir_text.c:1)
  - Reader: [`ir_text_read_module_file()`](src/ir_text.c:1)
  - Header: [`src/ir_text.h`](src/ir_text.h)

### Testing

- Added round-trip unit test: [`tests/ir_serialize_test.c`](tests/ir_serialize_test.c)

## [0.3.2.6.2] - 2026-02-11

### Added

- **Debug information pipeline (`--debug-info`)** — propagates source locations from tokens → AST → IR → MachineInst and emits GAS `.file`/`.loc` directives in assembly.
- **Variable name preservation (lightweight)** — preserves a best-effort `dbg_name` for variable-related IR instructions and emits readable `# متغير: ...` breadcrumbs in assembly when debug info is enabled.

### Changed

- Driver passes `-g` to the toolchain when `--debug-info` is enabled.

## [0.3.2.6.1] - 2026-02-11

### Added

- **IR arena allocator + bulk destruction (ساحة IR + تدمير دفعي)** — IR objects (types/values/insts/blocks/functions/globals/strings) are now allocated from a module-owned arena and released in one shot by `ir_module_free()`.
- **IR cloning** — deep clone support for IR functions (`src/ir_clone.c` / `src/ir_clone.h`).
- **Def-use chains for SSA regs** — build Def-Use caches to enable fast register use replacement without whole-function rescans (`src/ir_defuse.c` / `src/ir_defuse.h`).
- **IR mutation helpers** — new utilities in `src/ir_mutate.c` / `src/ir_mutate.h` to insert/remove instructions consistently while keeping block instruction counts correct.
- **Stable instruction IDs** — IR instructions now carry a per-function deterministic `id` assigned on insertion.

### Changed

- IR passes now set the active IR module context (`ir_module_set_current()`) so any newly created IR nodes are allocated in the correct arena.

## [0.3.2.6.0] - 2026-02-09

### Changed

- **Codebase soldering (مرحلة تلحيم القاعدة)** — hardening + hygiene without adding new language/IR features:
  - CMake two-tier warnings (`-Wall -Wextra -Wformat=2 -Wshadow`) with optional `-Werror` toggle.
  - Safer driver command construction (bounded appends instead of `sprintf/strcpy/strcat`).
  - Symbol-table name overflow guards in semantic analysis.
  - Updater version parsing hardened to support multi-part versions (up to 5 segments) + safer path building.
  - Parser integer parsing switched from `atoi` to checked `strtoll`.

## [0.3.2.5.3] - 2026-02-09

### Added

- **SSA verification (التحقق من SSA)** — validates SSA invariants after Mem2Reg and before Out-of-SSA:
  - New module: [`src/ir_verify_ssa.c`](src/ir_verify_ssa.c:1), header: [`src/ir_verify_ssa.h`](src/ir_verify_ssa.h:1).
  - New driver flag: `--verify-ssa` (requires `-O1`/`-O2`), integrated in [`src/main.c`](src/main.c:1).
  - Unit test: [`tests/ir_verify_ssa_test.c`](tests/ir_verify_ssa_test.c:1).

## [0.3.2.5.2] - 2026-02-09

### Added

- **Canonical Mem2Reg (ترقية الذاكرة إلى سجلات) with Phi insertion + SSA renaming** — upgrades `ir_mem2reg_run()` to insert `فاي` at dominance frontiers and rewrite `حمل/خزن` into SSA values across blocks:
  - Updated pass: [`ir_mem2reg_run()`](src/ir_mem2reg.c:1).
  - New tests: [`tests/ir_mem2reg_phi_test.c`](tests/ir_mem2reg_phi_test.c:1).
- **Out-of-SSA pass (الخروج من SSA)** — removes `فاي` before backend by inserting edge copies and splitting critical edges:
  - New pass: [`ir_outssa_run()`](src/ir_outssa.c:1).
  - Wired in driver before ISel in [`src/main.c`](src/main.c:1).
  - Unit test: [`tests/ir_outssa_test.c`](tests/ir_outssa_test.c:1).

## [0.3.2.5.1] - 2026-02-09

### Added

- **Baseline Mem2Reg pass (ترقية الذاكرة إلى سجلات)** — promotes simple single-block allocas by rewriting `حمل`/`خزن` into SSA `نسخ`:
  - New pass: [`ir_mem2reg_run()`](src/ir_mem2reg.c:1), descriptor: [`IR_PASS_MEM2REG`](src/ir_mem2reg.c:1).
  - Integrated into optimizer pipeline before constfold/copyprop/CSE/DCE in [`ir_optimizer_run()`](src/ir_optimizer.c:73).
  - Unit test: [`tests/ir_mem2reg_test.c`](tests/ir_mem2reg_test.c:1).

## [0.3.2.4-IR-FIX] - 2026-02-08

### Fixed

- **ISel logical operator size mismatch (إصلاح حجم العمليات المنطقية)** in [`src/isel.c`](src/isel.c):
  - Boolean comparison results (i1) produced 8-bit vregs, but `andq`/`orq` require 64-bit operands — caused assembler error `%r10b not allowed with andq`.
  - `isel_lower_logical()` now forces 64-bit operand size and widens any small vreg operands before emitting AND/OR/NOT machine instructions.
- **Function parameter ABI copies missing (إصلاح نسخ معاملات الدوال)** in [`src/isel.c`](src/isel.c):
  - Callee functions never copied ABI registers (RCX, RDX, R8, R9) to their parameter vregs — all function arguments read as garbage.
  - `isel_lower_func()` now prepends MOV instructions at the entry block to copy from ABI physical registers to parameter virtual registers.
- **IDIV RAX constraint violation (إصلاح قيد RAX في القسمة)** in [`src/isel.c`](src/isel.c):
  - Integer division used an arbitrary vreg for the dividend, but x86 `IDIV` requires the dividend in RAX — caused wrong results (e.g., 30/12 → 30 instead of 2).
  - `isel_lower_div()` now explicitly moves the dividend to RAX (vreg -2), performs CQO + IDIV, then moves the quotient from RAX to the destination vreg.

### Added

- **Comprehensive backend test (اختبار الخلفية الشامل)** — [`tests/backend_test.baa`](tests/backend_test.baa):
  - 27 test functions covering 63 assertions across all supported language features.
  - Tests: arithmetic (+, -, *, /), unary negation, all 6 comparisons, logical AND/OR, local/global variables, constants, if/else/elseif, while/for loops, break/continue, switch/case/default, function calls (0–4 args), recursion (factorial, fibonacci), nested calls, print, edge cases.
  - All 63 tests PASS with exit code 0.

## [0.3.2.4-setup] - 2026-02-08

### Added

- **Bundled GCC toolchain (تضمين أدوات GCC)** — Installer now ships a MinGW-w64 GCC distribution so users don't need a separate install:
  - Updated [`setup.iss`](setup.iss) to include `gcc\*` files under `{app}\gcc`.
  - Adds both `{app}` and `{app}\gcc\bin` to the system PATH (with duplicate-entry guards).
  - Post-install verification: warns if `gcc.exe` was not deployed successfully.
- **Automatic GCC resolution (اكتشاف GCC التلقائي)** in [`src/main.c`](src/main.c):
  - New `resolve_gcc_path()` — at startup, looks for `gcc.exe` relative to `baa.exe` (`gcc\bin\gcc.exe` or `..\gcc\bin\gcc.exe`), falls back to system PATH.
  - Assemble and link commands now use `get_gcc_command()` instead of hardcoded `"gcc"`.
- **GCC bundle preparation script** — [`scripts/prepare_gcc_bundle.ps1`](scripts/prepare_gcc_bundle.ps1):
  - Downloads WinLibs MinGW-w64 GCC 14.2 release.
  - Extracts only the necessary subdirectories (`bin`, `lib`, `libexec`, `x86_64-w64-mingw32`).
  - Verifies `gcc.exe` and reports bundle size.

### Changed

- **Installer metadata:** CompanyName / AppPublisher updated to "Omar Aglan"; all version strings synced to `0.3.2.4`.
- **[`src/baa.rc`](src/baa.rc):** FILEVERSION, PRODUCTVERSION, and string table updated to `0.3.2.4`.

## [0.3.2.4-LINK-FIX2] - 2026-02-08

### Changed

- **Register allocator refactor (إعادة هيكلة مخصص السجلات)** — Major cleanup and restructuring of [`src/regalloc.c`](src/regalloc.c):
  - Consistent brace style and spacing throughout, stronger NULL checks, clearer bitset helpers.
  - Extracted `interval_crosses_call()` as a standalone static helper (replaced invalid C++ nested function syntax with proper C11).
  - Improved call-aware spill decisions: intervals crossing CALL sites now prefer callee-saved registers to avoid incorrect clobbers; spill heuristic skips freeing caller-saved registers when a cross-call interval needs allocation.
  - Minor adjustments to allocation order handling and active interval management.
- **Tests:** Updated `ir_test.baa` constant from `٣` to `5`; regenerated corresponding `.s`/`.o` test artifacts.

## [0.3.2.4-LINK-FIX] - 2026-02-08

### Added

- **Windows x64 call ABI compliance (توافق ABI لاستدعاءات Win64)** in [`src/emit.c`](src/emit.c):
  - Shadow space (32 bytes) is now allocated before every CALL and argument home slots are written (`movq %rcx/rdx/r8/r9` → `0/8/16/24(%rsp)`), required by variadic/library callees such as `printf` and `scanf`.
- **Safe string emission (تهريب النصوص)** in [`src/emit.c`](src/emit.c):
  - New `emit_gas_escaped_string()` helper properly escapes quotes, backslashes, control characters, and non-printable bytes when emitting `.asciz` directives; prevents broken assembly output from string literals containing special characters.
- **Explicit format-string lowering (خفض صيغة الطباعة/القراءة)** in [`src/ir_lower.c`](src/ir_lower.c):
  - `lower_print()` now selects `"%d\n"` or `"%s\n"` based on value type and emits `printf(fmt, value)` with two arguments.
  - `lower_read()` now emits `scanf("%d", &var)` with an explicit format string argument.
- **Call-aware register allocation (تخصيص السجلات مع إدراك الاستدعاءات)** in [`src/regalloc.c`](src/regalloc.c):
  - Added `phys_reg_is_caller_saved()` helper (RAX, RCX, RDX, R8, R9, R10, R11).
  - Linear scan now builds a call-position table from the instruction map, reserves ABI registers (RAX, RCX, RDX, R8, R9) from general allocation, and steers intervals that cross CALL sites toward callee-saved registers.

### Changed

- **Tests:** Updated generated assembly/object artifacts to reflect the new ABI shadow-store and string table changes.

## [0.3.2.4] - 2026-02-08

### Changed

- **Backend integration (تكامل الخلفية)** — The default compilation pipeline is now fully IR-based end-to-end:
  - AST → IR Lowering → Optimizer → Instruction Selection → Register Allocation → Code Emission → Assembly.
  - Driver implementation: [`main()`](src/main.c:209).
- **Build system:** Retired legacy AST-based backend from the build.
- **Version:** Updated compiler version string via [`BAA_VERSION`](src/baa.h:18).
- **Docs:** Updated pipeline documentation and API notes in [`docs/INTERNALS.md`](docs/INTERNALS.md:1) and [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md:1).

## [0.3.2.3] - 2026-02-08

### Added

- **Code emission (إصدار كود التجميع)** — Final backend stage converting machine IR to x86-64 AT&T assembly:
  - `emit_module()` — Top-level entry point for emitting complete assembly file.
  - `emit_func()` — Emits single function with prologue/epilogue.
  - `emit_inst()` — Translates individual machine instructions to AT&T syntax.
  - Function prologue generation:
    - Stack frame setup (`push %rbp; mov %rsp, %rbp`)
    - Local stack allocation with 16-byte alignment
    - Callee-saved register preservation (RBX, RSI, RDI, R12-R15)
  - Function epilogue generation:
    - Callee-saved register restoration
    - Stack frame teardown (`leave; ret`)
  - Instruction emission for all machine opcodes:
    - Data movement: MOV, LEA, LOAD, STORE
    - Arithmetic: ADD, SUB, IMUL, NEG, CQO, IDIV
    - Comparison: CMP, TEST, SETcc (6 variants), MOVZX
    - Logical: AND, OR, NOT, XOR
    - Control flow: JMP, JE, JNE, CALL, RET
    - Stack: PUSH, POP
  - Data section emission:
    - `.rdata` section: format strings (fmt_int, fmt_str, fmt_scan_int)
    - `.data` section: global variables with initializers
    - String table: `.Lstr_N` labels for string literals
  - Windows x64 ABI compliance:
    - Shadow space allocation (32 bytes) before calls
    - RIP-relative addressing for globals
    - Function name translation (الرئيسية → main, اطبع → printf, اقرأ → scanf)
  - Entry point: [`emit_module()`](src/emit.c), header: [`src/emit.h`](src/emit.h).

### Changed

- **Build system:** Added `src/emit.c` to [`CMakeLists.txt`](CMakeLists.txt).
- **Driver pipeline:** Integrated code emission into main compilation flow in [`src/main.c`](src/main.c):
  - Pipeline now: IR → Optimizer → Instruction Selection → Register Allocation → **Code Emission** → Assembly file
  - Replaced legacy AST-based codegen with new IR-based backend

### Technical Details

- AT&T syntax: operand order is `source, destination` (opposite of Intel syntax)
- Register naming: `%rax`, `%rcx`, etc. with `%` prefix
- Size suffixes: `q` (64-bit), `l` (32-bit), `w` (16-bit), `b` (8-bit)
- Callee-saved register detection: scans all instructions to determine which registers need preservation
- Stack alignment: ensures 16-byte alignment after prologue for Windows x64 ABI
- Redundant move elimination: skips `mov %reg, %reg` instructions

## [0.3.2.2] - 2026-02-07

### Added

- **Register allocation (تخصيص السجلات)** — virtual register to physical register mapping for x86-64:
  - `PhysReg` enum — 16 x86-64 physical registers (RAX through R15, RSP/RBP always reserved).
  - `LiveInterval` struct — per-vreg live range with start/end positions, assigned physical register, spill info.
  - `BlockLiveness` struct — per-block def/use/live-in/live-out bitsets (uint64_t arrays).
  - `RegAllocCtx` struct — full allocation context: instruction numbering, liveness, intervals, vreg-to-phys mapping, spill tracking.
  - Liveness analysis pipeline:
    - `regalloc_number_insts()` — sequential instruction numbering with inst_map.
    - `regalloc_compute_def_use()` — per-block def/use bitset computation (handles two-address form).
    - `regalloc_compute_liveness()` — iterative dataflow to fixpoint (live_out = union of successors' live_in).
    - `regalloc_build_intervals()` — constructs live intervals from liveness sets.
  - Linear scan register allocation:
    - Allocation order: caller-saved first (R10, R11), then general (RSI, RDI), then callee-saved (RBX, R12-R15), then ABI regs (RAX, RCX, RDX, R8, R9).
    - Spill on register pressure: evicts longest-lived interval to stack.
    - Special vreg resolution: vreg -1→RBP, -2→RAX, -10→RCX, -11→RDX, -12→R8, -13→R9.
  - Operand rewrite: replaces all VREG operands with physical register numbers; spilled vregs become MEM `[RBP+offset]`.
  - Entry point: [`regalloc_run()`](src/regalloc.c), header: [`src/regalloc.h`](src/regalloc.h).
- **Tests:** Added [`tests/regalloc_test.c`](tests/regalloc_test.c) — 8 test suites, 51 assertions covering allocation, liveness, spilling, register pressure, and division constraints.

### Changed

- **Build system:** Added `src/regalloc.c` to [`CMakeLists.txt`](CMakeLists.txt).

## [0.3.2.1] - 2026-02-07

### Added

- **Instruction selection (اختيار التعليمات)** — IR → Machine IR lowering for x86-64:
  - `MachineOp` enum with 30+ x86-64 opcodes: ADD, SUB, IMUL, IDIV, NEG, CQO, MOV, LEA, LOAD, STORE, CMP, TEST, SETcc, MOVZX, AND, OR, NOT, XOR, JMP, JE, JNE, CALL, RET, PUSH, POP, NOP, LABEL, COMMENT.
  - `MachineOperand` with kinds: VREG, IMM, MEM (base+offset), LABEL, GLOBAL, FUNC.
  - `MachineInst`, `MachineBlock`, `MachineFunc`, `MachineModule` data structures (doubly-linked lists).
  - Instruction selection patterns:
    - Binary ops (add/sub/mul): `mov dst, lhs; op dst, rhs` with immediate inlining.
    - Division: `mov tmp, divisor; mov dst, dividend; cqo; idiv tmp` (temp reg for immediate divisor).
    - Comparison: `cmp lhs, rhs; setCC dst8; movzx dst64, dst8` (temp reg for immediate LHS).
    - Alloca: `lea dst, [rbp - offset]` with stack size tracking.
    - Load/Store: `mov dst, [ptr]` / `mov [ptr], val` (immediate-to-memory optimization).
    - Conditional branch: `test cond, cond; jne true_label; jmp false_label`.
    - Call: Windows x64 ABI — args in vregs -10..-13 (RCX/RDX/R8/R9), push for 5+.
    - Return: `mov %ret, val; ret` (vreg -2 = RAX placeholder).
    - Phi: NOP placeholder (deferred to register allocation).
    - Cast: MOVZX for widening, MOV for same-size/narrowing.
  - Full print system for Machine IR debugging.
  - Entry point: [`isel_run()`](src/isel.c:987), header: [`src/isel.h`](src/isel.h).
- **Tests:** Added [`tests/isel_test.c`](tests/isel_test.c) — 8 test suites, 56 assertions covering all instruction patterns.

### Changed

- **Build system:** Added `src/isel.c` to [`CMakeLists.txt`](CMakeLists.txt).

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
  - Driver now runs: Parse → Analyze → **Lower IR** (IR→backend integration was still pending at the time) in [`src/main.c`](src/main.c:325).

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
- Fixed state leakage between compilation units in the old backend.
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
- **Shared Symbol Definitions** — Moved `Symbol` and `ScopeType` to `src/baa.h` to allow sharing between Analysis and later compilation stages.

### Changed

- **Compiler Pipeline** — Updated `src/main.c` to invoke the analyzer after parsing. Compilation now aborts immediately if semantic errors are found.
- **Code Generator** — Refactored the (then-current) backend to rely on the shared symbol definitions.

### Technical Details

- Analyzer maintains separate symbol tables from later stages (isolation).
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
- **Register Names** — Fixed double-percent typo in register names in assembly output.

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
- Implemented string table generation

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

- Implemented label generation and conditional jumps in assembly generation
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
