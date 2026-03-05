# Component Ownership

This document defines the ownership boundaries for the current componentized `src/` layout.
It remains both an ownership map and a dependency contract during the compatibility-shim transition.

## Goals

- Keep each handwritten `src/**/*.c` and `src/**/*.h` module within a reviewable size budget.
- Make cross-component dependencies explicit during and after the physical file move.
- Give `v0.5.0` a stable contract for compatibility shims today and header cleanup later.

## Size Policy

- Scope: handwritten `src/**/*.c` and `src/**/*.h` files.
- Warning threshold: `700` physical lines per file.
- Error threshold: `1000` physical lines per file.
- Enforcement:
  - `scripts/check_module_sizes.py`
  - `scripts/qa_run.py --mode full|stress`
  - `.github/workflows/ci.yml` before full QA on Windows and Linux

## Canonical Components

| Component | Responsibility | Representative modules |
|-----------|----------------|------------------------|
| Frontend | Source loading, lexing, preprocessing, parsing, AST construction | `src/frontend/lexer.c`, `src/frontend/parser.c`, `src/frontend/analysis.c` |
| Middle-End | Semantic analysis, IR construction, IR verification, IR optimization, shared IR data layout | `src/middleend/ir*.c`, `src/middleend/ir_lower.c`, `src/middleend/ir_optimizer.c` |
| Backend | Target-aware lowering from IR to machine form and assembly emission | `src/backend/isel.c`, `src/backend/regalloc.c`, `src/backend/emit.c`, `src/backend/target.c`, `code_model.h` |
| Driver | CLI orchestration, per-file pipeline setup, toolchain execution, process management | `src/driver/main.c`, `src/driver/driver_*.c`, `src/driver/process.c` |
| Support | Shared diagnostics, file loading, updater glue, and cross-cutting declarations | `src/support/error.c`, `src/support/read_file.c`, `src/support/updater*.c`, `baa.h` |

## Ownership Rules

- Every source file belongs to exactly one canonical component.
- Cross-component calls should flow through the owning component's public header surface, not through private helper leakage.
- `baa.h` is shared support surface, not a dumping ground for unrelated internal helpers.
- When an oversized file is split, the new files inherit the original component owner unless the split intentionally creates a new boundary.
- Root-level `.c` files are temporary compatibility shims; the actual implementation now lives under the component subdirectories.
- Public/shared headers remain in the `src/` root during this phase to avoid a risky include-graph rewrite.

## Dependency Rules

| From | Allowed dependencies | Forbidden dependencies |
|------|----------------------|------------------------|
| Support | C runtime / system headers only | Frontend, Middle-End, Backend, Driver |
| Frontend | Support | Middle-End internals, Backend, Driver |
| Middle-End | Support, Frontend AST/contracts | Backend, Driver toolchain/process code |
| Backend | Support, Middle-End IR/contracts | Frontend parser/lexer internals, Driver toolchain/process code |
| Driver | Support, Frontend, Middle-End, Backend public entry points | Reaching into private static helpers of other components |

## Practical Rules For Future Splits

- Frontend should hand off AST and diagnostics data, not backend-specific lowering details.
- Middle-End should consume AST and produce IR; backend must consume IR, never raw AST.
- Backend-specific ABI and object-format knowledge belongs in backend-owned modules (`target.*`, `emit.*`, `regalloc.*`, `isel.*`).
- Toolchain launching, filesystem staging, and updater behavior remain driver-owned concerns.
- Shared helpers that do not fit a single component should move into an explicit support module instead of creating new ad-hoc cross-links.

## Current v0.5.0 Scope

This `v0.5.0` slice now covers policy, guard, and physical source placement:

- canonical component boundaries are now documented,
- dependency rules are now documented,
- module-size guarding is now wired into QA and CI,
- implementation files now live under component directories with compatibility shims preserving old source paths.

The following remain intentionally pending:

- splitting oversized modules,
- adding component-local facade headers across the whole tree.
