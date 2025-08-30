# Baa Language Roadmap (خارطة الطريق)

## Current Version State (January 2025 - Post-Priority 4 Documentation Update)

* **Build System:** ✅ **COMPLETE** - Modular, Target-Centric CMake with proper component linking
* **Preprocessor:** ✅ **COMPLETE** - Production-ready with C99 alignment, Arabic directives, comprehensive error recovery, macro redefinition checking
* **Lexer:** ✅ **COMPLETE** - Production-ready with Arabic numerals, advanced literals, doc comments, enhanced error handling and Arabic escape sequences
* **Core (Types, Operators, Utils):** ✅ **COMPLETE** - Foundational elements with Arabic type system and memory management
* **Parser & AST:** ✅ **COMPLETE** - **Priority 4 completed (July 2025)** - Full recursive descent parser with function support, unified AST system, and comprehensive error handling
* **Analysis:** 📋 **READY FOR PRIORITY 5** - Basic flow analysis structure exists; semantic analysis components ready for implementation
* **Codegen:** 📋 **READY FOR PRIORITY 5** - LLVM integration infrastructure ready; IR generation awaiting semantic analysis

This roadmap outlines the high-level plan for the Baa language project. For detailed status and plans for specific components, please refer to their respective roadmap documents (`LEXER_ROADMAP.md`, `PREPROCESSOR_ROADMAP.md`, `PARSER_ROADMAP.md`, `AST_ROADMAP.md`, `SEMANTIC_ANALYSIS_ROADMAP.md`, `LLVM_CODEGEN_ROADMAP.md`, etc.).

---

## Phase 1: Core Compiler Infrastructure & Language Basics (Largely Completed/Ongoing Refinement)

### 1.1 Build System & Core Utilities (Status: Refactored & Stable)

* [x] Modern CMake build system (target-centric, modular). (v0.1.18.0)
* [x] Core utilities: Memory management (`baa_malloc`, `baa_strdup`), basic string ops, file I/O.
* [x] Basic error reporting infrastructure.

### 1.2 Preprocessor (Status: Actively Developed, Most C99 Features Implemented)

* [x] File inclusion (`#تضمين`) with relative/standard paths, circular include detection.
* [x] Macro definition (`#تعريف`): object-like, function-like.
* [x] Macro expansion: Argument substitution, stringification (`#`), token pasting (`##`), variadic macros (`وسائط_إضافية`, `__وسائط_متغيرة__`), **full rescanning**.
* [x] **C99-Compliant Macro Redefinition Checking:** Intelligent comparison with whitespace normalization, warnings for incompatible redefinitions, errors for predefined macro redefinitions. (v0.1.25.0)
* [x] Macro undefinition (`#الغاء_تعريف`).
* [x] Conditional compilation (`#إذا`, `#إذا_عرف`, `#إذا_لم_يعرف`, `#وإلا_إذا`, `#إلا`, `#نهاية_إذا`).
* [x] Expression evaluation in conditionals (arithmetic, comparison, logical, bitwise, `معرف`, numeric literals).
* [x] Predefined macros (`__الملف__`, `__السطر__` (int), `__التاريخ__`, `__الوقت__`, `__الدالة__` (placeholder), `__إصدار_المعيار_باء__`).
* [x] `#خطأ` and `#تحذير` directives implemented.
* [x] Input encoding detection (UTF-8 default, UTF-16LE with BOM).
* [x] **Comprehensive Error Recovery System:** Multi-error reporting, smart synchronization strategies, configurable error limits, graceful degradation. (v0.1.24.0)
* **Next/Ongoing:**
  * Finalize known issues (e.g., complex `##` in rescans, zero-parameter function-like macro bug).
  * Implement remaining standard directives (`#سطر`, `#براغما`, `أمر_براغما`).
  * Add ternary operator support (`? :`) in conditional expressions.
  * *See `docs/PREPROCESSOR_ROADMAP.md`.*

### 1.3 Lexer (Status: Actively Developed, Most Core Features Implemented)

* [x] Tokenization of preprocessed UTF-16LE stream.
* [x] Arabic/English identifiers, Arabic-Indic digits, Arabic keywords.
* [x] Numeric literals: decimal, hex (`0x`), binary (`0b`), underscores, Arabic decimal separator (`٫`), basic scientific notation (`e`/`E`).
* [x] Integer literal suffixes (`غ`, `ط`, `طط` and combinations) tokenized.
* [x] String literals (`"..."`, `"""..."""` multiline, `خ"..."` raw).
* [x] Character literals (`'...'`).
* [x] Standard C & Unicode (`\uXXXX`) escape sequence processing.
* [x] Documentation comment token (`/** ... */`).
* [x] Accurate line/column tracking.
* **Next/Ongoing:**
  * Implement Baa-specific Arabic syntax for literals:
    * Float suffix `ح`.
    * Float exponent marker `أ`.
    * Arabic escape sequences (`\س`, `\م`, etc.).
  * Ensure `number_parser.c` fully interprets all lexed numeric forms with suffixes.
  * Enhance error handling (specific error tokens/messages, robust synchronization).
  * Expand unit and fuzz testing.
  * *See `docs/LEXER_ROADMAP.md`.*

### 1.4 Core Language Primitives (Status: Foundational Implementation)

* [x] Basic Type System (`src/types/`): `عدد_صحيح`, `عدد_حقيقي`, `حرف`, `منطقي`, `فراغ`. Array type structure.
* [x] Operator System (`src/operators/`): Definitions, precedence, basic validation stubs.

---

## Phase 2: Syntactic Analysis - Parser & AST (Current Major Focus: Redesign & Re-implementation)

* **Goal:** Develop a new, robust Parser and Abstract Syntax Tree (AST) representation.
* **Status:** Previous implementations removed. New design phase is complete, implementation starting.

### 2.1 Abstract Syntax Tree (AST) - New Design (v0.1.18.0)

* **[x]** Core structures defined (`BaaNode`, `BaaSourceSpan`, `BaaNodeKind`, `BaaAstNodeModifiers`).
* **[x]** Basic memory utilities (`baa_ast_new_node`, `baa_ast_free_node` shell) implemented.
* **[In Progress]** Implementing specific node types:
  * **[x]** Literal Expression Nodes (`BAA_NODE_KIND_LITERAL_EXPR` for int, string) with data structs and lifecycle functions.
* **(Planned Next)** Identifier Expression Nodes, basic Statement Nodes (ExprStmt, BlockStmt), Program Node.
* **(Planned Later)** Full suite of node types, type-safe accessors, AST Visitor, Pretty-Printer.
* *See `docs/AST.md` and `docs/AST_ROADMAP.md`.*

### 2.2 Parser - New Design

* ✅ **(COMPLETED)** Core parser infrastructure (recursive descent, token handling, error reporting/recovery).
* ✅ **(COMPLETED)** Parsing functions implemented for:
  * ✅ Primary expressions (literals, identifiers, grouping).
  * ✅ Unary and binary expressions with correct precedence and associativity.
  * ✅ Assignment expressions.
  * ✅ Basic statements (expression statements, blocks).
  * ✅ Variable declarations (with type annotations and initializers).
  * ✅ Control flow statements (`إذا`, `وإلا`, `طالما`, `لكل`, `إرجع`, `توقف`, `استمر`, `اختر`).
  * ✅ Function declarations (C-style: name, parameters, return type, body).
  * ✅ Function call expressions.
  * ✅ Type parsing (basic annotations, array types).
* ✅ **(COMPLETED)** Integration with the new AST generation.
* **(Next Phase)** Advanced language features: structs, unions, enums, pointers, arrays.
* *See `docs/PARSER.md` and `docs/PARSER_ROADMAP.md`.*

---

## Phase 3: Semantic Analysis (Depends on New Parser & AST)

* **Goal:** Implement semantic checks, name resolution, and type checking. Annotate AST for code generation.
* **Status:** Basic flow analysis structure in `src/analysis/` exists but needs integration with new AST. Full semantic analysis is planned.

### 3.1 Symbol Table & Name Resolution

* **(Planned)** Design and implement symbol table(s) and scope management (global, function, block).
* **(Planned)** Implement name resolution for identifiers, handling shadowing.
* **(Planned)** Report errors for undeclared/ambiguous identifiers.

### 3.2 Type Checking

* **(Planned)** Implement type checking for all expressions, statements, and declarations.
* **(Planned)** Validate operator usage based on operand types.
* **(Planned)** Enforce type compatibility for assignments, function calls, return statements.
* **(Planned)** Define and implement implicit/explicit type conversion rules.

### 3.3 Control Flow Analysis

* **(Planned)** Integrate and adapt existing `flow_analysis.c` for the new AST.
* **(Planned)** Verify "all paths return" for non-void functions.
* **(Planned)** Check for unreachable code, valid `break`/`continue` usage.

### 3.4 AST Annotation

* **(Planned)** Annotate AST nodes with resolved types, links to symbol table entries, etc.
* **(Planned)** Consider AST transformations (e.g., inserting explicit cast nodes).
* *See `docs/SEMANTIC_ANALYSIS.md` and `docs/SEMANTIC_ANALYSIS_ROADMAP.md`.*

---

## Phase 4: Code Generation (Depends on New Semantically-Analyzed AST)

* **Goal:** Translate the validated and annotated AST into LLVM IR, and subsequently to machine code.
* **Status:** Basic LLVM integration and stub backend exist. Actual IR generation from AST is pending.

### 4.1 LLVM IR Generation

* **(Planned)** Implement `BaaLLVMCodeGenerator` context.
* **(Planned)** Develop functions to generate LLVM IR for each AST node type (expressions, statements, functions, control flow).
* **(Planned)** Map Baa types to LLVM types.
* **(Planned)** Handle variable allocation (global, local/stack), loads, and stores.
* **(Planned)** Generate LLVM IR for operators and function calls.
* *See `docs/LLVM_CODEGEN.md` and `docs/LLVM_CODEGEN_ROADMAP.md`.*

### 4.2 Optimization & Output

* **(Planned)** Integrate LLVM optimization passes.
* **(Planned)** Implement emission of object files and assembly for target platforms (e.g., x86-64, ARM64, Wasm).
* **(Planned)** Basic debug information generation.

---

## Phase 5: Language Maturity & Ecosystem (Longer Term)

### 5.1 Advanced Language Features

* **(Future)** User-defined types: `بنية` (structs), `اتحاد` (unions), `تعداد` (enums) with full semantic and codegen support.
* **(Future)** Pointers (`مؤشر`).
* **(Future)** Module system.
* **(Future)** Advanced function features (optional/named parameters, rest parameters).
* **(Future)** More comprehensive standard library.

### 5.2 Tooling & Developer Experience

* **(Future)** Enhanced debugging support (mapping IR to Baa source).
* **(Future)** Language Server Protocol (LSP) implementation for IDE integration (autocompletion, diagnostics).
* **(Future)** Code formatter, linter.
* **(Future)** Package manager.

### 5.3 Testing & Documentation

* **(Ongoing)** Expand unit and integration test coverage for all components.
* **(Ongoing)** Develop comprehensive language specification and user tutorials in Arabic and English.

---

This roadmap is a living document and will be updated as the project progresses and priorities evolve. Community feedback is highly encouraged.
