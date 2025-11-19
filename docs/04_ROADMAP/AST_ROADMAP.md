## Updated `docs/AST_ROADMAP.md` (v3 - Current Status)

**Status: ✅ CORE AST & FUNCTION SUPPORT COMPLETE - This document outlines the implementation status of the Abstract Syntax Tree (AST). Priority 4 (Function Definitions and Calls) was completed in July 2025.**

The AST is built around a unified `BaaNode` structure defined in `include/baa/ast/ast_types.h`. Each node contains a `kind` enum, a `void* data` pointer to a specific structure, and a `BaaAstSourceSpan` for error reporting.

### Phase 0: Core Definitions & Setup (✅ COMPLETED)

**Goal:** Establish the absolute foundational types and structures for the AST.

1. **Define `BaaSourceLocation` and `BaaSourceSpan`:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** defined structures for filename, line, and column tracking.

2. **Define `BaaNodeKind` Enum:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Enum covers Program, Parameters, Functions, Statements (Expr, Block, VarDecl, If, While, For, Return, Break, Continue), and Expressions (Literal, Identifier, Binary, Unary, Call).

3. **Define `BaaNode` Base Structure:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Status:** ✅ **COMPLETED**

4. **Define `BaaAstNodeModifiers`:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Bitmask for modifiers like `BAA_MOD_CONST` (ثابت).

5. **Basic AST Memory Utilities:**
    * **Files:** `include/baa/ast/ast.h`, `src/ast/ast_node.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** `baa_ast_new_node` (allocation) and `baa_ast_free_node` (recursive cleanup) are implemented.

### Phase 1: Minimal Expressions & Statements (✅ COMPLETED)

**Goal:** Implement essential AST node types for basic parsing.

1. **Literal Expression Node (`BAA_NODE_KIND_LITERAL_EXPR`):**
    * **Files:** `src/ast/ast_expressions.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports Int, Float, Bool, Char, String, and Null literals. Includes `determined_type` pointer.

2. **Identifier Expression Node (`BAA_NODE_KIND_IDENTIFIER_EXPR`):**
    * **Files:** `src/ast/ast_expressions.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**

3. **Expression Statement Node (`BAA_NODE_KIND_EXPR_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**

4. **Block Statement Node (`BAA_NODE_KIND_BLOCK_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Handles dynamic lists of statements.

5. **Program Node (`BAA_NODE_KIND_PROGRAM`):**
    * **Files:** `src/ast/ast_program.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Root node containing top-level declarations.

### Phase 2: Basic Declarations & Expressions (✅ COMPLETED)

**Goal:** Expand AST capabilities to represent declarations and math/logic.

1. **Variable Declaration Node (`BAA_NODE_KIND_VAR_DECL_STMT`):**
    * **Files:** `src/ast/ast_declarations.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports name, modifiers, type node, and optional initializer.

2. **Type Representation Node (`BAA_NODE_KIND_TYPE`):**
    * **Files:** `src/ast/ast_types.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports Primitive types (`عدد_صحيح`, etc.) and Array types (`type[]`).

3. **Binary Expression Node (`BAA_NODE_KIND_BINARY_EXPR`):**
    * **Files:** `src/ast/ast_expressions.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports Arithmetic, Comparison, and Logical operators.

4. **Unary Expression Node (`BAA_NODE_KIND_UNARY_EXPR`):**
    * **Files:** `src/ast/ast_expressions.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports Prefix operators (`-`, `!`).

5. **Assignment Expression Node:**
    * **Status:** 🔄 **PENDING / INTEGRATION**
    * **Details:** Currently, `BAA_BINARY_OP_ASSIGN` exists in `operators.h`, but `ast_types.h` lists assignment operators as "future expansion". The parser likely handles assignment via binary expressions or specific logic.
    * **Action:** Clarify if a specific `BAA_NODE_KIND_ASSIGN` is needed or if `BINARY_EXPR` suffices.

### Phase 3: Control Flow & Function Definitions (✅ COMPLETED)

**Goal:** Support core control flow statements and function definitions (Priority 4).

1. **If Statement Node (`BAA_NODE_KIND_IF_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports `condition`, `then_stmt`, and optional `else_stmt`.

2. **While Statement Node (`BAA_NODE_KIND_WHILE_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**

3. **For Statement Node (`BAA_NODE_KIND_FOR_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports C-style for loops with initializer, condition, and increment.

4. **Return Statement Node (`BAA_NODE_KIND_RETURN_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports optional return value.

5. **Break & Continue Nodes (`BAA_NODE_KIND_BREAK_STMT`, `_CONTINUE_STMT`):**
    * **Files:** `src/ast/ast_statements.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**

6. **Parameter Node (`BAA_NODE_KIND_PARAMETER`):**
    * **Files:** `src/ast/ast_declarations.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**

7. **Function Definition Node (`BAA_NODE_KIND_FUNCTION_DEF`):**
    * **Files:** `src/ast/ast_declarations.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports return type, parameters, body block, and modifiers.

8. **Call Expression Node (`BAA_NODE_KIND_CALL_EXPR`):**
    * **Files:** `src/ast/ast_expressions.c`, `ast_types.h`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Supports callee expression and dynamic argument list.

### Phase 4: Advanced Language Features (Priority 5 - Planned)

**Goal:** Add support for complex data structures and advanced expressions.

1. **Array Literal Expression Node (`BAA_NODE_KIND_ARRAY_LITERAL_EXPR`):**
    * **Status:** 📅 **PLANNED**
    * **Description:** For syntax like `[1, 2, 3]`.

2. **Index Expression Node (`BAA_NODE_KIND_INDEX_EXPR`):**
    * **Status:** 📅 **PLANNED**
    * **Description:** For syntax like `arr[i]`. (Currently partially handled in parser but needs distinct node).

3. **Member Access Node (`BAA_NODE_KIND_MEMBER_ACCESS`):**
    * **Status:** 📅 **PLANNED**
    * **Description:** For syntax like `obj.field` or `obj->field`.

4. **Struct/Union Definition Nodes:**
    * **Status:** 📅 **PLANNED**

5. **Cast Expression Node:**
    * **Status:** 📅 **PLANNED**

### Phase 5: AST Utilities & Infrastructure (In Progress)

1. **Type-Safe Accessor Macros:**
    * **Status:** 🔄 **IN PROGRESS**
    * **Description:** Macros like `BaaNodeGetLiteralData` exist in tester tools but should be moved to a public `ast_utils.h`.

2. **AST Visitor Pattern:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Standardized way to traverse the tree for Semantic Analysis and Codegen.

3. **AST Pretty-Printer:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Output AST structure as text/JSON for debugging.

4. **Parent Pointers:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Evaluate adding `parent` pointers to `BaaNode` to simplify upward traversal during semantic analysis.
