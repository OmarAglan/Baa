# Baa Language Roadmap

**Last Updated:** 2025-12-05  
**Version:** v0.1.27.0

## Current Version State

**Overall Status:** Core compiler infrastructure production-ready. Full pipeline (Preprocessor → Lexer → Parser → AST → CodeGen stub) is operational.

| Component | Status |
|-----------|--------|
| **Build System** | ✅ Complete |
| **Preprocessor** | ✅ Production Ready |
| **Lexer** | ✅ Production Ready |
| **Parser & AST** | ✅ Production Ready |
| **Semantic Analysis** | 📋 Next Phase |
| **Code Generation** | 🔄 Stub (LLVM pending) |

---

## Phase 1: Core Infrastructure (Maintenance Mode)

### 1.1 Build System (Status: Stable)

* [x] Target-centric CMake structure.
* [x] LLVM conditional linking.
* [x] Automated test discovery.

### 1.2 Preprocessor (Status: ⚠️ Repairing)

* [x] Core Directives (`#تضمين`, `#تعريف`, `#إذا`).
* [x] Arabic Syntax Support.
* [x] Enhanced Error System.
* [ ] **FIX:** Restore advanced macro expansion (`#`, `##`).
* [ ] **FIX:** Resolve memory safety issues (SEGFAULTS).
* *See `docs/PREPROCESSOR_ROADMAP.md`.*

### 1.3 Lexer (Status: ✅ Stable)

* [x] Full Arabic Tokenization (Digits, Keywords).
* [x] Enhanced Error/Recovery System.
* [x] Comprehensive Literal Support (Scientific, Hex Float).
* *See `docs/LEXER_ROADMAP.md`.*

---

## Phase 2: Syntactic Analysis (Completed)

### 2.1 Abstract Syntax Tree (AST v3)

* [x] Unified `BaaNode` structure.
* [x] Expression Nodes (Binary, Unary, Call).
* [x] Statement Nodes (If, While, Return, Block).
* [x] Declaration Nodes (Var, Function, Parameter).
* *See `docs/AST_ROADMAP.md`.*

### 2.2 Parser (v3)

* [x] Recursive Descent implementation.
* [x] Precedence Climbing for expressions.
* [x] Statement dispatching.
* [x] Function definition parsing.
* *See `docs/PARSER_ROADMAP.md`.*

---

## Phase 3: Semantic Analysis (Upcoming Priority)

**Goal:** Validate the meaning of the code before generating machine code.

* **Symbol Tables:** Track variables/functions across scopes.
* **Name Resolution:** Link identifiers to declarations.
* **Type Checking:** Validate operations (`int + int`, `bool` conditions).
* **Flow Analysis:** Ensure return statements in non-void functions.
* *See `docs/SEMANTIC_ANALYSIS_ROADMAP.md`.*

---

## Phase 4: Code Generation (Planned)

**Goal:** Translate the validated AST to LLVM IR.

* **Infrastructure:** Re-enable `baa_codegen` library.
* **Type Mapping:** Map `عدد_صحيح` to `LLVMInt32`, etc.
* **IR Generation:** Implement `codegen_function`, `codegen_expr`, etc. using the new AST structure.
* *See `docs/LLVM_CODEGEN_ROADMAP.md`.*

---

## Phase 5: Advanced Features (Future)

* **Types:** Arrays, Structs (`بنية`), Pointers (`مؤشر`).
* **Modules:** Import system.
AST_Parser_Development_Progress__2025-07-04T16-35-09* **Tooling:** Language Server (LSP), Formatter.

---

## Immediate Action Plan

1. **Fix Preprocessor:** Resolve macro expansion regressions and memory crashes.
2. **Stabilize Test Suite:** Ensure all 12/12 preprocessor tests pass.
3. **Begin Semantic Analysis:** Implement Symbol Table infrastructure.
4. **Connect Codegen:** Update LLVM backend to consume new AST.
