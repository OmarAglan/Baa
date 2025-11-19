# Baa Language LLVM Code Generation Roadmap

**Status: 🛠️ PLANNED / REFACTORING REQUIRED**
**Current Focus: Alignment with AST v3 & Semantic Analysis**

This document outlines the plan to implement the LLVM backend. While a basic LLVM infrastructure (`src/codegen/llvm_codegen.c`) exists, it relies on outdated AST structures. **It must be rewritten to consume the new `BaaNode` AST (v3) and the Symbol Tables produced by Semantic Analysis.**

---

## Phase 0: Infrastructure & Cleanup (Immediate Next Step)

**Goal:** Re-enable the Codegen module within the new compiler architecture.

* [x] **Build System:** CMake correctly finds and links LLVM libraries (conditional compilation).
* [x] **Context Management:** `BaaLLVMContext` struct exists (Context, Module, Builder).
* [ ] **Cleanup:** Remove/Update old `codegen.c` logic that references obsolete AST types (`BaaBinaryExpr` vs `BaaBinaryExprData`).
* [ ] **Main Entry Point:** Update `baa_generate_llvm_ir` to accept the new `BaaNode*` root (Program Node).

## Phase 1: Type System Mapping

**Goal:** Translate Baa's high-level types to LLVM IR types.

* [ ] **Primitive Mapping:**
  * `عدد_صحيح` (int) -> `LLVMInt32Type` / `LLVMInt64Type`.
  * `عدد_حقيقي` (float) -> `LLVMDoubleType`.
  * `منطقي` (bool) -> `LLVMInt1Type`.
  * `حرف` (char) -> `LLVMInt32Type` (Unicode codepoint) or `LLVMInt16Type`.
  * `فراغ` (void) -> `LLVMVoidType`.
* [ ] **Derived Types (Priority 5):**
  * Arrays -> `LLVMArrayType`.
  * Strings -> `LLVMPointerType(LLVMInt8Type)` (C-style) or Struct (Length + Data).

## Phase 2: Expression Generation

**Goal:** Generate IR for mathematical and logical operations.

* [ ] **Literals:**
  * Generate `LLVMConstInt`, `LLVMConstReal`.
  * Handle String Literals (Global Constant String + Pointer).
* [ ] **Variable Access:**
  * Resolve symbols via the **Semantic Analysis Symbol Table**.
  * Generate `LLVMBuildLoad` for reading variables.
* [ ] **Binary Operations:**
  * Arithmetic: `Add`, `Sub`, `Mul`, `SDiv` (Signed), `SRem` (Modulo).
  * Comparison: `ICmpEQ`, `ICmpNE`, `ICmpSLT` (Signed Less Than), etc.
  * Logical: Short-circuiting logic (using Basic Blocks).
* [ ] **Unary Operations:** `Neg`, `Not`.

## Phase 3: Statement & Control Flow Generation

**Goal:** Implement the flow of execution logic.

* [ ] **Function Entry:**
  * Generate `entry` Basic Block.
  * Allocate stack space (`alloca`) for parameters and local variables.
* [ ] **If/Else (`إذا`):**
  * Generate `then`, `else`, and `merge` blocks.
  * Generate `CondBr` instruction.
  * Handle Phi nodes (if needed) or memory stack approach (simplest for v1).
* [ ] **Loops (`طالما`, `لكل`):**
  * Generate `header`, `body`, `exit` blocks.
  * Handle `break` (`توقف`) and `continue` (`استمر`) jumps.
* [ ] **Return (`إرجع`):**
  * Generate `Ret` or `RetVoid`.

## Phase 4: Function Generation

**Goal:** Complete the function definition and calling convention.

* [ ] **Function Prototypes:**
  * Construct `LLVMFunctionType` based on Baa return/param types.
  * Add functions to `LLVMModule`.
* [ ] **Function Calls (`call_expr`):**
  * Generate arguments.
  * Generate `Call` instruction.
* [ ] **Entry Point:** Ensure `main` (or Arabic equivalent) is correctly exported.

## Phase 5: Optimization & Artifact Generation

**Goal:** Produce executable binaries.

* [ ] **Optimization Pass Manager:**
  * Configure `LLVMPassManagerBuilder` (O1, O2, O3).
  * Run optimization pipeline.
* [ ] **Object File Emission:**
  * Initialize Target Machine (Host CPU/Features).
  * Emit `.o` (Object) file.
* [ ] **Linking:**
  * Invoke system linker (`ld`, `clang`, or `lld`) to create the final executable.

---

## Strategy

1. **Wait for Semantic Analysis:** Do not write Codegen logic until the AST is semantically validated and annotated (types are known).
2. **Stack-Based First:** Use `alloca` for all local variables. Rely on LLVM's `mem2reg` pass to optimize into SSA registers (Phi nodes). This significantly simplifies the frontend logic.
3. **Testing:** Use `lit` (LLVM Integrated Tester) or `FileCheck` style tests to verify the generated IR text against expected patterns.
