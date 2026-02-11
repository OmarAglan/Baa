# Baa IR Specification

> **Version:** 0.3.2.6.3 | [← Compiler Internals](INTERNALS.md) | [API Reference →](API_REFERENCE.md)

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

| Type | Arabic | Size | Description |
|------|--------|------|-------------|
| `i64` | `ص٦٤` | 8 bytes | 64-bit signed integer |
| `i32` | `ص٣٢` | 4 bytes | 32-bit signed integer |
| `i16` | `ص١٦` | 2 bytes | 16-bit signed integer |
| `i8` | `ص٨` | 1 byte | 8-bit signed integer / char |
| `i1` | `ص١` | 1 bit | Boolean |
| `void` | `فراغ` | 0 | No value |

### 2.2 Derived Types

| Type | Arabic Syntax | Example |
|------|---------------|---------|
| Pointer | `مؤشر[<type>]` | `مؤشر[ص٦٤]` |
| Array | `مصفوفة[<type>، <size>]` | `مصفوفة[ص٦٤، ١٠]` |
| Function | `دالة(<args>) -> <ret>` | `دالة(ص٦٤، ص٦٤) -> ص٦٤` |

### 2.3 Type Compatibility

- Integer types can be extended or truncated via `تحويل`
- Pointers are 64-bit on x86-64 target
- `ص١` (boolean) is zero-extended to larger integers

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

### 3.2 Phi Nodes

Phi nodes merge values at control flow join points:

```
%م٣ = فاي ص٦٤ [%م١، %كتلة_أ]، [%م٢، %كتلة_ب]
```

The `فاي` instruction selects a value based on which predecessor block was executed.

**Note:** Phi nodes are an IR-level SSA construct. Before instruction selection, the compiler runs an Out-of-SSA rewrite that removes `فاي` by inserting edge copies (so the backend does not need to implement `phi` directly).

**Verifier (v0.3.2.5.3):** The compiler provides `--verify-ssa` to validate SSA invariants after Mem2Reg and before Out-of-SSA (single-definition, dominance, and phi incoming-edge correctness).

### 3.3 Memory Model

Stack allocations create addressable memory:

```
%م٠ = حجز ص٦٤           // Allocate 8 bytes on stack
خزن ص٦٤ ١٠، %م٠          // Store value 10
%م١ = حمل ص٦٤، %م٠       // Load value back
```

---

## 4. Instruction Set

### 4.1 Arithmetic Instructions

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `add` | `جمع` | `%r = جمع <type> %a، %b` | Addition |
| `sub` | `طرح` | `%r = طرح <type> %a، %b` | Subtraction |
| `mul` | `ضرب` | `%r = ضرب <type> %a، %b` | Multiplication |
| `div` | `قسم` | `%r = قسم <type> %a، %b` | Signed division |
| `mod` | `باقي` | `%r = باقي <type> %a، %b` | Modulo |
| `neg` | `سالب` | `%r = سالب <type> %a` | Negation |

### 4.2 Memory Instructions

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `alloca` | `حجز` | `%r = حجز <type>` | Stack allocation |
| `load` | `حمل` | `%r = حمل <type>، %ptr` | Load from memory |
| `store` | `خزن` | `خزن <type> <val>، %ptr` | Store to memory |

### 4.3 Comparison Instructions

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `cmp` | `قارن` | `%r = قارن <pred> <type> %a، %b` | Compare values |

**Predicates (مقارنات):**

| Predicate | Arabic | Meaning |
|-----------|--------|---------|
| `eq` | `يساوي` | Equal |
| `ne` | `لا_يساوي` | Not equal |
| `gt` | `أكبر` | Greater than |
| `lt` | `أصغر` | Less than |
| `ge` | `أكبر_أو_يساوي` | Greater or equal |
| `le` | `أصغر_أو_يساوي` | Less or equal |

### 4.4 Logical Instructions

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `and` | `و` | `%r = و <type> %a، %b` | Bitwise AND |
| `or` | `أو` | `%r = أو <type> %a، %b` | Bitwise OR |
| `not` | `نفي` | `%r = نفي <type> %a` | Bitwise NOT |

### 4.5 Control Flow Instructions

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `br` | `قفز` | `قفز %label` | Unconditional jump |
| `br_cond` | `قفز_شرط` | `قفز_شرط %cond، %true، %false` | Conditional branch |
| `ret` | `رجوع` | `رجوع <type> <val>` | Return from function |
| `call` | `نداء` | `%r = نداء <func>(<args>)` | Function call |

### 4.6 SSA Instructions

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `phi` | `فاي` | `%r = فاي <type> [%v1، %b1]، [%v2، %b2]` | Phi node |

### 4.7 Type Conversion

| Opcode | Arabic | Syntax | Description |
|--------|--------|--------|-------------|
| `cast` | `تحويل` | `%r = تحويل <from> %v إلى <to>` | Type conversion |

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
عام @نص_ترحيب = نص "مرحباً"
```

---

## 6. Lowering from AST

### 6.1 AST Node Mapping

| AST Node | IR Output |
|----------|-----------|
| `NODE_INT` | Immediate value |
| `NODE_VAR_REF` | `حمل` from allocated memory |
| `NODE_BIN_OP` | Arithmetic instruction |
| `NODE_ASSIGN` | `خزن` instruction |
| `NODE_IF` | `قفز_شرط` with two blocks |
| `NODE_WHILE` | Loop with `فاي` for variables |
| `NODE_FUNC_DEF` | `دالة` declaration |
| `NODE_CALL_EXPR` | `نداء` instruction |
| `NODE_RETURN` | `رجوع` instruction |

### 6.2 Variable Handling

Local variables are lowered to stack allocations:

```baa
صحيح س = ١٠.
```

Becomes:

```
%م٠ = حجز ص٦٤
خزن ص٦٤ ١٠، %م٠
```

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
| Mem2Reg | `ترقية_الذاكرة_إلى_سجلات` | Promote simple allocas to direct SSA register use |
| Constant Fold | `طي_الثوابت` | Evaluate constants at compile time |
| Dead Code | `حذف_الميت` | Remove unreachable code |
| Copy Propagation | `نشر_النسخ` | Replace copies with original |
| Common Subexpr | `حذف_المكرر` | Eliminate redundant computations |

### 7.3 Pass Order

1. `تحليل_السيطرة` - Build dominator tree
2. `ترقية_الذاكرة_إلى_سجلات` - Promote simple allocas (Mem2Reg baseline)
3. `طي_الثوابت` - Fold constant expressions
4. `نشر_النسخ` - Propagate copies
5. `حذف_المكرر` - Eliminate common subexpressions
6. `حذف_الميت` - Remove dead code
7. `تحليل_الحياة` - Compute liveness for register allocation

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
global      ::= "عام" "@" ident "=" type value
function    ::= "دالة" "@" ident "(" params ")" "->" type "{" block+ "}"
params      ::= (type "%" ident ("،" type "%" ident)*)?
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

---

## 10. Implementation Notes

### 10.1 C Data Structures

The IR will be represented in C using:

```c
typedef enum {
    IR_OP_ADD,    // جمع
    IR_OP_SUB,    // طرح
    IR_OP_MUL,    // ضرب
    IR_OP_DIV,    // قسم
    IR_OP_MOD,    // باقي
    IR_OP_LOAD,   // حمل
    IR_OP_STORE,  // خزن
    IR_OP_ALLOCA, // حجز
    IR_OP_CMP,    // قارن
    IR_OP_BR,     // قفز
    IR_OP_BR_COND,// قفز_شرط
    IR_OP_RET,    // رجوع
    IR_OP_CALL,   // نداء
    IR_OP_PHI,    // فاي
    // ...
} IROp;

typedef struct IRInst {
    IROp op;
    IRType type;
    int dest;           // %م<dest>
    int operands[4];    // Source registers or immediates
    struct IRInst* next;
} IRInst;

typedef struct IRBlock {
    char* label;        // Arabic block name
    IRInst* first;
    IRInst* last;
    struct IRBlock* succs[2];  // Successors
    struct IRBlock* next;
} IRBlock;

typedef struct IRFunc {
    char* name;
    IRType ret_type;
    IRBlock* entry;
    int reg_count;      // Next available %م number
} IRFunc;
```

### 10.2 Printer Flag

The IR can be printed using `--dump-ir` flag:

```bash
baa --dump-ir source.baa
```

---

*[← Compiler Internals](INTERNALS.md) | [API Reference →](API_REFERENCE.md)]*
