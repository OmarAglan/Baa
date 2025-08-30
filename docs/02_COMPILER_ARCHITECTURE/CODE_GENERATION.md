# Baa Language LLVM Code Generation Design

**Status: This document outlines the planned design for the LLVM Code Generation phase. This phase operates on a semantically analyzed and annotated AST from the Semantic Analysis phase.**

## 1. Overview and Goals

The LLVM Code Generation phase is responsible for translating the Baa Abstract Syntax Tree (AST), which has been validated and annotated by the Semantic Analyzer, into LLVM Intermediate Representation (IR).

**Primary Goals:**

*   **Correctness:** Generate LLVM IR that accurately reflects the semantics of the Baa source program.
*   **LLVM IR Quality:** Produce "good quality" LLVM IR that is amenable to LLVM's optimization passes. This doesn't mean performing complex optimizations during this phase, but rather generating IR in a standard, understandable form (e.g., SSA form where appropriate, correct typing).
*   **Target Independence (Initial IR):** The initial LLVM IR generated should be largely target-independent. LLVM's backend will handle target-specific concerns.
*   **Debuggability:** Lay the groundwork for generating debug information that maps IR back to Baa source code.
*   **Modularity:** Design the code generator in a modular way, typically by having functions to generate IR for each type of AST node.

**Input:** Semantically Analyzed and Annotated Baa AST.
**Output:** An LLVM Module (`LLVMModuleRef`) containing the generated IR.

## 2. Core Components and Architecture

### 2.1. `BaaLLVMCodeGenerator` Context Structure
A central structure (e.g., `BaaLLVMCodeGenerator`) will manage the state of code generation:
*   `LLVMContextRef context`: The global LLVM context.
*   `LLVMModuleRef module`: The LLVM module being built.
*   `LLVMBuilderRef builder`: The LLVM IR instruction builder.
*   `LLVMValueRef current_function`: A pointer to the `LLVMFunctionRef` of the function currently being generated.
*   **Symbol Mapping:** A mechanism (e.g., a stack of hash maps for scopes) to map Baa AST symbols (variables, parameters, functions) to their corresponding `LLVMValueRef` (e.g., `alloca` instructions for local variables, function pointers).
*   **Type Mapping Cache:** A cache to store mappings from `BaaType*` to `LLVMTypeRef` to avoid redundant type creation.
*   **Control Flow Context:** Information needed for generating branches for loops and conditionals (e.g., pointers to current loop's header, exit, and increment blocks).

### 2.2. AST Traversal
The code generator will typically traverse the AST using a visitor pattern or a set of mutually recursive functions, one for each AST node kind (e.g., `codegen_expression(BaaExpr*)`, `codegen_statement(BaaStmt*)`).

### 2.3. Dispatch
A main dispatch function (e.g., `codegen_node(BaaNode*)`) will switch on the AST node's kind and call the appropriate specialized code generation function.

## 3. Mapping Baa Constructs to LLVM IR

### 3.1. Type Mapping
*   A utility function (`baa_type_to_llvm_type`) will convert Baa types (from `BaaType` structs) to `LLVMTypeRef`.
    *   **Primitive Types:** `عدد_صحيح` -> `LLVMInt32Type` (or `LLVMInt64Type` for `long long`), `عدد_حقيقي` -> `LLVMFloatType` (or `LLVMDoubleType`), `منطقي` -> `LLVMInt1Type`, `حرف` -> `LLVMInt16Type`, `فراغ` -> `LLVMVoidType`.
    *   **Array Types:** `BaaType` for arrays -> `LLVMArrayType`.
    *   **Pointer Types:** `BaaType` for pointers -> `LLVMPointerType`.
    *   **Struct/Union Types:** `BaaType` for structs/unions -> `LLVMStructType`. Named structs will be created in the LLVM module.
    *   **Function Types:** `BaaType` for functions -> `LLVMFunctionType`.

### 3.2. Literals
*   **Integers:** `LLVMConstInt`.
*   **Floats:** `LLVMConstReal`.
*   **Booleans:** `LLVMConstInt` (1-bit).
*   **Characters:** `LLVMConstInt` (16-bit for `wchar_t`).
*   **Strings:** `LLVMBuildGlobalStringPtr` (creates a global constant string and returns a pointer to it).
*   **Null:** `LLVMConstNull` for pointer types, or zero for other appropriate types.

### 3.3. Variables and Memory
*   **Global Variables:** `LLVMAddGlobal`. Initializers must be constant expressions. `ثابت` globals are marked with `LLVMSetGlobalConstant`.
*   **Local Variables (Stack):** `LLVMBuildAlloca` in the entry basic block of a function.
*   **Variable Access:**
    *   **Load:** `LLVMBuildLoad` from the pointer obtained from `alloca` or global variable.
    *   **Store:** `LLVMBuildStore` to the pointer.
*   **Symbol Mapping:** The `BaaLLVMCodeGenerator` will maintain a mapping from AST symbol nodes (or their unique IDs from semantic analysis) to the `LLVMValueRef` representing their memory location (alloca/global).

### 3.4. Operators
*   **Unary:** `LLVMBuildNSWNeg`, `LLVMBuildFNeg`, `LLVMBuildNot` (for bitwise NOT; logical NOT is usually `icmp ne val, 0`).
*   **Binary:**
    *   Arithmetic: `LLVMBuildAdd`, `LLVMBuildSub`, `LLVMBuildMul`, `LLVMBuildSDiv`, `LLVMBuildUDiv`, `LLVMBuildFDiv`, `LLVMBuildSRem`, `LLVMBuildURem`, `LLVMBuildFRem`. Use `NSW` (No Signed Wrap) / `NUW` (No Unsigned Wrap) flags if Baa guarantees no overflow or defines overflow behavior.
    *   Comparison: `LLVMBuildICmp` (integer comparison with predicates like `LLVMIntEQ`, `LLVMIntSLT`, etc.), `LLVMBuildFCmp` (float comparison).
    *   Logical (`&&`, `||`): Implemented using conditional branches and basic blocks to achieve short-circuiting behavior. PHI nodes (`LLVMBuildPhi`) are used to merge values from different paths.

### 3.5. Control Flow
*   **Basic Blocks:** LLVM IR is organized into basic blocks. Each function starts with an entry block. Control flow statements create new basic blocks and branch instructions.
*   **If Statements (`إذا`/`وإلا`):** `LLVMBuildCondBr` to branch to a `then` block or an `else` (or `merge`) block. Unconditional branches (`LLVMBuildBr`) link these blocks.
*   **Loops (`طالما`, `لكل`):**
    *   Typically involve a loop header block (for condition checking), a loop body block, and an exit/after-loop block.
    *   `LLVMBuildCondBr` for loop condition.
    *   `LLVMBuildBr` for looping back or exiting.
    *   `break` (`توقف`) jumps to the after-loop block.
    *   `continue` (`استمر`) jumps to the loop header (for `while`) or increment block (for `for`).
*   **Return Statements (`إرجع`):** `LLVMBuildRet` (with value) or `LLVMBuildRetVoid`.

### 3.6. Functions
*   **Definitions:** `LLVMAddFunction` creates a function in the module. The function type (`LLVMFunctionType`) is derived from Baa return and parameter types.
*   **Parameters:** `LLVMGetParam` retrieves `LLVMValueRef` for parameters within the function. These are typically stored in local `alloca` variables for mutability if Baa parameters are mutable by default.
*   **Calls:** `LLVMBuildCall`. Arguments are passed as an array of `LLVMValueRef`.
*   **Inline Hint (`مضمن`):** `LLVMAddFunctionAttr` with `LLVMInlineAttribute` or `LLVMAlwaysInlineAttribute`.
*   **Restrict Hint (`مقيد`):** `LLVMAddAttributeAtIndex` with `LLVMNoAliasAttribute` for pointer parameters.

### 3.7. Advanced Types (Structs, Arrays, Pointers - when implemented)
*   **Structs (`بنية`):**
    *   Types: `LLVMStructCreateNamed` and `LLVMStructSetBody`.
    *   Member Access: `LLVMBuildStructGEP` (or `LLVMBuildGEP`) to get a pointer to a member, followed by load/store.
*   **Arrays (`مصفوفة`):**
    *   Types: `LLVMArrayType`.
    *   Indexing: `LLVMBuildGEP`.
*   **Pointers (`مؤشر`):**
    *   Types: `LLVMPointerType`.
    *   Dereference: `LLVMBuildLoad` (for read), `LLVMBuildStore` (for write to pointed location).
    *   Address-of: Usually handled by using the `LLVMValueRef` of an `alloca` or global directly.

## 4. LLVM Module and Context Management

*   A single `LLVMContextRef` is typically used for the entire compilation.
*   A single `LLVMModuleRef` is created to hold all functions, global variables, and type definitions for the Baa program being compiled.
*   The module ID should correspond to the input filename or program name.

## 5. Optimization Strategy

*   The primary goal of this phase is to generate correct, unoptimized (or lightly canonicalized) LLVM IR.
*   Heavy optimization is delegated to LLVM's built-in pass managers.
*   The code generator will set up and run a sequence of LLVM optimization passes (e.g., using `LLVMPassManagerBuilder` or by adding individual passes like mem2reg, SROA, GVN, instruction combining, etc.).
*   The level of optimization can be configurable (e.g., -O0, -O1, -O2, -O3, -Os).

## 6. Target Code Emission

*   Initialize LLVM's target-specific components (e.g., `LLVMInitializeX86TargetInfo`, `LLVMInitializeX86Target`, etc.).
*   Create an `LLVMTargetMachineRef` for the desired target triple (e.g., "x86_64-pc-linux-gnu").
*   Use `LLVMTargetMachineEmitToFile` to generate object files (`.o`/`.obj`) or assembly (`.s`).
*   The final linking step to create an executable is typically handled by an external linker (e.g., `ld`, `clang`, `link.exe`), invoked by the compiler driver.

## 7. Error Handling

*   This phase assumes a semantically valid AST. However, internal errors during IR generation should be handled (e.g., assertions, error messages).
*   LLVM provides verification passes (`LLVMVerifyModule`, `LLVMVerifyFunction`) that can detect malformed IR. These should be used during development and testing.

## 8. Debug Information (Future Consideration)

*   Generating DWARF (or CodeView on Windows) debug information is crucial for source-level debugging.
*   This involves using LLVM's `DIBuilder` API to create metadata for files, scopes, functions, variables, types, and source locations, and attaching this metadata to LLVM IR instructions.

## Arabic/RTL Considerations

*   LLVM IR itself uses an ASCII-based syntax for identifiers. Original Arabic identifiers from Baa source will need to be mangled or mapped to valid LLVM IR identifiers if they are to be directly represented (though often, symbols are just memory locations/registers at this stage).
*   Debug information is where the original Arabic names and source locations (critical for RTL) must be accurately preserved and encoded.
