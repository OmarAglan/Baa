# Baa Roadmap (Updated)

> Track the development progress of the Baa programming language.
> **Current Status:** Phase 3 - Intermediate Representation (v0.3.0)

---

## 📚 Documentation Track (Definitive Arabic Book)

*Goal: Produce a Kernighan & Ritchie–style first book for Baa, in Arabic, serving as the definitive learning + reference resource.*

- [ ] **Write the Arabic "Baa Book"** — book-length guide in Arabic with exercises.
- [ ] **Define terminology glossary** — consistent Arabic technical vocabulary.
- [ ] **Create example suite** — verified, idiomatic examples that compile with v0.2.9.
- [ ] **Add exercises and challenges** — per chapter, with expected outputs.
- [ ] **Add debugging and performance chapters** — common pitfalls, diagnostics, optimization notes.
- [ ] **Native technical review** — review by Arabic-speaking engineers before release.

---

## ⚙️ Phase 3: The Intermediate Representation (v0.3.x) ← CURRENT

*Goal: Decouple the language from x86 Assembly to enable optimizations and multiple backends.*

> **Design Document:** See [BAA_IR_SPECIFICATION.md](docs/BAA_IR_SPECIFICATION.md) for full IR specification.

### v0.3.0: IR Foundation 🏗️ ← IN PROGRESS

#### v0.3.0.1: IR Data Structures ✅ COMPLETED (2026-01-15)

- [x] **Define `IROp` enum** — All opcodes: `IR_OP_ADD`, `IR_OP_SUB`, `IR_OP_MUL`, etc.
- [x] **Define `IRType` enum** — Types: `IR_TYPE_I64`, `IR_TYPE_I32`, `IR_TYPE_I8`, `IR_TYPE_I1`, `IR_TYPE_PTR`.
- [x] **Define `IRInst` struct** — Instruction with opcode, type, dest register, operands.
- [x] **Define `IRBlock` struct** — Basic block with label, instruction list, successors.
- [x] **Define `IRFunc` struct** — Function with name, return type, entry block, register counter.
- [x] **Create `ir.h`** — Header file with all IR definitions.
- [x] **Create `ir.c`** — Implementation with helper functions and IR printing.

#### v0.3.0.2: IR Builder Functions ✅ COMPLETED (2026-01-15)

- [x] **`IRBuilder` context struct** — Builder pattern with insertion point tracking.
- [x] **`ir_builder_create_func()`** — Create a new IR function.
- [x] **`ir_builder_create_block()`** — Create a new basic block with label.
- [x] **`ir_builder_set_insert_point()`** — Set insertion point for new instructions.
- [x] **`ir_builder_alloc_reg()`** — Allocate next virtual register `%م<n>`.
- [x] **`ir_builder_emit_*()`** — Emit instructions (add, sub, mul, div, load, store, br, ret, call, etc.).
- [x] **Control flow helpers** — `ir_builder_create_if_then()`, `ir_builder_create_while()`.
- [x] **Create `ir_builder.h`** — Header file with builder API.
- [x] **Create `ir_builder.c`** — Implementation of builder functions.

#### v0.3.0.3: AST to IR Lowering (Expressions) ✅ COMPLETED (2026-01-16)

- [x] **`lower_expr()`** — Main expression lowering dispatcher.
- [x] **Lower `NODE_INT`** — Return immediate value.
- [x] **Lower `NODE_VAR_REF`** — Generate `حمل` (load) instruction.
- [x] **Lower `NODE_BIN_OP`** — Generate `جمع`/`طرح`/`ضرب`/`قسم` instructions.
- [x] **Lower `NODE_UNARY_OP`** — Generate `سالب`/`نفي` instructions.
- [x] **Lower `NODE_CALL_EXPR`** — Generate `نداء` (call) instruction.

#### v0.3.0.4: AST to IR Lowering (Statements) ✅ COMPLETED (2026-01-16)

- [x] **`lower_stmt()`** — Main statement lowering dispatcher.
- [x] **Lower `NODE_VAR_DECL`** — Generate `حجز` (alloca) + `خزن` (store).
- [x] **Lower `NODE_ASSIGN`** — Generate `خزن` (store) instruction.
- [x] **Lower `NODE_RETURN`** — Generate `رجوع` (return) instruction.
- [x] **Lower `NODE_PRINT`** — Generate `نداء @اطبع()` call.
- [x] **Lower `NODE_READ`** — Generate `نداء @اقرأ()` call.

#### v0.3.0.5: AST to IR Lowering (Control Flow) ✅ COMPLETED (2026-01-16)

- [x] **Lower `NODE_IF`** — Create condition block + true/false blocks + merge block.
- [x] **Lower `NODE_WHILE`** — Create header/body/exit blocks with back edge.
- [x] **Lower `NODE_FOR`** — Create init/header/body/increment/exit blocks.
- [x] **Lower `NODE_SWITCH`** — Create comparison chain + case blocks.
- [x] **Lower `NODE_BREAK`** — Generate `قفز` to loop exit.
- [x] **Lower `NODE_CONTINUE`** — Generate `قفز` to loop header/increment.

#### v0.3.0.6: IR Printer ✅ COMPLETED (2026-01-17)

- [x] **`ir_print_func()`** — Print function header and all blocks.
- [x] **`ir_print_block()`** — Print block label and all instructions.
- [x] **`ir_print_inst()`** — Print single instruction with Arabic opcodes.
- [x] **Arabic numeral output** — Print register numbers in Arabic (٠١٢٣٤٥٦٧٨٩).
- [x] **`--dump-ir` CLI flag** — Add command-line option to print IR.

#### v0.3.0.7: Integration & Testing ✅ COMPLETED (2026-01-17)

- [x] **Integrate IR into pipeline** — AST → IR (skip direct codegen).
- [x] **Create `ir_test.baa`** — Simple test programs.
- [x] **Verify IR output** — Check IR text matches specification.
- [x] **Update `main.c`** — Add IR phase between analysis and codegen.
- [x] **Add `--emit-ir` flag** — Write IR to `.ir` file.
- [x] **Fix global variable resolution** — Proper lookup in `lower_expr()` and `lower_assign()`.

---

### v0.3.1: The Optimizer ⚡

#### v0.3.1.1: Analysis Infrastructure ✅ COMPLETED (2026-01-21)

- [x] **CFG validation** — Verify all blocks have terminators.
- [x] **Predecessor lists** — Build predecessor list for each block.
- [x] **Dominator tree** — Compute dominance relationships.
- [x] **Define `IRPass` interface** — Function pointer for optimization passes.

#### v0.3.1.2: Constant Folding (طي_الثوابت) ✅ COMPLETED (2026-01-22)

- [x] **Detect constant operands** — Both operands are immediate values.
- [x] **Fold arithmetic** — `جمع ص٦٤ ٥، ٣` → `٨`.
- [x] **Fold comparisons** — `قارن أكبر ص٦٤ ١٠، ٥` → `صواب`.
- [x] **Replace instruction** — Remove op, use constant result.

#### v0.3.1.3: Dead Code Elimination (حذف_الميت) ✅ COMPLETED (2026-01-27)

- [x] **Mark used values** — Walk from terminators backward.
- [x] **Identify dead instructions** — Result never used.
- [x] **Remove dead instructions** — Delete from block.
- [x] **Remove unreachable blocks** — No predecessors (except entry).

#### v0.3.1.4: Copy Propagation (نشر_النسخ)

- [ ] **Detect copy instructions** — `%م١ = %م٠` pattern.
- [ ] **Replace uses** — Substitute original for copy.
- [ ] **Remove redundant copies** — Delete copy instruction.

#### v0.3.1.5: Common Subexpression Elimination (حذف_المكرر)

- [ ] **Hash expressions** — Create signature for each operation.
- [ ] **Detect duplicates** — Same op + same operands.
- [ ] **Replace with existing result** — Reuse previous computation.

#### v0.3.1.6: Optimization Pipeline

- [ ] **Pass ordering** — Define optimal pass sequence.
- [ ] **Iteration** — Run passes until no changes.
- [ ] **`-O0`, `-O1`, `-O2` flags** — Control optimization level.
- [ ] **`--dump-ir-opt`** — Print IR after optimization.

---

### v0.3.2: The Backend (Target Independence) 🎯

#### v0.3.2.1: Instruction Selection

- [ ] **Define `MachineInst`** — Abstract machine instruction.
- [ ] **IR to Machine mapping** — `جمع` → `ADD`, `حمل` → `MOV`, etc.
- [ ] **Pattern matching** — Select optimal instruction sequences.
- [ ] **Handle immediates** — Inline constants where possible.

#### v0.3.2.2: Register Allocation

- [ ] **Liveness analysis** — Compute live ranges for each virtual register.
- [ ] **Linear scan allocator** — Simple, fast allocation algorithm.
- [ ] **Spilling** — Handle register pressure by spilling to stack.
- [ ] **Map to x64 registers** — RAX, RBX, RCX, RDX, R8-R15.

#### v0.3.2.3: Code Emission

- [ ] **Emit function prologue** — Stack setup, callee-saved registers.
- [ ] **Emit instructions** — Generate AT&T syntax assembly.
- [ ] **Emit function epilogue** — Stack teardown, return.
- [ ] **Emit data section** — Global variables and string literals.

#### v0.3.2.4: Backend Integration

- [ ] **Replace old codegen** — IR → Backend → Assembly.
- [ ] **Verify output** — Compare with old codegen results.
- [ ] **Performance testing** — Ensure no regression.
- [ ] **Remove legacy codegen** — Delete `codegen.c` direct AST translation.

---

### v0.3.2.5: SSA Construction 🔄

#### v0.3.2.5.1: Memory to Register Promotion

- [ ] **Identify promotable allocas** — Single-block allocas with no escaping.
- [ ] **Replace loads/stores** — Convert to direct register use.
- [ ] **Remove dead allocas** — Delete promoted `حجز` instructions.

#### v0.3.2.5.2: Phi Node Insertion

- [ ] **Compute dominance frontiers** — Where Phi nodes are needed.
- [ ] **Insert Phi placeholders** — Add `فاي` at join points.
- [ ] **Rename variables** — SSA renaming pass with reaching definitions.
- [ ] **Connect Phi operands** — Link values from predecessor blocks.

#### v0.3.2.5.3: SSA Validation

- [ ] **Verify SSA properties** — Each register defined exactly once.
- [ ] **Check dominance** — Definition dominates all uses.
- [ ] **Validate Phi nodes** — One operand per predecessor.
- [ ] **`--verify-ssa` flag** — Debug option to run SSA checks.

---

### v0.3.2.6: IR Stabilization & Polish 🧹

#### v0.3.2.6.1: IR Memory Management

- [ ] **Arena allocator for IR** — Fast allocation, bulk deallocation.
- [ ] **IR cloning** — Deep copy of functions/blocks.
- [ ] **IR destruction** — Clean up all IR memory.

#### v0.3.2.6.2: Debug Information

- [ ] **Source location tracking** — Map IR instructions to source lines.
- [ ] **Variable name preservation** — Keep original names for debugging.
- [ ] **`--debug-info` flag** — Emit debug metadata in assembly.

#### v0.3.2.6.3: IR Serialization

- [ ] **Text IR writer** — Output canonical IR text format.
- [ ] **Text IR reader** — Parse IR text back to data structures.
- [ ] **Round-trip testing** — Write → Read → Compare.

---

### v0.3.2.7: Advanced Optimizations 🚀

#### v0.3.2.7.1: Loop Optimizations

- [ ] **Loop detection** — Identify natural loops via back edges.
- [ ] **Loop invariant code motion** — Hoist constant computations.
- [ ] **Strength reduction** — Replace expensive ops (mul → shift).
- [ ] **Loop unrolling** — Optional with `-funroll-loops`.

#### v0.3.2.7.2: Inlining

- [ ] **Inline heuristics** — Small functions, single call site.
- [ ] **Inline expansion** — Copy function body to call site.
- [ ] **Post-inline cleanup** — Re-run optimization passes.

#### v0.3.2.7.3: Tail Call Optimization

- [ ] **Detect tail calls** — Call immediately before return.
- [ ] **Convert to jump** — Replace call+ret with jump.
- [ ] **Stack reuse** — Reuse caller's stack frame.

---

### v0.3.2.8: Multi-Target Preparation 🌐

#### v0.3.2.8.1: Target Abstraction

- [ ] **Define `Target` interface** — Register info, calling convention.
- [ ] **x86-64 Windows target** — Current implementation as first target.
- [ ] **Target selection** — `--target=x86_64-windows` flag.

#### v0.3.2.8.2: Calling Convention Abstraction

- [ ] **Define `CallingConv` struct** — Arg registers, return register.
- [ ] **Windows x64 ABI** — Current convention as default.
- [ ] **SystemV AMD64 ABI** — Linux/macOS convention.

#### v0.3.2.8.3: Code Model Options

- [ ] **Small code model** — All code/data within 2GB (default).
- [ ] **PIC support** — Position independent code flag.
- [ ] **Stack protection** — Optional stack canaries.

#### v0.3.2.8.4: Linux x86-64 Target 🐧

- [ ] **SystemV AMD64 ABI implementation** — Different calling convention.
- [ ] **ELF output support** — Instead of PE/COFF.
- [ ] **Linux syscall wrappers** — Or libc linking.
- [ ] **GCC/Clang backend for Linux** — For Linux assembly.
- [ ] **Cross-compilation** — `--target=x86_64-linux` from Windows.

---

### v0.3.2.9: IR Verification & Benchmarking ✅

#### v0.3.2.9.1: Comprehensive IR Verification

- [ ] **Well-formedness checks** — All functions have entry blocks.
- [ ] **Type consistency** — Operand types match instruction requirements.
- [ ] **CFG integrity** — All branches point to valid blocks.
- [ ] **SSA verification** — Run `--verify-ssa` on all test programs.
- [ ] **`baa --verify` mode** — Run all verification passes.

#### v0.3.2.9.2: Performance Benchmarking

- [ ] **Compile-time benchmark** — Compare old vs new codegen speed.
- [ ] **Runtime benchmark** — Compare generated code performance.
- [ ] **Memory usage profiling** — Track peak memory during compilation.
- [ ] **Benchmark suite** — Collection of representative programs.

#### v0.3.2.9.3: Regression Testing

- [ ] **Output comparison** — Old codegen vs IR-based codegen.
- [ ] **Test all v0.2.x programs** — Ensure backward compatibility.
- [ ] **Edge case testing** — Complex control flow, nested loops, recursion.
- [ ] **Error case testing** — Verify error messages unchanged.

#### v0.3.2.9.4: Documentation & Cleanup

- [ ] **Update INTERNALS.md** — Document new IR pipeline.
- [ ] **IR Developer Guide** — How to add new IR instructions.
- [ ] **Remove deprecated code** — Clean up old codegen paths.
- [ ] **Code review checklist** — Ensure code quality standards.

---

## 📚 Phase 3.5: Language Completeness (v0.3.3 - v0.3.12)

*Goal: Add essential features to make Baa practical for real-world programs and ready for self-hosting.*

### v0.3.3: Array Initialization 📊

**Goal:** Enable direct initialization of arrays with values.

#### Features

- [ ] **Array Literal Syntax** – Initialize arrays with comma-separated values using `{` `}`.
  
**Syntax:**

```baa
صحيح قائمة[٥] = {١، ٢، ٣، ٤، ٥}.

// With Arabic comma (،) or regular comma (,)
صحيح أرقام[٣] = {١٠، ٢٠، ٣٠}.
```

#### Implementation Tasks

- [ ] **Parser**: Handle `{` `}` initializer list after array declaration.
- [ ] **Parser**: Support both Arabic comma `،` (U+060C) and regular comma `,` as separators.
- [ ] **Semantic Analysis**: Verify initializer count matches array size.
- [ ] **Codegen**: Generate sequential assignments in `.data` section (for globals) or stack initialization (for locals).

#### Deferred to v0.3.8

- Multi-dimensional arrays: `صحيح مصفوفة[٣][٤].`
- Array length operator: `صحيح طول = حجم(قائمة).`

---

### v0.3.4: Enumerations & Structures 🏗️

**Goal:** Add compound types for better code organization and type safety.

#### Features

- [ ] **Enum Declaration** – Named integer constants with type safety.
- [ ] **Struct Declaration** – Group related data into composite types.
- [ ] **Member Access** – Use `:` (colon) operator for accessing members.

**Complete Example:**

```baa
// ١. تعريف التعداد (Enumeration)
تعداد لون {
    أحمر،
    أزرق،
    أسود،
    أبيض
}

// ٢. تعريف الهيكل (Structure)
هيكل سيارة {
    نص موديل.
    صحيح سنة_الصنع.
    تعداد لون لون_السيارة.
}

صحيح الرئيسية() {
    هيكل سيارة س.
    
    س:موديل = "تويوتا كورولا".
    س:سنة_الصنع = ٢٠٢٤.
    س:لون_السيارة = لون:أحمر.
    
    اطبع س:موديل.
    اطبع س:سنة_الصنع.
    
    إذا (س:لون_السيارة == لون:أحمر) {
        اطبع "تحذير: السيارات الحمراء سريعة!".
    }
    
    إرجع ٠.
}
```

#### Implementation Tasks

**Enumerations:**

- [ ] **Token**: Add `TOKEN_ENUM` for `تعداد` keyword.
- [ ] **Parser**: Parse enum declaration: `تعداد <name> { <members> }`.
- [ ] **Parser**: Support Arabic comma `،` between enum members.
- [ ] **Semantic**: Auto-assign integer values (0, 1, 2...).
- [ ] **Semantic**: Enum values accessible via `<enum_name>:<value_name>`.
- [ ] **Type System**: Add `TYPE_ENUM` to `DataType`.

**Structures:**

- [ ] **Token**: Add `TOKEN_STRUCT` for `هيكل` keyword.
- [ ] **Token**: Add `TOKEN_COLON` for `:` (already exists, verify usage).
- [ ] **Parser**: Parse struct declaration: `هيكل <name> { <fields> }`.
- [ ] **Parser**: Parse struct instantiation: `هيكل <name> <var>.`
- [ ] **Parser**: Parse member access: `<var>:<member>`.
- [ ] **Semantic**: Track struct definitions in symbol table.
- [ ] **Semantic**: Validate member access against struct definition.
- [ ] **Memory Layout**: Calculate field offsets with padding/alignment.
- [ ] **Codegen**: Emit struct definitions and member access code.

---

### v0.3.5: Character Type 📝

**Goal:** Add proper character type to align with C conventions.

#### Features

- [ ] **Character Type (`حرف`)** – Proper 1-byte character type (like C's `char`).
- [ ] **String-Char Relationship** – Strings (`نص`) become arrays of characters (`حرف[]`).

**Syntax:**

```baa
حرف ح = 'أ'.
نص اسم = "أحمد".  // Equivalent to: حرف اسم[] = {'أ', 'ح', 'م', 'د', '\0'}.
```

#### Implementation Tasks

- [ ] **Token**: Already have `TOKEN_CHAR` for literals.
- [ ] **Token**: Add `TOKEN_KEYWORD_CHAR` for `حرف` type keyword.
- [ ] **Type System**: Add `TYPE_CHAR` to `DataType` enum.
- [ ] **Semantic**: Distinguish between `char` and `int` (currently chars are ints).
- [ ] **Codegen**: Generate 1-byte storage for `حرف` (currently 8-byte).
- [ ] **String Representation**: Update internal string handling to use `char*`.

#### Deferred to v0.3.9

- String operations: `طول_نص()`, `دمج_نص()`, `قارن_نص()`
- String indexing: `اسم[٠]` returns `حرف`

---

### v0.3.6: Low-Level Operations 🔧

**Goal:** Add bitwise operations and low-level features needed for systems programming.

#### Features

- [ ] **Bitwise Operators**:

  ```baa
  صحيح أ = ٥ & ٣.      // AND: 5 & 3 = 1
  صحيح ب = ٥ | ٣.      // OR:  5 | 3 = 7
  صحيح ج = ٥ ^ ٣.      // XOR: 5 ^ 3 = 6
  صحيح د = ~٥.         // NOT: ~5 = -6
  صحيح هـ = ١ << ٤.    // Left shift:  1 << 4 = 16
  صحيح و = ١٦ >> ٢.    // Right shift: 16 >> 2 = 4
  ```

- [ ] **Sizeof Operator**:

  ```baa
  صحيح حجم_صحيح = حجم(صحيح).    // Returns 8
  صحيح حجم_حرف = حجم(حرف).      // Returns 1
  صحيح حجم_مصفوفة = حجم(قائمة). // Returns array size in bytes
  ```

- [ ] **Void Type**:

  ```baa
  عدم اطبع_رسالة() {
      اطبع "مرحباً".
      // No return needed
  }
  ```

- [ ] **Escape Sequences**:

  ```baa
  نص سطر = "سطر١\nسطر٢".     // Newline
  نص جدول = "عمود١\tعمود٢".  // Tab
  نص مسار = "C:\\ملفات".     // Backslash
  حرف صفر = '\٠'.            // Null character
  ```

#### Implementation Tasks

- [ ] **Lexer**: Tokenize `&`, `|`, `^`, `~`, `<<`, `>>`.
- [ ] **Parser**: Add bitwise operators with correct precedence.
- [ ] **Parser**: Parse `حجم(type)` and `حجم(expr)` expressions.
- [ ] **Lexer**: Add `عدم` keyword for void type.
- [ ] **Lexer**: Handle escape sequences in string/char literals.
- [ ] **Semantic**: Type check bitwise operations (integers only).
- [ ] **Codegen**: Generate bitwise assembly instructions.
- [ ] **Codegen**: Calculate sizes for `حجم` operator.

---

### v0.3.7: System Improvements 🔧

**Goal:** Refine and enhance existing compiler systems.

#### Focus Areas

- [ ] **Error Messages** – Improve clarity and helpfulness of diagnostic messages.
- [ ] **Code Quality** – Refactor complex functions, improve code organization.
- [ ] **Memory Management** – Fix memory leaks, improve buffer handling.
- [ ] **Performance** – Profile and optimize slow compilation paths.
- [ ] **Documentation** – Update all docs to reflect v0.3.3-0.3.6 changes.
- [ ] **Edge Cases** – Fix known bugs and handle corner cases.

#### Specific Improvements

- [ ] Improve panic mode recovery in parser.
- [ ] Better handling of UTF-8 edge cases in lexer.
- [ ] Optimize symbol table lookups (consider hash table).
- [ ] Add more comprehensive error recovery.
- [ ] Improve codegen output readability (comments in assembly).

---

### v0.3.8: Testing & Quality Assurance ✅

**Goal:** Establish robust testing infrastructure and fix accumulated issues.

#### Test System

- [ ] **Test Framework** – Create automated test runner.
  - Script to compile and run `.baa` test files.
  - Compare actual output vs expected output.
  - Report pass/fail with clear diagnostics.

- [ ] **Test Categories**:
  - [ ] **Lexer Tests** – Token generation, UTF-8 handling, preprocessor.
  - [ ] **Parser Tests** – Syntax validation, error recovery.
  - [ ] **Semantic Tests** – Type checking, scope validation.
  - [ ] **Codegen Tests** – Correct assembly output, execution results.
  - [ ] **Integration Tests** – Full programs with expected output.

- [ ] **Test Coverage**:
  - [ ] All language features (v0.0.1 - v0.3.7).
  - [ ] Edge cases and corner cases.
  - [ ] Error conditions (syntax errors, type mismatches, etc.).
  - [ ] Multi-file compilation scenarios.
  - [ ] Preprocessor directive combinations.

#### CI/CD Setup

- [ ] **GitHub Actions workflow**:

  ```yaml
  name: Baa CI
  on: [push, pull_request]
  jobs:
    build-and-test:
      runs-on: windows-latest
      steps:
        - uses: actions/checkout@v3
        - name: Build Baa
          run: gcc src/*.c -o baa.exe
        - name: Run Tests
          run: ./run_tests.bat
  ```

#### Bug Fixes & Refinements

- [ ] **Known Issues** – Fix all open bugs from previous versions.
- [ ] **Regression Testing** – Ensure new features don't break old code.
- [ ] **Stress Testing** – Test with large files, deep nesting, many symbols.
- [ ] **Arabic Text Edge Cases** – Test various Arabic Unicode scenarios.

---

### v0.3.9: Advanced Arrays & String Operations 📐

**Goal:** Complete array and string functionality.

#### Array Features

- [ ] **Multi-dimensional Arrays**:

  ```baa
  صحيح مصفوفة[٣][٤].
  مصفوفة[٠][٠] = ١٠.
  مصفوفة[١][٢] = ٢٠.
  ```

- [ ] **Array Length Operator**:

  ```baa
  صحيح قائمة[١٠].
  صحيح الطول = حجم(قائمة) / حجم(صحيح).  // Returns 10
  ```

- [ ] **Array Bounds Checking** (Optional debug mode):
  - Runtime checks with `-g` flag.
  - Panic on out-of-bounds access.

#### String Operations

- [ ] **String Length**: `صحيح الطول = طول_نص(اسم).`
- [ ] **String Concatenation**: `نص كامل = دمج_نص(اسم, " علي").`
- [ ] **String Comparison**: `صحيح نتيجة = قارن_نص(اسم, "محمد").`
- [ ] **String Indexing** (read-only): `حرف أول = اسم[٠].`
- [ ] **String Copy**: `نص نسخة = نسخ_نص(اسم).`

#### Implementation

- [ ] **Parser**: Parse multi-dimensional array declarations and access.
- [ ] **Semantic**: Track array dimensions in symbol table.
- [ ] **Codegen**: Calculate offsets for multi-dimensional arrays (row-major order).
- [ ] **Standard Library**: Create `baalib.baa` with string functions.
- [ ] **UTF-8 Aware**: Ensure functions handle multi-byte Arabic characters correctly.

---

### v0.3.10: Pointers & References 🎯

**Goal:** Add pointer types for manual memory management and data structures.

#### Features

- [ ] **Pointer Type Declaration**:

  ```baa
  صحيح* مؤشر.           // Pointer to integer
  حرف* نص_مؤشر.         // Pointer to character (C-string)
  هيكل سيارة* س_مؤشر.   // Pointer to struct
  ```

- [ ] **Address-of Operator** (`&`):

  ```baa
  صحيح س = ١٠.
  صحيح* م = &س.         // م points to س
  ```

- [ ] **Dereference Operator** (`*`):

  ```baa
  صحيح قيمة = *م.       // قيمة = 10
  *م = ٢٠.              // س now equals 20
  ```

- [ ] **Null Pointer**:

  ```baa
  صحيح* م = عدم.        // Null pointer
  إذا (م == عدم) {
      اطبع "مؤشر فارغ".
  }
  ```

- [ ] **Pointer Arithmetic**:

  ```baa
  صحيح قائمة[٥] = {١، ٢، ٣، ٤، ٥}.
  صحيح* م = &قائمة[٠].
  م = م + ١.             // Points to قائمة[١]
  اطبع *م.               // Prints 2
  ```

#### Implementation Tasks

- [ ] **Lexer**: Handle `*` in type context vs multiplication.
- [ ] **Parser**: Parse pointer type declarations.
- [ ] **Parser**: Parse address-of (`&`) and dereference (`*`) expressions.
- [ ] **Type System**: Add `TYPE_POINTER` with base type tracking.
- [ ] **Semantic**: Validate pointer operations (can't dereference non-pointer).
- [ ] **Semantic**: Type check pointer arithmetic.
- [ ] **Codegen**: Generate LEA for address-of.
- [ ] **Codegen**: Generate proper load/store for dereference.

---

### v0.3.11: Dynamic Memory 🧠

**Goal:** Enable heap allocation for dynamic data structures.

#### Features

- [ ] **Memory Allocation**:

  ```baa
  // Allocate memory for 10 integers
  صحيح* قائمة = حجز_ذاكرة(١٠ * حجم(صحيح)).
  
  // Allocate memory for a struct
  هيكل سيارة* س = حجز_ذاكرة(حجم(هيكل سيارة)).
  ```

- [ ] **Memory Deallocation**:

  ```baa
  تحرير_ذاكرة(قائمة).
  تحرير_ذاكرة(س).
  ```

- [ ] **Memory Reallocation**:

  ```baa
  // Resize array to 20 integers
  قائمة = إعادة_حجز(قائمة, ٢٠ * حجم(صحيح)).
  ```

- [ ] **Memory Operations**:

  ```baa
  // Copy memory
  نسخ_ذاكرة(وجهة, مصدر, حجم).
  
  // Set memory to value
  تعيين_ذاكرة(مؤشر, ٠, حجم).
  ```

#### Implementation Tasks

- [ ] **Runtime**: Link with C malloc/free or implement custom allocator.
- [ ] **Built-in Functions**: Add `حجز_ذاكرة`, `تحرير_ذاكرة`, `إعادة_حجز`.
- [ ] **Semantic**: Track allocated memory for warnings.
- [ ] **Codegen**: Generate calls to allocation functions.

---

### v0.3.12: File I/O 📁

**Goal:** Enable reading and writing files for compiler self-hosting.

#### Features

- [ ] **File Opening**:

  ```baa
  صحيح ملف = فتح_ملف("بيانات.txt", "قراءة").
  صحيح ملف_كتابة = فتح_ملف("ناتج.txt", "كتابة").
  صحيح ملف_إضافة = فتح_ملف("سجل.txt", "إضافة").
  ```

- [ ] **File Reading**:

  ```baa
  حرف حرف_واحد = اقرأ_حرف(ملف).
  نص سطر = اقرأ_سطر(ملف).
  صحيح بايتات = اقرأ_ملف(ملف, مخزن, حجم).
  ```

- [ ] **File Writing**:

  ```baa
  اكتب_حرف(ملف, 'أ').
  اكتب_سطر(ملف, "مرحباً").
  اكتب_ملف(ملف, بيانات, حجم).
  ```

- [ ] **File Closing**:

  ```baa
  اغلق_ملف(ملف).
  ```

- [ ] **File Status**:

  ```baa
  منطقي انتهى = نهاية_ملف(ملف).
  صحيح موقع = موقع_ملف(ملف).
  اذهب_لموقع(ملف, ٠).
  ```

#### Implementation Tasks

- [ ] **Runtime**: Wrap C stdio functions (fopen, fread, fwrite, fclose).
- [ ] **Built-in Functions**: Add file operation functions.
- [ ] **Error Handling**: Return error codes for failed operations.
- [ ] **Codegen**: Generate calls to file functions.

---

## 📚 Phase 4: Standard Library & Polish (v0.4.x)

*Goal: Make Baa production-ready with a comprehensive standard library.*

### v0.4.0: Formatted Output & Input 🖨️

**Goal:** Professional I/O capabilities.

- [ ] **Formatted Output**:

  ```baa
  اطبع_منسق("الاسم: %s، العمر: %d\n", اسم, عمر).
  ```

- [ ] **String Formatting**:

  ```baa
  نص رسالة = نسق("النتيجة: %d", قيمة).
  ```

- [ ] **Formatted Input**:

  ```baa
  نص إدخال = اقرأ_سطر().
  صحيح رقم = اقرأ_رقم().
  ```

### v0.4.1: Standard Library (مكتبة باء) 📚

- [ ] **Math Module** — `جذر_تربيعي()`, `أس()`, `مطلق()`, `عشوائي()`.
- [ ] **String Module** — Complete string manipulation.
- [ ] **IO Module** — File and console operations.
- [ ] **System Module** — Environment variables, command execution.
- [ ] **Time Module** — Date/time operations.

### v0.4.2: Floating Point Support 🔢

**Goal:** Add decimal number support.

- [ ] **Float Type (`عشري`)**:

  ```baa
  عشري باي = ٣.١٤١٥٩.
  عشري نصف = ٠.٥.
  ```

- [ ] **Float Operations** – Arithmetic, comparison, math functions.
- [ ] **Type Conversion** – `صحيح_إلى_عشري()`, `عشري_إلى_صحيح()`.

### v0.4.3: Error Handling 🛡️

**Goal:** Graceful error management.

- [ ] **Assertions**:

  ```baa
  تأكد(س > ٠, "س يجب أن يكون موجباً").
  ```

- [ ] **Error Codes** – Standardized error return values.
- [ ] **Panic Function** – `توقف_فوري("رسالة خطأ")`.

### v0.4.4: Final Polish 🎨

- [ ] **Complete Documentation** — All features documented.
- [ ] **Tutorial Series** — Step-by-step learning materials.
- [ ] **Example Programs** — Comprehensive example collection.
- [ ] **Performance Optimization** — Profile and optimize compiler.

---

## 🚀 Phase 5: Self-Hosting (v1.0.0)

*Goal: The ultimate proof of capability — Baa compiling itself.*

### v0.9.0: Bootstrap Preparation 🔧

#### v0.9.0.1: Freeze C Compiler

- [ ] **Tag final C version** — `git tag v0.9-bootstrap-c`
- [ ] **Document exact build steps** — GCC version, flags, environment.
- [ ] **Archive C compiler binary** — Store `baa.exe` built from C.
- [ ] **Create bootstrap documentation** — How to rebuild from scratch.

#### v0.9.0.2: Self-Hosting Requirements Check

- [ ] **Feature audit** — Verify all compiler-needed features exist.
- [ ] **Test complex programs** — Compile programs similar to compiler size.
- [ ] **Memory stress test** — Handle large source files.
- [ ] **Error recovery test** — Compiler handles malformed input gracefully.

### v0.9.1: Rewrite Lexer 📝

- [ ] **Port `lexer.c` → `lexer.baa`** — Token generation in Baa.
- [ ] **Compile with C-Baa** — Use C compiler to build.
- [ ] **Test lexer output** — Compare tokens with C version.
- [ ] **Fix any language gaps** — Add missing features discovered.

### v0.9.2: Rewrite Parser 🌳

- [ ] **Port `parser.c` → `parser.baa`** — AST construction in Baa.
- [ ] **Compile with C-Baa** — Build using C compiler.
- [ ] **Test AST output** — Compare trees with C version.
- [ ] **Handle recursion depth** — Ensure stack is sufficient.

### v0.9.3: Rewrite Semantic Analysis 🔍

- [ ] **Port `analysis.c` → `analysis.baa`** — Type checking in Baa.
- [ ] **Symbol table in Baa** — Rewrite symbol management.
- [ ] **Test type errors** — Verify same errors as C version.

### v0.9.4: Rewrite IR 🔄

- [ ] **Port `ir.c` → `ir.baa`** — IR generation in Baa.
- [ ] **Port `ir_lower.c` → `ir_lower.baa`** — Lowering in Baa.
- [ ] **Test IR output** — Compare with C version.

### v0.9.5: Rewrite Code Generator ⚙️

- [ ] **Port `codegen.c` → `codegen.baa`** — Assembly generation in Baa.
- [ ] **Handle all targets** — Windows x64, Linux x64.
- [ ] **Test generated assembly** — Compare with C version.

### v0.9.6: Rewrite Driver 🚗

- [ ] **Port `main.c` → `main.baa`** — CLI and orchestration in Baa.
- [ ] **Port `error.c` → `error.baa`** — Diagnostics in Baa.
- [ ] **Full compiler in Baa** — All components ported.

### v1.0.0: First Self-Compile 🏆

- [ ] **Compile Baa compiler with C-Baa** — Produces baa₁.
- [ ] **Test baa₁** — Run full test suite.
- [ ] **Compile Baa compiler with baa₁** — Produces baa₂.
- [ ] **Compile Baa compiler with baa₂** — Produces baa₃.
- [ ] **Verify baa₂ == baa₃** — Reproducible builds!
- [ ] **Release v1.0.0** — Historic milestone! 🎉

#### Bootstrap Verification Script

```bash
#!/bin/bash
# verify_bootstrap.sh

echo "Stage 0: Building with C compiler..."
./baa_c baa.baa -o baa1.exe

echo "Stage 1: Building with Baa (first generation)..."
./baa1.exe baa.baa -o baa2.exe

echo "Stage 2: Building with Baa (second generation)..."
./baa2.exe baa.baa -o baa3.exe

echo "Verifying reproducibility..."
if diff baa2.exe baa3.exe > /dev/null; then
    echo "✅ SUCCESS: baa2 and baa3 are identical!"
    echo "🎉 BAA IS SELF-HOSTING!"
else
    echo "❌ FAILURE: baa2 and baa3 differ!"
    exit 1
fi
```

---

## 🔨 Phase 6: Own Assembler (v1.5.0)

*Goal: Remove dependency on external assembler (GAS/MASM).*

### v1.5.0: Baa Assembler (مُجمِّع باء) 🔧

#### v1.5.0.1: Assembler Foundation

- [ ] **Define instruction encoding tables** — x86-64 opcode maps.
- [ ] **Parse assembly text** — Tokenize AT&T/Intel syntax.
- [ ] **Build instruction IR** — Internal representation of machine code.
- [ ] **Handle labels** — Track label addresses for jumps.

#### v1.5.0.2: x86-64 Encoding

- [ ] **REX prefixes** — 64-bit register encoding.
- [ ] **ModR/M and SIB bytes** — Addressing mode encoding.
- [ ] **Immediate encoding** — Handle different immediate sizes.
- [ ] **Displacement encoding** — Memory offset encoding.
- [ ] **Instruction validation** — Check valid operand combinations.

#### v1.5.0.3: Object File Generation

- [ ] **COFF format (Windows)** — Generate .obj files.
- [ ] **ELF format (Linux)** — Generate .o files.
- [ ] **Section handling** — .text, .data, .bss, .rodata.
- [ ] **Symbol table** — Export/import symbols.
- [ ] **Relocation entries** — Handle address fixups.

#### v1.5.0.4: Assembler Integration

- [ ] **Replace GAS calls** — Use internal assembler.
- [ ] **`--use-internal-asm` flag** — Optional internal assembler.
- [ ] **Verify output** — Compare with GAS output.
- [ ] **Performance test** — Ensure acceptable speed.

#### v1.5.0.5: Assembler Polish

- [ ] **Error messages** — Clear assembly error diagnostics.
- [ ] **Debug info** — Generate debug symbols.
- [ ] **Listing output** — Optional assembly listing with addresses.
- [ ] **Documentation** — Assembler internals guide.

---

## 🔗 Phase 7: Own Linker (v2.0.0)

*Goal: Remove dependency on external linker (ld/link.exe).*

### v2.0.0: Baa Linker (رابط باء) 🔗

#### v2.0.0.1: Linker Foundation

- [ ] **Parse object files** — Read COFF/ELF format.
- [ ] **Symbol resolution** — Match symbol references to definitions.
- [ ] **Section merging** — Combine sections from multiple objects.
- [ ] **Memory layout** — Assign virtual addresses to sections.

#### v2.0.0.2: Relocation Processing

- [ ] **Apply relocations** — Fix up addresses in code/data.
- [ ] **Handle relocation types** — PC-relative, absolute, GOT, PLT.
- [ ] **Overflow detection** — Check address range limits.

#### v2.0.0.3: Executable Generation (Windows)

- [ ] **PE header** — DOS stub, PE signature, file header.
- [ ] **Optional header** — Entry point, section alignment, subsystem.
- [ ] **Section headers** — .text, .data, .rdata, .bss.
- [ ] **Import table** — For C runtime and Windows API.
- [ ] **Export table** — If building DLLs (future).
- [ ] **Generate .exe** — Complete Windows executable.

#### v2.0.0.4: Executable Generation (Linux)

- [ ] **ELF header** — File identification, entry point.
- [ ] **Program headers** — Loadable segments.
- [ ] **Section headers** — .text, .data, .rodata, .bss.
- [ ] **Dynamic linking info** — For libc linkage.
- [ ] **Generate executable** — Complete Linux binary.

#### v2.0.0.5: Linker Features

- [ ] **Static libraries** — Link .a/.lib archives.
- [ ] **Library search paths** — `-L` flag support.
- [ ] **Entry point selection** — Custom entry point support.
- [ ] **Strip symbols** — Remove debug symbols for release.
- [ ] **Map file** — Generate link map for debugging.

#### v2.0.0.6: Linker Integration

- [ ] **Replace ld/link calls** — Use internal linker.
- [ ] **`--use-internal-linker` flag** — Optional internal linker.
- [ ] **Verify output** — Compare with system linker output.
- [ ] **End-to-end test** — Compile and link without external tools.

---

## 🏆 Phase 8: Full Independence (v3.0.0)

*Goal: Zero external dependencies — Baa builds itself with no external tools.*

### v3.0.0: Complete Toolchain 🛠️

#### v3.0.0.1: Remove C Runtime Dependency

**Windows:**

- [ ] **Direct Windows API calls** — Replace printf with WriteConsoleA.
- [ ] **Implement `اطبع` natively** — Direct syscall/API.
- [ ] **Implement `اقرأ` natively** — ReadConsoleA.
- [ ] **Implement memory functions** — HeapAlloc/HeapFree instead of malloc/free.
- [ ] **Implement file I/O** — CreateFile, ReadFile, WriteFile.
- [ ] **Custom entry point** — Replace C runtime startup.

**Linux:**

- [ ] **Direct syscalls** — write, read, mmap, exit.
- [ ] **Implement `اطبع` natively** — syscall to write(1, ...).
- [ ] **Implement `اقرأ` natively** — syscall to read(0, ...).
- [ ] **Implement memory functions** — mmap/munmap for allocation.
- [ ] **Implement file I/O** — open, read, write, close syscalls.
- [ ] **Custom _start** — No libc dependency.

#### v3.0.0.2: Native Standard Library

- [ ] **Rewrite string functions in Baa** — No C dependency.
- [ ] **Rewrite math functions in Baa** — Pure Baa implementation.
- [ ] **Rewrite memory functions in Baa** — Custom allocator.
- [ ] **Full standard library in Baa** — All library code in Baa.

#### v3.0.0.3: Self-Contained Build

- [ ] **Single binary compiler** — No external dependencies.
- [ ] **Cross-compilation support** — Build Linux binary on Windows and vice versa.
- [ ] **Reproducible builds** — Same source → identical binary.
- [ ] **Bootstrap from source** — Document minimal bootstrap path.

#### v3.0.0.4: Verification & Release

- [ ] **Full test suite passes** — All tests without external tools.
- [ ] **Benchmark comparison** — Performance vs GCC toolchain.
- [ ] **Security audit** — Review for vulnerabilities.
- [ ] **Documentation complete** — Full toolchain documentation.
- [ ] **Release v3.0.0** — Fully independent Baa! 🎉

### Toolchain Comparison

```
┌────────────────────────────────────────────────────────────────┐
│                    Baa Toolchain Evolution                     │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  v0.2.x (Current):                                             │
│  ┌─────────┐   ┌───────────────────────────────────────────┐  │
│  │   Baa   │ → │  GCC (assembler + linker + C runtime)     │  │
│  │ Compiler│   │                                           │  │
│  └─────────┘   └───────────────────────────────────────────┘  │
│                                                                │
│  v1.0.0 (Self-Hosting):                                        │
│  ┌─────────┐   ┌───────────────────────────────────────────┐  │
│  │   Baa   │ → │  GCC (assembler + linker + C runtime)     │  │
│  │ in Baa! │   │                                           │  │
│  └─────────┘   └───────────────────────────────────────────┘  │
│                                                                │
│  v1.5.0 (Own Assembler):                                       │
│  ┌─────────┐   ┌─────────┐   ┌─────────────────────────────┐  │
│  │   Baa   │ → │   Baa   │ → │  GCC (linker + C runtime)   │  │
│  │ Compiler│   │ Assembler│  │                             │  │
│  └─────────┘   └─────────┘   └─────────────────────────────┘  │
│                                                                │
│  v2.0.0 (Own Linker):                                          │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌───────────────┐  │
│  │   Baa   │ → │   Baa   │ → │   Baa   │ → │  C Runtime    │  │
│  │ Compiler│   │ Assembler│  │  Linker │   │  (printf etc) │  │
│  └─────────┘   └─────────┘   └─────────┘   └───────────────┘  │
│                                                                │
│  v3.0.0 (Full Independence):                                   │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │              Baa Toolchain (100% Baa)                   │  │
│  │  Compiler → Assembler → Linker → Native Runtime         │  │
│  │                                                         │  │
│  │                 No External Dependencies!               │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## 🏗️ Phase 2: Architecture Overhaul - Completed

<details>
<summary><strong>v0.2.0</strong> — The Driver (CLI & Build System)</summary>

- [x] **CLI Argument Parser** — Implement a custom argument parser to handle flags manually.
- [x] **Input/Output Control** (`-o`, `-S`, `-c`).
- [x] **Information Flags** (`--version`, `--help`, `-v`).
- [x] **Build Pipeline** — Orchestrate Lexer -> Parser -> Codegen -> GCC.

</details>

<details>
<summary><strong>v0.2.1</strong> — Polish & Branding</summary>

- [x] **Executable Icon** — Embed `.ico` resource.
- [x] **Metadata** — Version info, Copyright, Description in `.exe`.

</details>

<details>
<summary><strong>v0.2.2</strong> — The Diagnostic Engine Patch</summary>

- [x] **Source Tracking** — Update `Token` and `Node` to store Filename, Line, and Column.
- [x] **Error Module** — Create a dedicated error reporting system.
- [x] **Pretty Printing** — Display errors with context (`^` pointers).
- [x] **Panic Recovery** — Continue parsing after errors.

</details>

<details>
<summary><strong>v0.2.3</strong> Distribution & Updater Patch</summary>

- [x] **Windows Installer** — Create `setup.exe` using Inno Setup.
- [x] **PATH Integration** — Add compiler to system environment variables.
- [x] **Self-Updater** — Implement `baa update` command.

</details>

<details>
<summary><strong>v0.2.4</strong> The Semantic Pass (Type Checker)</summary>

- [x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
- [x] **Pass Separation** — Completely separate Parsing from Code Generation.
  - `parse()` returns a raw AST.
  - `analyze()` walks the AST to check types and resolve symbols.
  - `codegen()` takes a validated AST.
- [x] **Symbol Resolution** — Check for undefined variables before code generation starts.
- [x] **Scope Analysis** — Implement scope stack to properly handle nested blocks and variable shadowing.
- [x] **Type Checking** — Validate assignments (int = string now fails during semantic analysis).

</details>

<details>
<summary><strong>v0.2.5</strong> Multi-File & Include System</summary>

- [x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
- [x] **Include Directive** — `#تضمين "file.baahd"` (C-style `#include`).
- [x] **Header Files** — `.baahd` extension for declarations (function signatures, extern variables).
- [x] **Function Prototypes** — Declarations without types `صحيح دالة().` (Added).
- [x] **Multi-file CLI** — Accept multiple inputs: `baa main.baa lib.baa -o out.exe`.
- [x] **Linker Integration** — Compile each file to `.o` then link together.

</details>

<details>
<summary><strong>v0.2.6</strong> Preprocessor Directives</summary>

- [x] **Define** — `#تعريف اسم قيمة` for compile-time constants.
- [x] **Conditional** — `#إذا_عرف`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا`, `#وإلا_إذا`, `#نهاية_إذا` for conditional compilation.
- [x] **Undefine** — `#الغاء_تعريف` to remove definitions.

</details>

<details>
<summary><strong>v0.2.7</strong> Constants & Immutability</summary>

- [x] **Constant Keyword** — `ثابت` for immutable variables: `ثابت صحيح حد = ١٠٠.`
- [x] **Const Checking** — Semantic error on reassignment of constants.
- [x] **Array Constants** — Support constant arrays.

</details>

<details>
<summary><strong>v0.2.8</strong> Warnings & Diagnostics</summary>

- [x] **Warning System** — Separate warnings from errors (non-fatal).
- [x] **Unused Variables** — Warn if variable declared but never used.
- [x] **Dead Code** — Warn about code after `إرجع` or `توقف`.
- [x] **`-W` Flags** — `-Wall`, `-Werror` to control warning behavior.

</details>

<details>
<summary><strong>v0.2.9</strong> — Input & UX Polish</summary>

- [x] **Input Statement** — `اقرأ س.` (scanf) for reading user input.
- [x] **Boolean Type** — `منطقي` type with `صواب`/`خطأ` literals.
- [x] **Colored Output** — ANSI colors for errors (red), warnings (yellow). *(Implemented in v0.2.8)*
- [x] **Compile Timing** — Show compilation time with `-v`.

</details>

## 📦 Phase 1: Language Foundation (v0.1.x) - Completed

<details>
<summary><strong>v0.1.3</strong> — Control Flow & Optimizations</summary>

- [x] **Extended If** — Support `وإلا` (Else) and `وإلا إذا` (Else If) blocks.
- [x] **Switch Statement** — `اختر` (Switch), `حالة` (Case), `افتراضي` (Default)
- [x] **Constant Folding** — Compile-time math (`١ + ٢` → `٣`)

</details>

<details>
<summary><strong>v0.1.2</strong> — Recursion & Strings</summary>

- [x] **Recursion** — Stack alignment fix
- [x] **String Variables** — `نص` type
- [x] **Loop Control** — `توقف` (Break) & `استمر` (Continue)

</details>

<details>
<summary><strong>v0.1.1</strong> — Structured Data</summary>

- [x] **Arrays** — Fixed-size stack arrays (`صحيح قائمة[١٠]`)
- [x] **For Loop** — `لكل (..؛..؛..)` syntax
- [x] **Logic Operators** — `&&`, `||`, `!` with short-circuiting
- [x] **Postfix Operators** — `++`, `--`

</details>

<details>
<summary><strong>v0.1.0</strong> — Text & Unary</summary>

- [x] **Strings** — String literal support (`"..."`)
- [x] **Characters** — Character literals (`'...'`)
- [x] **Printing** — Updated `اطبع` to handle multiple types
- [x] **Negative Numbers** — Unary minus support

</details>

<details>
<summary><strong>v0.0.9</strong> — Advanced Math</summary>

- [x] **Math** — Multiplication, Division, Modulo
- [x] **Comparisons** — Greater/Less than logic (`<`, `>`, `<=`, `>=`)
- [x] **Parser** — Operator Precedence Climbing (PEMDAS)

</details>

<details>
<summary><strong>v0.0.8</strong> — Functions</summary>

- [x] **Functions** — Function definitions and calls
- [x] **Entry Point** — Mandatory `الرئيسية` exported as `main`
- [x] **Scoping** — Global vs Local variables
- [x] **Windows x64 ABI** — Register passing, stack alignment, shadow space

</details>

<details>
<summary><strong>v0.0.7</strong> — Loops</summary>

- [x] **While Loop** — `طالما` implementation
- [x] **Assignments** — Update existing variables

</details>

<details>
<summary><strong>v0.0.6</strong> — Control Flow</summary>

- [x] **If Statement** — `إذا` with blocks
- [x] **Comparisons** — `==`, `!=`
- [x] **Documentation** — Comprehensive Internals & API docs

</details>

<details>
<summary><strong>v0.0.5</strong> — Type System</summary>

- [x] Renamed `رقم` to `صحيح` (int)
- [x] Single line comments (`//`)

</details>

<details>
<summary><strong>v0.0.4</strong> — Variables</summary>

- [x] Variable declarations and stack offsets
- [x] Basic symbol table

</details>

<details>
<summary><strong>v0.0.3</strong> — I/O</summary>

- [x] `اطبع` (Print) via Windows `printf`
- [x] Multiple statements support

</details>

<details>
<summary><strong>v0.0.2</strong> — Math</summary>

- [x] Arabic numerals (٠-٩)
- [x] Addition and subtraction

</details>

<details>
<summary><strong>v0.0.1</strong> — Foundation</summary>

- [x] Basic pipeline: Lexer → Parser → Codegen → GCC

</details>

---

## 📊 Timeline Summary

| Phase | Version | Milestone | Dependencies |
|-------|---------|-----------|--------------|
| Phase 3 | v0.3.x | IR Complete | GCC |
| Phase 3.5 | v0.3.3-v0.3.12 | Language Complete | GCC |
| Phase 4 | v0.4.x | Standard Library | GCC |
| Phase 5 | v1.0.0 | **Self-Hosting** 🏆 | GCC |
| Phase 6 | v1.5.0 | Own Assembler | GCC (linker only) |
| Phase 7 | v2.0.0 | Own Linker | C Runtime only |
| Phase 8 | v3.0.0 | **Full Independence** 🏆 | **Nothing!** |

---

*For detailed changes, see the [Changelog](CHANGELOG.md)*
