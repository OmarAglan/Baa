# Component Ownership

This document defines the logical ownership boundaries for the current flat `src/` layout.
It is an ownership map and dependency contract, not yet a directory split plan.

## Goals

- Keep each handwritten `src/*.c` and `src/*.h` module within a reviewable size budget.
- Make cross-component dependencies explicit before any file moves or large refactors.
- Give `v0.5.0` a stable contract for future source splitting work.

## Size Policy

- Scope: handwritten `src/*.c` and `src/*.h` files.
- Warning threshold: `700` physical lines per file.
- Error threshold: `1000` physical lines per file.
- Enforcement:
  - `scripts/check_module_sizes.py`
  - `scripts/qa_run.py --mode full|stress`
  - `.github/workflows/ci.yml` before full QA on Windows and Linux

## Canonical Components

| Component | Responsibility | Representative modules |
|-----------|----------------|------------------------|
| Frontend | Source loading, lexing, preprocessing, parsing, AST construction | `lexer.c`, `parser.c`, `read_file.c` |
| Middle-End | Semantic analysis, IR construction, IR verification, IR optimization, shared IR data layout | `analysis.c`, `ir*.c`, `ir*.h` |
| Backend | Target-aware lowering from IR to machine form and assembly emission | `isel.c`, `regalloc.c`, `emit.c`, `target.c`, `code_model.h` |
| Driver | CLI orchestration, per-file pipeline setup, toolchain execution, process management, updater entry points | `main.c`, `driver_*.c`, `process.c`, `updater.c`, `updater_stub.c` |
| Support | Shared diagnostics and cross-cutting declarations that multiple components consume | `error.c`, `baa.h` |

## Ownership Rules

- Every source file belongs to exactly one canonical component.
- Cross-component calls should flow through the owning component's public header surface, not through private helper leakage.
- `baa.h` is shared support surface, not a dumping ground for unrelated internal helpers.
- When an oversized file is split, the new files inherit the original component owner unless the split intentionally creates a new boundary.
- Physical layout may stay flat under `src/` until the restructure work lands; logical ownership already applies now.

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

This sidecar slice completes the policy and guard work only:

- canonical component boundaries are now documented,
- dependency rules are now documented,
- module-size guarding is now wired into QA and CI.

The following remain intentionally pending:

- splitting oversized modules,
- moving files into component directories,
- adding component-local facade headers across the whole tree.
