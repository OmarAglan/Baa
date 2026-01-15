# Baa Roadmap

> Track the development progress of the Baa programming language.
> **Current Status:** Phase 3 - Intermediate Representation (v0.3.0)

---

## 📚 Documentation Track (Definitive Arabic Book)

*Goal: Produce a Kernighan & Ritchie–style first book for Baa, in Arabic, serving as the definitive learning + reference resource.*

- [ ] **Write the Arabic “Baa Book”** — book-length guide in Arabic with exercises.
- [ ] **Define terminology glossary** — consistent Arabic technical vocabulary.
- [ ] **Create example suite** — verified, idiomatic examples that compile with v0.2.9.
- [ ] **Add exercises and challenges** — per chapter, with expected outputs.
- [ ] **Add debugging and performance chapters** — common pitfalls, diagnostics, optimization notes.
- [ ] **Native technical review** — review by Arabic-speaking engineers before release.

---

## 🏗️ Phase 2: Architecture Overhaul (The Professional Arc)

*Goal: Transform Baa from a linear prototype into a modular, robust compiler toolchain.*

### v0.2.0: The Driver (CLI & Build System) 🖥️
- [x] **CLI Argument Parser** — Implement a custom argument parser to handle flags manually.
- [x] **Input/Output Control** (`-o`, `-S`, `-c`).
- [x] **Information Flags** (`--version`, `--help`, `-v`).
- [x] **Build Pipeline** — Orchestrate Lexer -> Parser -> Codegen -> GCC.

### v0.2.1: Polish & Branding 🎨
- [x] **Executable Icon** — Embed `.ico` resource.
- [x] **Metadata** — Version info, Copyright, Description in `.exe`.

### v0.2.2: The Diagnostic Engine 🚨
- [x] **Source Tracking** — Update `Token` and `Node` to store Filename, Line, and Column.
- [x] **Error Module** — Create a dedicated error reporting system.
- [x] **Pretty Printing** — Display errors with context (`^` pointers).
- [x] **Panic Recovery** — Continue parsing after errors.

### v0.2.3: Distribution & Updater 📦
- [x] **Windows Installer** — Create `setup.exe` using Inno Setup.
- [x] **PATH Integration** — Add compiler to system environment variables.
- [x] **Self-Updater** — Implement `baa update` command.

### v0.2.4: The Semantic Pass (Type Checker) 🧠
- [x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
- [x] **Pass Separation** — Completely separate Parsing from Code Generation.
    - `parse()` returns a raw AST.
    - `analyze()` walks the AST to check types and resolve symbols.
    - `codegen()` takes a validated AST.
- [x] **Symbol Resolution** — Check for undefined variables before code generation starts.
- [x] **Scope Analysis** — Implement scope stack to properly handle nested blocks and variable shadowing.
- [x] **Type Checking** — Validate assignments (int = string now fails during semantic analysis).

### v0.2.5: Multi-File & Include System 🔗
- [x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
- [x] **Include Directive** — `#تضمين "file.baahd"` (C-style `#include`).
- [x] **Header Files** — `.baahd` extension for declarations (function signatures, extern variables).
- [x] **Function Prototypes** — Declarations without types `صحيح دالة().` (Added).
- [x] **Multi-file CLI** — Accept multiple inputs: `baa main.baa lib.baa -o out.exe`.
- [x] **Linker Integration** — Compile each file to `.o` then link together.

### v0.2.6: Preprocessor Directives 📝
- [x] **Define** — `#تعريف اسم قيمة` for compile-time constants.
- [x] **Conditional** — `#إذا_عرف`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا`, `#وإلا_إذا`, `#نهاية_إذا` for conditional compilation.
- [x] **Undefine** — `#الغاء_تعريف` to remove definitions.

### v0.2.7: Constants & Immutability 🔒
- [x] **Constant Keyword** — `ثابت` for immutable variables: `ثابت صحيح حد = ١٠٠.`
- [x] **Const Checking** — Semantic error on reassignment of constants.
- [x] **Array Constants** — Support constant arrays.

### v0.2.8: Warnings & Diagnostics ⚠️
- [x] **Warning System** — Separate warnings from errors (non-fatal).
- [x] **Unused Variables** — Warn if variable declared but never used.
- [x] **Dead Code** — Warn about code after `إرجع` or `توقف`.
- [x] **`-W` Flags** — `-Wall`, `-Werror` to control warning behavior.

### v0.2.9: Input & UX Polish 🎨
- [x] **Input Statement** — `اقرأ س.` (scanf) for reading user input.
- [x] **Boolean Type** — `منطقي` type with `صواب`/`خطأ` literals.
- [x] **Colored Output** — ANSI colors for errors (red), warnings (yellow). *(Implemented in v0.2.8)*
- [x] **Compile Timing** — Show compilation time with `-v`.

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

#### v0.3.0.2: IR Builder Functions
- [ ] **`ir_create_func()`** — Create a new IR function.
- [ ] **`ir_create_block()`** — Create a new basic block with label.
- [ ] **`ir_append_inst()`** — Add instruction to block.
- [ ] **`ir_new_temp()`** — Allocate next virtual register `%م<n>`.
- [ ] **`ir_set_successor()`** — Link blocks for control flow.
- [ ] **Create `ir_builder.c`** — Implementation of builder functions.

#### v0.3.0.3: AST to IR Lowering (Expressions)
- [ ] **`lower_expr()`** — Main expression lowering dispatcher.
- [ ] **Lower `NODE_INT`** — Return immediate value.
- [ ] **Lower `NODE_VAR_REF`** — Generate `حمل` (load) instruction.
- [ ] **Lower `NODE_BIN_OP`** — Generate `جمع`/`طرح`/`ضرب`/`قسم` instructions.
- [ ] **Lower `NODE_UNARY_OP`** — Generate `سالب`/`نفي` instructions.
- [ ] **Lower `NODE_CALL_EXPR`** — Generate `نداء` (call) instruction.

#### v0.3.0.4: AST to IR Lowering (Statements)
- [ ] **`lower_stmt()`** — Main statement lowering dispatcher.
- [ ] **Lower `NODE_VAR_DECL`** — Generate `حجز` (alloca) + `خزن` (store).
- [ ] **Lower `NODE_ASSIGN`** — Generate `خزن` (store) instruction.
- [ ] **Lower `NODE_RETURN`** — Generate `رجوع` (return) instruction.
- [ ] **Lower `NODE_PRINT`** — Generate `نداء @اطبع()` call.
- [ ] **Lower `NODE_READ`** — Generate `نداء @اقرأ()` call.

#### v0.3.0.5: AST to IR Lowering (Control Flow)
- [ ] **Lower `NODE_IF`** — Create condition block + true/false blocks + merge block.
- [ ] **Lower `NODE_WHILE`** — Create header/body/exit blocks with back edge.
- [ ] **Lower `NODE_FOR`** — Create init/header/body/increment/exit blocks.
- [ ] **Lower `NODE_SWITCH`** — Create comparison chain + case blocks.
- [ ] **Lower `NODE_BREAK`** — Generate `قفز` to loop exit.
- [ ] **Lower `NODE_CONTINUE`** — Generate `قفز` to loop header/increment.

#### v0.3.0.6: IR Printer
- [ ] **`ir_print_func()`** — Print function header and all blocks.
- [ ] **`ir_print_block()`** — Print block label and all instructions.
- [ ] **`ir_print_inst()`** — Print single instruction with Arabic opcodes.
- [ ] **Arabic numeral output** — Print register numbers in Arabic (٠١٢٣٤٥٦٧٨٩).
- [ ] **`--dump-ir` CLI flag** — Add command-line option to print IR.

#### v0.3.0.7: Integration & Testing
- [ ] **Integrate IR into pipeline** — AST → IR (skip direct codegen).
- [ ] **Create `ir_test.baa`** — Simple test programs.
- [ ] **Verify IR output** — Check IR text matches specification.
- [ ] **Update `main.c`** — Add IR phase between analysis and codegen.

---

### v0.3.1: The Optimizer ⚡

#### v0.3.1.1: Analysis Infrastructure
- [ ] **CFG validation** — Verify all blocks have terminators.
- [ ] **Predecessor lists** — Build predecessor list for each block.
- [ ] **Dominator tree** — Compute dominance relationships.
- [ ] **Define `IRPass` interface** — Function pointer for optimization passes.

#### v0.3.1.2: Constant Folding (طي_الثوابت)
- [ ] **Detect constant operands** — Both operands are immediate values.
- [ ] **Fold arithmetic** — `جمع ص٦٤ ٥، ٣` → `٨`.
- [ ] **Fold comparisons** — `قارن أكبر ص٦٤ ١٠، ٥` → `صواب`.
- [ ] **Replace instruction** — Remove op, use constant result.

#### v0.3.1.3: Dead Code Elimination (حذف_الميت)
- [ ] **Mark used values** — Walk from terminators backward.
- [ ] **Identify dead instructions** — Result never used.
- [ ] **Remove dead instructions** — Delete from block.
- [ ] **Remove unreachable blocks** — No predecessors (except entry).

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
- [ ] **x86-64 target** — Current implementation as first target.
- [ ] **Target selection** — `--target=x86_64-windows` flag.

#### v0.3.2.8.2: Calling Convention Abstraction
- [ ] **Define `CallingConv` struct** — Arg registers, return register.
- [ ] **Windows x64 ABI** — Current convention as default.
- [ ] **SystemV AMD64 ABI** — Linux/macOS convention (future).

#### v0.3.2.8.3: Code Model Options
- [ ] **Small code model** — All code/data within 2GB (default).
- [ ] **PIC support** — Position independent code flag.
- [ ] **Stack protection** — Optional stack canaries.

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

## 📚 Phase 3.5: Language Completeness (v0.3.3 - v0.3.9)

*Goal: Add essential features to make Baa practical for real-world programs before Phase 4.*

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
// يحدد مجموعة من الألوان الممكنة
تعداد لون {
    أحمر،
    أزرق،
    أسود،
    أبيض
}
// ٢. تعريف الهيكل (Structure)
// يجمع بيانات السيارة
هيكل سيارة {
    نص موديل.
    صحيح سنة_الصنع.
    تعداد لون لون_السيارة.
}

صحيح الرئيسية() {
    // تعريف متغير من نوع الهيكل
    هيكل سيارة س.
    
    // ٣. استخدام النقطتين (:) للوصول لأعضاء الهيكل
    س:موديل = "تويوتا كورولا".
    س:سنة_الصنع = ٢٠٢٤.
    
    // ٤. استخدام النقطتين (:) للوصول لقيم التعداد
    س:لون_السيارة = لون:أحمر.
    
    // طباعة البيانات
    اطبع "بيانات السيارة الجديدة:".
    اطبع س:موديل.
    اطبع س:سنة_الصنع.
    
    // ٥. استخدام التعداد في الشروط
    إذا (س:لون_السيارة == لون:أحمر) {
        اطبع "تحذير: السيارات الحمراء سريعة!".
    } وإلا {
        اطبع "لون السيارة هادئ.".
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

### v0.3.5: Character Type 📝
**Goal:** Add proper character type to align with C conventions.

#### Features
- [ ] **Character Type (`حرف`)** – Proper 1-byte character type (like C's `char`).
- [ ] **String-Char Relationship** – Strings (`نص`) become arrays of characters (`حرف[]`).

**Syntax:**
```baa
// Character variable
حرف ح = 'أ'.
// String as char array (internal representation)
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

### v0.3.6: System Improvements 🔧
**Goal:** Refine and enhance existing compiler systems.

#### Focus Areas
- [ ] **Error Messages** – Improve clarity and helpfulness of diagnostic messages.
- [ ] **Code Quality** – Refactor complex functions, improve code organization.
- [ ] **Memory Management** – Fix memory leaks, improve buffer handling.
- [ ] **Performance** – Profile and optimize slow compilation paths.
- [ ] **Documentation** – Update all docs to reflect v0.3.3-0.3.5 changes.
- [ ] **Edge Cases** – Fix known bugs and handle corner cases.

#### Specific Improvements
- [ ] Improve panic mode recovery in parser.
- [ ] Better handling of UTF-8 edge cases in lexer.
- [ ] Optimize symbol table lookups (consider hash table).
- [ ] Add more comprehensive error recovery.
- [ ] Improve codegen output readability (comments in assembly).

---

### v0.3.7: Testing & Quality Assurance ✅
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
  - [ ] All language features (v0.0.1 - v0.3.6).
  - [ ] Edge cases and corner cases.
  - [ ] Error conditions (syntax errors, type mismatches, etc.).
  - [ ] Multi-file compilation scenarios.
  - [ ] Preprocessor directive combinations.

#### Bug Fixes & Refinements
- [ ] **Known Issues** – Fix all open bugs from previous versions.
- [ ] **Regression Testing** – Ensure new features don't break old code.
- [ ] **Stress Testing** – Test with large files, deep nesting, many symbols.
- [ ] **Arabic Text Edge Cases** – Test various Arabic Unicode scenarios.

#### Documentation
- [ ] **Testing Guide** – Document how to run tests and add new ones.
- [ ] **Known Limitations** – Document current language limitations.
- [ ] **Migration Guide** – Help users update code for v0.3.x changes.

---

### v0.3.8: Advanced Arrays & String Operations 📐
**Goal:** Complete array and string functionality.

#### Features
- [ ] **Multi-dimensional Arrays**:
  ```baa
  صحيح مصفوفة[٣][٤].
  مصفوفة[٠][٠] = ١٠.
  مصفوفة[١][٢] = ٢٠.
  ```
- [ ] **Array Length Operator**:
  ```baa
  صحيح قائمة[١٠].
  صحيح الطول = حجم(قائمة).  // Returns 10
  ```

- [ ] **Array Bounds Checking** (Optional debug mode):
  - Runtime checks with `-g` flag.
  - Panic on out-of-bounds access.

#### Implementation
- [ ] **Parser**: Parse multi-dimensional array declarations and access.
- [ ] **Semantic**: Track array dimensions in symbol table.
- [ ] **Codegen**: Calculate offsets for multi-dimensional arrays (row-major order).
- [ ] **Built-in**: Implement `حجم()` as compiler intrinsic or standard function.

---

### v0.3.9: String Operations Library 🔤
**Goal:** Make strings practical for real programs.

#### Features
- [ ] **String Length**:
  ```baa
  نص اسم = "أحمد".
  صحيح الطول = طول_نص(اسم).  // Returns 4
  ```

- [ ] **String Concatenation**:
  ```baa
  نص كامل = دمج_نص(اسم, " علي").  // "أحمد علي"
  ```

- [ ] **String Comparison**:
  ```baa
  صحيح نتيجة = قارن_نص(اسم, "محمد").  // 0 if equal, -1/<0/1 otherwise
  ```

- [ ] **String Indexing** (read-only):
  ```baa
  حرف أول = اسم[٠].  // Get character at index
  ```
- [ ] **String Copy**:
  ```baa
  نص نسخة = نسخ_نص(اسم).
  ```

#### Implementation
- [ ] **Standard Library**: Create `baalib.baa` with string functions.
- [ ] **C Integration**: Wrap C string functions (`strlen`, `strcmp`, `strcpy`, etc.).
- [ ] **UTF-8 Aware**: Ensure functions handle multi-byte Arabic characters correctly.
- [ ] **Memory Safety**: Document string memory management rules.

---

## 📚 Phase 4: Advanced Features & Standard Library (v0.4.x)

*Goal: Make Baa useful for real-world applications.*

### v0.4.0: Pointers & Memory Management 🎯
**Goal:** Add manual memory management capabilities.
- [ ] **Pointer Type**:
  ```baa
  صحيح* مؤشر.  // Pointer to integer
  ```
- [ ] **Address-of Operator** — `&` (or Arabic equivalent like `عنوان`).
- [ ] **Dereference Operator** — `*` (or Arabic equivalent like `قيمة`).
- [ ] **Dynamic Allocation**:
  ```baa
  صحيح* ذاكرة = حجز_ذاكرة(١٠ * حجم(صحيح)).  // malloc equivalent
  تحرير_ذاكرة(ذاكرة).  // free equivalent
  ```

- [ ] **Null Pointer** – `عدم` keyword for NULL.

### v0.4.1: Formatted Output & Input 🖨️
**Goal:** Professional I/O capabilities.

- [ ] **Formatted Output**:
  ```baa
  // Printf-style
  اطبع_منسق("الاسم: %s، العمر: %d", اسم, عمر).
  
  // Or interpolation
  اطبع("الاسم: {اسم}، العمر: {عمر}").
  ```
- [ ] **User Input**:
  ```baa
  نص إدخال = اقرأ_سطر().
  صحيح رقم = اقرأ_رقم().
  ```

### v0.4.2: File I/O 📁
**Goal:** Read and write files.

- [ ] **File Operations**:
  ```baa
  صحيح ملف = فتح_ملف("data.txt", "قراءة").
  نص سطر = اقرأ_سطر_من_ملف(ملف).
  اكتب_إلى_ملف(ملف, "نص جديد").
  اغلق_ملف(ملف).
  ```

### v0.4.3: Standard Library (BaaLib) 📚
- [ ] **IO Module** — File reading/writing (`ملف.اقرأ`, `ملف.اكتب`).
- [ ] **Math Module** – Advanced math functions:
  ```baa
  صحيح جذر = جذر_تربيعي(١٦).
  صحيح قوة = أس(٢, ١٠).
  ```
- [ ] **System Module** — Executing commands, environment variables.
- [ ] **Time Module** – Date/time operations.

### v0.4.4: Floating Point Support 🔢
**Goal:** Add decimal number support.

- [ ] **Float Type (`عشري`)**:
  ```baa
  عشري باي = ٣.١٤١٥٩.
  عشري نصف = ٠.٥.
  ```

- [ ] **Float Operations** – Arithmetic, comparison, math functions.
- [ ] **Type Conversion** – `صحيح إلى عشري()`, `عشري إلى صحيح()`.

### v0.4.5: Error Handling 🛡️
**Goal:** Graceful error management.

- [ ] **Assertions**:
  ```baa
  تأكد(س > ٠, "س يجب أن يكون موجباً").
  ```

- [ ] **Error Returns** – Convention for returning error codes.
- [ ] **Panic/Abort** – `توقف_فوري("رسالة خطأ")`.
 
---

## 🚀 Phase 5: Self-Hosting (v1.0.0)

*Goal: The ultimate proof of capability — Baa compiling itself.*

- [ ] **Rewrite Compiler** — Port `src/*.c` to `src/*.b`.
- [ ] **Bootstrap** — Use the C compiler (v0.4) to compile the Baa compiler (v1.0).
- [ ] **Optimization** — Ensure the Baa-written compiler is as fast as the C one.

---

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

*For detailed changes, see the [Changelog](CHANGELOG.md)*
