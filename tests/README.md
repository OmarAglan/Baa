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
├── test_integration_artifacts.py # ecosystem/tooling integration docs gate
├── test_module_visibility_docs.py # module/header/visibility contract docs gate
├── test_target_specs.py # target descriptor schema/contract coverage
├── test_utf8_validation.py # malformed UTF-8 byte regression coverage
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
Every mode also runs `tests/test_arabic_numerals.py` to verify Arabic numeral rendering in
machine-readable IR dumps.
Every mode also runs `tests/test_examples.py`, which compiles every public `examples/*.baa`
program with `-O2 --verify`.
Every mode also runs `tests/test_integration_artifacts.py`, which checks the required
ecosystem, compatibility, tooling, diagnostics JSON, conformance-suite, and SDK planning
documents for the contract sections external tooling depends on.
Every mode also runs `tests/test_module_visibility_docs.py`, which keeps the `.baa`/`.baahd`
file-role convention, current `خارجي`/`ساكن` visibility rules, and Takween migration boundary
documented.
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
