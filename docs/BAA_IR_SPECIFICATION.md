# Baa IR Specification

> **Version:** 0.4.0.0 | [← Compiler Internals](INTERNALS.md) | [API Reference →](API_REFERENCE.md)

This document specifies the Intermediate Representation (IR) for the Baa compiler. The IR uses Arabic naming conventions throughout, creating a culturally authentic yet technically robust design.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Type System](#2-type-system)
3. [SSA Form](#3-ssa-form)
4. [Instruction Set](#4-instruction-set)
5. [Control Flow Graph](#5-control-flow-graph)
6. [Lowering from AST](#6-lowering-from-ast)
7. [Optimization Passes](#7-optimization-passes)
8. [Examples](#8-examples)
9. [IR Text Format Grammar](#9-ir-text-format-grammar)
10. [Implementation Notes](#10-implementation-notes)

---

## 1. Overview

### 1.1 Design Principles

- **SSA Form**: Each virtual register is assigned exactly once
- **Arabic Identity**: All opcodes, types, and keywords use Arabic names
- **Unicode Native**: Full UTF-8 support for Arabic numerals and text
- **Target Independent**: Abstracts away x86-64 specifics until backend lowering

### 1.2 IR Pipeline Position

```
AST → [Baa IR] → Optimizations → Backend Codegen → x86-64 Assembly
```

---

## 2. Type System

### 2.1 Primitive Types

| Type | Arabic | C Enum | Size | Description |
|------|--------|--------|------|-------------|
| `i64` | `ص٦٤` | `IR_TYPE_I64` | 8 bytes | 64-bit signed integer |
| `i32` | `ص٣٢` | `IR_TYPE_I32` | 4 bytes | 32-bit signed integer |
| `i16` | `ص١٦` | `IR_TYPE_I16` | 2 bytes | 16-bit signed integer |
| `i8` | `ص٨` | `IR_TYPE_I8` | 1 byte | 8-bit signed integer |
| `i1` | `ص١` | `IR_TYPE_I1` | 1 bit | Boolean (stored as byte) |
| `u64` | `ط٦٤` | `IR_TYPE_U64` | 8 bytes | 64-bit unsigned integer |
| `u32` | `ط٣٢` | `IR_TYPE_U32` | 4 bytes | 32-bit unsigned integer |
| `u16` | `ط١٦` | `IR_TYPE_U16` | 2 bytes | 16-bit unsigned integer |
| `u8` | `ط٨` | `IR_TYPE_U8` | 1 byte | 8-bit unsigned integer |
| `char` | `حرف` | `IR_TYPE_CHAR` | 8 bytes | UTF-8 character (packed into i64) |
| `f64` | `ع٦٤` | `IR_TYPE_F64` | 8 bytes | 64-bit float |
| `void` | `فراغ` | `IR_TYPE_VOID` | 0 | No value (void) |

### 2.2 Derived Types

| Type | Arabic Syntax | C Access | Example |
|------|---------------|----------|---------|
| Pointer | `مؤشر[<type>]` | `type->data.pointee` | `مؤشر[ص٦٤]` |
| Array | `مصفوفة[<type>، <size>]` | `type->data.array` | `مصفوفة[ص٦٤، ١٠]` |
| Function | `دالة(<args>) -> <ret>` | `type->data.func` | `دالة(ص٦٤، ص٦٤) -> ص٦٤` |

### 2.3 Type Compatibility

- Integer types can be extended or truncated via `تحويل`
- Pointers are 64-bit on x86-64 target
- `ص١` (boolean) is zero-extended to larger integers

### 2.4 Data Layout (Target: x86-64)

The IR enforces a specific data layout to allow consistent lowering. On x86-64, this is the same for Windows and Linux targets:

| Type | Size (Bytes) | Alignment (Bytes) | Store Size (Bytes) |
|------|--------------|-------------------|--------------------|
| `i1` | 1 | 1 | 1 |
| `i8` | 1 | 1 | 1 |
| `i16` | 2 | 2 | 2 |
| `i32` | 4 | 4 | 4 |
| `i64` | 8 | 8 | 8 |
| `u8/u16/u32/u64` | 1/2/4/8 | 1/2/4/8 | 1/2/4/8 |
| `char` | 8 | 8 | 8 |
| `f64` | 8 | 8 | 8 |
| `ptr` | 8 | 8 | 8 |

**Memory Model Contract:**
- **Typed Pointers:** Memory operations (`load`, `store`) must respect the pointer's element type.
- **Aliasing:** Conservative. Any `store` to memory may alias any `load` unless provenance can be strictly proven (no TBAA yet).
- **Addressing:** Flat 64-bit address space.

---

## 3. SSA Form

### 3.1 Virtual Registers

Virtual registers use the prefix `%م` (مؤقت = temporary):

```
%م٠ = جمع ص٦٤ %م١، %م٢
%م٣ = ضرب ص٦٤ %م٠، ٢
```

**Rules:**

- Each `%م<n>` is assigned exactly once (SSA property)
- Numbers use Arabic numerals: `%م٠`, `%م١`, `%م٢٣`
- Function parameters: `%معامل٠`, `%معامل١`

### 3.2 Special Machine Virtual Registers

The backend uses special negative register numbers for physical registers:

| Register Number | Arabic Name | Physical Register | Purpose |
|-----------------|-------------|-------------------|---------|
| `-1` | `%إطار` | RBP | Frame pointer (base pointer) |
| `-2` | `%عائد` | RAX | ABI return value register |
| `-3` | `%مكدس` | RSP | Stack pointer |
| `-10` | `%معامل٠` | RCX/RCX (Windows/Linux) | First integer argument |
| `-11` | `%معامل١` | RDX/RDX | Second integer argument |
| `-12` | `%معامل٢` | R8/RDI | Third integer argument |
| `-13` | `%معامل٣` | R9/RSI | Fourth integer argument |
| `-14` onwards | `%معامل٤+` | Stack/R10+ | Additional arguments |

### 3.3 Phi Nodes

Phi nodes merge values at control flow join points:

```
%م٣ = فاي ص٦٤ [%م١، %كتلة_أ]، [%م٢، %كتلة_ب]
```

The `فاي` instruction selects a value based on which predecessor block was executed.

**Note:** Phi nodes are an IR-level SSA construct. Before instruction selection, the compiler runs an Out-of-SSA rewrite that removes `فاي` by inserting edge copies (so the backend does not need to implement `phi` directly).

**Verifiers:**

- **SSA verifier (v0.3.2.5.3):** The compiler provides `--verify-ssa` to validate SSA invariants after Mem2Reg and before Out-of-SSA (single-definition, dominance, and phi incoming-edge correctness).
- **IR well-formedness verifier (v0.3.2.6.5):** The compiler provides `--verify-ir` to validate general IR invariants (operand counts, type consistency, terminator rules, phi placement, and intra-module call signature checks).
- **All verifiers (v0.3.2.9.1):** The compiler provides `--verify` to run `--verify-ir` + `--verify-ssa` together (requires `-O1`/`-O2`).

### 3.4 Memory Model

Stack allocations create addressable memory:

```
%م٠ = حجز ص٦٤           // Allocate 8 bytes on stack
خزن ص٦٤ ١٠، %م٠          // Store value 10
%م١ = حمل ص٦٤، %م٠       // Load value back
```

---

## 4. Instruction Set

### 4.1 IROp Enum

```c
typedef enum {
    // Arithmetic Operations (العمليات الحسابية)
    IR_OP_ADD,      // جمع - Addition
    IR_OP_SUB,      // طرح - Subtraction
    IR_OP_MUL,      // ضرب - Multiplication
    IR_OP_DIV,      // قسم - Signed division
    IR_OP_MOD,      // باقي - Modulo (remainder)
    IR_OP_NEG,      // سالب - Negation (unary minus)
    
    // Memory Operations (عمليات الذاكرة)
    IR_OP_ALLOCA,   // حجز - Stack allocation
    IR_OP_LOAD,     // حمل - Load from memory
    IR_OP_STORE,    // خزن - Store to memory
    IR_OP_PTR_OFFSET, // إزاحة_مؤشر - Pointer offset
    
    // Comparison Operations (عمليات المقارنة)
    IR_OP_CMP,      // قارن - Compare (with predicate)
    
    // Logical Operations (العمليات المنطقية)
    IR_OP_AND,      // و - Bitwise AND
    IR_OP_OR,       // أو - Bitwise OR
    IR_OP_XOR,      // أو_حصري - Bitwise XOR
    IR_OP_NOT,      // نفي - Bitwise NOT
    IR_OP_SHL,      // ازاحة_يسار - Shift left
    IR_OP_SHR,      // ازاحة_يمين - Shift right
    
    // Control Flow Operations (عمليات التحكم)
    IR_OP_BR,       // قفز - Unconditional branch
    IR_OP_BR_COND,  // قفز_شرط - Conditional branch
    IR_OP_RET,      // رجوع - Return from function
    IR_OP_CALL,     // نداء - Function call
    
    // SSA Operations (عمليات SSA)
    IR_OP_PHI,      // فاي - Phi node for SSA
    IR_OP_COPY,     // نسخ - Copy (for SSA construction)
    
    // Type Conversion (تحويل الأنواع)
    IR_OP_CAST,     // تحويل - Type cast/conversion
    
    // Special Operations
    IR_OP_NOP,      // No operation (placeholder)
    
    IR_OP_COUNT     // Total number of opcodes
} IROp;
```

### 4.2 Arithmetic Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `add` | `جمع` | `IR_OP_ADD` | `%r = جمع <type> %a، %b` | Addition |
| `sub` | `طرح` | `IR_OP_SUB` | `%r = طرح <type> %a، %b` | Subtraction |
| `mul` | `ضرب` | `IR_OP_MUL` | `%r = ضرب <type> %a، %b` | Multiplication |
| `div` | `قسم` | `IR_OP_DIV` | `%r = قسم <type> %a، %b` | Signed division |
| `mod` | `باقي` | `IR_OP_MOD` | `%r = باقي <type> %a، %b` | Modulo |
| `neg` | `سالب` | `IR_OP_NEG` | `%r = سالب <type> %a` | Negation |

**Arithmetic Semantics (Strict):**
- **Overflow:** Standard **Two's Complement Wrap**. `INT_MAX + 1` → `INT_MIN`. No undefined behavior.
- **Division/Modulo by Zero:** Undefined in IR (backend may trap).
- **Signed Division Edge Case:** `INT64_MIN / -1` wraps to `INT64_MIN` (does not trap).
- **Signed Modulo Edge Case:** `INT64_MIN % -1` yields `0`.

### 4.3 Memory Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `alloca` | `حجز` | `IR_OP_ALLOCA` | `%r = حجز <type>` | Stack allocation |
| `load` | `حمل` | `IR_OP_LOAD` | `%r = حمل <type>، %ptr` | Load from memory |
| `store` | `خزن` | `IR_OP_STORE` | `خزن <type> <val>، %ptr` | Store to memory |
| `ptr_offset` | `إزاحة_مؤشر` | `IR_OP_PTR_OFFSET` | `%r = إزاحة_مؤشر <type> %base، %index` | GEP-like pointer arithmetic |

### 4.4 Comparison Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `cmp` | `قارن` | `IR_OP_CMP` | `%r = قارن <pred> <type> %a، %b` | Compare values |

**IRCmpPred Enum (مقارنات):**

```c
typedef enum {
    IR_CMP_EQ,      // يساوي - Equal
    IR_CMP_NE,      // لا_يساوي - Not equal
    IR_CMP_GT,      // أكبر - Greater than (signed)
    IR_CMP_LT,      // أصغر - Less than (signed)
    IR_CMP_GE,      // أكبر_أو_يساوي - Greater or equal (signed)
    IR_CMP_LE,      // أصغر_أو_يساوي - Less or equal (signed)

    // مقارنات بدون إشارة (unsigned)
    IR_CMP_UGT,     // أكبر - Greater than (unsigned)
    IR_CMP_ULT,     // أصغر - Less than (unsigned)
    IR_CMP_UGE,     // أكبر_أو_يساوي - Greater or equal (unsigned)
    IR_CMP_ULE,     // أصغر_أو_يساوي - Less or equal (unsigned)
} IRCmpPred;
```

**Predicates (مقارنات):**

| Predicate | Arabic | C Enum | Meaning |
|-----------|--------|--------|---------|
| `eq` | `يساوي` | `IR_CMP_EQ` | Equal |
| `ne` | `لا_يساوي` | `IR_CMP_NE` | Not equal |
| `gt` | `أكبر` | `IR_CMP_GT` | Greater than (signed) |
| `lt` | `أصغر` | `IR_CMP_LT` | Less than (signed) |
| `ge` | `أكبر_أو_يساوي` | `IR_CMP_GE` | Greater or equal (signed) |
| `le` | `أصغر_أو_يساوي` | `IR_CMP_LE` | Less or equal (signed) |
| `ugt` | `أكبر` | `IR_CMP_UGT` | Greater than (unsigned) |
| `ult` | `أصغر` | `IR_CMP_ULT` | Less than (unsigned) |
| `uge` | `أكبر_أو_يساوي` | `IR_CMP_UGE` | Greater or equal (unsigned) |
| `ule` | `أصغر_أو_يساوي` | `IR_CMP_ULE` | Less or equal (unsigned) |

**Note:** The Arabic names are the same for signed and unsigned comparisons. The operand types determine whether the comparison is signed or unsigned.

### 4.5 Logical Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `and` | `و` | `IR_OP_AND` | `%r = و <type> %a، %b` | Bitwise AND |
| `or` | `أو` | `IR_OP_OR` | `%r = أو <type> %a، %b` | Bitwise OR |
| `xor` | `أو_حصري` | `IR_OP_XOR` | `%r = أو_حصري <type> %a، %b` | Bitwise XOR |
| `not` | `نفي` | `IR_OP_NOT` | `%r = نفي <type> %a` | Bitwise NOT |
| `shl` | `ازاحة_يسار` | `IR_OP_SHL` | `%r = ازاحة_يسار <type> %a، %b` | Shift left |
| `shr` | `ازاحة_يمين` | `IR_OP_SHR` | `%r = ازاحة_يمين <type> %a، %b` | Shift right |

### 4.6 Control Flow Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `br` | `قفز` | `IR_OP_BR` | `قفز %label` | Unconditional jump |
| `br_cond` | `قفز_شرط` | `IR_OP_BR_COND` | `قفز_شرط %cond، %true، %false` | Conditional branch |
| `ret` | `رجوع` | `IR_OP_RET` | `رجوع <type> <val>` | Return from function |
| `call` | `نداء` | `IR_OP_CALL` | `%r = نداء @name(<args>)` | Function call |

### 4.7 SSA Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `phi` | `فاي` | `IR_OP_PHI` | `%r = فاي <type> [%v1، %b1]، [%v2، %b2]` | Phi node |
| `copy` | `نسخ` | `IR_OP_COPY` | `%r = نسخ <type> %v` | Register copy |

### 4.8 Type Conversion

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `cast` | `تحويل` | `IR_OP_CAST` | `%r = تحويل <from> %v إلى <to>` | Type conversion |

### 4.9 Miscellaneous Instructions

| Opcode | Arabic | C Enum | Syntax | Description |
|--------|--------|--------|--------|-------------|
| `nop` | `لاعمل` | `IR_OP_NOP` | `لاعمل` | No operation (placeholder) |

---

## 5. Control Flow Graph

### 5.1 Basic Blocks

Each basic block has:

- A unique Arabic label (e.g., `بداية:`, `حلقة:`, `نهاية:`)
- Zero or more non-terminating instructions
- Exactly one terminating instruction (`قفز`, `قفز_شرط`, or `رجوع`)

```
بداية:
    %م٠ = حجز ص٦٤
    خزن ص٦٤ ١٠، %م٠
    قفز %تحقق

تحقق:
    %م١ = حمل ص٦٤، %م٠
    %م٢ = قارن أكبر ص٦٤ %م١، ٠
    قفز_شرط %م٢، %جسم، %نهاية
```

### 5.2 Function Structure

```
دالة @<name>(<params>) -> <return_type> {
    <blocks>
}
```

Example:

```
دالة @مضاعفة(ص٦٤ %معامل٠) -> ص٦٤ {
بداية:
    %م٠ = ضرب ص٦٤ %معامل٠، ٢
    رجوع ص٦٤ %م٠
}
```

### 5.3 Global Declarations

```
عام @اسم_المتغير = ص٦٤ ١٠٠
عام @نص_ترحيب = نص "مرحباً"   // نص يُخزن كمؤشر إلى مصفوفة حرف[] ثابتة
internal global @عداد_داخلي = ص٦٤ ٠ // ربط داخلي (v0.3.7.5)

// مصفوفة عامة (تهيئة جزئية؛ الباقي أصفار مثل C)
عام @قائمة = مصفوفة[ص٦٤، ٥] {١، ٢، ٣}

// مصفوفة مؤشرات نصية عامة
عام @أسماء = مصفوفة[مؤشر[ص٨]، ٢] {"علي"، "منى"}
```

ملاحظة (v0.3.5): السلاسل النصية في IR لها جدولان:

- `.Lstr_N`: سلاسل C (`i8*`) لاستخدامها مع `printf/scanf` (مثل صيغ الطباعة).
- `.Lbs_N`: سلاسل باء (`حرف[]`) كقائمة محارف UTF-8 مع مُنهٍ صفري.

---

## 6. Lowering from AST

### 6.1 AST Node Mapping

| AST Node | IR Output |
|----------|-----------|
| `NODE_INT` | Immediate value |
| `NODE_VAR_REF` | `حمل` from local allocation or static/global backing symbol |
| `NODE_BIN_OP` | Arithmetic instruction |
| `NODE_ASSIGN` | `خزن` instruction |
| `NODE_IF` | `قفز_شرط` with two blocks |
| `NODE_WHILE` | Loop with `فاي` for variables |
| `NODE_FUNC_DEF` | `دالة` declaration |
| `NODE_CALL_EXPR` | `نداء` instruction |
| `NODE_RETURN` | `رجوع` instruction |
| `NODE_SIZEOF` | compile-time immediate `ص٦٤` constant |
| `NODE_ENUM_DECL` | Compile-time type table (no direct IR) |
| `NODE_STRUCT_DECL` | Compile-time layout table (no direct IR) |
| `NODE_UNION_DECL` | Compile-time layout table (no direct IR; all members offset 0) |
| `NODE_MEMBER_ACCESS` | Byte-based `إزاحة_مؤشر` on `ptr<i8>` + `حمل/خزن` |
| `NODE_MEMBER_ASSIGN` | Byte-based `إزاحة_مؤشر` on `ptr<i8>` + `خزن` |
| `NODE_ARRAY_DECL` | Stack/global array allocation + optional initializer lowering |
| `NODE_ARRAY_ACCESS` | Row-major linearized index → `إزاحة_مؤشر` + `حمل` |
| `NODE_ARRAY_ASSIGN` | Row-major linearized index → `إزاحة_مؤشر` + `خزن` |
| `NODE_NULL` | `IR_VAL_CONST_INT` (0) typed as pointer or func ptr |
| `NODE_CAST` | `تحويل` instruction |
| `NODE_UNARY_OP (ADDR)` | Returns underlying pointer register/global |
| `NODE_UNARY_OP (DEREF)`| `حمل` from pointer |
| `NODE_DEREF_ASSIGN` | `خزن` to pointer |

ملاحظة (`v0.3.9`): استدعاءات السلاسل `طول_نص/قارن_نص/نسخ_نص/دمج_نص/حرر_نص`
تُخفض داخلياً إلى حلقات/نداءات `malloc/free` حسب الحالة بدلاً من مجرد تمرير `call` خام.

ملاحظة (`v0.3.11`): استدعاءات الذاكرة `حجز_ذاكرة/تحرير_ذاكرة/إعادة_حجز/نسخ_ذاكرة/تعيين_ذاكرة`
تُخفض داخلياً إلى `malloc/free/realloc/memcpy/memset` مع الحفاظ على قواعد الـ shadowing.

### 6.2 Variable Handling

Local variables are lowered to stack allocations (automatic storage):

```baa
صحيح س = ١٠.
```

Becomes:

```
%م٠ = حجز ص٦٤
خزن ص٦٤ ١٠، %م٠
```

Static locals (`ساكن`) are lowered as internal globals:

```
internal global @__baa_static_<func>_<name>_<id> = ص٦٤ ٠
```

### 6.2.1 Multi-Dimensional Array Lowering (v0.3.9)

- Multi-dimensional AST indexing (`a[i][j]...[k]`) is flattened to a single linear index using row-major order:
  - `linear = (((i * dim1) + j) * dim2 + ...) + k`
- The linear index feeds `IR_OP_PTR_OFFSET` to compute the target element address.
- Rank/dimension validity is a semantic-phase contract; IR lowering assumes validated shape metadata.
- Optional debug-mode lowering may emit runtime bounds guard paths before computing the element address.

### 6.3 Control Flow Lowering

If-else statements:

```baa
إذا (س > ٠) { اطبع س. } وإلا { اطبع ٠. }
```

Becomes:

```
    %م١ = حمل ص٦٤، %م٠
    %م٢ = قارن أكبر ص٦٤ %م١، ٠
    قفز_شرط %م٢، %فرع_صواب، %فرع_خطأ
فرع_صواب:
    نداء @اطبع(%م١)
    قفز %نهاية
فرع_خطأ:
    نداء @اطبع(٠)
    قفز %نهاية
نهاية:
```

---

## 7. Optimization Passes

### 7.1 Analysis Passes

| Pass | Arabic | Description |
|------|--------|-------------|
| Dominance | `تحليل_السيطرة` | Compute dominator tree |
| Liveness | `تحليل_الحياة` | Track live ranges |
| CFG Analysis | `تحليل_التدفق` | Control flow analysis |

### 7.2 Transformation Passes

| Pass | Arabic | Description |
|------|--------|-------------|
| Canonicalize | `توحيد_الـIR` | Normalize instruction forms for better matching in later passes |
| InstCombine | `دمج_التعليمات` | Fast local simplifications (rewrite to copies/constants) |
| SCCP | `نشر_الثوابت_المتناثر` | Sparse conditional constant propagation + conditional branch folding |
| Inlining | `تضمين` | Inline small internal functions (O2) |
| Mem2Reg | `ترقية_الذاكرة_إلى_سجلات` | Promote simple allocas to direct SSA register use |
| Constant Fold | `طي_الثوابت` | Evaluate constants at compile time |
| Copy Propagation | `نشر_النسخ` | Replace copies with original |
| GVN | `ترقيم_القيم` | Remove redundant pure expressions across dominator scopes |
| Common Subexpr | `حذف_المكرر` | Eliminate redundant computations |
| Dead Code | `حذف_الميت` | Remove dead instructions + unreachable blocks |
| CFG Simplify | `تبسيط_CFG` | Merge trivial blocks, remove redundant branches |
| LICM | `LICM` | Hoist pure loop-invariant computations to preheaders |

### 7.3 Pass Order

**Optimizer pipeline (O1/O2):**

0. `تضمين` - Inline small internal functions with a single call site (O2) (v0.3.2.7.2)
1. `ترقية_الذاكرة_إلى_سجلات` - Promote safe allocas to SSA (Mem2Reg)
2. `توحيد_الـIR` - Canonicalize operand ordering/forms
3. `دمج_التعليمات` - Fast local simplifications (v0.3.2.8.6)
4. `نشر_الثوابت_المتناثر` - SCCP: constant propagation + reachability-based CFG folding (v0.3.2.8.6)
5. `طي_الثوابت` - Fold constant expressions
6. `نشر_النسخ` - Propagate copies
7. `ترقيم_القيم` - GVN: eliminate redundant pure expressions (O2) (v0.3.2.8.6)
8. `حذف_المكرر` - Eliminate common subexpressions (O2)
9. `حذف_الميت` - Remove dead code + unreachable blocks
10. `تبسيط_CFG` - Simplify CFG (merge trivial blocks, remove redundant branches)
11. `LICM` - Hoist pure loop-invariant computations to preheaders (v0.3.2.7.1)

**After optimization:**

- `الخروج_من_SSA` - Out-of-SSA edge copies (required before backend)
- Optional `-funroll-loops` - conservative full unroll of small constant-trip loops (after Out-of-SSA)

---

## 8. Examples

### 8.1 Simple Function

**Baa Source:**

```baa
صحيح مربع(صحيح س) {
    إرجع س * س.
}
```

**Baa IR:**

```
دالة @مربع(ص٦٤ %معامل٠) -> ص٦٤ {
بداية:
    %م٠ = ضرب ص٦٤ %معامل٠، %معامل٠
    رجوع ص٦٤ %م٠
}
```

### 8.2 Loop with Counter

**Baa Source:**

```baa
صحيح مجموع(صحيح ن) {
    صحيح ج = ٠.
    لكل (صحيح ع = ١؛ ع <= ن؛ ع++) {
        ج = ج + ع.
    }
    إرجع ج.
}
```

**Baa IR:**

```
دالة @مجموع(ص٦٤ %معامل٠) -> ص٦٤ {
بداية:
    %ج = حجز ص٦٤
    %ع = حجز ص٦٤
    خزن ص٦٤ ٠، %ج
    خزن ص٦٤ ١، %ع
    قفز %تحقق

تحقق:
    %م٠ = حمل ص٦٤، %ع
    %م١ = قارن أصغر_أو_يساوي ص٦٤ %م٠، %معامل٠
    قفز_شرط %م١، %جسم، %نهاية

جسم:
    %م٢ = حمل ص٦٤، %ج
    %م٣ = حمل ص٦٤، %ع
    %م٤ = جمع ص٦٤ %م٢، %م٣
    خزن ص٦٤ %م٤، %ج
    %م٥ = جمع ص٦٤ %م٣، ١
    خزن ص٦٤ %م٥، %ع
    قفز %تحقق

نهاية:
    %م٦ = حمل ص٦٤، %ج
    رجوع ص٦٤ %م٦
}
```

### 8.3 Conditional with Phi

**Baa Source:**

```baa
صحيح أقصى(صحيح أ، صحيح ب) {
    صحيح ن.
    إذا (أ > ب) { ن = أ. } وإلا { ن = ب. }
    إرجع ن.
}
```

**Baa IR (with phi optimization):**

```
دالة @أقصى(ص٦٤ %معامل٠، ص٦٤ %معامل١) -> ص٦٤ {
بداية:
    %م٠ = قارن أكبر ص٦٤ %معامل٠، %معامل١
    قفز_شرط %م٠، %أكبر، %أصغر

أكبر:
    قفز %دمج

أصغر:
    قفز %دمج

دمج:
    %م١ = فاي ص٦٤ [%معامل٠، %أكبر]، [%معامل١، %أصغر]
    رجوع ص٦٤ %م١
}
```

---

## 9. IR Text Format Grammar

```bnf
module      ::= (global | function)*
global      ::= ("internal")? ("const")? "global" "@" ident "=" type global_init
global_init ::= value
             | "zeroinit"
             | "{" (value ("," value)* ","?)? "}"
function    ::= "دالة" "@" ident "(" params variadic_opt? ")" "->" type "{" block+ "}"
params      ::= (type "%" ident ("،" type "%" ident)*)?
variadic_opt ::= "..." | "،" "..."
block       ::= label ":" instruction+
label       ::= arabic_word
instruction ::= assignment | terminator | store_inst
assignment  ::= "%" ident "=" opcode operands
terminator  ::= branch | return
branch      ::= "قفز" "%" label
              | "قفز_شرط" "%" ident "،" "%" label "،" "%" label
return      ::= "رجوع" type operand
store_inst  ::= "خزن" type operand "،" "%" ident
operand     ::= "%" ident | immediate
immediate   ::= arabic_number | "-" arabic_number
type        ::= "ص٦٤" | "ص٣٢" | "ص١٦" | "ص٨" | "ص١" | "فراغ" | pointer | array
pointer     ::= "مؤشر" "[" type "]"
array       ::= "مصفوفة" "[" type "،" number "]"
```

**Notes (v0.4.0.5):**

- IR text `call` يدعم:
  - نداء مباشر: `call <ret_type> @name(arg0, arg1, ...)`
  - نداء غير مباشر: `call <ret_type> <callee>(arg0, arg1, ...)` حيث `callee` يمكن أن يكون:
    - `%rN` قيمة من نوع `func(...) -> ...`
    - أو `@name` مرجع دالة (بعد تحسين/نشر قد يظهر كقيمة مباشرة)
- رأس الدالة في IR text يدعم لاحقة `...` للدوال المتغيرة المعاملات.
- في v0.4.0.5، دوال باء المتغيرة تُخفض داخلياً بإضافة معامل خفي (`i8*`) يحمل قاعدة المعاملات المتغيرة المعبأة.

---

## 10. Implementation Notes

### 10.1 C Data Structures

The IR is represented in C using the following structures (see `src/ir.h` for authoritative definitions):

#### IROp Enum

```c
typedef enum {
    // --------------------------------------------------------------------
    // Arithmetic Operations (العمليات الحسابية)
    // --------------------------------------------------------------------
    IR_OP_ADD,      // جمع - Addition
    IR_OP_SUB,      // طرح - Subtraction
    IR_OP_MUL,      // ضرب - Multiplication
    IR_OP_DIV,      // قسم - Signed division
    IR_OP_MOD,      // باقي - Modulo (remainder)
    IR_OP_NEG,      // سالب - Negation (unary minus)

    // --------------------------------------------------------------------
    // Memory Operations (عمليات الذاكرة)
    // --------------------------------------------------------------------
    IR_OP_ALLOCA,   // حجز - Stack allocation
    IR_OP_LOAD,     // حمل - Load from memory
    IR_OP_STORE,    // خزن - Store to memory
    IR_OP_PTR_OFFSET, // إزاحة_مؤشر - Pointer offset: base + index * sizeof(pointee)

    // --------------------------------------------------------------------
    // Comparison Operations (عمليات المقارنة)
    // --------------------------------------------------------------------
    IR_OP_CMP,      // قارن - Compare (with predicate)

    // --------------------------------------------------------------------
    // Logical Operations (العمليات المنطقية)
    // --------------------------------------------------------------------
    IR_OP_AND,      // و - Bitwise AND
    IR_OP_OR,       // أو - Bitwise OR
    IR_OP_XOR,      // أو_حصري - Bitwise XOR
    IR_OP_NOT,      // نفي - Bitwise NOT
    IR_OP_SHL,      // ازاحة_يسار - Shift left
    IR_OP_SHR,      // ازاحة_يمين - Shift right (arith/logical حسب النوع)

    // --------------------------------------------------------------------
    // Control Flow Operations (عمليات التحكم)
    // --------------------------------------------------------------------
    IR_OP_BR,       // قفز - Unconditional branch
    IR_OP_BR_COND,  // قفز_شرط - Conditional branch
    IR_OP_RET,      // رجوع - Return from function
    IR_OP_CALL,     // نداء - Function call

    // --------------------------------------------------------------------
    // SSA Operations (عمليات SSA)
    // --------------------------------------------------------------------
    IR_OP_PHI,      // فاي - Phi node for SSA
    IR_OP_COPY,     // نسخ - Copy (for SSA construction)

    // --------------------------------------------------------------------
    // Type Conversion (تحويل الأنواع)
    // --------------------------------------------------------------------
    IR_OP_CAST,     // تحويل - Type cast/conversion

    // --------------------------------------------------------------------
    // Special Operations
    // --------------------------------------------------------------------
    IR_OP_NOP,      // No operation (placeholder)

    IR_OP_COUNT     // Total number of opcodes
} IROp;
```

#### IRCmpPred Enum

```c
typedef enum {
    IR_CMP_EQ,      // يساوي - Equal
    IR_CMP_NE,      // لا_يساوي - Not equal
    IR_CMP_GT,      // أكبر - Greater than (signed)
    IR_CMP_LT,      // أصغر - Less than (signed)
    IR_CMP_GE,      // أكبر_أو_يساوي - Greater or equal (signed)
    IR_CMP_LE,      // أصغر_أو_يساوي - Less or equal (signed)

    // مقارنات بدون إشارة (unsigned)
    IR_CMP_UGT,     // أكبر - Greater than (unsigned)
    IR_CMP_ULT,     // أصغر - Less than (unsigned)
    IR_CMP_UGE,     // أكبر_أو_يساوي - Greater or equal (unsigned)
    IR_CMP_ULE,     // أصغر_أو_يساوي - Less or equal (unsigned)
} IRCmpPred;
```

#### IRTypeKind Enum

```c
typedef enum {
    IR_TYPE_VOID,   // فراغ - Void (no value)
    IR_TYPE_I1,     // ص١ - 1-bit integer (boolean)
    IR_TYPE_I8,     // ص٨ - 8-bit integer (byte/char)
    IR_TYPE_I16,    // ص١٦ - 16-bit integer
    IR_TYPE_I32,    // ص٣٢ - 32-bit integer
    IR_TYPE_I64,    // ص٦٤ - 64-bit integer (primary)
    IR_TYPE_U8,     // ط٨ - 8-bit unsigned integer
    IR_TYPE_U16,    // ط١٦ - 16-bit unsigned integer
    IR_TYPE_U32,    // ط٣٢ - 32-bit unsigned integer
    IR_TYPE_U64,    // ط٦٤ - 64-bit unsigned integer
    IR_TYPE_CHAR,   // حرف - UTF-8 character (packed)
    IR_TYPE_F64,    // ع٦٤ - 64-bit floating point (storage only for now)
    IR_TYPE_PTR,    // مؤشر - Pointer type
    IR_TYPE_ARRAY,  // مصفوفة - Array type
    IR_TYPE_FUNC,   // دالة - Function type
} IRTypeKind;
```

#### IRType Structure

```c
typedef struct IRType {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IRTypeKind kind;
    union {
        struct IRType* pointee;     // For IR_TYPE_PTR
        struct {
            struct IRType* element; // For IR_TYPE_ARRAY
            int count;
        } array;
        struct {
            struct IRType* ret;     // For IR_TYPE_FUNC
            struct IRType** params;
            int param_count;
        } func;
    } data;
} IRType;
```

#### IRValueKind Enum

```c
typedef enum {
    IR_VAL_NONE,        // No value (void)
    IR_VAL_CONST_INT,   // Constant integer
    IR_VAL_CONST_STR,   // Constant string (pointer to .rdata)
    IR_VAL_BAA_STR,     // Baa string (pointer to .rodata UTF-8 chars array)
    IR_VAL_REG,         // Virtual register (%م<n>)
    IR_VAL_GLOBAL,      // Global variable (@name)
    IR_VAL_FUNC,        // Function reference (@name)
    IR_VAL_BLOCK,       // Basic block reference
} IRValueKind;
```

**Note:** `IR_VAL_BAA_STR` represents Baa language strings (arrays of `حرف`) stored in read-only data, distinct from C-style strings (`IR_VAL_CONST_STR`).

#### Predefined Type Constants

The following global type constants are available for common types:

```c
extern IRType* IR_TYPE_VOID_T;   // فراغ
extern IRType* IR_TYPE_I1_T;     // ص١ (boolean)
extern IRType* IR_TYPE_I8_T;     // ص٨
extern IRType* IR_TYPE_I16_T;    // ص١٦
extern IRType* IR_TYPE_I32_T;    // ص٣٢
extern IRType* IR_TYPE_I64_T;    // ص٦٤
extern IRType* IR_TYPE_U8_T;     // ط٨
extern IRType* IR_TYPE_U16_T;    // ط١٦
extern IRType* IR_TYPE_U32_T;    // ط٣٢
extern IRType* IR_TYPE_U64_T;    // ط٦٤
extern IRType* IR_TYPE_CHAR_T;   // حرف (UTF-8 character)
extern IRType* IR_TYPE_F64_T;    // ع٦٤ (64-bit float)
```

#### IRPhiEntry Structure

```c
typedef struct IRPhiEntry {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IRValue* value;
    struct IRBlock* block;
    struct IRPhiEntry* next;
} IRPhiEntry;
```

#### IRValue Structure

```c
typedef struct IRValue {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IRValueKind kind;
    IRType* type;
    union {
        int64_t const_int;          // For IR_VAL_CONST_INT
        struct {
            char* data;             // For IR_VAL_CONST_STR
            int id;                 // String table ID
        } const_str;
        int reg_num;                // For IR_VAL_REG (%م<reg_num>)
        char* global_name;          // For IR_VAL_GLOBAL and IR_VAL_FUNC
        struct IRBlock* block;      // For IR_VAL_BLOCK
    } data;
} IRValue;
```

#### IRInst Structure

```c
typedef struct IRInst {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IROp op;                    // Opcode
    IRType* type;               // Result type (or void for stores/branches)

    // معرّف ثابت للتعليمة داخل الدالة (للتشخيص/الاختبارات)
    int id;

    // Destination register (for instructions that produce a value)
    int dest;                   // -1 if no destination

    // Operands (meaning depends on opcode)
    IRValue* operands[4];       // Up to 4 operands
    int operand_count;

    // For comparison instructions
    IRCmpPred cmp_pred;

    // For Phi nodes
    IRPhiEntry* phi_entries;    // Linked list of [value, block] pairs

    // For calls
    char* call_target;          // Function name
    IRValue* call_callee;       // قيمة الهدف عند النداء غير المباشر (NULL في النداء المباشر)
    IRValue** call_args;        // Argument list
    int call_arg_count;

    // Source location (for debugging)
    const char* src_file;
    int src_line;
    int src_col;

    // اسم رمز/متغير اختياري للتتبع (للديبغ)
    const char* dbg_name;

    // الأب (الكتلة التي تحتوي التعليمة)
    struct IRBlock* parent;

    // Linked list within block
    struct IRInst* prev;
    struct IRInst* next;
} IRInst;
```

#### IRBlock Structure

```c
typedef struct IRBlock {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* label;                // Block label (Arabic name like "بداية")
    int id;                     // Numeric ID for internal use

    // الأب (الدالة التي تحتوي الكتلة)
    struct IRFunc* parent;

    // Instructions (doubly-linked list)
    IRInst* first;
    IRInst* last;
    int inst_count;

    // Control flow graph edges
    struct IRBlock* succs[2];   // Successors (0-2 for br/br_cond)
    int succ_count;

    struct IRBlock** preds;     // Predecessors (dynamic array)
    int pred_count;
    int pred_capacity;

    // For dominance analysis
    struct IRBlock* idom;       // Immediate dominator
    struct IRBlock** dom_frontier;
    int dom_frontier_count;

    // Linked list of blocks in function
    struct IRBlock* next;
} IRBlock;
```

#### IRFunc Structure

```c
typedef struct IRFunc {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* name;                 // Function name
    IRType* ret_type;           // Return type

    // Parameters
    IRParam* params;
    int param_count;

    // Basic blocks (linked list)
    IRBlock* entry;             // Entry block (first block)
    IRBlock* blocks;            // All blocks
    int block_count;

    // Virtual register counter
    int next_reg;               // Next available %م<n>

    // عدّاد معرفات التعليمات
    int next_inst_id;

    // عدّاد تغييرات IR داخل الدالة (لتبطل التحليلات مثل Def-Use)
    uint32_t ir_epoch;

    // كاش Def-Use (تحليل) — يُخصّص على heap ويُعاد بناؤه عند التغييرات.
    IRDefUse* def_use;

    // Block ID counter
    int next_block_id;

    // Is this a prototype (declaration without body)?
    bool is_prototype;

    // هل الدالة متغيرة المعاملات (...)؟
    bool is_variadic;

    // Linked list of functions in module
    struct IRFunc* next;
} IRFunc;
```

#### IRGlobal Structure

```c
typedef struct IRGlobal {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* name;                 // Variable name
    IRType* type;               // Variable type
    // تهيئة عامة:
    // - للأنواع غير المصفوفة: init يحمل قيمة واحدة.
    // - للمصفوفات: init_elems يحمل قائمة (قد تكون جزئية) ويتم تعبئة الباقي بأصفار كما في C.
    IRValue* init;              // Initial scalar value (or NULL)
    IRValue** init_elems;       // Array initializer elements (or NULL)
    int init_elem_count;        // Number of provided initializer elements
    bool has_init_list;         // هل وُجدت '=' في المصدر (حتى لو كانت القائمة فارغة)
    bool is_const;              // Is this a constant?
    bool is_internal;           // هل الربط داخلي على مستوى الملف؟
    struct IRGlobal* next;
} IRGlobal;
```

#### IRParam Structure

```c
typedef struct IRParam {
    char* name;                 // Parameter name
    IRType* type;               // Parameter type
    int reg;                    // Virtual register assigned (%معامل<n>)
} IRParam;
```

#### IRStringEntry Structure

```c
typedef struct IRStringEntry {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* content;              // String content (UTF-8)
    int id;                     // Unique ID (.Lstr_<id>)
    struct IRStringEntry* next;
} IRStringEntry;
```

#### IRBaaStringEntry Structure

```c
typedef struct IRBaaStringEntry {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* content;              // String content (UTF-8)
    int id;                     // Unique ID (.Lbs_<id>)
    struct IRBaaStringEntry* next;
} IRBaaStringEntry;
```

#### IRModule Structure

```c
typedef struct IRModule {
    // ملاحظة: هذه البنية تُخصَّص بـ malloc وتحتوي على ساحة IR.
    char* name;                 // Module name (source file)

    // ساحة ذاكرة IR: كل كائنات IR تُخصَّص منها.
    IRArena arena;

    // كاش داخلي لأنواع شائعة لكل وحدة
    IRType* cached_i8_ptr_type;

    // Global variables
    IRGlobal* globals;
    int global_count;

    // Functions
    IRFunc* funcs;
    int func_count;

    // String table
    IRStringEntry* strings;
    int string_count;

    // Baa string table (as حرف[])
    IRBaaStringEntry* baa_strings;
    int baa_string_count;
} IRModule;
```

### 10.2 Arabic Name Conversion Functions

The following functions convert IR constructs to Arabic names:

```c
// Get Arabic opcode name
const char* ir_op_to_arabic(IROp op);

// Get Arabic comparison predicate name
const char* ir_cmp_pred_to_arabic(IRCmpPred pred);

// Get Arabic type name
const char* ir_type_to_arabic(IRType* type);

// Convert integer to Arabic numerals (٠١٢٣٤٥٦٧٨٩)
char* int_to_arabic_numerals(int n, char* buf);
```

### 10.3 IRBuilder API

The IRBuilder provides a convenient API for constructing IR:

```c
// Lifecycle
IRBuilder* ir_builder_new(IRModule* module);
void ir_builder_free(IRBuilder* builder);

// Function creation
IRFunc* ir_builder_create_func(IRBuilder* builder, const char* name, IRType* ret_type);
int ir_builder_add_param(IRBuilder* builder, const char* name, IRType* type);

// Block creation
IRBlock* ir_builder_create_block(IRBuilder* builder, const char* label);
void ir_builder_set_insert_point(IRBuilder* builder, IRBlock* block);

// Register allocation
int ir_builder_alloc_reg(IRBuilder* builder);

// Arithmetic emission
int ir_builder_emit_add(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_sub(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_mul(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_div(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_mod(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_neg(IRBuilder* builder, IRType* type, IRValue* operand);

// Memory emission
int ir_builder_emit_alloca(IRBuilder* builder, IRType* type);
int ir_builder_emit_load(IRBuilder* builder, IRType* type, IRValue* ptr);
void ir_builder_emit_store(IRBuilder* builder, IRValue* value, IRValue* ptr);
int ir_builder_emit_ptr_offset(IRBuilder* builder, IRType* ptr_type, IRValue* base, IRValue* index);

// Comparison emission
int ir_builder_emit_cmp(IRBuilder* builder, IRCmpPred pred, IRValue* lhs, IRValue* rhs);

// Unsigned comparison helpers
int ir_builder_emit_cmp_ugt(IRBuilder* builder, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_cmp_ult(IRBuilder* builder, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_cmp_uge(IRBuilder* builder, IRValue* lhs, IRValue* rhs);
int ir_builder_emit_cmp_ule(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

// Control flow emission
void ir_builder_emit_br(IRBuilder* builder, IRBlock* target);
void ir_builder_emit_br_cond(IRBuilder* builder, IRValue* cond, IRBlock* if_true, IRBlock* if_false);
void ir_builder_emit_ret(IRBuilder* builder, IRValue* value);

// Call emission
int ir_builder_emit_call(IRBuilder* builder, const char* target, IRType* ret_type,
                         IRValue** args, int arg_count);
int ir_builder_emit_call_indirect(IRBuilder* builder, IRValue* callee, IRType* ret_type,
                                  IRValue** args, int arg_count);

// SSA emission
int ir_builder_emit_phi(IRBuilder* builder, IRType* type);
void ir_builder_phi_add_incoming(IRBuilder* builder, int phi_reg, 
                                  IRValue* value, IRBlock* block);
int ir_builder_emit_copy(IRBuilder* builder, IRType* type, IRValue* source);

// Type conversion
int ir_builder_emit_cast(IRBuilder* builder, IRValue* value, IRType* to_type);

// Void return variants
void ir_builder_emit_ret_void(IRBuilder* builder);
void ir_builder_emit_ret_int(IRBuilder* builder, int64_t value);

// Void function calls
void ir_builder_emit_call_void(IRBuilder* builder, const char* target,
                                IRValue** args, int arg_count);

// Additional constant helpers
IRValue* ir_builder_const_i32(int32_t value);
IRValue* ir_builder_const_bool(int value);
IRValue* ir_builder_const_baa_string(IRBuilder* builder, const char* str);

// Global variable helpers
IRGlobal* ir_builder_create_global_init(IRBuilder* builder, const char* name,
                                         IRType* type, IRValue* init, int is_const);
IRValue* ir_builder_get_global(IRBuilder* builder, const char* name);

// Control flow structure helpers
void ir_builder_create_if_then(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* merge_label,
                                IRBlock** then_block, IRBlock** merge_block);
void ir_builder_create_if_else(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* else_label,
                                const char* merge_label,
                                IRBlock** then_block, IRBlock** else_block,
                                IRBlock** merge_block);
void ir_builder_create_while(IRBuilder* builder,
                              const char* header_label, const char* body_label,
                              const char* exit_label,
                              IRBlock** header_block, IRBlock** body_block,
                              IRBlock** exit_block);

// Statistics
int ir_builder_get_inst_count(IRBuilder* builder);
int ir_builder_get_block_count(IRBuilder* builder);
void ir_builder_print_stats(IRBuilder* builder);
```

### 10.4 Register Naming Convention

The IR uses the following register naming conventions:

| Register | Format | Example | Description |
|----------|--------|---------|-------------|
| Virtual | `%م<n>` | `%م٠`, `%م١٢` | SSA virtual registers |
| Parameter | `%معامل<n>` | `%معامل٠` | Function parameters |
| Special | `%إطار` (RBP) | `-1` | Frame pointer |
| Special | `%عائد` (RAX) | `-2` | Return value register |
| Special | `%مكدس` (RSP) | `-3` | Stack pointer |
| Arguments | `%معامل<n>` | `-10`, `-11`, ... | Argument registers |

**Note:** When printing IR with Arabic numerals, numbers use Arabic-Indic digits (٠١٢٣٤٥٦٧٨٩).

### 10.5 Memory Management

All IR objects are allocated from a memory arena (`IRArena`) associated with the module:

- **Arena allocation:** All IR structures (types, values, instructions, blocks, functions) are allocated from the module's arena
- **No individual frees:** Objects are not freed individually; the entire arena is freed when the module is destroyed
- **Automatic cleanup:** Call `ir_module_free()` to release all memory associated with a module

```c
IRModule* module = ir_module_new("example.baa");
// ... build IR ...
ir_module_free(module);  // Frees all arena memory
```
