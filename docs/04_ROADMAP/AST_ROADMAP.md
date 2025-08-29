## Updated `docs/AST_ROADMAP.md` (v2 - Detailed)

**Status: This document outlines the detailed implementation plan for the new Abstract Syntax Tree (AST), following Design v2. All items are planned unless otherwise noted.**

The new AST (Abstract Syntax Tree) will be built around a unified `BaaNode` structure. Each `BaaNode` will have a `kind` (from a comprehensive `BaaNodeKind` enum) and a `void* data` pointer to a specific data structure tailored to that kind. It will also include a `BaaSourceSpan` for precise error reporting. Memory management will be a key focus, with dedicated creation and freeing functions.

### Phase 0: Core Definitions & Setup

**Goal:** Establish the absolute foundational types and structures for the AST. **(Largely Completed)**

1. **Define `BaaSourceLocation` and `BaaSourceSpan`:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Description:** Implement the `BaaSourceLocation` (filename, line, column) and `BaaSourceSpan` (start and end `BaaSourceLocation`) structs. These are fundamental for error reporting and tooling.
    * **Details:**
        * `filename` in `BaaSourceLocation` should be `const wchar_t*` (assuming canonical, shared strings for filenames).
        * Line and column are `size_t` and 1-based.
    * **Status:** [x] COMPLETED (Step 0.1)

2. **Define Initial `BaaNodeKind` Enum:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Description:** Create the `BaaNodeKind` enum. Start with a few essential kinds needed for early testing, like `BAA_NODE_KIND_PROGRAM`, `BAA_NODE_KIND_LITERAL_EXPR`, `BAA_NODE_KIND_UNKNOWN`.
    * **Details:** Plan for future expansion to cover all language constructs.
    * **Status:** [x] COMPLETED (Step 0.1, includes `_LITERAL_EXPR` now)

3. **Define `BaaNode` Base Structure:**
    * **File:** `include/baa/ast/ast_types.h` (definition is there)
    * **Description:** Implement the `BaaNode` struct containing `BaaNodeKind kind`, `BaaSourceSpan span`, and `void* data`.
    * **Details:** Include comments explaining each field's purpose.
    * **Status:** [x] COMPLETED (Step 0.1)

4. **Define `BaaAstNodeModifiers` and Basic Flags:**
    * **File:** `include/baa/ast/ast_types.h`
    * **Description:** Define `typedef uint32_t BaaAstNodeModifiers;` and initial `#define` constants for `BAA_MOD_NONE`, `BAA_MOD_CONST`.
    * **Status:** [x] COMPLETED (Step 0.1)

5. **Basic AST Memory Utilities (Prototypes):**
    * **File:** `include/baa/ast/ast.h`
    * **Description:** Declare prototypes for the core node creation and destruction functions:
        * `BaaNode* baa_ast_new_node(BaaNodeKind kind, BaaSourceSpan span);` (allocates `BaaNode` and initializes basic fields, `data` is initially NULL or handled by more specific functions).
        * `void baa_ast_free_node(BaaNode* node);` (master free function).
    * **Details:** These will be the workhorses for AST manipulation.
    * **Status:** [x] COMPLETED (Step 0.2)

6. **Basic AST Memory Utilities (Implementation):**
    * **File:** `src/ast/ast_node.c`
    * **Description:** Implement `baa_ast_new_node`. This function will:
        * Allocate memory for a `BaaNode`.
        * Initialize `kind` and `span`.
        * Initialize `data` to `NULL`.
        * Return the allocated node or `NULL` on failure.
    * Implement the basic structure for `baa_ast_free_node`. Initially, it might just free `node->data` (if not NULL) and then `node` itself. The recursive freeing of children and specific data will be added as node types are implemented.
    * **Status:** [x] COMPLETED (Step 0.3)

### Phase 1: Minimal Expressions & Statements

**Goal:** Implement a few essential AST node types to allow the parser to start building a very basic tree.

1. **Literal Expression Node (`BAA_NODE_KIND_LITERAL_EXPR`):**
    * **File (Defs):** `ast_expressions.h` (for `BaaLiteralExprData`), `ast_types.h` (add `BAA_NODE_KIND_LITERAL_EXPR` to enum).
    * **File (Funcs):** `include/baa/ast/ast.h` (public creation prototypes), `src/ast/ast_expressions.h` (internal free helper prototype), `src/ast/ast_expressions.c` (implementations).
    * **Description:**
        * Define `BaaLiteralKind` enum (`BAA_LITERAL_KIND_INT`, `BAA_LITERAL_KIND_STRING`, etc.).
        * Define `BaaLiteralExprData` struct with `BaaLiteralKind literal_kind`, the `union value`, and `BaaType* determined_type` (pointer to canonical type).
        * Implement `BaaNode* baa_ast_new_literal_int_node(BaaSourceSpan span, long long value, BaaType* type);`
        * Implement `BaaNode* baa_ast_new_literal_string_node(BaaSourceSpan span, const wchar_t* value, BaaType* type);` (duplicates string).
        * Implement helper `void baa_ast_free_literal_expr_data(BaaLiteralExprData* data);`.
        * Update `baa_ast_free_node`'s dispatch to call the helper.
    * **Status:** [x] COMPLETED (Steps 1.1, 1.2) - 2025-01-04

2. **Identifier Expression Node (`BAA_NODE_KIND_IDENTIFIER_EXPR`):**
    * **File (Defs):** `include/baa/ast/ast_types.h` (add kind), `src/ast/ast_expressions.h` (for `BaaIdentifierExprData`).
    * **File (Funcs):** `include/baa/ast/ast.h` (public creation), `src/ast/ast_expressions.c` (implementation).
    * **Description:**
        * Define `BaaIdentifierExprData` struct with `wchar_t* name;`.
        * Implement `BaaNode* baa_ast_new_identifier_expr_node(BaaSourceSpan span, const wchar_t* name);` (duplicates name).
        * Update `baa_ast_free_node` to call `baa_ast_free_identifier_expr_data(BaaIdentifierExprData* data);` which frees `name`.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can create, inspect, and free identifier nodes. Name is correctly duplicated and freed.

3. **Expression Statement Node (`BAA_NODE_KIND_EXPR_STMT`):**
    * **File (Defs):** `include/baa/ast/ast_types.h` (add kind), `src/ast/ast_statements.h` (for `BaaExprStmtData`).
    * **File (Funcs):** `include/baa/ast/ast.h` (public creation), `src/ast/ast_statements.c` (implementation).
    * **Description:**
        * Define `BaaExprStmtData` struct with `BaaNode* expression;`.
        * Implement `BaaNode* baa_ast_new_expr_stmt_node(BaaSourceSpan span, BaaNode* expression_node);`.
        * Update `baa_ast_free_node` to call `baa_ast_free_expr_stmt_data(BaaExprStmtData* data);` which recursively calls `baa_ast_free_node(data->expression);`.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can create expression statements wrapping literal or identifier nodes. Freeing is recursive.

4. **Block Statement Node (`BAA_NODE_KIND_BLOCK_STMT`):**
    * **File (Defs):** `include/baa/ast/ast_types.h` (add kind), `src/ast/ast_statements.h` (for `BaaBlockStmtData`).
    * **File (Funcs):** `include/baa/ast/ast.h` (public creation), `src/ast/ast_statements.c` (implementation).
    * **Description:**
        * Define `BaaBlockStmtData` struct with `BaaNode** statements; size_t count; size_t capacity;`.
        * Implement `BaaNode* baa_ast_new_block_stmt_node(BaaSourceSpan span);`.
        * Implement `bool baa_ast_add_stmt_to_block(BaaNode* block_node, BaaNode* statement_node);` (handles dynamic array resizing).
        * Update `baa_ast_free_node` to call `baa_ast_free_block_stmt_data(BaaBlockStmtData* data);` which iterates through `statements`, calls `baa_ast_free_node` on each, then frees the `statements` array.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can create blocks, add statements. Freeing is recursive. Dynamic array works.

5. **Program Node (`BAA_NODE_KIND_PROGRAM`):**
    * **File (Defs):** `include/baa/ast/ast_types.h` (kind already added), `src/ast/ast_program.h` (for `BaaProgramData`).
    * **File (Funcs):** `include/baa/ast/ast.h` (public creation), `src/ast/ast_program.c` (implementation).
    * **Description:**
        * Define `BaaProgramData` struct similar to `BaaBlockStmtData` but for `top_level_declarations`.
        * Implement `BaaNode* baa_ast_new_program_node(BaaSourceSpan span);`.
        * Implement `bool baa_ast_add_declaration_to_program(BaaNode* program_node, BaaNode* declaration_node);`.
        * Implement creation, adding declarations, and freeing logic analogous to `BaaBlockStmtData`.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can create the root program node.

6. **Type-Safe Accessor Macros (Initial Set):**
    * **File:** `ast_utils.h`
    * **Description:** Implement the first set of accessor macros for the node types created in this phase (e.g., `BaaNodeGetLiteralData`, `BaaNodeGetBlockStmtData`).
    * **Details:** Macros should check `node`, `node->data`, and `node->kind` before casting and returning.
    * **Verification:** Macros compile and work correctly for valid and invalid node inputs.

### Phase 2: Basic Declarations & Binary/Unary Expressions ✅ COMPLETED

**Goal:** ✅ Expand AST capabilities to represent simple declarations and common expressions.

1. **Variable Declaration Statement Node (`BAA_NODE_KIND_VAR_DECL_STMT`):** ✅ COMPLETED
    * **File (Defs):** `ast_declarations.h` (for `BaaVarDeclData`), `ast_types.h` (add kind).
    * **File (Funcs):** `ast_declarations.c`.
    * **Description:**
        * ✅ Define `BaaVarDeclData` with `wchar_t* name; BaaAstNodeModifiers modifiers; BaaNode* type_node; BaaNode* initializer_expr;`.
        * ✅ Implement `baa_ast_new_var_decl_node(...)`.
        * ✅ Update `baa_ast_free_node` for `BaaVarDeclData` (free name, type_node, initializer_expr).
    * **Verification:** ✅ Can represent `ثابت عدد_صحيح س = 10.`.

2. **Type Representation Node (`BAA_NODE_KIND_TYPE`):**
    * **File (Defs):** `ast_types.h` (for `BaaTypeAstData`, `BaaTypeAstKind`), update `BaaNodeKind`.
    * **File (Funcs):** `ast_types.c`.
    * **Description:**
        * Define `BaaTypeAstKind` (`PRIMITIVE`, `ARRAY`, etc.).
        * Define `BaaTypeAstData` union to hold specific type syntax info (e.g., primitive name string, array element type node).
        * Implement `baa_ast_new_primitive_type_node(BaaSourceSpan span, const wchar_t* type_name_str);`.
        * Implement `baa_ast_new_array_type_node(BaaSourceSpan span, BaaNode* element_type_node, BaaNode* size_expr_node);`.
        * Update `baa_ast_free_node` for `BaaTypeAstData`.
    * **Verification:** Can represent simple type syntax like `عدد_صحيح` and `حرف[]`.

3. **Binary Expression Node (`BAA_NODE_KIND_BINARY_EXPR`):** ✅ COMPLETED
    * **File (Defs):** `ast_expressions.h` (for `BaaBinaryExprData`, `BaaBinaryOperatorKind`), `ast_types.h` (add kind).
    * **File (Funcs):** `ast_expressions.c`.
    * **Description:**
        * ✅ Define `BaaBinaryOperatorKind` enum (ADD, SUB, EQUAL, etc.).
        * ✅ Define `BaaBinaryExprData` with `BaaBinaryOperatorKind op_kind; BaaNode* left_expr; BaaNode* right_expr;`.
        * ✅ Implement `baa_ast_new_binary_expr_node(...)`.
        * ✅ Update `baa_ast_free_node` (recursively free left/right).
    * **Verification:** ✅ Can represent `a + b`.

4. **Unary Expression Node (`BAA_NODE_KIND_UNARY_EXPR`):** ✅ COMPLETED
    * **File (Defs):** `ast_expressions.h` (for `BaaUnaryExprData`, `BaaUnaryOperatorKind`), `ast_types.h` (add kind).
    * **File (Funcs):** `ast_expressions.c`.
    * **Description:**
        * ✅ Define `BaaUnaryOperatorKind` enum (UNARY_MINUS, LOGICAL_NOT, etc.).
        * ✅ Define `BaaUnaryExprData` with `BaaUnaryOperatorKind op_kind; BaaNode* operand_expr;`.
        * Implement `baa_ast_new_unary_expr_node(...)`.
        * Update `baa_ast_free_node` (recursively free operand).
    * **Verification:** Can represent `-x` or `!flag`.

5. **Assignment Expression Node (`BAA_NODE_KIND_ASSIGN_EXPR`):**
    * **File (Defs):** `ast_expressions.h` (for `BaaAssignExprData`), `ast_types.h` (add kind).
    * **File (Funcs):** `ast_expressions.c`.
    * **Description:**
        * Define `BaaAssignExprData` with `BaaNode* target_lvalue; BaaNode* value_expr;`. (Note: Target must be an lvalue, e.g., identifier or index expr).
        * Implement `baa_ast_new_assign_expr_node(...)`.
        * Update `baa_ast_free_node`.
    * **Verification:** Can represent `x = y`.

### Phase 3: Control Flow & Function Definitions

**Goal:** Support core control flow statements and function definitions.

1. **If Statement Node (`BAA_NODE_KIND_IF_STMT`):**
    * **File (Defs/Funcs):** `ast_statements.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaIfStmtData` (`condition_expr`, `then_stmt`, `else_stmt`). Implement creation/freeing.
    * **Verification:** Can represent `إذا (x > 0) { ... } وإلا { ... }`.

2. **Return Statement Node (`BAA_NODE_KIND_RETURN_STMT`):**
    * **File (Defs/Funcs):** `ast_statements.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaReturnStmtData` (`value_expr` (optional)). Implement creation/freeing.
    * **Verification:** Can represent `إرجع x.` and `إرجع.`.

3. **Parameter Node (`BAA_NODE_KIND_PARAMETER`):** ✅ **COMPLETED** (2025-07-06)
    * **File (Defs/Funcs):** `ast_declarations.h`/`.c` (for `BaaParameterData`), `ast_types.h`.
    * **Description:** ✅ Defined `BaaParameterData` (`name`, `type_node`). ✅ Implemented creation/freeing.
    * **Verification:** ✅ Can represent function parameters like `عدد_صحيح x`.

4. **Function Definition Node (`BAA_NODE_KIND_FUNCTION_DEF`):** ✅ **COMPLETED** (2025-07-06)
    * **File (Defs/Funcs):** `ast_declarations.h`/`.c` (for `BaaFunctionDefData`), `ast_types.h`.
    * **Description:** ✅ Defined `BaaFunctionDefData` (`name`, `modifiers`, `return_type_node`, `parameters` (array of `BaaNode*` of kind `PARAMETER`), `body` (block stmt), `is_variadic`). ✅ Implemented creation/freeing with parameter management utilities.
    * **Verification:** ✅ Can represent full function definitions with Arabic syntax.

5. **Call Expression Node (`BAA_NODE_KIND_CALL_EXPR`):** ✅ **COMPLETED** (2025-07-06)
    * **File (Defs/Funcs):** `ast_expressions.h`/`.c` (for `BaaCallExprData`), `ast_types.h`.
    * **Description:** ✅ Defined `BaaCallExprData` (`callee_expr`, `arguments` (array of `BaaNode*` expression kinds)). ✅ Implemented creation/freeing with argument management utilities.
    * **Verification:** ✅ Can represent function calls like `جمع(أ، ب)` with proper argument handling.

6. **While Statement Node (`BAA_NODE_KIND_WHILE_STMT`):**
    * **File (Defs/Funcs):** `ast_statements.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaWhileStmtData` (`condition_expr`, `body_stmt`). Implement creation/freeing.
    * **Verification:** Can represent `طالما (x < 10) { ... }`.

### Phase 4: Advanced Expressions and Statements

**Goal:** Add support for more complex language features.

1. **For Statement Node (`BAA_NODE_KIND_FOR_STMT`):**
    * **File (Defs/Funcs):** `ast_statements.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaForStmtData` (`initializer_stmt` (var decl or expr stmt), `condition_expr`, `increment_expr`, `body_stmt`). Implement creation/freeing.
    * **Verification:** Can represent `لكل (عدد_صحيح i = 0; i < 10; i++) { ... }`.

2. **Break & Continue Statement Nodes (`BAA_NODE_KIND_BREAK_STMT`, `BAA_NODE_KIND_CONTINUE_STMT`):**
    * **File (Defs/Funcs):** `ast_statements.h`/`.c`, `ast_types.h`.
    * **Description:** Data struct might be empty or hold label info later. Implement creation/freeing.
    * **Verification:** Simple break/continue can be represented.

3. **Array Literal Expression Node (`BAA_NODE_KIND_ARRAY_LITERAL_EXPR`):**
    * **File (Defs/Funcs):** `ast_expressions.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaArrayLiteralExprData` (`elements` (array of `BaaNode*` expression kinds)). Implement creation/freeing.
    * **Verification:** Can represent `[1, 2, "مرحبا"]`.

4. **Index Expression Node (`BAA_NODE_KIND_INDEX_EXPR`):**
    * **File (Defs/Funcs):** `ast_expressions.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaIndexExprData` (`array_expr`, `index_expr`). Implement creation/freeing.
    * **Verification:** Can represent `my_array[i]`.

5. **Cast Expression Node (`BAA_NODE_KIND_CAST_EXPR`):**
    * **File (Defs/Funcs):** `ast_expressions.h`/`.c`, `ast_types.h`.
    * **Description:** Define `BaaCastExprData` (`target_type_node` (kind `TYPE`), `expression_to_cast`). Implement creation/freeing.
    * **Verification:** Can represent `(عدد_حقيقي)my_int_var`.

### Phase 5: AST Infrastructure & Utilities

**Goal:** Finalize core AST utilities and prepare for semantic analysis and code generation.

1. **Complete Type-Safe Accessor Macros:**
    * **File:** `ast_utils.h`
    * **Description:** Implement accessor macros for ALL `BaaNodeKind` data types defined.
    * **Verification:** All macros are present and function correctly.

2. **AST Visitor Pattern Implementation:**
    * **File:** `ast_visitor.h`, `ast_visitor.c`
    * **Description:**
        * Define `BaaASTVisitor` struct with `pre_visit_node` and `post_visit_node` function pointers (or more specific visit methods if chosen).
        * Implement `void baa_ast_traverse(BaaNode* root_node, BaaASTVisitor* visitor, void* user_data);` to perform a depth-first traversal.
    * **Verification:** A simple visitor can traverse a basic AST.

3. **AST Pretty-Printer Utility:**
    * **File:** `ast_printer.h`, `ast_printer.c`
    * **Description:** Implement an AST pretty-printer using the Visitor pattern. It should output a human-readable textual representation of the AST.
    * **Details:** Output should clearly show node kinds, data (e.g., literal values, operator types, identifier names), and tree structure (indentation).
    * **Verification:** Pretty-printer output matches expected for various AST structures.

4. **Parent Pointers (Decision & Implementation if chosen):**
    * **Description:** Evaluate and decide whether to add `BaaNode* parent;` to `BaaNode`.
    * If yes: Update `baa_ast_new_node` and all functions that build the tree structure (e.g., `baa_ast_add_stmt_to_block`, `baa_ast_new_binary_expr_node`) to correctly set parent pointers. This adds complexity to tree construction and modification but can simplify some analyses.
    * **Verification:** Parent pointers are correctly set and managed.

5. **Unique Node IDs (Optional Enhancement):**
    * **Description:** Consider adding a `size_t node_id;` to `BaaNode`, assigned uniquely upon creation. Useful for debugging and analysis.
    * **Verification:** IDs are unique.

### Future Considerations (Beyond initial redesign scope)

* AST support for `struct`, `union`, `enum` declarations and member access.
* AST nodes for pointer operations (dereference, address-of).
* AST representation for module system (`#تضمين` if parser handles, explicit `module` keywords).
* AST Annotations for Semantic Analysis results (e.g., `resolved_type`, `symbol_table_entry` pointers within existing node data structures or `BaaNode` itself).
