# Baa Documentation Sync Protocol

> **Version:** 0.3.8 | [← Style Guide](STYLE_GUIDE.md) | [Compiler Internals →](INTERNALS.md)

This protocol defines how documentation is kept strictly aligned with compiler reality.

---

## 1. Source of Truth Order

When documentation and implementation differ, use this priority:

1. Compiler runtime behavior (`build/baa --help`, `--version`, compile/run results)
2. Passing QA/integration tests (`scripts/qa_run.py --mode full`, `tests/integration/**/*.baa`, `tests/neg/*.baa`, `tests/stress/*.baa`)
3. Current source code (`src/*.c`, `src/*.h`)
4. Release metadata (`CHANGELOG.md`, `ROADMAP.md`)
5. Explanatory docs (`docs/*.md`, `README.md`)

---

## 2. Required Doc Updates Per Implementation

For each shipped implementation, update all relevant docs in the same change set:

- `CHANGELOG.md`: Added/Changed/Fixed entry.
- `ROADMAP.md`: task status and completion marker.
- `docs/LANGUAGE.md`: user-visible syntax/semantics.
- `docs/INTERNALS.md`: architecture and pipeline behavior.
- `docs/API_REFERENCE.md`: C APIs/signatures/behavior contracts.
- `README.md` / `docs/USER_GUIDE.md`: usage/build/CLI surface.

If a section is intentionally deferred, mark it explicitly as deferred with target milestone.

---

## 3. Verification Gate (Mandatory)

Before considering docs synchronized:

```powershell
build\baa.exe --version
build\baa.exe --help
python scripts\qa_run.py --mode full
```

Then verify:

- Every documented CLI flag exists in `--help`.
- Every documented implemented feature has at least one runnable test path.
- No broken local Markdown links.
- No present-tense claims for removed/deferred behavior.

---

## 4. Step-by-Step Review Mode

For large releases, do documentation in tranches:

1. **Core Surface:** `README.md`, `docs/USER_GUIDE.md`
2. **Language Truth:** `docs/LANGUAGE.md`
3. **Compiler Truth:** `docs/INTERNALS.md`, `docs/API_REFERENCE.md`, `docs/BAA_IR_SPECIFICATION.md`
4. **Release Truth:** `CHANGELOG.md`, `ROADMAP.md`

Each tranche is complete only after the verification gate passes.

---

*Use this protocol as a release gate, not as optional guidance.*
