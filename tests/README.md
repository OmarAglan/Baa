# Baa Test Framework

This directory contains the Baa-only test framework.

## Layout

```text
tests/
├── integration/
│   ├── backend/   # compile + runtime integration tests
│   └── ir/        # compile-focused IR surface tests
├── neg/           # expected-fail diagnostics tests
├── bootstrap/     # Baa0 bootstrap subset compliance corpus
├── stress/        # stress tests (large inputs / scale)
├── snapshots/     # deterministic IR/QA snapshot contracts
├── fixtures/      # include files and multi-file fixtures
├── corpus_docs/   # extracted examples from docs
├── corpus_v2x_docs/ # historical docs corpus by version
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
- Bootstrap gate: `python scripts/qa_bootstrap_gate.py`
