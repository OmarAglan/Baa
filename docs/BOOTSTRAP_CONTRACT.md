# Baa Bootstrap Contract

> **Version:** 0.5.3 | [← Component Ownership](COMPONENT_OWNERSHIP.md) | [Language Spec →](LANGUAGE.md)

This document freezes the language, ABI, standard-library, and IR contracts that Phase 5 bootstrap work must preserve.

---

## 1. Summary

This is the `v0.5.1` freeze contract for bootstrap readiness. It does not introduce new compiler behavior; it records the behavior that compiler-in-Baa migration work must treat as stable.

The authoritative implementation and reference inputs are:

- `docs/LANGUAGE.md` for syntax and source-level semantics.
- `stdlib/baalib.baahd` for callable standard-library declarations.
- `src/backend/target.h`, `src/backend/target.c`, and `src/support/target_contract.h` for target and ABI contracts.
- `docs/BAA_IR_SPECIFICATION.md`, `src/middleend/ir.h`, `src/middleend/ir_verify_ir.c`, and `src/middleend/ir_verify_ssa.c` for IR invariants.

---

## 2. Language Freeze

The source grammar and semantic surface are frozen at the behavior documented in `docs/LANGUAGE.md`.

Frozen language surface:

- File format: UTF-8 `.baa` source files and `.baahd` headers.
- Statement terminator: `.`; Arabic semicolon `؛` remains the `لكل` separator.
- Preprocessor directives: `#تضمين`, `#تعريف`, `#إذا_عرف`, `#وإلا`, `#نهاية`, `#الغاء_تعريف`, and `#خطأ`.
- Keywords and type forms currently recognized by the lexer/parser, including scalar integer widths, unsigned widths, `عشري`, `عشري٣٢`, `حرف`, `نص`, `منطقي`, `عدم`, `نوع`, `دالة(...) -> ...`, `ثابت`, `ساكن`, `تعداد`, `هيكل`, `اتحاد`, `مجمع`, and `كـ`.
- Top-level declarations: functions, prototypes, globals, static globals, arrays, type aliases, enums, structs, and unions.
- Statements and expressions documented in `docs/LANGUAGE.md`, including control flow, pointer operations, casts, `حجم`, formatted I/O builtins, variadics, and inline assembly.
- Entry points: `صحيح الرئيسية()` and `صحيح الرئيسية(صحيح عدد، نص[] معاملات)`.

Freeze policy:

- No new syntax is admitted into the bootstrap subset without an explicit post-freeze roadmap decision.
- Parser recovery changes may improve diagnostics, but they must not change accepted valid programs.
- Existing diagnostics may be clarified in Arabic, but bootstrap parity checks should anchor behavior rather than exact punctuation where practical.

---

## 3. Standard Library Freeze

The callable stdlib contract is frozen to `stdlib/baalib.baahd`.

Frozen declarations:

```baa
صحيح طول_نص(نص س).
صحيح قارن_نص(نص أ، نص ب).
نص نسخ_نص(نص س).
نص دمج_نص(نص أ، نص ب).
عدم حرر_نص(نص س).

عدم* حجز_ذاكرة(صحيح عدد_بايتات).
عدم تحرير_ذاكرة(عدم* مؤشر).
عدم* إعادة_حجز(عدم* مؤشر، صحيح عدد_بايتات).
عدم* نسخ_ذاكرة(عدم* وجهة، عدم* مصدر، صحيح عدد_بايتات).
عدم* تعيين_ذاكرة(عدم* مؤشر، صحيح قيمة، صحيح عدد_بايتات).

عدم* فتح_ملف(نص مسار، نص وضع).
صحيح اغلق_ملف(عدم* ملف).
حرف اقرأ_حرف(عدم* ملف).
صحيح اكتب_حرف(عدم* ملف، حرف ح).
صحيح اقرأ_ملف(عدم* ملف، عدم* مخزن، صحيح عدد_بايتات).
صحيح اكتب_ملف(عدم* ملف، عدم* بيانات، صحيح عدد_بايتات).
منطقي نهاية_ملف(عدم* ملف).
صحيح موقع_ملف(عدم* ملف).
صحيح اذهب_لموقع(عدم* ملف، صحيح موقع).
نص اقرأ_سطر(عدم* ملف).
صحيح اكتب_سطر(عدم* ملف، نص سطر).

عشري جذر_تربيعي(عشري قيمة).
عشري أس(عشري أساس، عشري قوة).
عشري جيب(عشري زاوية).
عشري جيب_تمام(عشري زاوية).
عشري ظل(عشري زاوية).
صحيح مطلق(صحيح قيمة).
صحيح عشوائي().

نص متغير_بيئة(نص اسم).
صحيح نفذ_أمر(نص أمر).

صحيح وقت_حالي().
نص وقت_كنص(صحيح طابع_زمني).

عدم تأكد(منطقي شرط، نص رسالة).
عدم توقف_فوري(نص رسالة).
صحيح كود_خطأ_النظام().
عدم ضبط_كود_خطأ_النظام(صحيح كود).
نص نص_كود_خطأ(صحيح كود).

نوع قائمة_معاملات = عدم*.
عدم بدء_معاملات(قائمة_معاملات قائمة، صحيح آخر_ثابت).
صحيح معامل_تالي(قائمة_معاملات قائمة، صحيح وسم_نوع).
عدم نهاية_معاملات(قائمة_معاملات قائمة).
```

Frozen error-code constants:

```baa
#تعريف كود_نجاح 0
#تعريف كود_فشل 1
#تعريف كود_وسيط_غير_صحيح 2
#تعريف كود_غير_مدعوم 3
#تعريف كود_نفاد_ذاكرة 4
#تعريف كود_نظام 5
```

Ownership rules:

- Functions returning `نص` from copy/format/time/error/file-line APIs may allocate heap memory as documented by `stdlib/baalib.baahd`.
- `عدم*` remains the stable opaque handle representation for dynamic memory and file handles.
- Variadic runtime intrinsics remain compiler-recognized builtins and are valid only under the semantic rules documented in `docs/LANGUAGE.md`.

---

## 4. Target ABI Freeze

Supported target names:

- `x86_64-windows`
- `x86_64-linux`

Common target invariants:

- Object code is x86-64.
- Pointer size is 8 bytes.
- Stack alignment at call boundaries is 16 bytes.
- Integer and pointer return values use RAX.
- Cross-target compilation is guaranteed for assembly output (`-S`) only unless the matching host/cross toolchain is available.

Windows x64 ABI:

- Object format: COFF.
- Integer argument registers: RCX, RDX, R8, R9.
- Caller shadow/home space: 32 bytes.
- Register arguments are homed for calls where required by the backend contract.
- Callee-saved registers: RBX, RBP, RDI, RSI, R12, R13, R14, R15.

SystemV AMD64 ABI:

- Object format: ELF.
- Integer argument registers: RDI, RSI, RDX, RCX, R8, R9.
- No Windows-style shadow space.
- Variadic calls conservatively set AL according to the current backend contract.
- Callee-saved registers: RBX, RBP, R12, R13, R14, R15.

Backend special machine virtual registers remain frozen:

| Virtual register | Meaning |
|------------------|---------|
| `-1` | frame pointer / RBP |
| `-2` | ABI return register / RAX |
| `-3` | stack pointer / RSP |
| `-4` | backend scratch / R11 |
| `-5` | division remainder constraint / RDX |
| `-6` | shift-count constraint / RCX |
| `-10..` | target-dependent ABI integer argument registers |

---

## 5. IR Freeze

The IR contract is frozen to the Arabic-first IR documented in `docs/BAA_IR_SPECIFICATION.md` and represented in `src/middleend/ir.h`.

Frozen IR invariants:

- IR objects are module/arena-owned unless a specific API documents heap ownership.
- Instructions belong to exactly one basic block and maintain parent links when inserted through mutation helpers.
- Each basic block has a single terminator: `قفز`, `قفز_شرط`, or `رجوع`.
- `فاي` instructions appear before non-phi instructions and contain one incoming value per predecessor.
- `--verify-ir` validates operand counts, operand/result types, terminator placement, phi shape, CFG block references, globals, and intra-module call signatures.
- `--verify-ssa` validates single definitions, dominance of uses, and phi predecessor correctness after Mem2Reg and before Out-of-SSA.
- Out-of-SSA must remove phi nodes before backend lowering.
- Arabic IR text conventions remain stable: opcodes such as `جمع/طرح/ضرب`, registers `%م...`, and block labels `كتلة_...`.

Freeze policy:

- New IR opcodes, type kinds, verifier rules, or text format changes require explicit updates to `docs/BAA_IR_SPECIFICATION.md`, `docs/IR_DEVELOPER_GUIDE.md`, and focused tests.
- Optimization passes may change IR shape, but they must preserve verifier-clean IR and stable semantics for the same source program.

---

## 6. Bootstrap Admission

A compiler slice is eligible for Phase 5 migration only when it preserves this contract and passes the relevant parity checks:

- The existing quick/full QA suite remains green.
- `--verify`, `--verify-ir`, and `--verify-ssa` remain clean for IR-affecting paths.
- Multi-file include resolution and symbol visibility behavior from `v0.5.2` remain unchanged.
- Diagnostics remain Arabic-first and deterministic enough for negative-test anchors.

`docs/BAA0_SPEC.md`, `scripts/qa_bootstrap_gate.py`, and `tests/bootstrap/` are later Phase 4.5 artifacts and will build on this contract.

---

*[← Component Ownership](COMPONENT_OWNERSHIP.md) | [Language Spec →](LANGUAGE.md)*
