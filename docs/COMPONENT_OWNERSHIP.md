# Component Ownership

This document defines the ownership boundaries for the current componentized `src/` layout.
It remains both an ownership map and a dependency contract during the compatibility-shim transition.

## Goals

- Keep each handwritten `src/**/*.c` and `src/**/*.h` module within a reviewable size budget.
- Make cross-component dependencies explicit during and after the physical file move.
- Give the current hardening pass a stable contract for component boundaries today and header cleanup later.

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
| Support | Shared diagnostics, file loading, updater glue, and cross-cutting declarations | `src/support/error.c`, `src/support/read_file.c`, `src/support/updater*.c`, `src/support/target_contract.h`, `baa.h` |

## Ownership Rules

- Every source file belongs to exactly one canonical component.
- Cross-component calls should flow through the owning component's public header surface, not through private helper leakage.
- `baa.h` is a compatibility umbrella only; new shared declarations must live in component-owned public headers first.
- When an oversized file is split, the new files inherit the original component owner unless the split intentionally creates a new boundary.
- The build now compiles component source files directly from their subdirectories; root-level source compatibility shims have been removed.
- The `src/` root is no longer used for compatibility wrappers; component headers now live with their owning component.
- Each component may expose a local internal facade header inside its own directory (`frontend_internal.h`, `middleend_internal.h`, `backend_internal.h`, `driver_internal.h`, `support_internal.h`).
- These local facades are transition boundaries for implementation files only; they are not public API.

## Dependency Rules

| From | Allowed dependencies | Forbidden dependencies |
|------|----------------------|------------------------|
| Support | C runtime / system headers only | Frontend, Middle-End, Backend, Driver |
| Frontend | Support | Middle-End internals, Backend, Driver |
| Middle-End | Support, Frontend AST/contracts | Backend, Driver toolchain/process code |
| Backend | Support, Middle-End IR/contracts | Frontend parser/lexer internals, Driver toolchain/process code |
| Driver | Support, Frontend, Middle-End, Backend public entry points | Reaching into private static helpers of other components |

Practical note:

- The shared target contract is now exposed through `src/support/target_contract.h`, so middle-end code can query target identity without depending on backend-owned `target.h`.
- `support/diagnostics.h` no longer imports frontend lexer declarations; callers pass any token-like object that exposes `filename`, `line`, and `col`.

## Practical Rules For Future Splits

- Frontend should hand off AST and diagnostics data, not backend-specific lowering details.
- Middle-End should consume AST and produce IR; backend must consume IR, never raw AST.
- Backend-specific ABI and object-format knowledge belongs in backend-owned modules (`target.*`, `emit.*`, `regalloc.*`, `isel.*`).
- Toolchain launching, filesystem staging, and updater behavior remain driver-owned concerns.
- Shared helpers that do not fit a single component should move into an explicit support module instead of creating new ad-hoc cross-links.

## Current v0.5.0 Scope

The current hardening state covers policy, guard, physical source placement, local internal facades, and two header-relocation waves:

- canonical component boundaries are now documented,
- dependency rules are now documented,
- module-size guarding is now wired into QA and CI,
- implementation files now live under component directories and are built directly from those paths,
- each component now has a local internal facade header to centralize its implementation-facing include surface.
- the build now points at component source files directly and no longer relies on root `.c` shims,
- `driver*.h` and `process.h` now live under `src/driver/`,
- `emit.h`, `isel.h`, `regalloc.h`, `target.h`, and `code_model.h` now live under `src/backend/`,
- all `ir*.h` headers now live under `src/middleend/`,
- the root `src/` directory is reduced to the `baa.h` compatibility umbrella plus non-C source assets.
- shared declarations previously living in `baa.h` now live under `src/frontend/` and `src/support/`.
- the build intentionally avoids project-wide include directories; components include their own headers or explicit relative dependency headers only.

The following remain intentionally pending:

- longer-term dependency tightening beyond the current component facade layer.
