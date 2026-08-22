# Baa Test Framework

This directory contains the Baa-only test framework.

## Layout

```text
tests/
├── integration/
│   ├── backend/   # compile + runtime integration tests
│   └── ir/        # compile-focused IR surface tests
├── neg/           # expected-fail diagnostics tests
├── stress/        # stress tests (large inputs / scale)
├── snapshots/     # deterministic IR/QA snapshot contracts
├── fixtures/      # include files and multi-file fixtures
├── corpus_docs/   # extracted examples from docs
├── corpus_v2x_docs/ # historical docs corpus by version
├── test_arabic_numerals.py # Arabic numeral IR-output regression coverage
├── test_examples.py # public examples compile gate
├── test_fast_check.py # --check parser/semantic no-output coverage
├── test_function_capacity.py # tooling-scale module function-capacity guard
├── test_header_self_check.py # --check-header parser/semantic no-output coverage
├── test_integration_artifacts.py # ecosystem/tooling integration docs gate
├── test_json_diagnostics.py # --diagnostics=json machine-readable diagnostics coverage
├── test_module_visibility_docs.py # module/header/visibility contract docs gate
├── test_nazm_emitter.py # normal and shadow Nazm object/link/runtime parity
├── test_one_definition.py # multi-file exported-symbol duplicate diagnostics
├── test_target_specs.py # target descriptor schema/contract coverage
├── test_utf8_validation.py # malformed UTF-8 and direct -S path regressions
├── test_toolchain_unicode_paths.py # direct Windows GCC/LD Unicode path matrix
├── test.py        # integration runner
└── regress.py     # regression runner (integration + corpus + neg)
```

## Markers

Supported line markers inside `.baa` files:

- `// RUN:` execution contract (`expect-pass`, `expect-fail`, `runtime`, `compile-only`, `skip`)
- `// EXPECT:` required stderr marker for expected-fail tests
- `// EXPECT-NOT:` stderr marker that must not appear in expected-fail tests
- `// EXPECT-DIAG-COUNT:` exact `[Error]`/`[Warning]` diagnostic count for expected-fail tests
- `// FLAGS:` extra compiler flags for this test
- `// ARGS:` runtime executable arguments
- `// STDIN:` stdin lines for runtime tests
- `// EXPECT-EXIT:` expected runtime exit status
- `// EXPECT-OUT:` stdout marker for runtime tests
- `// EXPECT-ERR:` stderr marker for runtime tests
- `// EXPECT-ASM:` assembly marker for `-S` tests
- `// EXPECT-NOT-ASM:` assembly marker that must not appear in `-S` tests

## Runner Entry Points

- Quick smoke: `python scripts/qa_run.py --mode quick`
- Full regression: `python scripts/qa_run.py --mode full`
- Stress suite: `python scripts/qa_run.py --mode stress`
- Release gate: `python scripts/qa_run.py --mode release`

Every mode starts with the C reference-compiler policy tests and guard. They verify that normal
build entrypoints have no bootstrap compiler requirement and that the `baa` target remains a
C/RC-only build.

Compiler discovery is also recorded as a `compiler-preflight` result. Missing binaries and
invalid `BAA` overrides produce a normal failed QA summary rather than an unhandled exception.

Every mode runs `tests/test_utf8_validation.py`, which creates malformed byte sequences at
runtime to verify UTF-8 diagnostics without storing invalid UTF-8 source files in the repository.
On Windows, every mode runs `tests/test_toolchain_unicode_paths.py` against the resolved GCC.
It proves that no-copy aliases to real Arabic artifacts handle spaces, long paths, multiple
objects, a UTF-8 response file and Arabic entry symbol, linked output, and runtime behavior.
It also launches six Baa builds concurrently from one directory to reject shared temporary
artifact names. Other hosts report this platform-specific matrix as skipped.
Every mode also runs `tests/test_arabic_numerals.py` to verify Arabic numeral rendering in
machine-readable IR dumps.
Every mode also runs `tests/test_examples.py`, which compiles every public `examples/*.باء`
program with `-O2 --verify`.
Every mode also runs `tests/test_header_self_check.py`, which verifies `--check-header`
accepts declaration headers, rejects invalid header declarations, and emits no code/output.
Every mode also runs `tests/test_fast_check.py`, which verifies `--check` accepts valid
source files, rejects semantic errors, records check-mode dependency manifests, and emits no
assembly, objects, or executables.
Every mode also runs `tests/test_function_capacity.py`, which keeps tooling-scale Baa modules
above the former 128-function ceiling by checking a deterministic 192-function source file.
Every mode also runs `tests/test_json_diagnostics.py`, which verifies `--diagnostics=json`
emits the stable top-level schema, error/warning summaries, source spans, codes, severities,
categories, and hints for Qalam/Takween consumers.
Every mode also runs `tests/test_integration_artifacts.py`, which checks the required
ecosystem, compatibility, tooling, diagnostics JSON, conformance-suite, and SDK planning
documents for the contract sections external tooling depends on.
Every mode also runs `tests/test_module_visibility_docs.py`, which keeps the `.baa`/`.baahd`
file-role convention, current `خارجي`/`ساكن` visibility rules, and Takween migration boundary
documented.
Every mode also runs `tests/test_one_definition.py`, which verifies multi-file duplicate
exported function/global diagnostics while keeping duplicate `ساكن` file-local globals valid.
Every mode also runs `tests/test_nazm_emitter.py`. When both binaries are available, its
100-source host corpus compiles through the explicit shadow route and the normal
`--assembler=nazm` route; compile-only objects and runnable link/runtime behavior must match
the GAS baseline without fallback. It also repeats normal Nazm source, object, and manifest
generation byte-for-byte while physical intermediate filenames remain process-unique.
Every mode also runs `tests/test_target_specs.py`, which validates the hosted
`x86_64-windows`/`x86_64-linux` descriptors and keeps the `i386` planning descriptors
explicitly experimental and freestanding.
The integration and negative suites also lock the v0.6.4 text-helper contract for empty strings,
prefix comparison signs, copy ownership, and bad `طول_نص`/`قارن_نص`/`نسخ_نص` calls.

Full, stress, and release modes run focused unit coverage for the determinism gate. Release mode
then compares repeated version/build-date output, negative diagnostics and exit status, IR,
assembly, manifests, verifier behavior, and cross-target assembly.

Full and higher modes also verify that Git-backed historical documentation extraction preserves
Arabic fenced programs under UTF-8, including on Windows.
