# Baa Roadmap (Updated)

> Track the development progress of the Baa programming language.
> **Current Status:** Phase 4.5 - Bootstrap Readiness (v0.5.x) ← IN PROGRESS

---

## 📚 Documentation Track (Definitive Arabic Book)

*Goal: Produce a Kernighan & Ritchie–style first book for Baa, in Arabic, serving as the definitive learning + reference resource.*

- [~] **Write the Arabic "Baa Book"** — book-length guide in Arabic with exercises. — draft exists in `docs/BAA_BOOK_AR.md`
- [ ] **Define terminology glossary** — consistent Arabic technical vocabulary.
- [ ] **Create example suite** — verified, idiomatic examples that compile with v0.3.7.
- [ ] **Add exercises and challenges** — per chapter, with expected outputs.
- [ ] **Add debugging and performance chapters** — common pitfalls, diagnostics, optimization notes.
- [ ] **Native technical review** — review by Arabic-speaking engineers before release.

---

## ⚙️ Phase 3: The Intermediate Representation (v0.3.x)

*Goal: Decouple the language from x86 Assembly to enable optimizations and multiple backends.*

> **Design Document:** See [BAA_IR_SPECIFICATION.md](docs/BAA_IR_SPECIFICATION.md) for full IR specification.

### v0.3.0: IR Foundation 🏗️

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

#### v0.3.1.4: Copy Propagation (نشر_النسخ) ✅ COMPLETED (2026-01-27)

- [x] **Detect copy instructions** — `IR_OP_COPY` (`نسخ`) instruction pattern.
- [x] **Replace uses** — Substitute original for copy in operands / call args / phi entries.
- [x] **Remove redundant copies** — Delete `نسخ` instruction after propagation.

#### v0.3.1.5: Common Subexpression Elimination (حذف_المكرر) ✅ COMPLETED (2026-02-07)

- [x] **Hash expressions** — Create signature for each operation.
- [x] **Detect duplicates** — Same op + same operands.
- [x] **Replace with existing result** — Reuse previous computation.

#### v0.3.1.6: Optimization Pipeline ✅ COMPLETED (2026-02-07)

- [x] **Pass ordering** — Define optimal pass sequence (constfold → copyprop → CSE → DCE).
- [x] **Iteration** — Run passes until no changes (fixpoint, max 10 iterations).
- [x] **`-O0`, `-O1`, `-O2` flags** — Control optimization level.
- [x] **`--dump-ir-opt`** — Print IR after optimization.

---

### v0.3.2: The Backend (Target Independence) 🎯

#### v0.3.2.1: Instruction Selection ✅ COMPLETED (2026-02-07)

- [x] **Define `MachineInst`** — Abstract machine instruction.
- [x] **IR to Machine mapping** — `جمع` → `ADD`, `حمل` → `MOV`, etc.
- [x] **Pattern matching** — Select optimal instruction sequences.
- [x] **Handle immediates** — Inline constants where possible.

#### v0.3.2.2: Register Allocation ✅ COMPLETED (2026-02-07)

- [x] **Liveness analysis** — Compute live ranges for each virtual register.
- [x] **Linear scan allocator** — Simple, fast allocation algorithm.
- [x] **Spilling** — Handle register pressure by spilling to stack.
- [x] **Map to x64 registers** — RAX, RBX, RCX, RDX, R8-R15.

#### v0.3.2.3: Code Emission ✅ COMPLETED (2026-02-08)

- [x] **Emit function prologue** — Stack setup, callee-saved registers.
- [x] **Emit instructions** — Generate AT&T syntax assembly.
- [x] **Emit function epilogue** — Stack teardown, return.
- [x] **Emit data section** — Global variables and string literals.

#### v0.3.2.4: Backend Integration

- [x] **Replace old codegen** — IR → Backend → Assembly.
- [x] **Verify output** — Compare with old codegen results.
- [x] **Performance testing** — Ensure no regression.
- [x] **Remove legacy codegen** — Retire the legacy AST backend from the build.

#### 0.3.2.4-IR-FIX: ISel Bug Fixes & Backend Testing ✅ COMPLETED (2026-02-08)

- [x] **Fix ISel logical op size mismatch** — `isel_lower_logical()` forced 64-bit operand size; widened 8-bit boolean vregs to prevent assembler errors.
- [x] **Fix function parameter ABI copies** — `isel_lower_func()` prepends MOV from RCX/RDX/R8/R9 to parameter vregs at entry block.
- [x] **Fix IDIV RAX constraint** — `isel_lower_div()` explicitly routes dividend through RAX (vreg -2) for correct division results.
- [x] **Comprehensive backend test** — `tests/integration/backend/backend_test.baa`: 27 functions, 63 assertions, all PASS.

#### 0.3.2.4-setup: Installer & GCC Bundling ✅ COMPLETED (2026-02-08)

- [x] **Bundle MinGW-w64 GCC** — Ship GCC toolchain in `gcc/` subfolder inside the installer.
- [x] **Auto-detect bundled GCC** — `resolve_gcc_path()` in `main.c` finds `gcc.exe` relative to `baa.exe`.
- [x] **Update installer (setup.iss)** — Add `gcc\*` files, dual PATH entries, post-install GCC verification.
- [x] **GCC bundle script** — `scripts/prepare_gcc_bundle.ps1` downloads and prepares the minimal toolchain.
- [x] **Sync version metadata** — `baa.rc` and `setup.iss` updated to `0.3.2.4`, publisher to "Omar Aglan".

---

### v0.3.2.5: SSA Construction 🔄

**Strategy (Canonical SSA / الطريقة القياسية لـ SSA):**
- **Mem2Reg (ترقية الذاكرة إلى سجلات)** بأسلوب Cytron/LLVM القياسي:
  - حساب **المسيطرات (Dominators)** و **حدود السيطرة (Dominance Frontiers)**
  - إدراج عقد **فاي (Phi)** عند نقاط الدمج (join points)
  - **إعادة التسمية (SSA Renaming)** لبناء تعريفات واصلة (reaching definitions)
- هذا الأسلوب هو الأساس طويل المدى لتحسينات متقدمة لاحقاً مثل: GVN/CSE و LICM و PRE وغيرها.

#### v0.3.2.5.1: Memory to Register Promotion ✅ COMPLETED (2026-02-09)

- [x] **Identify promotable allocas** — Single-block allocas with no escaping (correctness-first baseline; design stays compatible with full Mem2Reg).
- [x] **Replace loads/stores** — Convert to direct register use.
- [x] **Remove dead allocas** — Delete promoted `حجز` instructions.

#### v0.3.2.5.2: Phi Node Insertion ✅ COMPLETED (2026-02-09)

- [x] **Compute dominance frontiers** — Where Phi nodes are needed.
- [x] **Insert Phi placeholders** — Add `فاي` at join points.
- [x] **Rename variables** — SSA renaming pass with reaching definitions.
- [x] **Connect Phi operands** — Link values from predecessor blocks.

#### v0.3.2.5.3: SSA Validation ✅ COMPLETED (2026-02-09)

- [x] **Verify SSA properties** — Each register defined exactly once.
- [x] **Check dominance** — Definition dominates all uses.
- [x] **Validate Phi nodes** — One operand per predecessor.
- [x] **`--verify-ssa` flag** — Debug option to run SSA checks.

---

### v0.3.2.6: IR Stabilization & Polish 🧹

#### v0.3.2.6.0: Codebase Soldering (مرحلة_تلحيم_القاعدة) ✅ COMPLETED (2026-02-09)

- [x] **Enable compiler warnings (two-tier)** — Default warnings on; optional `-Werror` hardening toggle.
- [x] **Fix unsafe string building in driver** — Replace `sprintf/strcpy/strcat` command construction with bounded appends and overflow checks.
- [x] **Fix symbol-table name overflow** — Guard `Symbol.name[32]` writes; reject/diagnose identifiers that exceed the limit.
- [x] **Harden updater version parsing** — Support multi-part versions like `0.3.2.5.3`; replace `sprintf` with `snprintf`; check parse results.
- [x] **Audit `strncpy` usage** — Ensure explicit NUL-termination and bounds safety in lexer and helpers.
- [x] **Replace `atoi` with checked parsing** — Use `strtoll` + validation for integer literals and array sizes; produce safe diagnostics.
- [x] **Warning clean build** — Zero warnings under default warning set; `-Werror` build passes.

#### v0.3.2.6.1: IR Memory Management

✅ COMPLETED (2026-02-11)

- [x] **Arena allocator for IR** — Fast allocation, bulk deallocation.
- [x] **IR cloning** — Deep copy of functions/blocks.
- [x] **IR destruction** — Clean up all IR memory.
- [x] **Def-use chains for SSA regs** — Build and maintain use lists to make IR passes fast and safe (avoid whole-function rescans).
- [x] **Instruction numbering / stable IDs** — Deterministic per-function instruction IDs for analyses, debugging, and regression tests.
- [x] **IR mutation helpers** — Central utilities to insert/remove instructions and update CFG metadata (pred/succ/dominance caches) consistently.

#### v0.3.2.6.2: Debug Information

✅ COMPLETED (2026-02-11)
 
- [x] **Source location tracking** — Map IR instructions to source lines.
- [x] **Variable name preservation** — Keep original names for debugging.
- [x] **`--debug-info` flag** — Emit debug metadata in assembly.

#### v0.3.2.6.3: IR Serialization

✅ COMPLETED (2026-02-11)

- [x] **Text IR writer** — Output canonical IR text format.
- [x] **Text IR reader** — Parse IR text back to data structures.
- [x] **Round-trip testing** — Write → Read → Compare.

#### v0.3.2.6.4: Register Allocator Liveness Fix (إصلاح حيوية مخصص السجلات)

✅ COMPLETED (2026-02-12)
 
- [x] **Fix liveness across loop back-edges** — Linear scan liveness analysis does not correctly propagate live ranges across loop back-edges when many variables are simultaneously live, causing register clobbering and segfaults.
- [x] **Extend live intervals to loop ends** — Ensure variables used inside loops have their intervals extended to cover the entire loop body including back-edges.
- [x] **Add block-level scoping in semantic analyzer** — Currently function-level only; for-loop variables cannot be redeclared in the same function, requiring unique names.
- [x] **Stress test with high register pressure** — Validate fix with functions containing 8+ live variables across multiple nested loops.

#### v0.3.2.6.5: IR Verification & Canonicalization (تحقق_الـIR_وتوحيده)

✅ COMPLETED (2026-02-13)
 
- [x] **IR well-formedness verifier (`--verify-ir`)** — Validate operand counts, type consistency, terminator rules, phi placement, and call signatures (separate from `--verify-ssa`).
- [x] **Verifier gate in optimizer (debug)** — Optional mode to run `--verify-ir`/`--verify-ssa` after each pass iteration to catch pass bugs early.
- [x] **Canonicalization pass** — Normalize commutative operands, constant placement, and comparison canonical forms to make CSE/DCE/constfold more effective.
- [x] **CFG simplification pass** — Merge trivial blocks, remove redundant branches, and provide a reusable critical-edge splitting utility for IR passes.

#### v0.3.2.6.6: IR Semantics & Data Layout (دلالات_الـIR_وتخطيط_البيانات)

- [x] **Define IR arithmetic semantics** — Document and enforce overflow behavior (recommended: two’s-complement wrap), and clarify `i1` truthiness and `div/mod` rules for negatives.
- [x] **Data layout helpers** — Add size/alignment queries per `IRType` (incl. pointer size) as the foundation for future `Target` abstraction and correct aggregate lowering.
- [x] **Memory model contract** — Specify and verify rules for `حجز/حمل/خزن` (typed pointers, aliasing assumptions, and what is/ isn’t legal for optimization).

#### v0.3.2.6.7: SSA Verification Fix in Switch (إصلاح التحقق من SSA في جملة اختر)

✅ COMPLETED (2026-02-14)

- [x] **Fix SSA verification failure** — Resolved dominance issue in CSE pass for `switch` statements with `default` cases.

---

### v0.3.2.7: Advanced Optimizations 🚀

#### v0.3.2.7.1: Loop Optimizations

✅ COMPLETED (2026-02-16)

- [x] **Loop detection** — Identify natural loops via back edges.
- [x] **Loop invariant code motion** — Hoist constant computations.
- [x] **Strength reduction** — Replace expensive ops (mul → shift).
- [x] **Loop unrolling** — Optional with `-funroll-loops`.

#### v0.3.2.7.2: Inlining

✅ COMPLETED (2026-02-16)

- [x] **Inline heuristics** — Small functions, single call site.
- [x] **Inline expansion** — Copy function body to call site.
- [x] **Post-inline cleanup** — Re-run optimization passes.

#### v0.3.2.7.3: Tail Call Optimization

✅ COMPLETED (2026-02-17)

- [x] **Detect tail calls** — `call` immediately followed by `ret` (بدون تعليمات بينهما).
- [x] **Convert to jump** — Replace call+ret with a tail jump (no new return address).
- [x] **Stack reuse** — Reuse caller's stack frame + caller shadow space (Windows x64 ABI).
- [x] **Limitation (for v0.3.2.7.3)** — initial implementation supports only <= 4 arguments (register args); stack-args tail calls are scheduled in v0.3.2.8.5.

---

### v0.3.2.8: Multi-Target Preparation 🌐

#### v0.3.2.8.1: Target Abstraction

✅ COMPLETED (2026-02-17)

- [x] **Define `Target` interface** — OS/object-format, data layout, calling convention, asm directives.
- [x] **x86-64 Windows target** — Keep current behavior as the first concrete target.
- [x] **Host default target** — Windows host defaults to `x86_64-windows`, Linux host defaults to `x86_64-linux`.
- [x] **Target selection** — `--target=x86_64-windows|x86_64-linux` flag.

#### v0.3.2.8.2: Calling Convention Abstraction

✅ COMPLETED (2026-02-17)

- [x] **Define `CallingConv` struct** — arg regs order/count, return regs, caller/callee-saved masks.
- [x] **Stack rules** — stack alignment + varargs rules are modeled; stack-arg placement is deferred (backend rejects stack args for now).
- [x] **Windows x64 ABI** — RCX/RDX/R8/R9 + 32-byte shadow/home space.
- [x] **SystemV AMD64 ABI** — RDI/RSI/RDX/RCX/R8/R9, no shadow space, sets `AL=0` for calls (conservative).

#### v0.3.2.8.3: Code Model Options

✅ COMPLETED (2026-02-17)

- [x] **Small code model** — Default (only supported model in v0.3.2.8.3).
- [x] **PIC/PIE flags** — `-fPIC`/`-fPIE` (Linux/ELF; initial support).
- [x] **Stack protection** — stack canaries on ELF via `-fstack-protector*`.

#### v0.3.2.8.4: Linux x86-64 Target 🐧

✅ COMPLETED (2026-02-17)

- [x] **Native Linux build of compiler** — build `baa` on Linux with GCC/Clang + CMake.
- [x] **SystemV AMD64 ABI implementation** — different calling convention from Windows.
- [x] **ELF output support** — `.rodata`/`.data`/`.text` directives compatible with ELF GAS.
- [x] **Link with host gcc (for now)** — produce ELF executables via host toolchain; later reduce/remove GCC dependency.
- [x] **Cross-compilation (later)** — optional `--target=x86_64-linux` from Windows once a cross toolchain story exists.

#### v0.3.2.8.5: Windows x64 Stack Args + Full Tail Calls

✅ COMPLETED (2026-02-17)

- [x] **Proper stack-arg calling** — outgoing call frames support stack args on Windows x64 and SysV AMD64.
- [x] **Tail calls beyond reg-args** — enable TCO with stack args conservatively (requires enough incoming stack-arg area).
- [x] **Varargs home space (Windows)** — home/register args are written into shadow space before calls.

#### v0.3.2.8.6: Aggressive IR & Optimizations (GCC/MSVC-like)

✅ COMPLETED (2026-02-17)

*Goal: move toward compiler-grade IR optimizations in pragmatic steps.*

- [x] **InstCombine** — local simplification patterns (canonical `COPY` rewrites).
- [x] **SCCP** — sparse conditional constant propagation + `br_cond` folding.
- [x] **GVN** — dominator-scoped global value numbering for pure expressions.
- [x] **Mem2Reg promotability unlock** — must-def initialization analysis (instead of “init store must be in alloca block”).
- [x] **Pipeline wiring** — run InstCombine+SCCP early; GVN at `-O2` before CSE.

---

### v0.3.2.9: IR Verification & Benchmarking ✅

#### v0.3.2.9.1: Comprehensive IR Verification ✅ COMPLETED (2026-02-17)

- [x] **Well-formedness checks** — All functions have entry blocks.
- [x] **Type consistency** — Operand types match instruction requirements.
- [x] **CFG integrity** — All branches point to valid blocks.
- [x] **SSA verification** — Run `--verify-ssa` on all test programs.
- [x] **`baa --verify` mode** — Run all verification passes.

#### v0.3.2.9.2: Performance Benchmarking ✅ COMPLETED (2026-02-17)

- [x] **Compile-time benchmark** — Compare compiler-only (`-S`) and end-to-end compile wall time.
- [x] **Runtime benchmark** — Run deterministic `bench/runtime_*.baa` programs.
- [x] **Memory usage profiling** — Track peak RSS on Linux via `/usr/bin/time -v` and IR arena stats via `--time-phases`.
- [x] **Benchmark suite** — Collection of representative programs (`bench/*.baa`).

#### v0.3.2.9.3: Regression Testing ✅ COMPLETED (2026-02-17)

- [x] **Output comparison** — Compare IR-based output against a reference corpus.
- [x] **Test all v0.2.x programs** — Ensure backward compatibility.
- [x] **Edge case testing** — Complex control flow, nested loops, recursion.
- [x] **Error case testing** — Verify error messages unchanged.

#### v0.3.2.9.4: Documentation & Cleanup

- [x] **Update INTERNALS.md** — Document new IR pipeline.
- [x] **IR Developer Guide** — How to add new IR instructions.
- [x] **Driver split** — Extract CLI parsing, toolchain execution, and per-file compile pipeline into dedicated `src/driver_*.c/.h` modules.
- [x] **Driver link safety** — Remove fixed-size `argv_link` construction; build link argv dynamically based on object count.
- [x] **Shared file reader** — Provide `read_file()` as a shared module (`src/read_file.c`) for the lexer include system.
- [x] **Line ending normalization** — Add `.gitattributes` to keep the repo LF-normalized and reduce diff churn.
- [x] **Remove deprecated code** — Remove legacy AST-based codegen paths and backend-compare mode.
- [x] **Code review checklist** — Ensure code quality standards.

---

## 📚 Phase 3.5: Language Completeness (v0.3.3 - v0.3.12)

*Goal: Add essential features to make Baa practical for real-world programs and ready for self-hosting.*

### v0.3.3: Array Initialization 📊 ✅ COMPLETED (2026-02-18)

**Goal:** Enable direct initialization of arrays with values.

#### Features

- [x] **Array Literal Syntax** – Initialize arrays with comma-separated values using `{` `}` (supports partial init + zero-fill like C).
  
**Syntax:**

```baa
صحيح قائمة[٥] = {١، ٢، ٣، ٤، ٥}.

// With Arabic comma (،) or regular comma (,)
صحيح أرقام[٣] = {١٠، ٢٠، ٣٠}.
```

#### Implementation Tasks

- [x] **Parser**: Handle `{` `}` initializer list after array declaration.
- [x] **Parser**: Support both Arabic comma `،` (U+060C) and regular comma `,` as separators.
- [x] **Semantic Analysis**: Allow partial init (`count <= size`) and zero-fill the remainder; reject overflow.
- [x] **Codegen**: Emit `.data` initializers for globals and runtime stores for locals (including zero-fill).

#### Deferred to v0.3.9

- [x] Multi-dimensional arrays: `صحيح مصفوفة[٣][٤].` ✅ COMPLETED (2026-02-25)
- [x] Array length operator: `صحيح طول = حجم(قائمة) / حجم(صحيح).` ✅ COMPLETED (2026-02-25)

---

### v0.3.4: Enumerations & Structures 🏗️ ✅ COMPLETED (2026-02-18)

**Goal:** Add compound types for better code organization and type safety.

#### Features

- [x] **Enum Declaration** – Named integer constants with type safety.
- [x] **Struct Declaration** – Group related data into composite types (supports nested structs + enum fields).
- [x] **Member Access** – Use `:` (colon) operator for accessing members.

### v0.3.4.5: Union Types (الاتحادات) 🔀 ✅ COMPLETED (2026-02-19)
**Goal:** Memory-efficient variant types for parsers and data structures.

#### Features
- [x] **Union Declaration**:
  ```baa
  اتحاد قيمة {
      صحيح رقم.
      نص نص_قيمة.
      منطقي منطق.
  }
  ```

- [x] **Union Usage**:
  ```baa
  اتحاد قيمة ق.
  ق:رقم = ٤٢.        // All members share same memory
  ق:نص_قيمة = "مرحبا". // Overwrites previous value
  ```

- [x] **Tagged Union Pattern** (manual):
  ```baa
  تعداد نوع_قيمة { رقم، نص_ق }
   
  هيكل قيمة_موسومة {
      تعداد نوع_قيمة نوع.
      اتحاد قيمة بيانات.
  }
  ```

#### Implementation Tasks
- [x] **Token**: Add `TOKEN_UNION` for `اتحاد` keyword.
- [x] **Parser**: Parse union declaration similar to struct.
- [x] **Semantic**: All members start at offset 0.
- [x] **Memory Layout**: Size = max member size, align = max member align.
- [x] **Codegen**: Generate union storage + member access code.

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

- [x] **Token**: Add `TOKEN_ENUM` for `تعداد` keyword.
- [x] **Parser**: Parse enum declaration: `تعداد <name> { <members> }`.
- [x] **Parser**: Support Arabic comma `،` between enum members.
- [x] **Semantic**: Auto-assign integer values (0, 1, 2...).
- [x] **Semantic**: Enum values accessible via `<enum_name>:<value_name>`.
- [x] **Type System**: Add `TYPE_ENUM` to `DataType`.

**Structures:**

- [x] **Token**: Add `TOKEN_STRUCT` for `هيكل` keyword.
- [x] **Token**: Add `TOKEN_COLON` for `:` (already exists, verify usage).
- [x] **Parser**: Parse struct declaration: `هيكل <name> { <fields> }`.
- [x] **Parser**: Parse struct instantiation: `هيكل <name> <var>.`
- [x] **Parser**: Parse member access: `<var>:<member>`.
- [x] **Semantic**: Track struct definitions in symbol table.
- [x] **Semantic**: Validate member access against struct definition.
- [x] **Memory Layout**: Calculate field offsets with padding/alignment (supports nested structs).
- [x] **Codegen**: Emit struct storage + member access code.

---

### v0.3.5: Character Type 📝 (يشمل تقديم `عشري`)

**Goal:** Add a proper character type with UTF-8 source support.

ملاحظة: نوع `عشري` قُدِّم في v0.3.5 كثوابت/تخزين، واكتمل في v0.3.5.5 (عمليات + ABI).

#### Features

- [x] **Character Type (`حرف`)** – UTF-8 character value (variable-length, 1..4 bytes) stored as a packed scalar.
- [x] **String-Char Relationship** – Strings (`نص`) are represented as arrays of `حرف` (`حرف[]`) with indexing (`اسم[٠]`).

- [x] **Float Type (`عشري`)** – *Deferred from v0.3.4.5* (introduced as a basic type + literals in v0.3.5; completed in v0.3.5.5 with ops/ABI).

**Syntax:**

```baa
حرف ح = 'أ'.
نص اسم = "أحمد".  // Equivalent to: حرف اسم[] = {'أ', 'ح', 'م', 'د', '\0'}.
```

#### Implementation Tasks

- [x] **Token**: Already have `TOKEN_CHAR` for literals.
- [x] **Token**: Add `TOKEN_KEYWORD_CHAR` for `حرف` type keyword.
- [x] **Type System**: Add `TYPE_CHAR` to `DataType` enum.
- [x] **Semantic**: Distinguish between `char` and `int`.
- [x] **Codegen**: Store `حرف` as packed `i64` (bytes + length) and support UTF-8 printing.
- [x] **String Representation**: Update internal string handling to use `حرف[]`.

#### Deferred to v0.3.9

- String operations: `طول_نص()`, `دمج_نص()`, `قارن_نص()`

### v0.3.5.5: Numeric Types (Sized Integers + `عشري`) ✅ COMPLETED (2026-02-24)

**Goal:** Make numeric types practical for systems programming: sized integers + usable `عشري` (f64).

#### Features

- [x] **Sized Integer Types**:
  ```baa
  ص٨ بايت_موقع = -١٢٨.        // int8_t:  -128 to 127
  ص١٦ قصير = -٣٢٠٠٠.          // int16_t: -32768 to 32767
  ص٣٢ عادي = -٢٠٠٠٠٠٠٠٠٠.     // int32_t
  ص٦٤ طويل = ٩٠٠٠٠٠٠٠٠٠٠٠٠.   // int64_t (current صحيح)

  ط٨ بايت = ٢٥٥.               // uint8_t
  ط١٦ قصير٢ = ٦٥٠٠٠.            // uint16_t
  ط٣٢ عادي٢ = ٤٠٠٠٠٠٠٠٠٠.       // uint32_t
  ط٦٤ طويل٢ = ١٨٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠. // uint64_t
  ```

- [x] **C-like semantics**:
  - integer promotions + usual arithmetic conversions
  - correct signed/unsigned comparisons
  - correct `div/mod` semantics for signed vs unsigned

- [x] **`عشري` usability**:
  - arithmetic `+ - * /`
  - comparisons `== != < > <= >=`
  - `اطبع` supports `عشري`
  - ABI lowering on SysV AMD64 + Windows x64 (XMM regs + SysV varargs rules)

#### Implementation Tasks

- [x] **Lexer**: Tokenize `ص٨`, `ص١٦`, `ص٣٢`, `ص٦٤`, `ط٨`, `ط١٦`, `ط٣٢`, `ط٦٤`.
- [x] **Type System**: Add size and signedness to integer types.
- [x] **IR**: Add unsigned variants and allow `f64` ops + verifier updates.
- [x] **Optimizer**: Ensure integer/float correctness (avoid invalid float folds; unsigned-aware folds).
- [x] **ISel/Emitter**: Generate correct-sized integer ops and scalar SSE2 for `f64`.
- [x] **ABI (Windows + SystemV)**: Pass/return `عشري` in XMM registers; handle SysV varargs rules.
- [x] **Semantic**: Warn on implicit narrowing conversions.
- [x] **Semantic**: Handle signed/unsigned comparison warnings.

---

### v0.3.6: Low-Level Operations 🔧 ✅ COMPLETED (2026-02-24)

**Goal:** Add bitwise operations and low-level features needed for systems programming.

#### Features

- [x] **Bitwise Operators**:

  ```baa
  صحيح أ = ٥ & ٣.      // AND: 5 & 3 = 1
  صحيح ب = ٥ | ٣.      // OR:  5 | 3 = 7
  صحيح ج = ٥ ^ ٣.      // XOR: 5 ^ 3 = 6
  صحيح د = ~٥.         // NOT: ~5 = -6
  صحيح هـ = ١ << ٤.    // Left shift:  1 << 4 = 16
  صحيح و = ١٦ >> ٢.    // Right shift: 16 >> 2 = 4
  ```

- [x] **Sizeof Operator**:

  ```baa
  صحيح حجم_صحيح = حجم(صحيح).    // Returns 8
  صحيح حجم_حرف = حجم(حرف).      // Returns 1
  صحيح حجم_مصفوفة = حجم(قائمة). // Returns array size in bytes
  ```

- [x] **Void Type**:

  ```baa
  عدم اطبع_رسالة() {
      اطبع "مرحباً".
      // No return needed
  }
  ```

- [x] **Escape Sequences**:

  ```baa
  نص سطر = "سطر١\س سطر٢".     // Newline (/س)
  نص جدول = "عمود١\ت عمود٢".  // Tab (/ت)
  نص مسار = "C:\\ملفات".     // Backslash
  حرف صفر = '\٠'.            // Null character
  ```

#### Implementation Tasks

- [x] **Lexer**: Tokenize `&`, `|`, `^`, `~`, `<<`, `>>`.
- [x] **Parser**: Add bitwise operators with correct precedence.
- [x] **Parser**: Parse `حجم(type)` and `حجم(expr)` expressions.
- [x] **Lexer**: Add `عدم` keyword for void type.
- [x] **Lexer**: Handle escape sequences in string/char literals.
- [x] **Semantic**: Type check bitwise operations (integers only).
- [x] **Codegen**: Generate bitwise assembly instructions.
- [x] **Codegen**: Calculate sizes for `حجم` operator.

### v0.3.6.5: Type Aliases (أسماء الأنواع البديلة) 🏷️ ✅ COMPLETED (2026-02-23)
**Goal:** Create custom type names for readability and abstraction.

#### Features
- [x] **Simple Type Alias**:
  ```baa
  نوع معرف = ط٦٤.
  نوع نتيجة = ص٣٢.
  
  معرف رقم_المستخدم = ١٢٣٤٥.
  نتيجة كود_خطأ = -١.
  ```

- [ ] **Pointer Type Alias**:
  ```baa
  نوع نص_ثابت = ثابت حرف*.
  نوع مؤشر_بايت = ط٨*.
  ```
  *(Deferred — requires pointer type grammar from v0.3.10.)*

#### Implementation Tasks
- [x] **Token/AST plumbing**: Added `TOKEN_TYPE_ALIAS`/`NODE_TYPE_ALIAS` support in shared structures.
- [x] **Parser**: Parse `نوع <name> = <type>.` at top-level with C-like declare-before-use semantics.
- [x] **Semantic**: Resolve aliases during type checking and validate alias targets (`enum/struct/union`).
- [x] **Symbol Table**: Store type aliases separately and enforce strict name-collision diagnostics.

---

### v0.3.7: System Improvements 🔧 ✅ COMPLETED (2026-02-25)

**Goal:** Refine and enhance existing compiler systems.

#### Focus Areas

- [x] **Error Messages** – Improve clarity and helpfulness of diagnostic messages.
- [x] **Code Quality** – Refactor complex functions, improve code organization.
- [x] **Memory Management** – Fix memory leaks, improve buffer handling.
- [x] **Performance** – Profile and optimize slow compilation paths.
- [x] **Documentation** – Update all docs to reflect v0.3.3-0.3.7 changes.
- [x] **Edge Cases** – Fix known bugs and handle corner cases.

#### Specific Improvements

- [x] Improve panic mode recovery in parser.
- [x] Better handling of UTF-8 edge cases in lexer.
- [x] Optimize symbol table lookups (consider hash table).
- [x] Add more comprehensive error recovery.
- [x] Improve assembly output readability (comments in assembly).

### v0.3.7.5: Static Local Variables (متغيرات ساكنة محلية) ✅ COMPLETED (2026-02-25)
**Goal:** Variables that persist between function calls.

#### Features
- [x] **Static Local Syntax**:
  ```baa
  صحيح عداد() {
      ساكن صحيح ع = ٠.  // Initialized once, persists
      ع = ع + ١.
      إرجع ع.
  }
  
  // First call returns 1, second returns 2, etc.
  ```

#### Implementation Tasks
- [x] **Token**: Add `TOKEN_STATIC` for `ساكن` keyword.
- [x] **Semantic**: Static locals go in .data section, not stack.
- [x] **Codegen**: Generate unique global label for static locals.
- [x] **Codegen**: Initialize in .data section.

---

### v0.3.8: Testing & Quality Assurance ✅ COMPLETED (2026-02-25)

**Goal:** Establish robust testing infrastructure and fix accumulated issues.

#### Test System

- [x] **Test Framework** – Create automated test runner.
  - Script to compile and run `.baa` test files.
  - Compare actual output vs expected output.
  - Report pass/fail with clear diagnostics.

- [x] **Test Categories**:
  - [x] **Lexer Tests** – Token generation, UTF-8 handling, preprocessor.
  - [x] **Parser Tests** – Syntax validation, error recovery.
  - [x] **Semantic Tests** – Type checking, scope validation.
  - [x] **Codegen Tests** – Correct assembly output, execution results.
  - [x] **Integration Tests** – Full programs with expected output.

- [x] **Test Coverage**:
  - [x] All language features (v0.0.1 - v0.3.7).
  - [x] Edge cases and corner cases.
  - [x] Error conditions (syntax errors, type mismatches, etc.).
  - [x] Multi-file compilation scenarios.
  - [x] Preprocessor directive combinations.

#### CI/CD Setup

- [x] **GitHub Actions workflow**:

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

- [x] **Known Issues** – Fix all open bugs from previous versions.
- [x] **Regression Testing** – Ensure new features don't break old code.
- [x] **Stress Testing** – Test with large files, deep nesting, many symbols.
- [x] **Arabic Text Edge Cases** – Test various Arabic Unicode scenarios.

---

### v0.3.9: Advanced Arrays & String Operations 📐 ✅ COMPLETED (2026-02-25)

**Goal:** Complete array and string functionality.

#### Array Features

- [x] **Multi-dimensional Arrays** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح مصفوفة[٣][٤].
  مصفوفة[٠][٠] = ١٠.
  مصفوفة[١][٢] = ٢٠.
  ```

- [x] **Array Length Operator** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح قائمة[١٠].
  صحيح الطول = حجم(قائمة) / حجم(صحيح).  // Returns 10
  ```

- [x] **Array Bounds Checking** (Optional debug mode) ✅ COMPLETED (2026-02-25):
  - Runtime checks in debug-oriented lowering mode (`--debug-info` path).
  - Abort path on out-of-bounds access.

#### String Operations

- [x] **String Length** ✅ COMPLETED (2026-02-25): `صحيح الطول = طول_نص(اسم).`
- [x] **String Concatenation** ✅ COMPLETED (2026-02-25): `نص كامل = دمج_نص(اسم, " علي").`
- [x] **String Comparison** ✅ COMPLETED (2026-02-25): `صحيح نتيجة = قارن_نص(اسم, "محمد").`
- [x] **String Indexing** (read-only) ✅ COMPLETED (2026-02-25): `حرف أول = اسم[٠].`
- [x] **String Copy** ✅ COMPLETED (2026-02-25): `نص نسخة = نسخ_نص(اسم).`

#### Implementation

- [x] **Parser**: Parse multi-dimensional array declarations and access.
- [x] **Semantic**: Track array dimensions in symbol table.
- [x] **Codegen**: Calculate offsets for multi-dimensional arrays (row-major order).
- [x] **Standard Library**: Create `baalib.baa` with string functions.
- [x] **UTF-8 Aware**: Ensure functions handle multi-byte Arabic characters correctly.

---

### v0.3.10: Pointers & References 🎯

**Goal:** Add pointer types for manual memory management and data structures.

#### Features

- [x] **Pointer Type Declaration** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح* مؤشر.           // Pointer to integer
  حرف* نص_مؤشر.         // Pointer to character (C-string)
  هيكل سيارة* س_مؤشر.   // Pointer to struct
  ```

- [x] **Address-of Operator** (`&`) ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح س = ١٠.
  صحيح* م = &س.         // م points to س
  ```

- [x] **Dereference Operator** (`*`) ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح قيمة = *م.       // قيمة = 10
  *م = ٢٠.              // س now equals 20
  ```

- [x] **Null Pointer** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح* م = عدم.        // Null pointer
  إذا (م == عدم) {
      اطبع "مؤشر فارغ".
  }
  ```

- [x] **Pointer Arithmetic** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح قائمة[٥] = {١، ٢، ٣، ٤، ٥}.
  صحيح* م = &قائمة[٠].
  م = م + ١.             // Points to قائمة[١]
  اطبع *م.               // Prints 2
  ```

#### Implementation Tasks

- [x] **Lexer/Parser integration**: Handle `*` في سياق النوع/فك الإشارة والضرب و`&` كـ bitwise/address-of.
- [x] **Parser**: Parse pointer declarations + `عدم` كمؤشر فارغ + جملة `*ptr = value.`.
- [x] **Type System**: Add `TYPE_POINTER` with base type tracking in AST/symbol metadata.
- [x] **Semantic**: Validate pointer operations (dereference/address-of/null/pointer compare/arithmetic).
- [x] **Codegen**: Lower address-of/dereference/pointer assignment and pointer arithmetic safely.

### v0.3.10.5: Type Casting (تحويل الأنواع) 🔄 ✅ COMPLETED (2026-02-27)
**Goal:** Explicit type conversions for low-level programming.

#### Features
- [x] **Cast Syntax**:
  ```baa
  صحيح س = ٦٥.
  حرف ح = كـ<حرف>(س).
  ```

- [x] **Numeric Casts**:
  ```baa
  ص٣٢ صغير = كـ<ص٣٢>(قيمة_كبيرة).  // Truncation
  ص٦٤ كبير = كـ<ص٦٤>(قيمة_صغيرة).  // Sign extension
  ط٦٤ بدون = كـ<ط٦٤>(موقع).        // Signed to unsigned
  ```

- [x] **Pointer Casts**:
  ```baa
  ط٨* بايتات = كـ<ط٨*>(مؤشر_هيكل).  // Reinterpret
  عدم* عام = كـ<عدم*>(أي_مؤشر).     // To void pointer
  هيكل س* محدد = كـ<هيكل س*>(عام). // From void pointer
  ```

- [x] **Pointer Difference (`pointer - pointer`)**:
  ```baa
  صحيح* أ = &قائمة[٠].
  صحيح* ب = أ + ٣.
  صحيح فرق = ب - أ. // = 3 (فرق عناصر)
  ```

#### Implementation Tasks
- [x] **Lexer**: Tokenize `كـ` keyword and `<>` for type parameter.
- [x] **Parser**: Parse `كـ<type>(expr)` form.
- [x] **Semantic**: Validate cast safety, warn on dangerous casts.
- [x] **Codegen**: Generate appropriate conversion instructions.

### v0.3.10.6: Function Pointers (مؤشرات الدوال) 📍
**Goal:** First-class function references for callbacks and dispatch tables.

#### Features
- [x] **Function Pointer Type**:
  ```baa
  // Pointer to function taking two صحيح, returning صحيح
  نوع دالة_ثنائية = دالة(صحيح، صحيح) -> صحيح.
  
  // Or inline
  دالة(صحيح، صحيح) -> صحيح مؤشر_دالة.
  ```

- [x] **Assign Function to Pointer**:
  ```baa
  صحيح جمع(صحيح أ، صحيح ب) { إرجع أ + ب. }
  صحيح ضرب(صحيح أ، صحيح ب) { إرجع أ * ب. }
  
  دالة_ثنائية عملية = جمع.   // Points to جمع
  عملية = ضرب.               // Now points to ضرب
  ```

- [x] **Call Through Pointer**:
  ```baa
  صحيح نتيجة = عملية(١٠، ٢٠).  // Calls ضرب(10, 20) = 200
  ```

- [x] **Function Pointer as Parameter**:
  ```baa
  صحيح طبق(صحيح[] قائمة، صحيح حجم، دالة_ثنائية د) {
      صحيح نتيجة = قائمة[٠].
      لكل (صحيح ع = ١؛ ع < حجم؛ ع++) {
          نتيجة = د(نتيجة، قائمة[ع]).
      }
      إرجع نتيجة.
  }
  
  // Usage
  صحيح مجموع = طبق(أرقام، ١٠، جمع).
  ```

- [x] **Null Function Pointer**:
  ```baa
  دالة_ثنائية فارغ = عدم.
  إذا (فارغ != عدم) {
      فارغ(١، ٢).
  }
  ```

#### Implementation Tasks
- [x] **Parser**: Parse function type syntax `دالة(...) -> نوع`.
- [x] **Type System**: Add function pointer type with signature plumbing.
- [x] **Semantic**: Type-check function pointer assignments.
- [x] **Semantic**: Validate call through pointer matches signature.
- [x] **Codegen**: Generate indirect call instructions.
- [x] **IR**: Add function pointer type to IR.

✅ COMPLETED (2026-02-28)

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

- [x] **Runtime**: ربط مباشر مع libc (`malloc/free/realloc/memcpy/memset`).
- [x] **Built-in Functions**: إضافة `حجز_ذاكرة`, `تحرير_ذاكرة`, `إعادة_حجز`, `نسخ_ذاكرة`, `تعيين_ذاكرة`.
- [x] **Semantic**: دعم `عدم*` (void*) كـ مؤشّر عام (تحويلات ضمنية مع مؤشرات الكائنات).
- [x] **Codegen**: خفض الدوال إلى استدعاءات C القياسية مع قواعد shadowing.

✅ COMPLETED (2026-03-01)

---

### v0.3.12: File I/O 📁

**Goal:** Enable reading and writing files for compiler self-hosting.

#### Features

- [x] **File Opening**:

  ```baa
  عدم* ملف = فتح_ملف("بيانات.txt", "قراءة").
  عدم* ملف_كتابة = فتح_ملف("ناتج.txt", "كتابة").
  عدم* ملف_إضافة = فتح_ملف("سجل.txt", "إضافة").
  ```

- [x] **File Reading**:

  ```baa
  حرف حرف_واحد = اقرأ_حرف(ملف).
  نص سطر = اقرأ_سطر(ملف).
  صحيح بايتات = اقرأ_ملف(ملف, مخزن, حجم).
  ```

- [x] **File Writing**:

  ```baa
  اكتب_حرف(ملف, 'أ').
  اكتب_سطر(ملف, "مرحباً").
  اكتب_ملف(ملف, بيانات, حجم).
  ```

- [x] **File Closing**:

  ```baa
  اغلق_ملف(ملف).
  ```

- [x] **File Status**:

  ```baa
  منطقي انتهى = نهاية_ملف(ملف).
  صحيح موقع = موقع_ملف(ملف).
  اذهب_لموقع(ملف, ٠).
  ```

#### Implementation Tasks

- [x] **Runtime**: Wrap C stdio functions (fopen, fread, fwrite, fclose, fgetc, fputc, fputs, feof, ftello/fseeko).
- [x] **Built-in Functions**: Add file operation functions using `عدم*` as an opaque `FILE*` handle.
- [x] **Error Handling**: Return error codes for failed operations (`فتح_ملف` يعيد `عدم`، و `اكتب_سطر/اكتب_حرف` تعيد -1 عند الفشل).
- [x] **Codegen**: Generate direct libc stdio calls with shadowing rules.

✅ COMPLETED (2026-03-02)

### v0.3.12.5: Command Line Arguments (معاملات سطر الأوامر) 🖥️
**Goal:** Access program arguments - essential for compiler self-hosting.

#### Features
- [x] **Main with Arguments**:
  ```baa
  صحيح الرئيسية(صحيح عدد، نص[] معاملات) {
      // عدد = argument count (like argc)
      // معاملات = argument array (like argv)
      
      إذا (عدد < ٢) {
          اطبع "الاستخدام: برنامج <ملف>".
          إرجع ١.
      }
      
      نص اسم_البرنامج = معاملات[٠].
      نص ملف_إدخال = معاملات[١].
      
      اطبع "تجميع: ".
      اطبع ملف_إدخال.
      
      إرجع ٠.
  }
  ```

#### Implementation Tasks
- [x] **Parser**: Allow parameters in `الرئيسية` function.
- [x] **Semantic**: Validate main signature matches expected pattern.
- [x] **Codegen**: Link with proper C runtime entry point.
- [x] **Codegen (Opt-in)**: `--startup=custom` — custom entrypoint symbol (`__baa_start`) while keeping CRT/libc init.
- [ ] **Codegen (Full Independence)**: True `_start` without CRT/libc (deferred to Phase 8).

✅ COMPLETED (2026-03-02)

---

## 📚 Phase 4: Standard Library & Polish (v0.4.x)

*Goal: Make Baa production-ready with a comprehensive standard library.*

### v0.4.0: Formatted Output & Input 🖨️

**Goal:** Professional I/O capabilities.

- [x] **Formatted Output**:

  ```baa
  اطبع_منسق("الاسم: %ن، العمر: %ص\س", اسم, عمر).
  ```

- [x] **String Formatting**:

  ```baa
  نص رسالة = نسق("النتيجة: %ص", قيمة).
  حرر_نص(رسالة).
  ```

- [x] **Formatted Input**:

  ```baa
  نص سطر = اقرأ_سطر().
  صحيح رقم = اقرأ_رقم().

  صحيح أ = ٠.
  عشري ب = ٠.
  نص س = عدم.

  // ملاحظة: في الإدخال، %ن يتطلب عرضاً رقمياً (مثلاً %10ن).
  صحيح مقروء = اقرأ_منسق("%ص %ع %10ن", &أ, &ب, &س).
  إذا (مقروء == 3) { حرر_نص(س). }
  ```

✅ COMPLETED (2026-03-02)

### v0.4.0.5: Variadic Functions (دوال متغيرة المعاملات) ✅
**Goal:** Functions accepting variable number of arguments.

#### Features
- [x] **Variadic Declaration**:
  ```baa
  عدم اطبع_منسق(نص تنسيق، ...) {
      // Implementation using variadic access
  }
  ```

- [x] **Variadic Access Macros/Functions**:
  ```baa
  عدم اطبع_أرقام(صحيح عدد، ...) {
      قائمة_معاملات معاملات.
      بدء_معاملات(معاملات، عدد).
      
      لكل (صحيح ع = ٠؛ ع < عدد؛ ع++) {
          صحيح قيمة = معامل_تالي(معاملات، صحيح).
          اطبع قيمة.
      }
      
      نهاية_معاملات(معاملات).
  }
  
  // Usage
  اطبع_أرقام(٣، ١٠، ٢٠، ٣٠).
  ```

#### Implementation Tasks
- [x] **Lexer**: Tokenize `...` (ellipsis).
- [x] **Parser**: Parse variadic function declarations (including `دالة(...)->...` signatures).
- [x] **Type System**: Handle variadic function types and variadic call arity/type checks.
- [x] **Codegen (Windows x64)**: Variadic calls lowered عبر وسيط داخلي موحد (`__baa_va_base`) متوافق مع مسار الباك-إند الحالي.
- [x] **Codegen (Linux x64)**: نفس وسيط المعاملات الداخلي لضمان سلوك موحد على `x86_64-linux`.
- [x] **Built-ins**: Implement `بدء_معاملات`, `معامل_تالي`, `نهاية_معاملات`.

✅ COMPLETED (2026-03-02)

### v0.4.0.6: Inline Assembly (المجمع المدمج) ✅
**Goal:** Embed assembly code for low-level operations.

#### Features
- [x] **Basic Inline Assembly**:
  ```baa
  مجمع {
      "nop"
  }
  ```

- [x] **With Outputs and Inputs**:
  ```baa
  صحيح قراءة_عداد() {
      ط٣٢ منخفض.
      ط٣٢ مرتفع.
      مجمع {
          "rdtsc"
          : "=a" (منخفض)، "=d" (مرتفع)
      }
      إرجع (كـ<ص٦٤>(مرتفع) << ٣٢) | كـ<ص٦٤>(منخفض).
  }
  ```

#### Implementation Tasks
- [x] **Token**: Add `TOKEN_ASM` for `مجمع` keyword.
- [x] **Parser**: Parse inline assembly blocks.
- [x] **Codegen**: Emit assembly directly with proper constraints.
- [x] **Semantic**: Validate constraint syntax.

✅ COMPLETED (2026-03-02)

#### Deferred to v3.0
- Full constraint support (memory, register classes)
- Clobber lists
  ```

### v0.4.1: Standard Library (مكتبة باء) 📚

- [x] **Math Module** — `جذر_تربيعي()`, `أس()`, `مطلق()`, `عشوائي()`.
- [x] **String Module** — Complete string manipulation.
- [x] **IO Module** — File and console operations.
- [x] **System Module** — Environment variables, command execution.
- [x] **Time Module** — Date/time operations.

✅ COMPLETED (2026-03-02)

### v0.4.2: Floating Point Extensions ✅

**Goal:** Extend floating point beyond the core `عشري` (completed in v0.3.5.5).

- [x] **Math functions** — `جذر_تربيعي()`, `أس()`, `جيب()`, `جيب_تمام()`, `ظل()`.
- [x] **Formatting** — better float printing options (precision + scientific `%أ`).
- [x] **Additional float types** — `عشري٣٢` keyword (current lowering alias to `عشري`/f64 in v0.4.2).

✅ COMPLETED (2026-03-02)

### v0.4.3: Error Handling 🛡️

**Goal:** Graceful error management.

- [x] **Assertions**:

  ```baa
  تأكد(س > ٠, "س يجب أن يكون موجباً").
  ```

- [x] **Error Codes** – Standardized error return values.
- [x] **Panic Function** – `توقف_فوري("رسالة خطأ")`.

✅ COMPLETED (2026-03-02)

### v0.4.4: Final Polish 🎨

- [x] **Complete Documentation** — All features documented.
- [x] **Tutorial Series** — Step-by-step learning materials.
- [x] **Example Programs** — Comprehensive example collection.
- [x] **Performance Optimization** — Profile and optimize compiler.

✅ COMPLETED (2026-03-02)

## 🧱 Phase 4.5: Bootstrap Readiness (v0.5.x)

*Goal: Prepare the codebase, toolchain, and language contracts for a low-risk self-hosting transition.*

### v0.5.0: File & Component Organization 🗂️

- [x] **Define canonical component boundaries** — Frontend / Middle-End / Backend / Driver / Support.
- [x] **Set module-size policy** — target `<= 700` lines/file, hard cap `1000` lines for hand-written C modules.
- [x] **Split oversized modules first** — `analysis.c`, `emit.c`, `ir.c`, `ir_lower.c`, `ir_text.c`, `isel.c`, `lexer.c`, `parser.c`, `regalloc.c`, and `ir_verify_ir.c` are now under the hard cap via companion implementation splits.
- [x] **Restructure source layout safely** — component directories now exist under `src/`, and the build now targets those files directly.
- [x] **Add local module facades** — `frontend_internal.h`, `middleend_internal.h`, `backend_internal.h`, `driver_internal.h`, and `support_internal.h` now define component-local include surfaces.
- [x] **Update build graph** — `CMakeLists.txt` remains explicit/deterministic and the Windows build uses C-only include propagation while header wrappers remain transitional.
- [x] **Add size-regression guard** — `scripts/check_module_sizes.py` enforces warn `700` / error `1000` and runs in `qa_run.py` + CI before full QA.
- [x] **Document ownership map** — `docs/COMPONENT_OWNERSHIP.md` defines module responsibilities + dependency rules.

✅ COMPLETED (2026-03-06)

Partial status update (2026-03-06):
- policy/guard/documentation are in place for `v0.5.0`,
- all remaining hard-cap hotspots were split below the `1000`-line limit,
- `scripts/module_size_allowlist.txt` is now empty,
- source files were reorganized under `src/frontend`, `src/middleend`, `src/backend`, `src/driver`, and `src/support`,
- `CMakeLists.txt` now builds directly from component sources, while root-level compatibility is limited to selected header wrappers during header migration.

### v0.5.1: Language + ABI Freeze 🔒

- [ ] **Freeze grammar surface** — avoid syntax churn before bootstrap.
- [ ] **Freeze stdlib signatures** — lock callable contracts used by compiler-in-Baa.
- [ ] **Freeze target ABI contracts** — Windows x64 + SystemV AMD64 invariants.
- [ ] **Freeze IR invariants** — verifier-enforced guarantees documented.

### v0.5.2: Module & Multi-File Hardening 📦

- [ ] **Deterministic include/import resolution** — stable behavior across hosts.
- [ ] **Cycle diagnostics** — clear Arabic diagnostics for dependency loops.
- [ ] **Symbol visibility rules** — explicit cross-file linkage behavior.
- [~] **Header/API hygiene** — root `.c` shims are gone, `driver*.h`, `process.h`, `target.h`, `code_model.h`, `emit.h`, `isel.h`, `regalloc.h`, and all `ir*.h` now live under component directories, and temporary root header wrappers were removed. Remaining work is the larger public-surface cleanup around `baa.h`.

### v0.5.3: Build System Maturity ⚙️

- [ ] **Incremental compilation model** — avoid full rebuilds on small edits.
- [ ] **Dependency tracking** — reliable invalidation for headers/includes.
- [ ] **Reproducible outputs** — stable artifacts for same inputs/toolchain.
- [ ] **Build profile presets** — dev/debug/release/verify presets.

### v0.5.4: Diagnostics & Recovery Quality 🩺

- [ ] **Improve span accuracy** — tighter line/column ranges.
- [ ] **Add actionable fix hints** — Arabic-first suggestions for common errors.
- [ ] **Strengthen panic recovery** — fewer cascading diagnostics.
- [ ] **Negative test expansion** — enforce diagnostic contracts.

### v0.5.5: Runtime Safety Layer 🛡️

- [ ] **Assertion runtime contract** — stable behavior in debug/release modes.
- [ ] **Panic/error-code policy** — consistent fatal/non-fatal paths.
- [ ] **Safety toggles** — explicit compile-time/runtime control flags.
- [ ] **Document failure semantics** — deterministic exit/status behavior.

### v0.5.6: Determinism & QA Gates ✅

- [ ] **Cross-target parity suite** — Linux/Windows behavior consistency.
- [ ] **Fuzz + stress expansion** — parser/semantic/IR robustness.
- [ ] **IR/SSA regression locking** — snapshots + verifier gating.
- [ ] **Release gate checklist** — mandatory pass criteria before Phase 5.

### v0.5.7: Bootstrap Subset (Baa0) Definition 📐

- [ ] **Define minimal subset** — features required to implement compiler slices in Baa.
- [ ] **Ban unstable features in Baa0** — keep bootstrap surface conservative.
- [ ] **Publish Baa0 compliance suite** — tests for subset guarantees.
- [ ] **Document migration plan** — C modules to Baa equivalents by priority.

### v0.5.8: Self-Hosting Pilot (Last Step Before Phase 5) 🧪

- [ ] **Rewrite one compiler slice in Baa** — start with `lexer` (or smallest high-value slice).
- [ ] **Mixed pipeline build** — C host compiler + Baa pilot module in one build.
- [ ] **Behavioral equivalence checks** — output parity vs C implementation.
- [ ] **Go/No-Go report for Phase 5** — readiness decision based on objective gates.

#### Phase 4.5 Exit Criteria (Mandatory Before Phase 5)

- [ ] **Cross-target QA green** — `quick/full` pass on both `x86_64-windows` and `x86_64-linux`.
- [ ] **Determinism checks green** — stable IR text and stable diagnostics for identical inputs.
- [x] **File-size governance active** — CI guard for module-size budget is enforced.
- [ ] **Contracts frozen and published** — grammar/ABI/IR/Baa0 docs tagged and versioned.
- [ ] **Bootstrap handoff bundle ready** — scripts + manifests + reproducible build notes archived.

#### Phase 4.5 Required Artifacts

- [x] `docs/COMPONENT_OWNERSHIP.md` — boundaries + owners + allowed dependencies.
- [ ] `docs/BOOTSTRAP_CONTRACT.md` — frozen ABI/IR/language requirements for v0.9.
- [ ] `docs/BAA0_SPEC.md` — bootstrap subset definition and exclusions.
- [ ] `scripts/qa_bootstrap_gate.py` — one-command admission checks for Phase 5.
- [ ] `tests/bootstrap/` — parity corpus dedicated to migration from C → Baa slices.

---

## 🚀 Phase 5: Self-Hosting (v1.0.0)

*Goal: The ultimate proof of capability — Baa compiling itself.*

### Phase 5 Operating Rules (No Scope Drift)

- [ ] **Feature freeze during migration** — no new language features while porting compiler slices.
- [ ] **Parity-first policy** — semantic/diagnostic/IR behavior must match C baseline unless explicitly approved.
- [ ] **Target parity policy** — every migration milestone must pass on Windows + Linux.
- [ ] **Rollback-ready checkpoints** — each slice port keeps a reversible gate in CI.

### v0.9.0: Bootstrap Execution Baseline 🔧

#### v0.9.0.1: Consume Phase 4.5 Artifacts

- [ ] **Use frozen contracts from v0.5** — language/ABI/IR/Baa0 are inputs, not redefined in v0.9.
- [ ] **Pin Stage-0 toolchain manifest** — inherit deterministic build inputs from v0.5 gates.
- [ ] **Create bootstrap snapshot tag** — lock the handoff commit before Baa rewrites.

#### v0.9.0.2: Mixed C+Baa Harness

- [ ] **Enable mixed-unit builds** — compile/link `.c` and `.baa` compiler units together.
- [ ] **Define bridge boundaries** — stable ABI/data-layout interfaces between C and Baa modules.
- [ ] **Add parity harness** — module-level equivalence checks (tokens/AST/IR/asm as applicable).
- [ ] **Golden corpus harness** — fixed corpus with expected outputs for every migration step.
- [ ] **Diff tooling** — normalized comparator for diagnostics and IR text.

### v0.9.1: Rewrite Lexer 📝

- [ ] **Port lexer slice to Baa** — use split layout from v0.5 component organization.
- [ ] **Build through mixed harness** — keep remaining compiler units in C.
- [ ] **Token-stream parity tests** — compare lexer output against C baseline.
- [ ] **No feature expansion in v0.9** — only correctness/portability fixes permitted.
- [ ] **UTF-8 and preprocessor parity** — nested includes/macros/conditionals must match C behavior.
- [ ] **Stress corpus parity** — large and malformed sources produce equivalent diagnostics.

### v0.9.2: Rewrite Parser 🌳

- [ ] **Port parser slice to Baa** — AST construction with existing grammar contracts.
- [ ] **Build through mixed harness** — C/Baa hybrid remains supported.
- [ ] **AST + diagnostics parity tests** — match C baseline trees and key error anchors.
- [ ] **Recursion/stack validation** — ensure depth safety on both targets.
- [ ] **Panic recovery parity** — same synchronization behavior for statement/declaration/switch modes.
- [ ] **Alias/type grammar parity** — no regressions in parser-side alias resolution.

### v0.9.3: Rewrite Semantic Analysis 🔍

- [ ] **Port semantic slice to Baa** — preserve existing type/ABI rules.
- [ ] **Port symbol/scope tables** — with ownership semantics equivalent to C version.
- [ ] **Negative-suite parity** — verify same diagnostics and failure points.
- [ ] **Warning parity** — warning classes and trigger points match baseline behavior.
- [ ] **Deterministic diagnostics** — stable ordering/text for repeated runs.

### v0.9.4: Rewrite IR 🔄

- [ ] **Port IR core + lowering slices to Baa** — aligned with v0.5 split modules.
- [ ] **IR verifier parity** — preserve well-formedness and SSA contracts.
- [ ] **IR text parity tests** — compare canonical IR output vs C baseline.
- [ ] **Optimizer gate parity** — `--verify-ir/--verify-ssa` remain clean across corpus.
- [ ] **Arena/ownership parity** — preserve module-owned bulk-free behavior.

### v0.9.5: Rewrite Code Generator ⚙️

- [ ] **Port backend slices to Baa** — ISel/RegAlloc/Emit with unchanged ABI behavior.
- [ ] **Target parity** — Windows x64 and Linux x64 assembly equivalence gates.
- [ ] **Runtime parity** — integration tests must match C compiler behavior.
- [ ] **ABI edge-case suite** — variadics, float args/returns, pointer-heavy calls, stack alignment.
- [ ] **Asm diff audit** — differences explained and approved when semantically equivalent.

### v0.9.6: Rewrite Driver 🚗

- [ ] **Port driver/diagnostics slices to Baa** — CLI orchestration + error reporting.
- [ ] **Retire mixed mode** — switch default build to full-Baa compiler pipeline.
- [ ] **Full compiler in Baa** — all core components ported and wired.
- [ ] **CLI parity matrix** — all important flags preserve behavior (`--verify`, `--target`, `-S`, `-c`, ...).
- [ ] **Multi-file parity** — compile/link workflows remain deterministic and stable.

### v1.0.0: First Self-Compile 🏆

- [ ] **Compile Baa compiler with C-Baa** — Produces baa₁.
- [ ] **Test baa₁** — Run full test suite.
- [ ] **Compile Baa compiler with baa₁** — Produces baa₂.
- [ ] **Compile Baa compiler with baa₂** — Produces baa₃.
- [ ] **Verify baa₂ == baa₃** — Reproducible builds!
- [ ] **Release v1.0.0** — Historic milestone! 🎉

#### Phase 5 Exit Criteria (Release Gates)

- [ ] **Functional parity** — all core QA suites pass using baa₁/baa₂ toolchains.
- [ ] **Reproducibility parity** — bootstrap stages produce stable outputs per target.
- [ ] **Operational parity** — driver flags + diagnostics match expected contracts.
- [ ] **Performance budget** — compile-time regression remains within agreed threshold.
- [ ] **Documentation parity** — bootstrap and recovery procedures are complete.

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

### Phase 6 Architecture Scope

- [ ] **Frontend** — assembly lexer/parser with strict diagnostics.
- [ ] **Core** — instruction model + encoder + relocation planner.
- [ ] **Writers** — COFF/ELF object emitters with symbol/relocation tables.
- [ ] **Integration** — backend handoff + CLI controls + verification pipeline.

### v1.5.0: Baa Assembler (مُجمِّع باء) 🔧

#### v1.5.0.1: Assembler Foundation

- [ ] **Define instruction encoding tables** — x86-64 opcode maps.
- [ ] **Parse assembly text** — Tokenize AT&T/Intel syntax.
- [ ] **Build instruction IR** — Internal representation of machine code.
- [ ] **Handle labels** — Track label addresses for jumps.
- [ ] **MVP syntax decision** — AT&T as canonical input first, Intel optional later.
- [ ] **Source mapping** — retain line/column mapping for assembler diagnostics.

#### v1.5.0.2: x86-64 Encoding

- [ ] **REX prefixes** — 64-bit register encoding.
- [ ] **ModR/M and SIB bytes** — Addressing mode encoding.
- [ ] **Immediate encoding** — Handle different immediate sizes.
- [ ] **Displacement encoding** — Memory offset encoding.
- [ ] **Instruction validation** — Check valid operand combinations.
- [ ] **Coverage matrix** — define required instruction families for Baa backend output.
- [ ] **Relocation-aware encoding** — encode fixup placeholders for unresolved symbols.

#### v1.5.0.3: Object File Generation

- [ ] **COFF format (Windows)** — Generate .obj files.
- [ ] **ELF format (Linux)** — Generate .o files.
- [ ] **Section handling** — .text, .data, .bss, .rodata.
- [ ] **Symbol table** — Export/import symbols.
- [ ] **Relocation entries** — Handle address fixups.
- [ ] **ABI metadata correctness** — calling-convention relevant symbol attributes.
- [ ] **Cross-platform conformance tests** — validate produced `.obj/.o` with system linkers.

#### v1.5.0.4: Assembler Integration

- [ ] **Replace GAS calls** — Use internal assembler.
- [ ] **`--use-internal-asm` flag** — Optional internal assembler.
- [ ] **Verify output** — Compare with GAS output.
- [ ] **Performance test** — Ensure acceptable speed.
- [ ] **Fallback policy** — clear fallback to external assembler on unsupported patterns.
- [ ] **CI dual-mode runs** — external/internal assembler comparison in regression pipeline.

#### v1.5.0.5: Assembler Polish

- [ ] **Error messages** — Clear assembly error diagnostics.
- [ ] **Debug info** — Generate debug symbols.
- [ ] **Listing output** — Optional assembly listing with addresses.
- [ ] **Documentation** — Assembler internals guide.

#### v1.5.0.6: Assembler Release Gate

- [ ] **Default-on readiness** — internal assembler can become default for supported targets.
- [ ] **Stability gate** — crash-free stress/fuzz-lite runs across assembler corpus.
- [ ] **Parity signoff** — approved diff report vs GAS on representative corpus.

---

## 🔗 Phase 7: Own Linker (v2.0.0)

*Goal: Remove dependency on external linker (ld/link.exe).*

### Phase 7 Architecture Scope

- [ ] **Object ingestion layer** — robust COFF/ELF readers with strict validation.
- [ ] **Link graph core** — symbol table, section graph, relocation plan, address assignment.
- [ ] **Format writers** — PE/ELF executable writers with deterministic layout rules.
- [ ] **Runtime bridge layer** — controlled integration with CRT/system runtime requirements.
- [ ] **Verification layer** — parity checks vs system linkers + deterministic output checks.

### Phase 7 Operating Rules

- [ ] **Deterministic link ordering** — stable output independent of host filesystem ordering.
- [ ] **No silent symbol resolution** — ambiguous/duplicate/undefined symbols must emit explicit diagnostics.
- [ ] **Relocation correctness first** — overflow and invalid relocation types are hard failures.
- [ ] **Target isolation** — Windows and Linux paths share abstractions, not target-specific hacks.

### v2.0.0: Baa Linker (رابط باء) 🔗

#### v2.0.0.1: Linker Foundation

- [ ] **Parse object files** — Read COFF/ELF format.
- [ ] **Symbol resolution** — Match symbol references to definitions.
- [ ] **Section merging** — Combine sections from multiple objects.
- [ ] **Memory layout** — Assign virtual addresses to sections.
- [ ] **Archive scanning strategy** — deterministic one-pass/two-pass policy for `.a/.lib`.
- [ ] **Input validation policy** — reject malformed object metadata with Arabic diagnostics.

#### v2.0.0.2: Relocation Processing

- [ ] **Apply relocations** — Fix up addresses in code/data.
- [ ] **Handle relocation types** — PC-relative, absolute, GOT, PLT.
- [ ] **Overflow detection** — Check address range limits.
- [ ] **Relocation test matrix** — per-target coverage for required relocation kinds.
- [ ] **Late-binding audit logs** — debug trace mode for relocation decisions.

#### v2.0.0.3: Executable Generation (Windows)

- [ ] **PE header** — DOS stub, PE signature, file header.
- [ ] **Optional header** — Entry point, section alignment, subsystem.
- [ ] **Section headers** — .text, .data, .rdata, .bss.
- [ ] **Import table** — For C runtime and Windows API.
- [ ] **Export table** — If building DLLs (future).
- [ ] **Generate .exe** — Complete Windows executable.
- [ ] **PE conformance checks** — verify headers/alignments with tooling (`dumpbin`/`llvm-readobj`).
- [ ] **Subsystem policies** — console/gui/custom-startup rules documented and tested.

#### v2.0.0.4: Executable Generation (Linux)

- [ ] **ELF header** — File identification, entry point.
- [ ] **Program headers** — Loadable segments.
- [ ] **Section headers** — .text, .data, .rodata, .bss.
- [ ] **Dynamic linking info** — For libc linkage.
- [ ] **Generate executable** — Complete Linux binary.
- [ ] **ELF conformance checks** — validate segments/sections with `readelf`/`objdump`.
- [ ] **PIE/non-PIE policy** — deterministic handling for code model and relocation mode.

#### v2.0.0.5: Linker Features

- [ ] **Static libraries** — Link .a/.lib archives.
- [ ] **Library search paths** — `-L` flag support.
- [ ] **Entry point selection** — Custom entry point support.
- [ ] **Strip symbols** — Remove debug symbols for release.
- [ ] **Map file** — Generate link map for debugging.
- [ ] **Section GC plan** — optional dead-section elimination with safety checks.
- [ ] **Weak symbol semantics** — explicit target-specific behavior for weak/strong bindings.

#### v2.0.0.6: Linker Integration

- [ ] **Replace ld/link calls** — Use internal linker.
- [ ] **`--use-internal-linker` flag** — Optional internal linker.
- [ ] **Verify output** — Compare with system linker output.
- [ ] **End-to-end test** — Compile and link without external tools.
- [ ] **Dual-link CI mode** — run internal and system linker paths on same corpus.
- [ ] **Fallback policy** — controlled fallback with reasoned diagnostics when unsupported.

#### v2.0.0.7: Linker Release Gate

- [ ] **Cross-target parity signoff** — runtime and symbol behavior match system linker expectations.
- [ ] **Determinism signoff** — stable binary layout for identical inputs/toolchains.
- [ ] **Stress signoff** — large multi-object links and archive-heavy workloads remain stable.

#### Phase 7 Exit Criteria

- [ ] **Default-on readiness** — internal linker can be default for supported targets.
- [ ] **Regression stability** — quick/full/stress QA pass with internal linker enabled.
- [ ] **Debuggability baseline** — diagnostics + map output sufficient for failure triage.

---

## 🏆 Phase 8: Full Independence (v3.0.0)

*Goal: Zero external dependencies — Baa builds itself with no external tools.*

### Phase 8 Architecture Scope

- [ ] **Runtime kernel layer** — startup, syscall/API bridge, memory primitives, and process exit flow.
- [ ] **Native stdlib layer** — string/math/memory/io modules implemented in Baa with stable ABI contracts.
- [ ] **Bootstrap provenance layer** — staged compiler lineage (`baa0 -> baa1 -> baa2`) with hashable artifacts.
- [ ] **Hermetic build layer** — controlled inputs, pinned flags, deterministic packaging outputs.
- [ ] **Operational verification layer** — dependency scans, runtime smoke tests, and cross-target release gates.

### Phase 8 Operating Rules

- [ ] **No hidden host dependencies** — any required host tool must be explicitly listed in bootstrap docs.
- [ ] **Reproducibility-first policy** — build determinism regressions block release.
- [ ] **Cross-target parity policy** — Windows/Linux runtime features must ship together or stay gated.
- [ ] **Fail-fast runtime diagnostics** — startup/syscall/runtime failures must emit explicit Arabic-first errors.
- [ ] **Recovery-path requirement** — each independence milestone must define rollback and re-bootstrap steps.

### v3.0.0: Complete Toolchain 🛠️

#### v3.0.0.1: Remove C Runtime Dependency

**Windows:**

- [ ] **Direct Windows API calls** — Replace printf with WriteConsoleA.
- [ ] **Implement `اطبع` natively** — Direct syscall/API.
- [ ] **Implement `اقرأ` natively** — ReadConsoleA.
- [ ] **Implement memory functions** — HeapAlloc/HeapFree instead of malloc/free.
- [ ] **Implement file I/O** — CreateFile, ReadFile, WriteFile.
- [ ] **Custom entry point** — Replace C runtime startup.
- [ ] **SEH-aware startup** — preserve stable crash/exit semantics without CRT helpers.
- [ ] **Win64 ABI compliance checks** — stack alignment, shadow space, and return path verified.

**Linux:**

- [ ] **Direct syscalls** — write, read, mmap, exit.
- [ ] **Implement `اطبع` natively** — syscall to write(1, ...).
- [ ] **Implement `اقرأ` natively** — syscall to read(0, ...).
- [ ] **Implement memory functions** — mmap/munmap for allocation.
- [ ] **Implement file I/O** — open, read, write, close syscalls.
- [ ] **Custom _start** — No libc dependency.
- [ ] **Syscall ABI validation** — register/stack contracts verified on x86_64 SystemV.
- [ ] **Signal/exit behavior checks** — deterministic termination semantics across test corpus.

#### v3.0.0.2: Native Standard Library

- [ ] **Rewrite string functions in Baa** — No C dependency.
- [ ] **Rewrite math functions in Baa** — Pure Baa implementation.
- [ ] **Rewrite memory functions in Baa** — Custom allocator.
- [ ] **Full standard library in Baa** — All library code in Baa.
- [ ] **Module boundary contracts** — document stable APIs for `core`, `memory`, `string`, `math`, `io`.
- [ ] **Behavioral parity suite** — compare stdlib behavior vs prior runtime expectations.
- [ ] **Allocator policy gates** — fragmentation/throughput baselines for long-running workloads.

#### v3.0.0.3: Self-Contained Build

- [ ] **Single binary compiler** — No external dependencies.
- [ ] **Cross-compilation support** — Build Linux binary on Windows and vice versa.
- [ ] **Reproducible builds** — Same source → identical binary.
- [ ] **Bootstrap from source** — Document minimal bootstrap path.
- [ ] **Hermetic manifest** — lock source inputs, flags, and artifact metadata per release.
- [ ] **Stage replay tooling** — deterministic scripts to replay bootstrap on clean machines.
- [ ] **Provenance hashing** — publish hashes for each stage artifact and final binaries.

#### v3.0.0.4: Verification & Release

- [ ] **Full test suite passes** — All tests without external tools.
- [ ] **Benchmark comparison** — Performance vs GCC toolchain.
- [ ] **Security audit** — Review for vulnerabilities.
- [ ] **Documentation complete** — Full toolchain documentation.
- [ ] **Release v3.0.0** — Fully independent Baa! 🎉
- [ ] **Supply-chain audit** — verify release pipeline does not reintroduce external tool reliance.
- [ ] **Disaster-recovery drill** — validate clean-room bootstrap from tagged sources.

#### v3.0.0.5: Independence Release Gate

- [ ] **Zero-dependency signoff** — compile/assemble/link/run path requires only Baa-delivered artifacts.
- [ ] **Deterministic bootstrap signoff** — repeated stage builds are byte-stable per target.
- [ ] **Cross-target signoff** — Windows + Linux release candidates pass identical gate checklist.
- [ ] **Operational signoff** — upgrade/rollback/bootstrap recovery procedures validated and documented.

#### Phase 8 Exit Criteria

- [ ] **Runtime independence** — no libc/CRT dependency in default build+run workflow.
- [ ] **Toolchain independence** — compiler, assembler, linker, and runtime owned by Baa project.
- [ ] **Reproducibility baseline** — release artifacts are verifiable and reproducible from source.
- [ ] **Sustainability baseline** — maintenance docs and on-call triage playbooks are complete.

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
  - The backend pipeline consumes a validated AST.
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
iled changes, see the [Changelog](CHANGELOG.md)*
