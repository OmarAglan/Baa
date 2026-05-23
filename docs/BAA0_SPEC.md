# Baa0 Bootstrap Subset Specification

> **Version:** 0.9.0.2 | [← Bootstrap Contract](BOOTSTRAP_CONTRACT.md) | [Language Spec →](LANGUAGE.md)

Baa0 is the conservative source subset used for early Phase 5 compiler-slice rewrites. It is not a new language mode and does not change what the compiler accepts. It defines which Baa features are allowed in bootstrap-owned source files so the first self-hosting work stays deterministic, portable, and easy to compare against the C baseline.

---

## 1. Summary

Baa0 is intended for small compiler slices such as token helpers, lexer support routines, and simple frontend utilities. A Baa0 source must compile with the frozen v0.5 language, ABI, stdlib, and IR contracts in `docs/BOOTSTRAP_CONTRACT.md`, and it must avoid features whose behavior is target-sensitive, ABI-sensitive, non-deterministic, or too broad for the first bootstrap pass.

Required gate:

```bash
python scripts/qa_bootstrap_gate.py
```

The gate compiles the Baa0 corpus, runs runtime-marked cases, verifies IR/SSA where applicable, checks cross-target assembly generation with `-S`, and rejects banned Baa0 features in bootstrap-positive sources.

---

## 2. Allowed Baa0 Surface

Allowed source forms:

- UTF-8 `.baa` and `.baahd` files.
- `#تضمين`, `#تعريف`, `#إذا_عرف`, `#وإلا`, `#نهاية`, and `#الغاء_تعريف`.
- Top-level function definitions and prototypes.
- Top-level and local constants, globals, and `ساكن` data when initialization is compile-time constant.
- Scalar types: `صحيح`, `ص٨`, `ص١٦`, `ص٣٢`, `ص٦٤`, `ط٨`, `ط١٦`, `ط٣٢`, `ط٦٤`, `منطقي`, `حرف`, `نص`, and `عدم`.
- Pointers, `عدم*` opaque handles, address-of, dereference, pointer comparison, pointer arithmetic with integer offsets, and `حجم`.
- One-dimensional arrays and array indexing.
- `نوع` aliases, `تعداد`, and plain `هيكل` records.
- Direct function calls, including calls to Baa0-approved stdlib declarations.
- `إذا` / `وإلا`, `طالما`, `لكل`, `توقف`, `استمر`, and `إرجع`.
- Integer, boolean, comparison, bitwise, shift, and logical operators.
- Casts with `كـ` when the source and target types are already accepted by the frozen semantic rules.

Allowed stdlib categories:

- String helpers: `طول_نص`, `قارن_نص`, `نسخ_نص`, `دمج_نص`, `حرر_نص`.
- Dynamic memory helpers: `حجز_ذاكرة`, `تحرير_ذاكرة`, `إعادة_حجز`, `نسخ_ذاكرة`, `تعيين_ذاكرة`.
- File helpers from `stdlib/baalib.baahd` when used deterministically by tests or compiler code.
- Runtime failure helpers: `تأكد`, `توقف_فوري`, and system error-code text helpers.
- Formatting/printing builtins only for diagnostics or test output, not for defining semantic behavior.

---

## 3. Excluded From Baa0

The following features remain valid in full Baa, but are banned in Baa0 bootstrap-positive sources:

- Floating-point source surface: `عشري`, `عشري٣٢`, floating literals, and math stdlib calls such as `جذر_تربيعي`, `أس`, `جيب`, `جيب_تمام`, and `ظل`.
- Inline assembly through `مجمع`.
- Variadic functions and variadic builtins: `قائمة_معاملات`, `بدء_معاملات`, `معامل_تالي`, and `نهاية_معاملات`.
- Function pointer types through `دالة(...) -> ...`.
- `اتحاد`.
- Multidimensional arrays.
- `اختر`, `حالة`, and `افتراضي`.
- Non-deterministic or host-side-effect stdlib calls: `وقت_حالي`, `وقت_كنص`, `عشوائي`, `متغير_بيئة`, and `نفذ_أمر`.
- Custom startup, target-specific assembly assumptions, or behavior that requires host toolchain details beyond normal Baa compilation.

If a compiler slice requires one of these features, it is not eligible for the first Baa0 migration wave. The feature can be reconsidered only by updating this document, the bootstrap gate, and the compliance suite together.

---

## 4. Migration Priority

Recommended order for C-to-Baa migration:

1. Pure helpers with simple ownership and deterministic output, such as token classification, identifier predicates, and small UTF-8 utilities.
2. Lexer support routines that operate on caller-owned buffers and return scalar status or simple structs.
3. Parser or semantic helper slices with narrow inputs and explicit diagnostics.
4. Larger frontend modules only after the previous slices pass parity checks on Windows and Linux.

Deferred until after Baa0:

- Backend, register allocation, emission, and target ABI logic.
- Driver process/toolchain orchestration.
- Updater and installer logic.
- Optimizer passes with complex IR mutation until Baa0 parity is established.

---

## 5. Compliance Rules

A Baa0 candidate is compliant when:

- It passes `scripts/qa_bootstrap_gate.py`.
- It has no banned Baa0 feature tokens after comments and string/character literals are ignored.
- It compiles with `-O2 --verify-ir --verify-ssa`.
- Runtime-marked tests exit with the expected status and output.
- Cross-target assembly generation succeeds for `--target=x86_64-windows -S` and `--target=x86_64-linux -S`.
- Any diagnostics remain Arabic-first and deterministic enough for negative-test anchors.

The compliance suite under `tests/bootstrap/` is the reference corpus for this subset.

---

*[← Bootstrap Contract](BOOTSTRAP_CONTRACT.md) | [Language Spec →](LANGUAGE.md)*
