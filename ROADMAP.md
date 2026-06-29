# Baa Roadmap (Updated)

> Track the development progress of the Baa programming language.
> \*\*Current Status:\*\* Phase 4.5 - Reference Compiler Stabilization (v0.5.x) ← IN PROGRESS

\---

## 📚 Documentation Track (Definitive Arabic Book)

*Goal: Produce a Kernighan \& Ritchie–style first book for Baa, in Arabic, serving as the definitive learning + reference resource.*

* \[\~] **Write the Arabic "Baa Book"** — book-length guide in Arabic with exercises. — draft exists in `docs/BAA\_BOOK\_AR.md`
* \[ ] **Define terminology glossary** — consistent Arabic technical vocabulary.
* \[ ] **Create example suite** — verified, idiomatic examples that compile with v0.3.7.
* \[ ] **Add exercises and challenges** — per chapter, with expected outputs.
* \[ ] **Add debugging and performance chapters** — common pitfalls, diagnostics, optimization notes.
* \[ ] **Native technical review** — review by Arabic-speaking engineers before release.

\---

## ⚙️ Phase 3: The Intermediate Representation (v0.3.x)

*Goal: Decouple the language from x86 Assembly to enable optimizations and multiple backends.*

> \*\*Design Document:\*\* See \[BAA\_IR\_SPECIFICATION.md](docs/BAA\_IR\_SPECIFICATION.md) for full IR specification.

### v0.3.0: IR Foundation 🏗️

#### v0.3.0.1: IR Data Structures ✅ COMPLETED (2026-01-15)

* \[x] **Define `IROp` enum** — All opcodes: `IR\_OP\_ADD`, `IR\_OP\_SUB`, `IR\_OP\_MUL`, etc.
* \[x] **Define `IRType` enum** — Types: `IR\_TYPE\_I64`, `IR\_TYPE\_I32`, `IR\_TYPE\_I8`, `IR\_TYPE\_I1`, `IR\_TYPE\_PTR`.
* \[x] **Define `IRInst` struct** — Instruction with opcode, type, dest register, operands.
* \[x] **Define `IRBlock` struct** — Basic block with label, instruction list, successors.
* \[x] **Define `IRFunc` struct** — Function with name, return type, entry block, register counter.
* \[x] **Create `ir.h`** — Header file with all IR definitions.
* \[x] **Create `ir.c`** — Implementation with helper functions and IR printing.

#### v0.3.0.2: IR Builder Functions ✅ COMPLETED (2026-01-15)

* \[x] **`IRBuilder` context struct** — Builder pattern with insertion point tracking.
* \[x] **`ir\_builder\_create\_func()`** — Create a new IR function.
* \[x] **`ir\_builder\_create\_block()`** — Create a new basic block with label.
* \[x] **`ir\_builder\_set\_insert\_point()`** — Set insertion point for new instructions.
* \[x] **`ir\_builder\_alloc\_reg()`** — Allocate next virtual register `%م<n>`.
* \[x] **`ir\_builder\_emit\_\*()`** — Emit instructions (add, sub, mul, div, load, store, br, ret, call, etc.).
* \[x] **Control flow helpers** — `ir\_builder\_create\_if\_then()`, `ir\_builder\_create\_while()`.
* \[x] **Create `ir\_builder.h`** — Header file with builder API.
* \[x] **Create `ir\_builder.c`** — Implementation of builder functions.

#### v0.3.0.3: AST to IR Lowering (Expressions) ✅ COMPLETED (2026-01-16)

* \[x] **`lower\_expr()`** — Main expression lowering dispatcher.
* \[x] **Lower `NODE\_INT`** — Return immediate value.
* \[x] **Lower `NODE\_VAR\_REF`** — Generate `حمل` (load) instruction.
* \[x] **Lower `NODE\_BIN\_OP`** — Generate `جمع`/`طرح`/`ضرب`/`قسم` instructions.
* \[x] **Lower `NODE\_UNARY\_OP`** — Generate `سالب`/`نفي` instructions.
* \[x] **Lower `NODE\_CALL\_EXPR`** — Generate `نداء` (call) instruction.

#### v0.3.0.4: AST to IR Lowering (Statements) ✅ COMPLETED (2026-01-16)

* \[x] **`lower\_stmt()`** — Main statement lowering dispatcher.
* \[x] **Lower `NODE\_VAR\_DECL`** — Generate `حجز` (alloca) + `خزن` (store).
* \[x] **Lower `NODE\_ASSIGN`** — Generate `خزن` (store) instruction.
* \[x] **Lower `NODE\_RETURN`** — Generate `رجوع` (return) instruction.
* \[x] **Lower `NODE\_PRINT`** — Generate `نداء @اطبع()` call.
* \[x] **Lower `NODE\_READ`** — Generate `نداء @اقرأ()` call.

#### v0.3.0.5: AST to IR Lowering (Control Flow) ✅ COMPLETED (2026-01-16)

* \[x] **Lower `NODE\_IF`** — Create condition block + true/false blocks + merge block.
* \[x] **Lower `NODE\_WHILE`** — Create header/body/exit blocks with back edge.
* \[x] **Lower `NODE\_FOR`** — Create init/header/body/increment/exit blocks.
* \[x] **Lower `NODE\_SWITCH`** — Create comparison chain + case blocks.
* \[x] **Lower `NODE\_BREAK`** — Generate `قفز` to loop exit.
* \[x] **Lower `NODE\_CONTINUE`** — Generate `قفز` to loop header/increment.

#### v0.3.0.6: IR Printer ✅ COMPLETED (2026-01-17)

* \[x] **`ir\_print\_func()`** — Print function header and all blocks.
* \[x] **`ir\_print\_block()`** — Print block label and all instructions.
* \[x] **`ir\_print\_inst()`** — Print single instruction with Arabic opcodes.
* \[x] **Arabic numeral output** — Print register numbers in Arabic (٠١٢٣٤٥٦٧٨٩).
* \[x] **`--dump-ir` CLI flag** — Add command-line option to print IR.

#### v0.3.0.7: Integration \& Testing ✅ COMPLETED (2026-01-17)

* \[x] **Integrate IR into pipeline** — AST → IR (skip direct codegen).
* \[x] **Create `ir\_test.baa`** — Simple test programs.
* \[x] **Verify IR output** — Check IR text matches specification.
* \[x] **Update `main.c`** — Add IR phase between analysis and codegen.
* \[x] **Add `--emit-ir` flag** — Write IR to `.ir` file.
* \[x] **Fix global variable resolution** — Proper lookup in `lower\_expr()` and `lower\_assign()`.

\---

### v0.3.1: The Optimizer ⚡

#### v0.3.1.1: Analysis Infrastructure ✅ COMPLETED (2026-01-21)

* \[x] **CFG validation** — Verify all blocks have terminators.
* \[x] **Predecessor lists** — Build predecessor list for each block.
* \[x] **Dominator tree** — Compute dominance relationships.
* \[x] **Define `IRPass` interface** — Function pointer for optimization passes.

#### v0.3.1.2: Constant Folding (طي\_الثوابت) ✅ COMPLETED (2026-01-22)

* \[x] **Detect constant operands** — Both operands are immediate values.
* \[x] **Fold arithmetic** — `جمع ص٦٤ ٥، ٣` → `٨`.
* \[x] **Fold comparisons** — `قارن أكبر ص٦٤ ١٠، ٥` → `صواب`.
* \[x] **Replace instruction** — Remove op, use constant result.

#### v0.3.1.3: Dead Code Elimination (حذف\_الميت) ✅ COMPLETED (2026-01-27)

* \[x] **Mark used values** — Walk from terminators backward.
* \[x] **Identify dead instructions** — Result never used.
* \[x] **Remove dead instructions** — Delete from block.
* \[x] **Remove unreachable blocks** — No predecessors (except entry).

#### v0.3.1.4: Copy Propagation (نشر\_النسخ) ✅ COMPLETED (2026-01-27)

* \[x] **Detect copy instructions** — `IR\_OP\_COPY` (`نسخ`) instruction pattern.
* \[x] **Replace uses** — Substitute original for copy in operands / call args / phi entries.
* \[x] **Remove redundant copies** — Delete `نسخ` instruction after propagation.

#### v0.3.1.5: Common Subexpression Elimination (حذف\_المكرر) ✅ COMPLETED (2026-02-07)

* \[x] **Hash expressions** — Create signature for each operation.
* \[x] **Detect duplicates** — Same op + same operands.
* \[x] **Replace with existing result** — Reuse previous computation.

#### v0.3.1.6: Optimization Pipeline ✅ COMPLETED (2026-02-07)

* \[x] **Pass ordering** — Define optimal pass sequence (constfold → copyprop → CSE → DCE).
* \[x] **Iteration** — Run passes until no changes (fixpoint, max 10 iterations).
* \[x] **`-O0`, `-O1`, `-O2` flags** — Control optimization level.
* \[x] **`--dump-ir-opt`** — Print IR after optimization.

\---

### v0.3.2: The Backend (Target Independence) 🎯

#### v0.3.2.1: Instruction Selection ✅ COMPLETED (2026-02-07)

* \[x] **Define `MachineInst`** — Abstract machine instruction.
* \[x] **IR to Machine mapping** — `جمع` → `ADD`, `حمل` → `MOV`, etc.
* \[x] **Pattern matching** — Select optimal instruction sequences.
* \[x] **Handle immediates** — Inline constants where possible.

#### v0.3.2.2: Register Allocation ✅ COMPLETED (2026-02-07)

* \[x] **Liveness analysis** — Compute live ranges for each virtual register.
* \[x] **Linear scan allocator** — Simple, fast allocation algorithm.
* \[x] **Spilling** — Handle register pressure by spilling to stack.
* \[x] **Map to x64 registers** — RAX, RBX, RCX, RDX, R8-R15.

#### v0.3.2.3: Code Emission ✅ COMPLETED (2026-02-08)

* \[x] **Emit function prologue** — Stack setup, callee-saved registers.
* \[x] **Emit instructions** — Generate AT\&T syntax assembly.
* \[x] **Emit function epilogue** — Stack teardown, return.
* \[x] **Emit data section** — Global variables and string literals.

#### v0.3.2.4: Backend Integration

* \[x] **Replace old codegen** — IR → Backend → Assembly.
* \[x] **Verify output** — Compare with old codegen results.
* \[x] **Performance testing** — Ensure no regression.
* \[x] **Remove legacy codegen** — Retire the legacy AST backend from the build.

#### 0.3.2.4-IR-FIX: ISel Bug Fixes \& Backend Testing ✅ COMPLETED (2026-02-08)

* \[x] **Fix ISel logical op size mismatch** — `isel\_lower\_logical()` forced 64-bit operand size; widened 8-bit boolean vregs to prevent assembler errors.
* \[x] **Fix function parameter ABI copies** — `isel\_lower\_func()` prepends MOV from RCX/RDX/R8/R9 to parameter vregs at entry block.
* \[x] **Fix IDIV RAX constraint** — `isel\_lower\_div()` explicitly routes dividend through RAX (vreg -2) for correct division results.
* \[x] **Comprehensive backend test** — `tests/integration/backend/backend\_test.baa`: 27 functions, 63 assertions, all PASS.

#### 0.3.2.4-setup: Installer \& GCC Bundling ✅ COMPLETED (2026-02-08)

* \[x] **Bundle MinGW-w64 GCC** — Ship GCC toolchain in `gcc/` subfolder inside the installer.
* \[x] **Auto-detect bundled GCC** — `resolve\_gcc\_path()` in `main.c` finds `gcc.exe` relative to `baa.exe`.
* \[x] **Update installer (setup.iss)** — Add `gcc\\\*` files, dual PATH entries, post-install GCC verification.
* \[x] **GCC bundle script** — `scripts/prepare\_gcc\_bundle.ps1` downloads and prepares the minimal toolchain.
* \[x] **Sync version metadata** — `baa.rc` and `setup.iss` updated to `0.3.2.4`, publisher to "Omar Aglan".

\---

### v0.3.2.5: SSA Construction 🔄

**Strategy (Canonical SSA / الطريقة القياسية لـ SSA):**

* **Mem2Reg (ترقية الذاكرة إلى سجلات)** بأسلوب Cytron/LLVM القياسي:

  * حساب **المسيطرات (Dominators)** و **حدود السيطرة (Dominance Frontiers)**
  * إدراج عقد **فاي (Phi)** عند نقاط الدمج (join points)
  * **إعادة التسمية (SSA Renaming)** لبناء تعريفات واصلة (reaching definitions)
* هذا الأسلوب هو الأساس طويل المدى لتحسينات متقدمة لاحقاً مثل: GVN/CSE و LICM و PRE وغيرها.

#### v0.3.2.5.1: Memory to Register Promotion ✅ COMPLETED (2026-02-09)

* \[x] **Identify promotable allocas** — Single-block allocas with no escaping (correctness-first baseline; design stays compatible with full Mem2Reg).
* \[x] **Replace loads/stores** — Convert to direct register use.
* \[x] **Remove dead allocas** — Delete promoted `حجز` instructions.

#### v0.3.2.5.2: Phi Node Insertion ✅ COMPLETED (2026-02-09)

* \[x] **Compute dominance frontiers** — Where Phi nodes are needed.
* \[x] **Insert Phi placeholders** — Add `فاي` at join points.
* \[x] **Rename variables** — SSA renaming pass with reaching definitions.
* \[x] **Connect Phi operands** — Link values from predecessor blocks.

#### v0.3.2.5.3: SSA Validation ✅ COMPLETED (2026-02-09)

* \[x] **Verify SSA properties** — Each register defined exactly once.
* \[x] **Check dominance** — Definition dominates all uses.
* \[x] **Validate Phi nodes** — One operand per predecessor.
* \[x] **`--verify-ssa` flag** — Debug option to run SSA checks.

\---

### v0.3.2.6: IR Stabilization \& Polish 🧹

#### v0.3.2.6.0: Codebase Soldering (مرحلة\_تلحيم\_القاعدة) ✅ COMPLETED (2026-02-09)

* \[x] **Enable compiler warnings (two-tier)** — Default warnings on; optional `-Werror` hardening toggle.
* \[x] **Fix unsafe string building in driver** — Replace `sprintf/strcpy/strcat` command construction with bounded appends and overflow checks.
* \[x] **Fix symbol-table name overflow** — Guard `Symbol.name\[32]` writes; reject/diagnose identifiers that exceed the limit.
* \[x] **Harden updater version parsing** — Support multi-part versions like `0.3.2.5.3`; replace `sprintf` with `snprintf`; check parse results.
* \[x] **Audit `strncpy` usage** — Ensure explicit NUL-termination and bounds safety in lexer and helpers.
* \[x] **Replace `atoi` with checked parsing** — Use `strtoll` + validation for integer literals and array sizes; produce safe diagnostics.
* \[x] **Warning clean build** — Zero warnings under default warning set; `-Werror` build passes.

#### v0.3.2.6.1: IR Memory Management

✅ COMPLETED (2026-02-11)

* \[x] **Arena allocator for IR** — Fast allocation, bulk deallocation.
* \[x] **IR cloning** — Deep copy of functions/blocks.
* \[x] **IR destruction** — Clean up all IR memory.
* \[x] **Def-use chains for SSA regs** — Build and maintain use lists to make IR passes fast and safe (avoid whole-function rescans).
* \[x] **Instruction numbering / stable IDs** — Deterministic per-function instruction IDs for analyses, debugging, and regression tests.
* \[x] **IR mutation helpers** — Central utilities to insert/remove instructions and update CFG metadata (pred/succ/dominance caches) consistently.

#### v0.3.2.6.2: Debug Information

✅ COMPLETED (2026-02-11)

* \[x] **Source location tracking** — Map IR instructions to source lines.
* \[x] **Variable name preservation** — Keep original names for debugging.
* \[x] **`--debug-info` flag** — Emit debug metadata in assembly.

#### v0.3.2.6.3: IR Serialization

✅ COMPLETED (2026-02-11)

* \[x] **Text IR writer** — Output canonical IR text format.
* \[x] **Text IR reader** — Parse IR text back to data structures.
* \[x] **Round-trip testing** — Write → Read → Compare.

#### v0.3.2.6.4: Register Allocator Liveness Fix (إصلاح حيوية مخصص السجلات)

✅ COMPLETED (2026-02-12)

* \[x] **Fix liveness across loop back-edges** — Linear scan liveness analysis does not correctly propagate live ranges across loop back-edges when many variables are simultaneously live, causing register clobbering and segfaults.
* \[x] **Extend live intervals to loop ends** — Ensure variables used inside loops have their intervals extended to cover the entire loop body including back-edges.
* \[x] **Add block-level scoping in semantic analyzer** — Currently function-level only; for-loop variables cannot be redeclared in the same function, requiring unique names.
* \[x] **Stress test with high register pressure** — Validate fix with functions containing 8+ live variables across multiple nested loops.

#### v0.3.2.6.5: IR Verification \& Canonicalization (تحقق\_الـIR\_وتوحيده)

✅ COMPLETED (2026-02-13)

* \[x] **IR well-formedness verifier (`--verify-ir`)** — Validate operand counts, type consistency, terminator rules, phi placement, and call signatures (separate from `--verify-ssa`).
* \[x] **Verifier gate in optimizer (debug)** — Optional mode to run `--verify-ir`/`--verify-ssa` after each pass iteration to catch pass bugs early.
* \[x] **Canonicalization pass** — Normalize commutative operands, constant placement, and comparison canonical forms to make CSE/DCE/constfold more effective.
* \[x] **CFG simplification pass** — Merge trivial blocks, remove redundant branches, and provide a reusable critical-edge splitting utility for IR passes.

#### v0.3.2.6.6: IR Semantics \& Data Layout (دلالات\_الـIR\_وتخطيط\_البيانات)

* \[x] **Define IR arithmetic semantics** — Document and enforce overflow behavior (recommended: two’s-complement wrap), and clarify `i1` truthiness and `div/mod` rules for negatives.
* \[x] **Data layout helpers** — Add size/alignment queries per `IRType` (incl. pointer size) as the foundation for future `Target` abstraction and correct aggregate lowering.
* \[x] **Memory model contract** — Specify and verify rules for `حجز/حمل/خزن` (typed pointers, aliasing assumptions, and what is/ isn’t legal for optimization).

#### v0.3.2.6.7: SSA Verification Fix in Switch (إصلاح التحقق من SSA في جملة اختر)

✅ COMPLETED (2026-02-14)

* \[x] **Fix SSA verification failure** — Resolved dominance issue in CSE pass for `switch` statements with `default` cases.

\---

### v0.3.2.7: Advanced Optimizations 🚀

#### v0.3.2.7.1: Loop Optimizations

✅ COMPLETED (2026-02-16)

* \[x] **Loop detection** — Identify natural loops via back edges.
* \[x] **Loop invariant code motion** — Hoist constant computations.
* \[x] **Strength reduction** — Replace expensive ops (mul → shift).
* \[x] **Loop unrolling** — Optional with `-funroll-loops`.

#### v0.3.2.7.2: Inlining

✅ COMPLETED (2026-02-16)

* \[x] **Inline heuristics** — Small functions, single call site.
* \[x] **Inline expansion** — Copy function body to call site.
* \[x] **Post-inline cleanup** — Re-run optimization passes.

#### v0.3.2.7.3: Tail Call Optimization

✅ COMPLETED (2026-02-17)

* \[x] **Detect tail calls** — `call` immediately followed by `ret` (بدون تعليمات بينهما).
* \[x] **Convert to jump** — Replace call+ret with a tail jump (no new return address).
* \[x] **Stack reuse** — Reuse caller's stack frame + caller shadow space (Windows x64 ABI).
* \[x] **Limitation (for v0.3.2.7.3)** — initial implementation supports only <= 4 arguments (register args); stack-args tail calls are scheduled in v0.3.2.8.5.

\---

### v0.3.2.8: Multi-Target Preparation 🌐

#### v0.3.2.8.1: Target Abstraction

✅ COMPLETED (2026-02-17)

* \[x] **Define `Target` interface** — OS/object-format, data layout, calling convention, asm directives.
* \[x] **x86-64 Windows target** — Keep current behavior as the first concrete target.
* \[x] **Host default target** — Windows host defaults to `x86\_64-windows`, Linux host defaults to `x86\_64-linux`.
* \[x] **Target selection** — `--target=x86\_64-windows|x86\_64-linux` flag.

#### v0.3.2.8.2: Calling Convention Abstraction

✅ COMPLETED (2026-02-17)

* \[x] **Define `CallingConv` struct** — arg regs order/count, return regs, caller/callee-saved masks.
* \[x] **Stack rules** — stack alignment + varargs rules are modeled; stack-arg placement is deferred (backend rejects stack args for now).
* \[x] **Windows x64 ABI** — RCX/RDX/R8/R9 + 32-byte shadow/home space.
* \[x] **SystemV AMD64 ABI** — RDI/RSI/RDX/RCX/R8/R9, no shadow space, sets `AL=0` for calls (conservative).

#### v0.3.2.8.3: Code Model Options

✅ COMPLETED (2026-02-17)

* \[x] **Small code model** — Default (only supported model in v0.3.2.8.3).
* \[x] **PIC/PIE flags** — `-fPIC`/`-fPIE` (Linux/ELF; initial support).
* \[x] **Stack protection** — stack canaries on ELF via `-fstack-protector\*`.

#### v0.3.2.8.4: Linux x86-64 Target 🐧

✅ COMPLETED (2026-02-17)

* \[x] **Native Linux build of compiler** — build `baa` on Linux with GCC/Clang + CMake.
* \[x] **SystemV AMD64 ABI implementation** — different calling convention from Windows.
* \[x] **ELF output support** — `.rodata`/`.data`/`.text` directives compatible with ELF GAS.
* \[x] **Link with host gcc (for now)** — produce ELF executables via host toolchain; later reduce/remove GCC dependency.
* \[x] **Cross-compilation (later)** — optional `--target=x86\_64-linux` from Windows once a cross toolchain story exists.

#### v0.3.2.8.5: Windows x64 Stack Args + Full Tail Calls

✅ COMPLETED (2026-02-17)

* \[x] **Proper stack-arg calling** — outgoing call frames support stack args on Windows x64 and SysV AMD64.
* \[x] **Tail calls beyond reg-args** — enable TCO with stack args conservatively (requires enough incoming stack-arg area).
* \[x] **Varargs home space (Windows)** — home/register args are written into shadow space before calls.

#### v0.3.2.8.6: Aggressive IR \& Optimizations (GCC/MSVC-like)

✅ COMPLETED (2026-02-17)

*Goal: move toward compiler-grade IR optimizations in pragmatic steps.*

* \[x] **InstCombine** — local simplification patterns (canonical `COPY` rewrites).
* \[x] **SCCP** — sparse conditional constant propagation + `br\_cond` folding.
* \[x] **GVN** — dominator-scoped global value numbering for pure expressions.
* \[x] **Mem2Reg promotability unlock** — must-def initialization analysis (instead of “init store must be in alloca block”).
* \[x] **Pipeline wiring** — run InstCombine+SCCP early; GVN at `-O2` before CSE.

\---

### v0.3.2.9: IR Verification \& Benchmarking ✅

#### v0.3.2.9.1: Comprehensive IR Verification ✅ COMPLETED (2026-02-17)

* \[x] **Well-formedness checks** — All functions have entry blocks.
* \[x] **Type consistency** — Operand types match instruction requirements.
* \[x] **CFG integrity** — All branches point to valid blocks.
* \[x] **SSA verification** — Run `--verify-ssa` on all test programs.
* \[x] **`baa --verify` mode** — Run all verification passes.

#### v0.3.2.9.2: Performance Benchmarking ✅ COMPLETED (2026-02-17)

* \[x] **Compile-time benchmark** — Compare compiler-only (`-S`) and end-to-end compile wall time.
* \[x] **Runtime benchmark** — Run deterministic `bench/runtime\_\*.baa` programs.
* \[x] **Memory usage profiling** — Track peak RSS on Linux via `/usr/bin/time -v` and IR arena stats via `--time-phases`.
* \[x] **Benchmark suite** — Collection of representative programs (`bench/\*.baa`).

#### v0.3.2.9.3: Regression Testing ✅ COMPLETED (2026-02-17)

* \[x] **Output comparison** — Compare IR-based output against a reference corpus.
* \[x] **Test all v0.2.x programs** — Ensure backward compatibility.
* \[x] **Edge case testing** — Complex control flow, nested loops, recursion.
* \[x] **Error case testing** — Verify error messages unchanged.

#### v0.3.2.9.4: Documentation \& Cleanup

* \[x] **Update INTERNALS.md** — Document new IR pipeline.
* \[x] **IR Developer Guide** — How to add new IR instructions.
* \[x] **Driver split** — Extract CLI parsing, toolchain execution, and per-file compile pipeline into dedicated `src/driver\_\*.c/.h` modules.
* \[x] **Driver link safety** — Remove fixed-size `argv\_link` construction; build link argv dynamically based on object count.
* \[x] **Shared file reader** — Provide `read\_file()` as a shared module (`src/read\_file.c`) for the lexer include system.
* \[x] **Line ending normalization** — Add `.gitattributes` to keep the repo LF-normalized and reduce diff churn.
* \[x] **Remove deprecated code** — Remove legacy AST-based codegen paths and backend-compare mode.
* \[x] **Code review checklist** — Ensure code quality standards.

\---

## 📚 Phase 3.5: Language Completeness (v0.3.3 - v0.3.12)

*Goal: Add essential features to make Baa practical for real-world programs and suitable for future staged bootstrap experiments, without making self-hosting part of the current release path.*

### v0.3.3: Array Initialization 📊 ✅ COMPLETED (2026-02-18)

**Goal:** Enable direct initialization of arrays with values.

#### Features

* \[x] **Array Literal Syntax** – Initialize arrays with comma-separated values using `{` `}` (supports partial init + zero-fill like C).

**Syntax:**

```baa
صحيح قائمة\[٥] = {١، ٢، ٣، ٤، ٥}.

// With Arabic comma (،) or regular comma (,)
صحيح أرقام\[٣] = {١٠، ٢٠، ٣٠}.
```

#### Implementation Tasks

* \[x] **Parser**: Handle `{` `}` initializer list after array declaration.
* \[x] **Parser**: Support both Arabic comma `،` (U+060C) and regular comma `,` as separators.
* \[x] **Semantic Analysis**: Allow partial init (`count <= size`) and zero-fill the remainder; reject overflow.
* \[x] **Codegen**: Emit `.data` initializers for globals and runtime stores for locals (including zero-fill).

#### Deferred to v0.3.9

* \[x] Multi-dimensional arrays: `صحيح مصفوفة\[٣]\[٤].` ✅ COMPLETED (2026-02-25)
* \[x] Array length operator: `صحيح طول = حجم(قائمة) / حجم(صحيح).` ✅ COMPLETED (2026-02-25)

\---

### v0.3.4: Enumerations \& Structures 🏗️ ✅ COMPLETED (2026-02-18)

**Goal:** Add compound types for better code organization and type safety.

#### Features

* \[x] **Enum Declaration** – Named integer constants with type safety.
* \[x] **Struct Declaration** – Group related data into composite types (supports nested structs + enum fields).
* \[x] **Member Access** – Use `:` (colon) operator for accessing members.

### v0.3.4.5: Union Types (الاتحادات) 🔀 ✅ COMPLETED (2026-02-19)

**Goal:** Memory-efficient variant types for parsers and data structures.

#### Features

* \[x] **Union Declaration**:

```baa
  اتحاد قيمة {
      صحيح رقم.
      نص نص\_قيمة.
      منطقي منطق.
  }
  ```

* \[x] **Union Usage**:

```baa
  اتحاد قيمة ق.
  ق:رقم = ٤٢.        // All members share same memory
  ق:نص\_قيمة = "مرحبا". // Overwrites previous value
  ```

* \[x] **Tagged Union Pattern** (manual):

```baa
  تعداد نوع\_قيمة { رقم، نص\_ق }
   
  هيكل قيمة\_موسومة {
      تعداد نوع\_قيمة نوع.
      اتحاد قيمة بيانات.
  }
  ```

#### Implementation Tasks

* \[x] **Token**: Add `TOKEN\_UNION` for `اتحاد` keyword.
* \[x] **Parser**: Parse union declaration similar to struct.
* \[x] **Semantic**: All members start at offset 0.
* \[x] **Memory Layout**: Size = max member size, align = max member align.
* \[x] **Codegen**: Generate union storage + member access code.

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
    صحيح سنة\_الصنع.
    تعداد لون لون\_السيارة.
}

صحيح الرئيسية() {
    هيكل سيارة س.
    
    س:موديل = "تويوتا كورولا".
    س:سنة\_الصنع = ٢٠٢٤.
    س:لون\_السيارة = لون:أحمر.
    
    اطبع س:موديل.
    اطبع س:سنة\_الصنع.
    
    إذا (س:لون\_السيارة == لون:أحمر) {
        اطبع "تحذير: السيارات الحمراء سريعة!".
    }
    
    إرجع ٠.
}
```

#### Implementation Tasks

**Enumerations:**

* \[x] **Token**: Add `TOKEN\_ENUM` for `تعداد` keyword.
* \[x] **Parser**: Parse enum declaration: `تعداد <name> { <members> }`.
* \[x] **Parser**: Support Arabic comma `،` between enum members.
* \[x] **Semantic**: Auto-assign integer values (0, 1, 2...).
* \[x] **Semantic**: Enum values accessible via `<enum\_name>:<value\_name>`.
* \[x] **Type System**: Add `TYPE\_ENUM` to `DataType`.

**Structures:**

* \[x] **Token**: Add `TOKEN\_STRUCT` for `هيكل` keyword.
* \[x] **Token**: Add `TOKEN\_COLON` for `:` (already exists, verify usage).
* \[x] **Parser**: Parse struct declaration: `هيكل <name> { <fields> }`.
* \[x] **Parser**: Parse struct instantiation: `هيكل <name> <var>.`
* \[x] **Parser**: Parse member access: `<var>:<member>`.
* \[x] **Semantic**: Track struct definitions in symbol table.
* \[x] **Semantic**: Validate member access against struct definition.
* \[x] **Memory Layout**: Calculate field offsets with padding/alignment (supports nested structs).
* \[x] **Codegen**: Emit struct storage + member access code.

\---

### v0.3.5: Character Type 📝 (يشمل تقديم `عشري`)

**Goal:** Add a proper character type with UTF-8 source support.

ملاحظة: نوع `عشري` قُدِّم في v0.3.5 كثوابت/تخزين، واكتمل في v0.3.5.5 (عمليات + ABI).

#### Features

* \[x] **Character Type (`حرف`)** – UTF-8 character value (variable-length, 1..4 bytes) stored as a packed scalar.
* \[x] **String-Char Relationship** – Strings (`نص`) are represented as arrays of `حرف` (`حرف\[]`) with indexing (`اسم\[٠]`).
* \[x] **Float Type (`عشري`)** – *Deferred from v0.3.4.5* (introduced as a basic type + literals in v0.3.5; completed in v0.3.5.5 with ops/ABI).

**Syntax:**

```baa
حرف ح = 'أ'.
نص اسم = "أحمد".  // Equivalent to: حرف اسم\[] = {'أ', 'ح', 'م', 'د', '\\0'}.
```

#### Implementation Tasks

* \[x] **Token**: Already have `TOKEN\_CHAR` for literals.
* \[x] **Token**: Add `TOKEN\_KEYWORD\_CHAR` for `حرف` type keyword.
* \[x] **Type System**: Add `TYPE\_CHAR` to `DataType` enum.
* \[x] **Semantic**: Distinguish between `char` and `int`.
* \[x] **Codegen**: Store `حرف` as packed `i64` (bytes + length) and support UTF-8 printing.
* \[x] **String Representation**: Update internal string handling to use `حرف\[]`.

#### Deferred to v0.3.9

* String operations: `طول\_نص()`, `دمج\_نص()`, `قارن\_نص()`

### v0.3.5.5: Numeric Types (Sized Integers + `عشري`) ✅ COMPLETED (2026-02-24)

**Goal:** Make numeric types practical for systems programming: sized integers + usable `عشري` (f64).

#### Features

* \[x] **Sized Integer Types**:

```baa
  ص٨ بايت\_موقع = -١٢٨.        // int8\_t:  -128 to 127
  ص١٦ قصير = -٣٢٠٠٠.          // int16\_t: -32768 to 32767
  ص٣٢ عادي = -٢٠٠٠٠٠٠٠٠٠.     // int32\_t
  ص٦٤ طويل = ٩٠٠٠٠٠٠٠٠٠٠٠٠.   // int64\_t (current صحيح)

  ط٨ بايت = ٢٥٥.               // uint8\_t
  ط١٦ قصير٢ = ٦٥٠٠٠.            // uint16\_t
  ط٣٢ عادي٢ = ٤٠٠٠٠٠٠٠٠٠.       // uint32\_t
  ط٦٤ طويل٢ = ١٨٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠٠. // uint64\_t
  ```

* \[x] **C-like semantics**:

  * integer promotions + usual arithmetic conversions
  * correct signed/unsigned comparisons
  * correct `div/mod` semantics for signed vs unsigned
* \[x] **`عشري` usability**:

  * arithmetic `+ - \* /`
  * comparisons `== != < > <= >=`
  * `اطبع` supports `عشري`
  * ABI lowering on SysV AMD64 + Windows x64 (XMM regs + SysV varargs rules)

#### Implementation Tasks

* \[x] **Lexer**: Tokenize `ص٨`, `ص١٦`, `ص٣٢`, `ص٦٤`, `ط٨`, `ط١٦`, `ط٣٢`, `ط٦٤`.
* \[x] **Type System**: Add size and signedness to integer types.
* \[x] **IR**: Add unsigned variants and allow `f64` ops + verifier updates.
* \[x] **Optimizer**: Ensure integer/float correctness (avoid invalid float folds; unsigned-aware folds).
* \[x] **ISel/Emitter**: Generate correct-sized integer ops and scalar SSE2 for `f64`.
* \[x] **ABI (Windows + SystemV)**: Pass/return `عشري` in XMM registers; handle SysV varargs rules.
* \[x] **Semantic**: Warn on implicit narrowing conversions.
* \[x] **Semantic**: Handle signed/unsigned comparison warnings.

\---

### v0.3.6: Low-Level Operations 🔧 ✅ COMPLETED (2026-02-24)

**Goal:** Add bitwise operations and low-level features needed for systems programming.

#### Features

* \[x] **Bitwise Operators**:

```baa
  صحيح أ = ٥ \& ٣.      // AND: 5 \& 3 = 1
  صحيح ب = ٥ | ٣.      // OR:  5 | 3 = 7
  صحيح ج = ٥ ^ ٣.      // XOR: 5 ^ 3 = 6
  صحيح د = \~٥.         // NOT: \~5 = -6
  صحيح هـ = ١ << ٤.    // Left shift:  1 << 4 = 16
  صحيح و = ١٦ >> ٢.    // Right shift: 16 >> 2 = 4
  ```

* \[x] **Sizeof Operator**:

```baa
  صحيح حجم\_صحيح = حجم(صحيح).    // Returns 8
  صحيح حجم\_حرف = حجم(حرف).      // Returns 1
  صحيح حجم\_مصفوفة = حجم(قائمة). // Returns array size in bytes
  ```

* \[x] **Void Type**:

```baa
  عدم اطبع\_رسالة() {
      اطبع "مرحباً".
      // No return needed
  }
  ```

* \[x] **Escape Sequences**:

```baa
  نص سطر = "سطر١\\س سطر٢".     // Newline (/س)
  نص جدول = "عمود١\\ت عمود٢".  // Tab (/ت)
  نص مسار = "C:\\\\ملفات".     // Backslash
  حرف صفر = '\\٠'.            // Null character
  ```

#### Implementation Tasks

* \[x] **Lexer**: Tokenize `\&`, `|`, `^`, `\~`, `<<`, `>>`.
* \[x] **Parser**: Add bitwise operators with correct precedence.
* \[x] **Parser**: Parse `حجم(type)` and `حجم(expr)` expressions.
* \[x] **Lexer**: Add `عدم` keyword for void type.
* \[x] **Lexer**: Handle escape sequences in string/char literals.
* \[x] **Semantic**: Type check bitwise operations (integers only).
* \[x] **Codegen**: Generate bitwise assembly instructions.
* \[x] **Codegen**: Calculate sizes for `حجم` operator.

### v0.3.6.5: Type Aliases (أسماء الأنواع البديلة) 🏷️ ✅ COMPLETED (2026-02-23)

**Goal:** Create custom type names for readability and abstraction.

#### Features

* \[x] **Simple Type Alias**:

```baa
  نوع معرف = ط٦٤.
  نوع نتيجة = ص٣٢.
  
  معرف رقم\_المستخدم = ١٢٣٤٥.
  نتيجة كود\_خطأ = -١.
  ```

* \[ ] **Pointer Type Alias**:

```baa
  نوع نص\_ثابت = ثابت حرف\*.
  نوع مؤشر\_بايت = ط٨\*.
  ```

  *(Deferred — requires pointer type grammar from v0.3.10.)*

  #### Implementation Tasks

* \[x] **Token/AST plumbing**: Added `TOKEN\_TYPE\_ALIAS`/`NODE\_TYPE\_ALIAS` support in shared structures.
* \[x] **Parser**: Parse `نوع <name> = <type>.` at top-level with C-like declare-before-use semantics.
* \[x] **Semantic**: Resolve aliases during type checking and validate alias targets (`enum/struct/union`).
* \[x] **Symbol Table**: Store type aliases separately and enforce strict name-collision diagnostics.

  \---

  ### v0.3.7: System Improvements 🔧 ✅ COMPLETED (2026-02-25)

  **Goal:** Refine and enhance existing compiler systems.

  #### Focus Areas

* \[x] **Error Messages** – Improve clarity and helpfulness of diagnostic messages.
* \[x] **Code Quality** – Refactor complex functions, improve code organization.
* \[x] **Memory Management** – Fix memory leaks, improve buffer handling.
* \[x] **Performance** – Profile and optimize slow compilation paths.
* \[x] **Documentation** – Update all docs to reflect v0.3.3-0.3.7 changes.
* \[x] **Edge Cases** – Fix known bugs and handle corner cases.

  #### Specific Improvements

* \[x] Improve panic mode recovery in parser.
* \[x] Better handling of UTF-8 edge cases in lexer.
* \[x] Optimize symbol table lookups (consider hash table).
* \[x] Add more comprehensive error recovery.
* \[x] Improve assembly output readability (comments in assembly).

  ### v0.3.7.5: Static Local Variables (متغيرات ساكنة محلية) ✅ COMPLETED (2026-02-25)

  **Goal:** Variables that persist between function calls.

  #### Features

* \[x] **Static Local Syntax**:

  ```baa
  صحيح عداد() {
      ساكن صحيح ع = ٠.  // Initialized once, persists
      ع = ع + ١.
      إرجع ع.
  }
  
  // First call returns 1, second returns 2, etc.
  ```

  #### Implementation Tasks

* \[x] **Token**: Add `TOKEN\_STATIC` for `ساكن` keyword.
* \[x] **Semantic**: Static locals go in .data section, not stack.
* \[x] **Codegen**: Generate unique global label for static locals.
* \[x] **Codegen**: Initialize in .data section.

  \---

  ### v0.3.8: Testing \& Quality Assurance ✅ COMPLETED (2026-02-25)

  **Goal:** Establish robust testing infrastructure and fix accumulated issues.

  #### Test System

* \[x] **Test Framework** – Create automated test runner.

  * Script to compile and run `.baa` test files.
  * Compare actual output vs expected output.
  * Report pass/fail with clear diagnostics.
* \[x] **Test Categories**:

  * \[x] **Lexer Tests** – Token generation, UTF-8 handling, preprocessor.
  * \[x] **Parser Tests** – Syntax validation, error recovery.
  * \[x] **Semantic Tests** – Type checking, scope validation.
  * \[x] **Codegen Tests** – Correct assembly output, execution results.
  * \[x] **Integration Tests** – Full programs with expected output.
* \[x] **Test Coverage**:

  * \[x] All language features (v0.0.1 - v0.3.7).
  * \[x] Edge cases and corner cases.
  * \[x] Error conditions (syntax errors, type mismatches, etc.).
  * \[x] Multi-file compilation scenarios.
  * \[x] Preprocessor directive combinations.

  #### CI/CD Setup

* \[x] **GitHub Actions workflow**:

  ```yaml
  name: Baa CI
  on: \[push, pull\_request]
  jobs:
    build-and-test:
      runs-on: windows-latest
      steps:
        - uses: actions/checkout@v3
        - name: Build Baa
          run: gcc src/\*.c -o baa.exe
        - name: Run Tests
          run: ./run\_tests.bat
  ```

  #### Bug Fixes \& Refinements

* \[x] **Known Issues** – Fix all open bugs from previous versions.
* \[x] **Regression Testing** – Ensure new features don't break old code.
* \[x] **Stress Testing** – Test with large files, deep nesting, many symbols.
* \[x] **Arabic Text Edge Cases** – Test various Arabic Unicode scenarios.

  \---

  ### v0.3.9: Advanced Arrays \& String Operations 📐 ✅ COMPLETED (2026-02-25)

  **Goal:** Complete array and string functionality.

  #### Array Features

* \[x] **Multi-dimensional Arrays** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح مصفوفة\[٣]\[٤].
  مصفوفة\[٠]\[٠] = ١٠.
  مصفوفة\[١]\[٢] = ٢٠.
  ```

* \[x] **Array Length Operator** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح قائمة\[١٠].
  صحيح الطول = حجم(قائمة) / حجم(صحيح).  // Returns 10
  ```

* \[x] **Array Bounds Checking** (Optional runtime mode) ✅ COMPLETED (2026-02-25; flag contract stabilized 2026-05-05):

  * Runtime checks in explicit `-fruntime-checks` lowering mode.
  * Deterministic `exit(1)` path on out-of-bounds access.

  #### String Operations

* \[x] **String Length** ✅ COMPLETED (2026-02-25): `صحيح الطول = طول\_نص(اسم).`
* \[x] **String Concatenation** ✅ COMPLETED (2026-02-25): `نص كامل = دمج\_نص(اسم, " علي").`
* \[x] **String Comparison** ✅ COMPLETED (2026-02-25): `صحيح نتيجة = قارن\_نص(اسم, "محمد").`
* \[x] **String Indexing** (read-only) ✅ COMPLETED (2026-02-25): `حرف أول = اسم\[٠].`
* \[x] **String Copy** ✅ COMPLETED (2026-02-25): `نص نسخة = نسخ\_نص(اسم).`

  #### Implementation

* \[x] **Parser**: Parse multi-dimensional array declarations and access.
* \[x] **Semantic**: Track array dimensions in symbol table.
* \[x] **Codegen**: Calculate offsets for multi-dimensional arrays (row-major order).
* \[x] **Standard Library**: Create `baalib.baa` with string functions.
* \[x] **UTF-8 Aware**: Ensure functions handle multi-byte Arabic characters correctly.

  \---

  ### v0.3.10: Pointers \& References 🎯

  **Goal:** Add pointer types for manual memory management and data structures.

  #### Features

* \[x] **Pointer Type Declaration** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح\* مؤشر.           // Pointer to integer
  حرف\* نص\_مؤشر.         // Pointer to character (C-string)
  هيكل سيارة\* س\_مؤشر.   // Pointer to struct
  ```

* \[x] **Address-of Operator** (`\&`) ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح س = ١٠.
  صحيح\* م = \&س.         // م points to س
  ```

* \[x] **Dereference Operator** (`\*`) ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح قيمة = \*م.       // قيمة = 10
  \*م = ٢٠.              // س now equals 20
  ```

* \[x] **Null Pointer** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح\* م = عدم.        // Null pointer
  إذا (م == عدم) {
      اطبع "مؤشر فارغ".
  }
  ```

* \[x] **Pointer Arithmetic** ✅ COMPLETED (2026-02-25):

  ```baa
  صحيح قائمة\[٥] = {١، ٢، ٣، ٤، ٥}.
  صحيح\* م = \&قائمة\[٠].
  م = م + ١.             // Points to قائمة\[١]
  اطبع \*م.               // Prints 2
  ```

  #### Implementation Tasks

* \[x] **Lexer/Parser integration**: Handle `\*` في سياق النوع/فك الإشارة والضرب و`\&` كـ bitwise/address-of.
* \[x] **Parser**: Parse pointer declarations + `عدم` كمؤشر فارغ + جملة `\*ptr = value.`.
* \[x] **Type System**: Add `TYPE\_POINTER` with base type tracking in AST/symbol metadata.
* \[x] **Semantic**: Validate pointer operations (dereference/address-of/null/pointer compare/arithmetic).
* \[x] **Codegen**: Lower address-of/dereference/pointer assignment and pointer arithmetic safely.

  ### v0.3.10.5: Type Casting (تحويل الأنواع) 🔄 ✅ COMPLETED (2026-02-27)

  **Goal:** Explicit type conversions for low-level programming.

  #### Features

* \[x] **Cast Syntax**:

  ```baa
  صحيح س = ٦٥.
  حرف ح = كـ<حرف>(س).
  ```

* \[x] **Numeric Casts**:

  ```baa
  ص٣٢ صغير = كـ<ص٣٢>(قيمة\_كبيرة).  // Truncation
  ص٦٤ كبير = كـ<ص٦٤>(قيمة\_صغيرة).  // Sign extension
  ط٦٤ بدون = كـ<ط٦٤>(موقع).        // Signed to unsigned
  ```

* \[x] **Pointer Casts**:

  ```baa
  ط٨\* بايتات = كـ<ط٨\*>(مؤشر\_هيكل).  // Reinterpret
  عدم\* عام = كـ<عدم\*>(أي\_مؤشر).     // To void pointer
  هيكل س\* محدد = كـ<هيكل س\*>(عام). // From void pointer
  ```

* \[x] **Pointer Difference (`pointer - pointer`)**:

  ```baa
  صحيح\* أ = \&قائمة\[٠].
  صحيح\* ب = أ + ٣.
  صحيح فرق = ب - أ. // = 3 (فرق عناصر)
  ```

  #### Implementation Tasks

* \[x] **Lexer**: Tokenize `كـ` keyword and `<>` for type parameter.
* \[x] **Parser**: Parse `كـ<type>(expr)` form.
* \[x] **Semantic**: Validate cast safety, warn on dangerous casts.
* \[x] **Codegen**: Generate appropriate conversion instructions.

  ### v0.3.10.6: Function Pointers (مؤشرات الدوال) 📍

  **Goal:** First-class function references for callbacks and dispatch tables.

  #### Features

* \[x] **Function Pointer Type**:

  ```baa
  // Pointer to function taking two صحيح, returning صحيح
  نوع دالة\_ثنائية = دالة(صحيح، صحيح) -> صحيح.
  
  // Or inline
  دالة(صحيح، صحيح) -> صحيح مؤشر\_دالة.
  ```

* \[x] **Assign Function to Pointer**:

  ```baa
  صحيح جمع(صحيح أ، صحيح ب) { إرجع أ + ب. }
  صحيح ضرب(صحيح أ، صحيح ب) { إرجع أ \* ب. }
  
  دالة\_ثنائية عملية = جمع.   // Points to جمع
  عملية = ضرب.               // Now points to ضرب
  ```

* \[x] **Call Through Pointer**:

  ```baa
  صحيح نتيجة = عملية(١٠، ٢٠).  // Calls ضرب(10, 20) = 200
  ```

* \[x] **Function Pointer as Parameter**:

  ```baa
  صحيح طبق(صحيح\[] قائمة، صحيح حجم، دالة\_ثنائية د) {
      صحيح نتيجة = قائمة\[٠].
      لكل (صحيح ع = ١؛ ع < حجم؛ ع++) {
          نتيجة = د(نتيجة، قائمة\[ع]).
      }
      إرجع نتيجة.
  }
  
  // Usage
  صحيح مجموع = طبق(أرقام، ١٠، جمع).
  ```

* \[x] **Null Function Pointer**:

  ```baa
  دالة\_ثنائية فارغ = عدم.
  إذا (فارغ != عدم) {
      فارغ(١، ٢).
  }
  ```

  #### Implementation Tasks

* \[x] **Parser**: Parse function type syntax `دالة(...) -> نوع`.
* \[x] **Type System**: Add function pointer type with signature plumbing.
* \[x] **Semantic**: Type-check function pointer assignments.
* \[x] **Semantic**: Validate call through pointer matches signature.
* \[x] **Codegen**: Generate indirect call instructions.
* \[x] **IR**: Add function pointer type to IR.

  ✅ COMPLETED (2026-02-28)

  \---

  ### v0.3.11: Dynamic Memory 🧠

  **Goal:** Enable heap allocation for dynamic data structures.

  #### Features

* \[ ] **Memory Allocation**:

  ```baa
  // Allocate memory for 10 integers
  صحيح\* قائمة = حجز\_ذاكرة(١٠ \* حجم(صحيح)).
  
  // Allocate memory for a struct
  هيكل سيارة\* س = حجز\_ذاكرة(حجم(هيكل سيارة)).
  ```

* \[ ] **Memory Deallocation**:

  ```baa
  تحرير\_ذاكرة(قائمة).
  تحرير\_ذاكرة(س).
  ```

* \[ ] **Memory Reallocation**:

  ```baa
  // Resize array to 20 integers
  قائمة = إعادة\_حجز(قائمة, ٢٠ \* حجم(صحيح)).
  ```

* \[ ] **Memory Operations**:

  ```baa
  // Copy memory
  نسخ\_ذاكرة(وجهة, مصدر, حجم).
  
  // Set memory to value
  تعيين\_ذاكرة(مؤشر, ٠, حجم).
  ```

  #### Implementation Tasks

* \[x] **Runtime**: ربط مباشر مع libc (`malloc/free/realloc/memcpy/memset`).
* \[x] **Built-in Functions**: إضافة `حجز\_ذاكرة`, `تحرير\_ذاكرة`, `إعادة\_حجز`, `نسخ\_ذاكرة`, `تعيين\_ذاكرة`.
* \[x] **Semantic**: دعم `عدم\*` (void\*) كـ مؤشّر عام (تحويلات ضمنية مع مؤشرات الكائنات).
* \[x] **Codegen**: خفض الدوال إلى استدعاءات C القياسية مع قواعد shadowing.

  ✅ COMPLETED (2026-03-01)

  \---

  ### v0.3.12: File I/O 📁

  **Goal:** Enable reading and writing files for systems programs and future staged bootstrap experiments.

  #### Features

* \[x] **File Opening**:

  ```baa
  عدم\* ملف = فتح\_ملف("بيانات.txt", "قراءة").
  عدم\* ملف\_كتابة = فتح\_ملف("ناتج.txt", "كتابة").
  عدم\* ملف\_إضافة = فتح\_ملف("سجل.txt", "إضافة").
  ```

* \[x] **File Reading**:

  ```baa
  حرف حرف\_واحد = اقرأ\_حرف(ملف).
  نص سطر = اقرأ\_سطر(ملف).
  صحيح بايتات = اقرأ\_ملف(ملف, مخزن, حجم).
  ```

* \[x] **File Writing**:

  ```baa
  اكتب\_حرف(ملف, 'أ').
  اكتب\_سطر(ملف, "مرحباً").
  اكتب\_ملف(ملف, بيانات, حجم).
  ```

* \[x] **File Closing**:

  ```baa
  اغلق\_ملف(ملف).
  ```

* \[x] **File Status**:

  ```baa
  منطقي انتهى = نهاية\_ملف(ملف).
  صحيح موقع = موقع\_ملف(ملف).
  اذهب\_لموقع(ملف, ٠).
  ```

  #### Implementation Tasks

* \[x] **Runtime**: Wrap C stdio functions (fopen, fread, fwrite, fclose, fgetc, fputc, fputs, feof, ftello/fseeko).
* \[x] **Built-in Functions**: Add file operation functions using `عدم\*` as an opaque `FILE\*` handle.
* \[x] **Error Handling**: Return error codes for failed operations (`فتح\_ملف` يعيد `عدم`، و `اكتب\_سطر/اكتب\_حرف` تعيد -1 عند الفشل).
* \[x] **Codegen**: Generate direct libc stdio calls with shadowing rules.

  ✅ COMPLETED (2026-03-02)

  ### v0.3.12.5: Command Line Arguments (معاملات سطر الأوامر) 🖥️

  **Goal:** Access program arguments for general programs and future staged bootstrap experiments.

  #### Features

* \[x] **Main with Arguments**:

  ```baa
  صحيح الرئيسية(صحيح عدد، نص\[] معاملات) {
      // عدد = argument count (like argc)
      // معاملات = argument array (like argv)
      
      إذا (عدد < ٢) {
          اطبع "الاستخدام: برنامج <ملف>".
          إرجع ١.
      }
      
      نص اسم\_البرنامج = معاملات\[٠].
      نص ملف\_إدخال = معاملات\[١].
      
      اطبع "تجميع: ".
      اطبع ملف\_إدخال.
      
      إرجع ٠.
  }
  ```

  #### Implementation Tasks

* \[x] **Parser**: Allow parameters in `الرئيسية` function.
* \[x] **Semantic**: Validate main signature matches expected pattern.
* \[x] **Codegen**: Link with proper C runtime entry point.
* \[x] **Codegen (Opt-in)**: `--startup=custom` — custom entrypoint symbol (`\_\_baa\_start`) while keeping CRT/libc init.
* \[ ] **Codegen (Full Independence)**: True `\_start` without CRT/libc (deferred to Phase 8).

  ✅ COMPLETED (2026-03-02)

  \---

  ## 📚 Phase 4: Standard Library \& Polish (v0.4.x)

  *Goal: Make Baa production-ready with a comprehensive standard library.*

  ### v0.4.0: Formatted Output \& Input 🖨️

  **Goal:** Professional I/O capabilities.

* \[x] **Formatted Output**:

  ```baa
  اطبع\_منسق("الاسم: %ن، العمر: %ص\\س", اسم, عمر).
  ```

* \[x] **String Formatting**:

  ```baa
  نص رسالة = نسق("النتيجة: %ص", قيمة).
  حرر\_نص(رسالة).
  ```

* \[x] **Formatted Input**:

  ```baa
  نص سطر = اقرأ\_سطر().
  صحيح رقم = اقرأ\_رقم().

  صحيح أ = ٠.
  عشري ب = ٠.
  نص س = عدم.

  // ملاحظة: في الإدخال، %ن يتطلب عرضاً رقمياً (مثلاً %10ن).
  صحيح مقروء = اقرأ\_منسق("%ص %ع %10ن", \&أ, \&ب, \&س).
  إذا (مقروء == 3) { حرر\_نص(س). }
  ```

  ✅ COMPLETED (2026-03-02)

  ### v0.4.0.5: Variadic Functions (دوال متغيرة المعاملات) ✅

  **Goal:** Functions accepting variable number of arguments.

  #### Features

* \[x] **Variadic Declaration**:

  ```baa
  عدم اطبع\_منسق(نص تنسيق، ...) {
      // Implementation using variadic access
  }
  ```

* \[x] **Variadic Access Macros/Functions**:

  ```baa
  عدم اطبع\_أرقام(صحيح عدد، ...) {
      قائمة\_معاملات معاملات.
      بدء\_معاملات(معاملات، عدد).
      
      لكل (صحيح ع = ٠؛ ع < عدد؛ ع++) {
          صحيح قيمة = معامل\_تالي(معاملات، صحيح).
          اطبع قيمة.
      }
      
      نهاية\_معاملات(معاملات).
  }
  
  // Usage
  اطبع\_أرقام(٣، ١٠، ٢٠، ٣٠).
  ```

  #### Implementation Tasks

* \[x] **Lexer**: Tokenize `...` (ellipsis).
* \[x] **Parser**: Parse variadic function declarations (including `دالة(...)->...` signatures).
* \[x] **Type System**: Handle variadic function types and variadic call arity/type checks.
* \[x] **Codegen (Windows x64)**: Variadic calls lowered عبر وسيط داخلي موحد (`\_\_baa\_va\_base`) متوافق مع مسار الباك-إند الحالي.
* \[x] **Codegen (Linux x64)**: نفس وسيط المعاملات الداخلي لضمان سلوك موحد على `x86\_64-linux`.
* \[x] **Built-ins**: Implement `بدء\_معاملات`, `معامل\_تالي`, `نهاية\_معاملات`.

  ✅ COMPLETED (2026-03-02)

  ### v0.4.0.6: Inline Assembly (المجمع المدمج) ✅

  **Goal:** Embed assembly code for low-level operations.

  #### Features

* \[x] **Basic Inline Assembly**:

  ```baa
  مجمع {
      "nop"
  }
  ```

* \[x] **With Outputs and Inputs**:

  ```baa
  صحيح قراءة\_عداد() {
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

* \[x] **Token**: Add `TOKEN\_ASM` for `مجمع` keyword.
* \[x] **Parser**: Parse inline assembly blocks.
* \[x] **Codegen**: Emit assembly directly with proper constraints.
* \[x] **Semantic**: Validate constraint syntax.

  ✅ COMPLETED (2026-03-02)

  #### Deferred to v3.0

* Full constraint support (memory, register classes)
* Clobber lists

  ```

### v0.4.1: Standard Library (مكتبة باء) 📚

  ### v0.4.1: Standard Library (مكتبة باء) 📚

* \[x] **Math Module** — `جذر\_تربيعي()`, `أس()`, `مطلق()`, `عشوائي()`.
* \[x] **String Module** — Complete string manipulation.
* \[x] **IO Module** — File and console operations.
* \[x] **System Module** — Environment variables, command execution.
* \[x] **Time Module** — Date/time operations.

  ✅ COMPLETED (2026-03-02)

  ### v0.4.2: Floating Point Extensions ✅

  **Goal:** Extend floating point beyond the core `عشري` (completed in v0.3.5.5).

* \[x] **Math functions** — `جذر\_تربيعي()`, `أس()`, `جيب()`, `جيب\_تمام()`, `ظل()`.
* \[x] **Formatting** — better float printing options (precision + scientific `%أ`).
* \[x] **Additional float types** — `عشري٣٢` keyword (current lowering alias to `عشري`/f64 in v0.4.2).

  ✅ COMPLETED (2026-03-02)

  ### v0.4.3: Error Handling 🛡️

  **Goal:** Graceful error management.

* \[x] **Assertions**:

  ```baa
  تأكد(س > ٠, "س يجب أن يكون موجباً").
  ```

* \[x] **Error Codes** – Standardized error return values.
* \[x] **Panic Function** – `توقف\_فوري("رسالة خطأ")`.

  ✅ COMPLETED (2026-03-02)

  ### v0.4.4: Final Polish 🎨

* \[x] **Complete Documentation** — All features documented.
* \[x] **Tutorial Series** — Step-by-step learning materials.
* \[x] **Example Programs** — Comprehensive example collection.
* \[x] **Performance Optimization** — Profile and optimize compiler.

  ✅ COMPLETED (2026-03-02)

  ## 🧱 Phase 4.5: Reference Compiler Stabilization (v0.5.x)

  *Goal: Keep the C implementation as the official reference compiler, stabilize the language/toolchain contracts, and defer self-hosting to a future staged effort after the language has matured.*

  ### v0.5.0: File \& Component Organization 🗂️

* \[x] **Define canonical component boundaries** — Frontend / Middle-End / Backend / Driver / Support.
* \[x] **Set module-size policy** — target `<= 700` lines/file, hard cap `1000` lines for hand-written C modules.
* \[x] **Split oversized modules first** — `analysis.c`, `emit.c`, `ir.c`, `ir\_lower.c`, `ir\_text.c`, `isel.c`, `lexer.c`, `parser.c`, `regalloc.c`, and `ir\_verify\_ir.c` are now under the hard cap via companion implementation splits.
* \[x] **Restructure source layout safely** — component directories now exist under `src/`, and the build now targets those files directly.
* \[x] **Add local module facades** — `frontend\_internal.h`, `middleend\_internal.h`, `backend\_internal.h`, `driver\_internal.h`, and `support\_internal.h` now define component-local include surfaces.
* \[x] **Update build graph** — `CMakeLists.txt` remains explicit/deterministic and the Windows build uses C-only include propagation while header wrappers remain transitional.
* \[x] **Add size-regression guard** — `scripts/check\_module\_sizes.py` enforces warn `700` / error `1000` and runs in `qa\_run.py` + CI before full QA.
* \[x] **Document ownership map** — `docs/COMPONENT\_OWNERSHIP.md` defines module responsibilities + dependency rules.

  ✅ COMPLETED (2026-03-06)

  Partial status update (2026-03-06):

* policy/guard/documentation are in place for `v0.5.0`,
* all remaining hard-cap hotspots were split below the `1000`-line limit,
* `scripts/module\_size\_allowlist.txt` is now empty,
* source files were reorganized under `src/frontend`, `src/middleend`, `src/backend`, `src/driver`, and `src/support`,
* `CMakeLists.txt` now builds directly from component sources, while root-level compatibility is limited to selected header wrappers during header migration.

  ### v0.5.1: Language + ABI Freeze 🔒

* \[x] **Freeze grammar surface** — avoid syntax churn before bootstrap.
* \[x] **Freeze stdlib signatures** — lock callable contracts used by compiler-in-Baa.
* \[x] **Freeze target ABI contracts** — Windows x64 + SystemV AMD64 invariants.
* \[x] **Freeze IR invariants** — verifier-enforced guarantees documented.

  ✅ COMPLETED (2026-04-28)

  ### v0.5.2: Module \& Multi-File Hardening 📦

* \[x] **Deterministic include/import resolution** — include resolution now canonicalizes successful paths before they become active lexer filenames, so equivalent relative spellings collapse to a single resolved path.
* \[x] **Cycle diagnostics** — `#تضمين` cycles are now rejected early with Arabic diagnostics that print the include chain instead of recursing until depth exhaustion.
* \[x] **Symbol visibility rules** — top-level functions keep external linkage by default, top-level `ساكن` globals/arrays keep file-local internal linkage, and multi-file QA now locks this contract with dedicated smoke coverage.
* \[x] **Header/API hygiene** — `baa.h` is now a compatibility umbrella only; shared declarations were split into component-owned public headers under `src/frontend/` and `src/support/`, diagnostics no longer depend on frontend lexer headers, and the middle-end now consumes a small shared target contract instead of touching backend target layout directly.

  ### v0.5.3: Build System Maturity ⚙️

* \[x] **Incremental compilation model** — avoid full rebuilds on small edits.
* \[x] **Dependency tracking** — reliable invalidation for headers/includes.
* \[x] **Reproducible outputs** — stable artifacts for same inputs/toolchain.
* \[x] **Build profile presets** — dev/debug/release/verify presets.

  ✅ COMPLETED (2026-04-28)

  ### v0.5.4: Diagnostics \& Recovery Quality 🩺

* \[x] **Improve span accuracy** — tighter line/column ranges.
* \[x] **Add actionable fix hints** — Arabic-first suggestions for common errors.
* \[x] **Strengthen panic recovery** — fewer cascading diagnostics.
* \[x] **Negative test expansion** — enforce diagnostic contracts.

  ✅ COMPLETED (2026-05-01)

  ### v0.5.5: Runtime Safety Layer 🛡️

* \[x] **Assertion runtime contract** — stable behavior in debug/release modes.
* \[x] **Panic/error-code policy** — consistent fatal/non-fatal paths.
* \[x] **Safety toggles** — explicit compile-time/runtime control flags.
* \[x] **Document failure semantics** — deterministic exit/status behavior.

  ✅ COMPLETED (2026-05-05)

  ### v0.5.6: Determinism \& QA Gates ✅

* \[x] **Cross-target parity suite** — Linux/Windows behavior consistency.
* \[x] **Fuzz + stress expansion** — parser/semantic/IR robustness.
* \[x] **IR/SSA regression locking** — snapshots + verifier gating.
* \[x] **Release gate checklist** — mandatory pass criteria before Phase 5.

  ✅ COMPLETED (2026-05-09)

  ### Future Baa0 Bootstrap Subset Definition 📐 — DEFERRED UNTIL AFTER v0.9

  These items are future planning work, not Phase 4.5 release gates. They must not add a
  Baa-built compiler dependency to the mainline build.

* \[ ] **Define minimal future subset** — features that could later support compiler slices in Baa without depending on unstable language behavior.
* \[ ] **Ban unstable features in Baa0** — keep any future bootstrap surface conservative and deterministic.
* \[ ] **Publish Baa0 compliance suite** — tests for subset guarantees, but not a production migration gate yet.
* \[ ] **Document migration policy** — C remains the reference compiler; Baa rewrites stay experimental until post-v0.9 staged gates.

  ### v0.5.8: Reference Compiler Reset 🧭

  **Goal:** Stop broad compiler migration work and re-anchor the project around a stable C reference compiler.

* \[x] **Declare the C compiler as the reference implementation** — `docs/BOOTSTRAP_CONTRACT.md` defines the root CMake target as the official implementation.
* \[x] **Move C→Baa migration work to an experimental branch** — `origin/moving-to-baa` preserves the Baa lexer/parser and bootstrap wiring outside the release path.
* \[x] **Remove misleading bootstrap assumptions from the main build** — the C-only build has no bootstrap input, and `scripts/check\_reference\_compiler\_policy.py` prevents regressions.
* \[x] **Audit migration artifacts** — `docs/MIGRATION\_ARTIFACT\_AUDIT.md` records branch contents and the keep/defer/re-evaluate disposition.
* \[x] **Write self-hosting policy note** — `docs/BOOTSTRAP_CONTRACT.md` keeps future work staged, parity-tested, rollback-ready, and outside the v0.9 release path.
* \[x] **Update roadmap language** — current milestones use future staged bootstrap readiness rather than active rewrite framing.

  ✅ COMPLETED (2026-06-29)

  ### v0.5.9: Reference Compiler Release Candidate ✅

  **Goal:** Produce a clean, reproducible, cross-platform baseline before new v0.6.x language work.

* \[ ] **Windows full QA signoff** — `quick/full/stress/release` gates pass on Windows.
* \[ ] **Linux full QA signoff** — `quick/full/stress/release` gates pass on Linux.
* \[ ] **Reproducible build check** — stable version/date/manifest outputs for identical inputs.
* \[ ] **Determinism gate** — stable IR, optimized IR, assembly, diagnostics, and manifests.
* \[x] **Known limitations page** — `docs/KNOWN\_LIMITATIONS.md` lists unsupported targets, language/type restrictions, safety boundaries, and draft-only tooling surfaces.
* \[ ] **Release branch discipline** — only fixes, tests, and documentation polish after RC cut.

  Reproducibility coverage update (2026-06-29): the release gate now compares repeated
  `--version` output (including the configured build date) and negative diagnostic text/exit
  status in addition to IR, assembly, manifests, verifier behavior, and snapshots. Final
  Windows/Linux RC receipts remain pending.

  RC gate reliability update (2026-06-29): compiler discovery now emits a structured
  `compiler-preflight` result, including missing/invalid compiler paths in summary JSON instead
  of aborting before a release receipt is written.

  #### Phase 4.5 Exit Criteria

* \[x] **C-reference build is simple** — clean checkout builds from C/RC inputs only, with the policy checked in every QA mode.
* \[ ] **Cross-target QA green** — `quick/full` pass on both `x86\_64-windows` and `x86\_64-linux`.
* \[x] **Determinism checks green** — stable IR text and stable diagnostics for identical inputs.
* \[x] **File-size governance active** — CI guard for module-size budget is enforced.
* \[ ] **Contracts frozen and published** — grammar/ABI/IR/stdlib docs tagged and versioned.
* \[x] **Future bootstrap policy published** — self-hosting is explicitly deferred until after v0.9 stabilization.

  #### Phase 4.5 Required Artifacts

* \[x] `docs/COMPONENT\_OWNERSHIP.md` — boundaries + owners + allowed dependencies.
* \[x] `docs/BOOTSTRAP\_CONTRACT.md` — frozen ABI/IR/language requirements and future bootstrap policy.
* \[x] `docs/MIGRATION\_ARTIFACT\_AUDIT.md` — experimental-branch inventory and mainline disposition.

  Future optional artifacts (not Phase 4.5 gates):

* \[ ] `docs/BAA0\_SPEC.md` — post-v0.9 bootstrap subset definition and exclusions.
* \[ ] `tests/bootstrap/` — post-v0.9 parity corpus for staged migration experiments.

  \---

  ## 🧭 Ecosystem Ownership Boundaries

  *Goal: Keep the Baa repository focused on the compiler, language, ABI, diagnostics, stdlib contracts, and release-quality gates while sibling projects own their own product areas.*

* **Takween** owns the Arabic-first project build workflow for Baa projects (`تكوين تهيئة/بناء/تشغيل/تنظيف`). Baa should expose stable compiler flags, manifest formats, include rules, and diagnostics for Takween to consume; Baa should not duplicate Takween as an internal `baa build` system.
* **Qalam-IDE** owns the editor/IDE experience for Arabic-syntax systems languages starting with Baa. Baa should expose stable parser/check/diagnostic surfaces that Qalam can call; Baa should not duplicate Qalam as an internal IDE or editor extension roadmap.
* **Baa** owns the compiler core, language specification, standard-library contracts, runtime checks, diagnostics, target ABI behavior, tests, and release artifacts.

  \---

  ## 🧱 Phase 6: Language Usability \& Safety (v0.6.x)

  *Goal: Make Baa more practical as an Arabic-first systems language without expanding into external build-system or IDE ownership.*

  ### v0.6.0: Systems Language Completeness I 🧱

* \[ ] **`خارجي` declarations** — stable external function/global declarations for interop and headers.
* \[ ] **Global declaration vs definition rules** — header-safe shared globals without accidental duplicate definitions.
* \[ ] **Struct field initialization** — basic named-field initialization for `هيكل` values.
* \[ ] **Aggregate assignment policy** — either support selected safe cases or reject with precise diagnostics.
* \[ ] **Const pointer rules** — clarify pointer-to-const vs const-pointer semantics.
* \[ ] **Better null-pointer diagnostics** — warn or error when misuse is statically obvious.

  ### v0.6.1: Arabic Diagnostics II 🩺

* \[ ] **Diagnostic codes** — stable identifiers such as `B0001` / `B0120` for errors and warnings.
* \[ ] **Multi-line spans** — underline full expressions/statements when one-line spans are insufficient.
* \[ ] **Fix-it hints** — suggest missing `.`, wrong `؛`, wrong type, or missing include where reliable.
* \[ ] **Diagnostic categories** — syntax, type, include, backend, runtime, warning.
* \[ ] **`--explain <CODE>`** — detailed Arabic explanation for each stable diagnostic code.
* \[ ] **Negative diagnostic expansion** — lock diagnostic count, hints, and cascade behavior.

  ### v0.6.2: Standard Library Core 📦

* \[ ] **Dynamic array/vector API** — push/pop/length/free helpers for common element patterns.
* \[ ] **Byte buffer API** — explicit buffer type for compiler/tooling-style programs.
* \[ ] **Path API** — join, dirname, basename, extension, normalize.
* \[ ] **String builder** — efficient incremental text construction with explicit ownership.
* \[ ] **Result/error helpers** — consistent success/failure convention across stdlib modules.
* \[ ] **Ownership documentation** — every allocating stdlib function states who frees the result.

  ### v0.6.3: Runtime Safety Guards 🛡️

* \[ ] **Null pointer checks** — optional trap before dereference where the compiler can emit one.
* \[ ] **Division-by-zero checks** — optional runtime guard for integer division and modulo.
* \[ ] **Shift-width checks** — optional guard for invalid runtime shift counts.
* \[ ] **Expanded bounds checks** — arrays and string indexing where shape information is available.
* \[ ] **Readable panic format** — file, line, function, and Arabic message.
* \[ ] **Selective safety flags** — `-fruntime-checks=<kind>` style suboptions.

  ### v0.6.4: UTF-8/Text Correctness 📝

* \[ ] **Clarify `حرف` representation** — document byte/codepoint/grapheme policy precisely.
* \[ ] **String indexing policy** — define exactly what `نص\[index]` returns and what it does not promise.
* \[ ] **UTF-8 validation tests** — identifiers, strings, chars, includes, diagnostics.
* \[ ] **Arabic numeral normalization tests** — source tokens, diagnostics, and IR output.
* \[ ] **Text stdlib helpers** — safe length/copy/compare behavior documented and tested.
* \[ ] **Known Unicode limitations** — no hidden claim of full grapheme-aware text processing unless implemented.

  ### v0.6.5: Documentation Lock 📚

* \[ ] **Update Baa Book to current scope** — remove stale version references and align examples with current behavior.
* \[ ] **Verified example suite** — every public example compiles in CI.
* \[ ] **Exercises with expected output** — beginner, intermediate, and systems-level exercises.
* \[ ] **Terminology glossary** — one preferred Arabic term per core compiler/language concept.
* \[ ] **Native Arabic technical review** — language quality pass by Arabic-speaking engineers.
* \[ ] **Docs version sync gate** — reject release when docs show stale version headers.

  \---

  ## 🧪 Phase 7: Compiler Testing \& Integration Surfaces (v0.7.x)

  *Goal: Strengthen Baa’s compiler-facing surfaces for Takween, Qalam-IDE, and future tooling without owning those external products inside this repository.*

&#x09;#### Required integration artifacts

* \[ ] `docs/ECOSYSTEM\\\_BOUNDARIES.md` — ownership rules between Baa, Takween, Qalam-IDE, and PyramidOS.
* \[ ] `docs/COMPATIBILITY\\\_MATRIX.md` — version compatibility table for compiler/tooling contracts.
* \[ ] `docs/TOOLING\\\_CONTRACTS.md` — stable CLI, manifest, exit-code, and machine-readable output contracts.
* \[ ] `docs/DIAGNOSTICS\\\_JSON\\\_SCHEMA.md` — stable diagnostics JSON schema for Takween/Qalam.
* \[ ] `docs/TARGET\\\_SPECIFICATION.md` — target descriptor model for hosted and future freestanding targets.
* \[ ] `docs/CONFORMANCE\\\_SUITE.md` — language, ABI, stdlib, diagnostics, and target conformance plan.
* \[ ] `docs/SDK\\\_RELEASE\\\_PLAN.md` — future Baa SDK bundle/versioning plan.
* \[ ] `targets/x86\\\_64-linux.json` and `targets/x86\\\_64-windows.json` — first hosted target descriptors.

  \[ ] `targets/i386-elf.experimental.json` and `targets/i386-pyramidos.experimental.json` — planning descriptors only, not supported targets yet.

  ### v0.7.0: Takween Integration Contract 🏗️

* \[ ] **Stable compiler invocation contract** — document flags Takween may rely on for build/run/clean workflows.
* \[ ] **Manifest compatibility** — guarantee deterministic fields Takween can consume from `--emit-build-manifest`.
* \[ ] **Include/dependency contract** — document canonical dependency output and invalidation expectations.
* \[ ] **Exit-code contract** — stable compiler exit statuses for build tools.
* \[ ] **Machine-readable diagnostics plan** — define JSON diagnostics shape for future Takween/Qalam consumption.
* \[ ] **No internal Baa project build system** — Takween remains the owner of Arabic-first project build UX.

  ### v0.7.1: Module and Visibility Cleanup 🧩

* \[ ] **Header/source convention** — formalize `.baahd` vs `.baa` usage.
* \[ ] **Visibility modifiers** — public/internal rules for functions and globals.
* \[ ] **Include-cycle diagnostics** — improve chain output and hints.
* \[ ] **One-definition checks** — better duplicate symbol diagnostics.
* \[ ] **Header self-check mode** — check declarations without emitting code.
* \[ ] **Migration guide** — from raw multi-file builds to Takween-managed builds.

  ### v0.7.2: Qalam-IDE Integration Contract ✍️

* \[ ] **Fast check mode** — parse/type-check without assembly/linking for editor feedback.
* \[ ] **Machine-readable diagnostics** — JSON diagnostics with file, line, column, span, code, severity, and hint.
* \[ ] **Token dump mode** — stable token stream for syntax-highlighting/debugging integration.
* \[ ] **Symbol outline mode** — functions, globals, structs, enums, and type aliases for IDE navigation.
* \[ ] **Completion metadata export** — keywords, builtins, stdlib symbols, and snippets in a stable format.
* \[ ] **No internal Baa IDE roadmap** — Qalam-IDE remains the owner of editor UI/UX.

  ### v0.7.3: Compiler Testing II 🧪

* \[ ] **Coverage reporting** — CI coverage for C compiler core.
* \[ ] **Fuzz targets** — lexer, parser, IR reader, include resolver.
* \[ ] **Differential tests** — compare `-O0` vs `-O2` runtime output.
* \[ ] **Crash minimization workflow** — reduce failing fuzz cases into committed regressions.
* \[ ] **Backend stress tests** — stack args, calls, structs, arrays, floats, and pointer-heavy programs.
* \[ ] **Release dashboard** — summarize pass/fail, coverage, fuzz corpus size, and determinism checks.

  \---

  ## 🎯 Phase 8: Backend, Optimizer, and Performance Reliability (v0.8.x)

  *Goal: Improve generated-code trustworthiness and prevent silent regressions before the v0.9 beta freeze.*

  ### v0.8.0: Backend Correctness Hardening 🎯

* \[ ] **ABI test matrix** — Windows x64 and SysV calls, returns, varargs, and stack args.
* \[ ] **Struct/union layout tests** — size, alignment, and field-offset expectations.
* \[ ] **Floating-point ABI tests** — parameters, returns, varargs edge cases.
* \[ ] **Stack alignment verifier** — static backend checks before emission.
* \[ ] **Assembly golden tests** — stable snippets for sensitive cases.
* \[ ] **Cross-target `-S` release gate** — both Windows and Linux assembly output.

  ### v0.8.1: Optimizer Reliability ⚙️

* \[ ] **Pass pipeline documentation** — exact O0/O1/O2 pass order.
* \[ ] **Per-pass verifier gate** — mandatory in CI debug mode.
* \[ ] **Optimization remarks** — report applied and missed optimizations.
* \[ ] **Alias-analysis baseline** — conservative, documented, test-backed.
* \[ ] **Optimization stress corpus** — loops, branches, calls, pointers, and aggregate-heavy cases.
* \[ ] **No unsafe optimization without verifier coverage** — correctness first.

  ### v0.8.2: Performance Budget 📊

* \[ ] **Benchmark baselines** — compile time, runtime, and memory.
* \[ ] **Regression thresholds** — fail CI on large slowdowns.
* \[ ] **Phase timing JSON** — machine-readable `--time-phases` output.
* \[ ] **Memory budget tracking** — IR arena, parser allocations, backend allocations.
* \[ ] **Benchmark documentation** — exact local reproduction commands.
* \[ ] **Performance changelog entries** — record meaningful wins and regressions.

  \---

  ## 🚦 Phase 9: Stable Beta and Future Bootstrap Plan (v0.9.x)

  *Goal: Freeze a serious pre-1.0 baseline and define a future staged self-hosting plan without executing a production compiler rewrite in this roadmap window.*

  ### v0.9.0: Stable Beta Freeze 🧊

* \[ ] **Language freeze candidate** — syntax and semantics locked for 1.0 review.
* \[ ] **Stdlib freeze candidate** — ownership, errors, memory, text, file, and path APIs documented.
* \[ ] **ABI freeze candidate** — Windows/Linux behavior documented and tested.
* \[ ] **Diagnostics freeze candidate** — diagnostic IDs, spans, hints, and `--explain` behavior stable.
* \[ ] **External tooling contracts freeze** — Takween/Qalam-facing outputs remain stable through 1.0 review.
* \[ ] **Full release QA** — Windows + Linux quick/full/stress/release gates.
* \[ ] **Book + spec sync** — all docs updated to v0.9.0.
* \[ ] **Stage-0 bootstrap plan only** — define future self-hosting stages, rollback, and parity gates; do not make self-hosting the mainline compiler yet.
* \[ ] **Freestanding OS-dev profile plan only** — define the future `i386-elf`/`i386-pyramidos` path for PyramidOS experiments; do not move PyramidOS kernel core to Baa yet.
* \[ ] \*\*Compatibility matrix freeze\*\* — Baa/Takween/Qalam contract versions are listed and stable through 1.0 review.
* \[ ] \*\*Conformance suite v1 seed\*\* — syntax, semantics, diagnostics, stdlib, ABI, and target tests define the stable-beta behavior.

  \---

  ## 🧬 Future OS Development Profile: Freestanding / PyramidOS Target (post-v0.9)

  *Goal: Make Baa capable of compiling small freestanding objects for PyramidOS and OS-development experiments without pretending that the PyramidOS kernel can immediately move from C/Assembly to Baa.*

  ### Scope Decision

* \[ ] **Baa v0.9 is not an OS-dev-ready release by itself** — v0.9 freezes the hosted language/toolchain baseline and only plans the freestanding path.
* \[ ] **PyramidOS kernel core remains C + Assembly** — bootloader, entry code, GDT/IDT/ISR, PMM/VMM, ATA, and panic paths stay in the current reference implementation until explicit gates pass.
* \[ ] **Baa enters PyramidOS gradually** — host tools first, then tiny mixed-link smoke objects, then userland, then carefully selected kernel leaf helpers.
* \[ ] **No hidden hosted dependencies** — freestanding Baa code must not silently call libc/CRT, file I/O, heap allocation, formatted I/O, or startup helpers.
* \[ ] **Object/linker-script compatibility first** — Baa must fit the PyramidOS linker script, section layout, symbols, and QEMU boot gate before any migration claim.
* \[ ] **Rollback is mandatory** — every Baa-in-PyramidOS experiment must be removable without breaking the C/Assembly kernel path.

  ### v0.10.0: Freestanding Profile Foundation 🧱

  **Goal:** Add a hosted-vs-freestanding compiler mode split.

* \[ ] **`--freestanding` mode** — compile with no hosted OS/runtime assumptions.
* \[ ] **`--no-stdlib` mode** — reject or disable stdlib calls unless explicitly provided by the target.
* \[ ] **`--kernel` profile alias** — convenience mode for freestanding, no-stdlib, object-only defaults.
* \[ ] **Disable hosted builtins** — no implicit lowering to libc/CRT for print, read, files, time, environment, or allocation.
* \[ ] **Explicit runtime contract diagnostics** — Arabic errors when hosted features are used in freestanding mode.
* \[ ] **Custom entry symbol support** — allow kernel/userland entry symbols without assuming `الرئيسية` maps to hosted `main`.
* \[ ] **Object-only release gate** — freestanding mode initially produces assembly/object outputs only, not hosted executables.

  ### v0.10.1: `i386-elf` / `i386-pyramidos` Target 🎯

  **Goal:** Add the target shape needed by the current 32-bit PyramidOS kernel.

* \[ ] **`--target=i386-elf` baseline** — 32-bit x86 freestanding object/assembly output.
* \[ ] **Optional `--target=i386-pyramidos` alias** — PyramidOS-specific ABI/layout defaults once proven useful.
* \[ ] **32-bit data layout** — pointer size, integer sizes, stack alignment, and aggregate layout documented and verified.
* \[ ] **cdecl-style call ABI** — stack arguments, return values, caller/callee-saved registers, and name decoration policy.
* \[ ] **32-bit instruction selection** — lower Baa IR to i386-compatible machine instructions.
* \[ ] **32-bit register allocation** — EAX/EBX/ECX/EDX/ESI/EDI/EBP/ESP constraints and spilling.
* \[ ] **GAS/NASM compatibility decision** — choose canonical emitted assembly syntax for PyramidOS builds.
* \[ ] **Cross-toolchain integration** — support `i686-elf`/`i386-elf` assembler flow where available, without requiring hosted GCC linkage.

  ### v0.10.2: Kernel-Safety Language Features 🛡️

  **Goal:** Add the minimum low-level controls required for kernel code.

* \[ ] **Volatile memory access** — explicit volatile load/store for MMIO and hardware registers.
* \[ ] **Packed structs** — stable layout controls for descriptor tables and hardware data structures.
* \[ ] **Alignment attributes** — align variables/types/sections for page tables, stacks, and hardware structures.
* \[ ] **Custom section attributes** — place symbols in `.text`, `.rodata`, `.data`, `.bss`, boot/kernel-specific sections.
* \[ ] **Compile-time layout assertions** — verify `حجم`, alignment, and field offsets for ABI-critical structures.
* \[ ] **Inline assembly hardening** — tested patterns for `cli`, `sti`, `hlt`, `in`, `out`, `lgdt`, `lidt`, and control-register access.
* \[ ] **Interrupt-wrapper contract** — keep ISR/IRQ wrappers in Assembly first; only add Baa interrupt ABI support after wrapper tests pass.

  ### v0.10.3: PyramidOS Mixed-Link Smoke Gate 🧪

  **Goal:** Prove one tiny Baa object can link into and boot inside PyramidOS without touching critical paths.

* \[ ] **Leaf Baa function object** — compile a tiny non-critical Baa function to an object file.
* \[ ] **C-callable ABI test** — call the Baa function from C diagnostic code and verify arguments/return value.
* \[ ] **Linker script compatibility** — place Baa sections correctly in the PyramidOS image.
* \[ ] **Symbol map verification** — confirm symbols appear at expected addresses in `kernel.map`.
* \[ ] **QEMU boot smoke** — boot PyramidOS and run a diagnostic command that exercises the Baa function.
* \[ ] **No critical subsystem migration** — bootloader, GDT/IDT/ISR, PMM/VMM, heap, ATA, and VFS remain C/Assembly in this milestone.
* \[ ] **Rollback test** — removing the Baa object restores the exact C/Assembly-only build behavior.

  ### v0.10.4: PyramidOS Userland Foundation 👤

  **Goal:** Prefer Baa first for PyramidOS userland once Ring 3/syscalls exist.

* \[ ] **Userland target profile** — compile small Ring 3 Baa programs for PyramidOS when the OS ABI exists.
* \[ ] **Syscall wrapper declarations** — `.baahd` headers for stable PyramidOS syscalls.
* \[ ] **Minimal no-stdlib runtime** — startup + syscall exit/write path only.
* \[ ] **Userland hello program** — Baa program prints via PyramidOS syscall, not host libc.
* \[ ] **Userland ABI tests** — argument passing, return codes, stack alignment, and failure behavior.
* \[ ] **Separation policy** — userland Baa can advance faster than kernel Baa because it is less boot-critical.

  ### v0.10.5: Kernel Leaf-Helper Pilot 🌱

  **Goal:** Try Baa inside the kernel only where failure risk is low and rollback is trivial.

* \[ ] **Allowed migration list** — small pure helpers only: string length, small formatting helpers, table lookup, diagnostics formatting.
* \[ ] **Forbidden migration list** — no bootloader, entry, GDT/IDT/ISR, PMM/VMM, heap allocator, ATA/PIO, scheduler, or panic core.
* \[ ] **C reference parity tests** — compare Baa helper behavior against existing C helpers.
* \[ ] **No heap by default** — kernel Baa code must not allocate unless PyramidOS exposes an explicit allocator contract.
* \[ ] **No implicit panics** — freestanding failure behavior must be explicit and kernel-safe.
* \[ ] **QEMU regression gate** — every helper migration must pass boot + shell + diagnostic smoke tests.
* \[ ] **Rollback branch discipline** — each helper migration lands as an isolated, revertable change.

  #### OS Development Profile Exit Criteria

* \[ ] **Freestanding mode is real** — no hidden libc/CRT dependency for accepted freestanding programs.
* \[ ] **i386 target is proven** — assembly/object output matches PyramidOS target expectations.
* \[ ] **Layout controls are tested** — packed/aligned/section features are verified with compile-time and binary checks.
* \[ ] **Mixed-link gate passes** — PyramidOS boots in QEMU with one C-called Baa object.
* \[ ] **Userland path is defined** — Baa has a cleaner first real OS role outside the kernel core.
* \[ ] **Kernel migration remains gated** — no critical subsystem is moved without separate design review and rollback plan.

  \---

  ## 🔨 Future Toolchain Independence: Own Assembler (post-v0.9)

  *Goal: Remove dependency on external assembler (GAS/MASM).*

  ### Phase 6 Architecture Scope

* \[ ] **Frontend** — assembly lexer/parser with strict diagnostics.
* \[ ] **Core** — instruction model + encoder + relocation planner.
* \[ ] **Writers** — COFF/ELF object emitters with symbol/relocation tables.
* \[ ] **Integration** — backend handoff + CLI controls + verification pipeline.

  ### v1.5.0: Baa Assembler (مُجمِّع باء) 🔧

  #### v1.5.0.1: Assembler Foundation

* \[ ] **Define instruction encoding tables** — x86-64 opcode maps.
* \[ ] **Parse assembly text** — Tokenize AT\&T/Intel syntax.
* \[ ] **Build instruction IR** — Internal representation of machine code.
* \[ ] **Handle labels** — Track label addresses for jumps.
* \[ ] **MVP syntax decision** — AT\&T as canonical input first, Intel optional later.
* \[ ] **Source mapping** — retain line/column mapping for assembler diagnostics.

  #### v1.5.0.2: x86-64 Encoding

* \[ ] **REX prefixes** — 64-bit register encoding.
* \[ ] **ModR/M and SIB bytes** — Addressing mode encoding.
* \[ ] **Immediate encoding** — Handle different immediate sizes.
* \[ ] **Displacement encoding** — Memory offset encoding.
* \[ ] **Instruction validation** — Check valid operand combinations.
* \[ ] **Coverage matrix** — define required instruction families for Baa backend output.
* \[ ] **Relocation-aware encoding** — encode fixup placeholders for unresolved symbols.

  #### v1.5.0.3: Object File Generation

* \[ ] **COFF format (Windows)** — Generate .obj files.
* \[ ] **ELF format (Linux)** — Generate .o files.
* \[ ] **Section handling** — .text, .data, .bss, .rodata.
* \[ ] **Symbol table** — Export/import symbols.
* \[ ] **Relocation entries** — Handle address fixups.
* \[ ] **ABI metadata correctness** — calling-convention relevant symbol attributes.
* \[ ] **Cross-platform conformance tests** — validate produced `.obj/.o` with system linkers.

  #### v1.5.0.4: Assembler Integration

* \[ ] **Replace GAS calls** — Use internal assembler.
* \[ ] **`--use-internal-asm` flag** — Optional internal assembler.
* \[ ] **Verify output** — Compare with GAS output.
* \[ ] **Performance test** — Ensure acceptable speed.
* \[ ] **Fallback policy** — clear fallback to external assembler on unsupported patterns.
* \[ ] **CI dual-mode runs** — external/internal assembler comparison in regression pipeline.

  #### v1.5.0.5: Assembler Polish

* \[ ] **Error messages** — Clear assembly error diagnostics.
* \[ ] **Debug info** — Generate debug symbols.
* \[ ] **Listing output** — Optional assembly listing with addresses.
* \[ ] **Documentation** — Assembler internals guide.

  #### v1.5.0.6: Assembler Release Gate

* \[ ] **Default-on readiness** — internal assembler can become default for supported targets.
* \[ ] **Stability gate** — crash-free stress/fuzz-lite runs across assembler corpus.
* \[ ] **Parity signoff** — approved diff report vs GAS on representative corpus.

  \---

  ## 🔗 Future Toolchain Independence: Own Linker (post-v0.9)

  *Goal: Remove dependency on external linker (ld/link.exe).*

  ### Phase 7 Architecture Scope

* \[ ] **Object ingestion layer** — robust COFF/ELF readers with strict validation.
* \[ ] **Link graph core** — symbol table, section graph, relocation plan, address assignment.
* \[ ] **Format writers** — PE/ELF executable writers with deterministic layout rules.
* \[ ] **Runtime bridge layer** — controlled integration with CRT/system runtime requirements.
* \[ ] **Verification layer** — parity checks vs system linkers + deterministic output checks.

  ### Phase 7 Operating Rules

* \[ ] **Deterministic link ordering** — stable output independent of host filesystem ordering.
* \[ ] **No silent symbol resolution** — ambiguous/duplicate/undefined symbols must emit explicit diagnostics.
* \[ ] **Relocation correctness first** — overflow and invalid relocation types are hard failures.
* \[ ] **Target isolation** — Windows and Linux paths share abstractions, not target-specific hacks.

  ### v2.0.0: Baa Linker (رابط باء) 🔗

  #### v2.0.0.1: Linker Foundation

* \[ ] **Parse object files** — Read COFF/ELF format.
* \[ ] **Symbol resolution** — Match symbol references to definitions.
* \[ ] **Section merging** — Combine sections from multiple objects.
* \[ ] **Memory layout** — Assign virtual addresses to sections.
* \[ ] **Archive scanning strategy** — deterministic one-pass/two-pass policy for `.a/.lib`.
* \[ ] **Input validation policy** — reject malformed object metadata with Arabic diagnostics.

  #### v2.0.0.2: Relocation Processing

* \[ ] **Apply relocations** — Fix up addresses in code/data.
* \[ ] **Handle relocation types** — PC-relative, absolute, GOT, PLT.
* \[ ] **Overflow detection** — Check address range limits.
* \[ ] **Relocation test matrix** — per-target coverage for required relocation kinds.
* \[ ] **Late-binding audit logs** — debug trace mode for relocation decisions.

  #### v2.0.0.3: Executable Generation (Windows)

* \[ ] **PE header** — DOS stub, PE signature, file header.
* \[ ] **Optional header** — Entry point, section alignment, subsystem.
* \[ ] **Section headers** — .text, .data, .rdata, .bss.
* \[ ] **Import table** — For C runtime and Windows API.
* \[ ] **Export table** — If building DLLs (future).
* \[ ] **Generate .exe** — Complete Windows executable.
* \[ ] **PE conformance checks** — verify headers/alignments with tooling (`dumpbin`/`llvm-readobj`).
* \[ ] **Subsystem policies** — console/gui/custom-startup rules documented and tested.

  #### v2.0.0.4: Executable Generation (Linux)

* \[ ] **ELF header** — File identification, entry point.
* \[ ] **Program headers** — Loadable segments.
* \[ ] **Section headers** — .text, .data, .rodata, .bss.
* \[ ] **Dynamic linking info** — For libc linkage.
* \[ ] **Generate executable** — Complete Linux binary.
* \[ ] **ELF conformance checks** — validate segments/sections with `readelf`/`objdump`.
* \[ ] **PIE/non-PIE policy** — deterministic handling for code model and relocation mode.

  #### v2.0.0.5: Linker Features

* \[ ] **Static libraries** — Link .a/.lib archives.
* \[ ] **Library search paths** — `-L` flag support.
* \[ ] **Entry point selection** — Custom entry point support.
* \[ ] **Strip symbols** — Remove debug symbols for release.
* \[ ] **Map file** — Generate link map for debugging.
* \[ ] **Section GC plan** — optional dead-section elimination with safety checks.
* \[ ] **Weak symbol semantics** — explicit target-specific behavior for weak/strong bindings.

  #### v2.0.0.6: Linker Integration

* \[ ] **Replace ld/link calls** — Use internal linker.
* \[ ] **`--use-internal-linker` flag** — Optional internal linker.
* \[ ] **Verify output** — Compare with system linker output.
* \[ ] **End-to-end test** — Compile and link without external tools.
* \[ ] **Dual-link CI mode** — run internal and system linker paths on same corpus.
* \[ ] **Fallback policy** — controlled fallback with reasoned diagnostics when unsupported.

  #### v2.0.0.7: Linker Release Gate

* \[ ] **Cross-target parity signoff** — runtime and symbol behavior match system linker expectations.
* \[ ] **Determinism signoff** — stable binary layout for identical inputs/toolchains.
* \[ ] **Stress signoff** — large multi-object links and archive-heavy workloads remain stable.

  #### Phase 7 Exit Criteria

* \[ ] **Default-on readiness** — internal linker can be default for supported targets.
* \[ ] **Regression stability** — quick/full/stress QA pass with internal linker enabled.
* \[ ] **Debuggability baseline** — diagnostics + map output sufficient for failure triage.

  \---

  ## 🏆 Future Toolchain Independence: Full Independence (post-v0.9)

  *Goal: Zero external dependencies — Baa builds itself with no external tools.*

  ### Phase 8 Architecture Scope

* \[ ] **Runtime kernel layer** — startup, syscall/API bridge, memory primitives, and process exit flow.
* \[ ] **Native stdlib layer** — string/math/memory/io modules implemented in Baa with stable ABI contracts.
* \[ ] **Bootstrap provenance layer** — staged compiler lineage (`baa0 -> baa1 -> baa2`) with hashable artifacts.
* \[ ] **Hermetic build layer** — controlled inputs, pinned flags, deterministic packaging outputs.
* \[ ] **Operational verification layer** — dependency scans, runtime smoke tests, and cross-target release gates.

  ### Phase 8 Operating Rules

* \[ ] **No hidden host dependencies** — any required host tool must be explicitly listed in bootstrap docs.
* \[ ] **Reproducibility-first policy** — build determinism regressions block release.
* \[ ] **Cross-target parity policy** — Windows/Linux runtime features must ship together or stay gated.
* \[ ] **Fail-fast runtime diagnostics** — startup/syscall/runtime failures must emit explicit Arabic-first errors.
* \[ ] **Recovery-path requirement** — each independence milestone must define rollback and re-bootstrap steps.

  ### v3.0.0: Complete Toolchain 🛠️

  #### v3.0.0.1: Remove C Runtime Dependency

  **Windows:**

* \[ ] **Direct Windows API calls** — Replace printf with WriteConsoleA.
* \[ ] **Implement `اطبع` natively** — Direct syscall/API.
* \[ ] **Implement `اقرأ` natively** — ReadConsoleA.
* \[ ] **Implement memory functions** — HeapAlloc/HeapFree instead of malloc/free.
* \[ ] **Implement file I/O** — CreateFile, ReadFile, WriteFile.
* \[ ] **Custom entry point** — Replace C runtime startup.
* \[ ] **SEH-aware startup** — preserve stable crash/exit semantics without CRT helpers.
* \[ ] **Win64 ABI compliance checks** — stack alignment, shadow space, and return path verified.

  **Linux:**

* \[ ] **Direct syscalls** — write, read, mmap, exit.
* \[ ] **Implement `اطبع` natively** — syscall to write(1, ...).
* \[ ] **Implement `اقرأ` natively** — syscall to read(0, ...).
* \[ ] **Implement memory functions** — mmap/munmap for allocation.
* \[ ] **Implement file I/O** — open, read, write, close syscalls.
* \[ ] **Custom \_start** — No libc dependency.
* \[ ] **Syscall ABI validation** — register/stack contracts verified on x86\_64 SystemV.
* \[ ] **Signal/exit behavior checks** — deterministic termination semantics across test corpus.

  #### v3.0.0.2: Native Standard Library

* \[ ] **Rewrite string functions in Baa** — No C dependency.
* \[ ] **Rewrite math functions in Baa** — Pure Baa implementation.
* \[ ] **Rewrite memory functions in Baa** — Custom allocator.
* \[ ] **Full standard library in Baa** — All library code in Baa.
* \[ ] **Module boundary contracts** — document stable APIs for `core`, `memory`, `string`, `math`, `io`.
* \[ ] **Behavioral parity suite** — compare stdlib behavior vs prior runtime expectations.
* \[ ] **Allocator policy gates** — fragmentation/throughput baselines for long-running workloads.

  #### v3.0.0.3: Self-Contained Build

* \[ ] **Single binary compiler** — No external dependencies.
* \[ ] **Cross-compilation support** — Build Linux binary on Windows and vice versa.
* \[ ] **Reproducible builds** — Same source → identical binary.
* \[ ] **Bootstrap from source** — Document minimal bootstrap path.
* \[ ] **Hermetic manifest** — lock source inputs, flags, and artifact metadata per release.
* \[ ] **Stage replay tooling** — deterministic scripts to replay bootstrap on clean machines.
* \[ ] **Provenance hashing** — publish hashes for each stage artifact and final binaries.

  #### v3.0.0.4: Verification \& Release

* \[ ] **Full test suite passes** — All tests without external tools.
* \[ ] **Benchmark comparison** — Performance vs GCC toolchain.
* \[ ] **Security audit** — Review for vulnerabilities.
* \[ ] **Documentation complete** — Full toolchain documentation.
* \[ ] **Release v3.0.0** — Fully independent Baa! 🎉
* \[ ] **Supply-chain audit** — verify release pipeline does not reintroduce external tool reliance.
* \[ ] **Disaster-recovery drill** — validate clean-room bootstrap from tagged sources.

  #### v3.0.0.5: Independence Release Gate

* \[ ] **Zero-dependency signoff** — compile/assemble/link/run path requires only Baa-delivered artifacts.
* \[ ] **Deterministic bootstrap signoff** — repeated stage builds are byte-stable per target.
* \[ ] **Cross-target signoff** — Windows + Linux release candidates pass identical gate checklist.
* \[ ] **Operational signoff** — upgrade/rollback/bootstrap recovery procedures validated and documented.

  #### Phase 8 Exit Criteria

* \[ ] **Runtime independence** — no libc/CRT dependency in default build+run workflow.
* \[ ] **Toolchain independence** — compiler, assembler, linker, and runtime owned by Baa project.
* \[ ] **Reproducibility baseline** — release artifacts are verifiable and reproducible from source.
* \[ ] **Sustainability baseline** — maintenance docs and on-call triage playbooks are complete.

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
│  v0.9.x (Stable Beta):                                         │
│  ┌─────────┐   ┌───────────────────────────────────────────┐  │
│  │   Baa   │ → │  GCC (assembler + linker + C runtime)     │  │
│  │ C ref   │   │  + future staged bootstrap plan only      │  │
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

  \---

  ## 🏗️ Phase 2: Architecture Overhaul - Completed

  <details>
<summary><strong>v0.2.0</strong> — The Driver (CLI \& Build System)</summary>

* \[x] **CLI Argument Parser** — Implement a custom argument parser to handle flags manually.
* \[x] **Input/Output Control** (`-o`, `-S`, `-c`).
* \[x] **Information Flags** (`--version`, `--help`, `-v`).
* \[x] **Build Pipeline** — Orchestrate Lexer -> Parser -> Codegen -> GCC.

  </details>

  <details>
<summary><strong>v0.2.1</strong> — Polish \& Branding</summary>

* \[x] **Executable Icon** — Embed `.ico` resource.
* \[x] **Metadata** — Version info, Copyright, Description in `.exe`.

  </details>

  <details>
<summary><strong>v0.2.2</strong> — The Diagnostic Engine Patch</summary>

* \[x] **Source Tracking** — Update `Token` and `Node` to store Filename, Line, and Column.
* \[x] **Error Module** — Create a dedicated error reporting system.
* \[x] **Pretty Printing** — Display errors with context (`^` pointers).
* \[x] **Panic Recovery** — Continue parsing after errors.

  </details>

  <details>
<summary><strong>v0.2.3</strong> Distribution \& Updater Patch</summary>

* \[x] **Windows Installer** — Create `setup.exe` using Inno Setup.
* \[x] **PATH Integration** — Add compiler to system environment variables.
* \[x] **Self-Updater** — Implement `baa update` command.

  </details>

  <details>
<summary><strong>v0.2.4</strong> The Semantic Pass (Type Checker)</summary>

* \[x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
* \[x] **Pass Separation** — Completely separate Parsing from Code Generation.

  * `parse()` returns a raw AST.
  * `analyze()` walks the AST to check types and resolve symbols.
  * The backend pipeline consumes a validated AST.
* \[x] **Symbol Resolution** — Check for undefined variables before code generation starts.
* \[x] **Scope Analysis** — Implement scope stack to properly handle nested blocks and variable shadowing.
* \[x] **Type Checking** — Validate assignments (int = string now fails during semantic analysis).

  </details>

  <details>
<summary><strong>v0.2.5</strong> Multi-File \& Include System</summary>

* \[x] **File Extension Migration** — Change `.b` to `.baa`. Reserved `.baahd` for headers.
* \[x] **Include Directive** — `#تضمين "file.baahd"` (C-style `#include`).
* \[x] **Header Files** — `.baahd` extension for declarations (function signatures, extern variables).
* \[x] **Function Prototypes** — Declarations without types `صحيح دالة().` (Added).
* \[x] **Multi-file CLI** — Accept multiple inputs: `baa main.baa lib.baa -o out.exe`.
* \[x] **Linker Integration** — Compile each file to `.o` then link together.

  </details>

  <details>
<summary><strong>v0.2.6</strong> Preprocessor Directives</summary>

* \[x] **Define** — `#تعريف اسم قيمة` for compile-time constants.
* \[x] **Conditional** — `#إذا\_عرف`, `#إذا\_عرف`, `#إذا\_لم\_يعرف`, `#وإلا`, `#وإلا\_إذا`, `#نهاية\_إذا` for conditional compilation.
* \[x] **Undefine** — `#الغاء\_تعريف` to remove definitions.

  </details>

  <details>
<summary><strong>v0.2.7</strong> Constants \& Immutability</summary>

* \[x] **Constant Keyword** — `ثابت` for immutable variables: `ثابت صحيح حد = ١٠٠.`
* \[x] **Const Checking** — Semantic error on reassignment of constants.
* \[x] **Array Constants** — Support constant arrays.

  </details>

  <details>
<summary><strong>v0.2.8</strong> Warnings \& Diagnostics</summary>

* \[x] **Warning System** — Separate warnings from errors (non-fatal).
* \[x] **Unused Variables** — Warn if variable declared but never used.
* \[x] **Dead Code** — Warn about code after `إرجع` or `توقف`.
* \[x] **`-W` Flags** — `-Wall`, `-Werror` to control warning behavior.

  </details>

  <details>
<summary><strong>v0.2.9</strong> — Input \& UX Polish</summary>

* \[x] **Input Statement** — `اقرأ س.` (scanf) for reading user input.
* \[x] **Boolean Type** — `منطقي` type with `صواب`/`خطأ` literals.
* \[x] **Colored Output** — ANSI colors for errors (red), warnings (yellow). *(Implemented in v0.2.8)*
* \[x] **Compile Timing** — Show compilation time with `-v`.

  </details>

  ## 📦 Phase 1: Language Foundation (v0.1.x) - Completed

  <details>
<summary><strong>v0.1.3</strong> — Control Flow \& Optimizations</summary>

* \[x] **Extended If** — Support `وإلا` (Else) and `وإلا إذا` (Else If) blocks.
* \[x] **Switch Statement** — `اختر` (Switch), `حالة` (Case), `افتراضي` (Default)
* \[x] **Constant Folding** — Compile-time math (`١ + ٢` → `٣`)

  </details>

  <details>
<summary><strong>v0.1.2</strong> — Recursion \& Strings</summary>

* \[x] **Recursion** — Stack alignment fix
* \[x] **String Variables** — `نص` type
* \[x] **Loop Control** — `توقف` (Break) \& `استمر` (Continue)

  </details>

  <details>
<summary><strong>v0.1.1</strong> — Structured Data</summary>

* \[x] **Arrays** — Fixed-size stack arrays (`صحيح قائمة\[١٠]`)
* \[x] **For Loop** — `لكل (..؛..؛..)` syntax
* \[x] **Logic Operators** — `\&\&`, `||`, `!` with short-circuiting
* \[x] **Postfix Operators** — `++`, `--`

  </details>

  <details>
<summary><strong>v0.1.0</strong> — Text \& Unary</summary>

* \[x] **Strings** — String literal support (`"..."`)
* \[x] **Characters** — Character literals (`'...'`)
* \[x] **Printing** — Updated `اطبع` to handle multiple types
* \[x] **Negative Numbers** — Unary minus support

  </details>

  <details>
<summary><strong>v0.0.9</strong> — Advanced Math</summary>

* \[x] **Math** — Multiplication, Division, Modulo
* \[x] **Comparisons** — Greater/Less than logic (`<`, `>`, `<=`, `>=`)
* \[x] **Parser** — Operator Precedence Climbing (PEMDAS)

  </details>

  <details>
<summary><strong>v0.0.8</strong> — Functions</summary>

* \[x] **Functions** — Function definitions and calls
* \[x] **Entry Point** — Mandatory `الرئيسية` exported as `main`
* \[x] **Scoping** — Global vs Local variables
* \[x] **Windows x64 ABI** — Register passing, stack alignment, shadow space

  </details>

  <details>
<summary><strong>v0.0.7</strong> — Loops</summary>

* \[x] **While Loop** — `طالما` implementation
* \[x] **Assignments** — Update existing variables

  </details>

  <details>
<summary><strong>v0.0.6</strong> — Control Flow</summary>

* \[x] **If Statement** — `إذا` with blocks
* \[x] **Comparisons** — `==`, `!=`
* \[x] **Documentation** — Comprehensive Internals \& API docs

  </details>

  <details>
<summary><strong>v0.0.5</strong> — Type System</summary>

* \[x] Renamed `رقم` to `صحيح` (int)
* \[x] Single line comments (`//`)

  </details>

  <details>
<summary><strong>v0.0.4</strong> — Variables</summary>

* \[x] Variable declarations and stack offsets
* \[x] Basic symbol table

  </details>

  <details>
<summary><strong>v0.0.3</strong> — I/O</summary>

* \[x] `اطبع` (Print) via Windows `printf`
* \[x] Multiple statements support

  </details>

  <details>
<summary><strong>v0.0.2</strong> — Math</summary>

* \[x] Arabic numerals (٠-٩)
* \[x] Addition and subtraction

  </details>

  <details>
<summary><strong>v0.0.1</strong> — Foundation</summary>

* \[x] Basic pipeline: Lexer → Parser → Codegen → GCC

  </details>

  \---

  ## 📊 Timeline Summary

|Phase|Version|Milestone|Dependencies / Owner|
|-|-|-|-|
|Phase 3|v0.3.x|IR Complete|GCC|
|Phase 3.5|v0.3.3-v0.3.12|Language Complete|GCC|
|Phase 4|v0.4.x|Standard Library|GCC|
|Phase 4.5|v0.5.x|Reference Compiler Stabilization|GCC|
|Phase 6|v0.6.x|Language Usability \& Safety|Baa compiler|
|Phase 7|v0.7.x|Testing \& External Integration Contracts|Baa + Takween/Qalam contracts|
|Phase 8|v0.8.x|Backend/Optimizer/Performance Reliability|Baa compiler|
|Phase 9|v0.9.x|Stable Beta + Future Bootstrap/OS-dev Plan|Baa compiler|
|Future|v0.10.x / post-v0.9|Freestanding OS Development Profile|Baa compiler + PyramidOS C/ASM reference|
|Future|post-v0.10|Self-hosting / own assembler / own linker|Separate staged decision|

\---

*For detailed changes, see the* [*Changelog*](CHANGELOG.md)
