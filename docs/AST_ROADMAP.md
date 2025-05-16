# Baa Language AST Implementation Roadmap (New Design)

**Status: This document outlines the implementation plan for the new Abstract Syntax Tree (AST) following the removal of the previous implementation. All items are planned unless otherwise noted.**

## Core Design Principles (Adopted from AST.md)

- [x] Standardized Node Structure (Base `BaaNode`, `BaaExpr`, `BaaStmt`)
- [x] Data Pointer Approach (`void* data` for specific node data)
- [x] Type Enumeration (`BaaNodeKind`, `BaaExprKind`, `BaaStmtKind`)
- [x] Clear Memory Ownership Rules (Creator allocates, user frees via `baa_free_node`, etc.)
- [x] Consistent String Handling (Duplication for identifiers/literals)

## Phase 1: Foundational AST Structures

- **Base Nodes & Kinds:**
  - [ ] Define `BaaNode`, `BaaExpr`, `BaaStmt` base structs in `ast.h` (or equivalent header).
    - [ ] **New Sub-task:** Clarify and implement the precise interrelationship and hierarchy between `BaaNode`, `BaaExpr`, and `BaaStmt` (e.g., decide on struct embedding vs. unified kind enum vs. distinct hierarchies as per `AST.md` discussion, and implement chosen approach).
  - [ ] Define `BaaNodeKind` enum (`BAA_NODE_PROGRAM`, `BAA_NODE_FUNCTION`, `BAA_NODE_EXPRESSION`, `BAA_NODE_STATEMENT`, etc.).
  - [ ] Define `BaaExprKind` enum.
  - [ ] Define `BaaStmtKind` enum.
  - [ ] Implement `BaaSourceLocation` struct:
    - [ ] Initial implementation with start line/column and filename.
    - [ ] Future Consideration: Evaluate adding end line/column or token length for more precise source mapping.
  - [ ] Implement creation functions (`baa_ast_create_node`, `baa_ast_create_expr`, `baa_ast_create_stmt`).
    - [ ] **New Sub-task:** Implement type-safe accessor helpers/macros for `void* data` payloads (e.g., `AS_LITERAL_EXPR_DATA(node)`) to improve code safety and readability when accessing specific node data.
  - [ ] Implement destruction functions (`baa_ast_free_node`, `baa_ast_free_expr`, `baa_ast_free_stmt`).

- **Basic Literal Expressions (`BAA_EXPR_LITERAL`):**
  - [ ] Define `BaaLiteralKind` enum (`BAA_LITERAL_BOOL`, `BAA_LITERAL_INT`, `BAA_LITERAL_FLOAT`, `BAA_LITERAL_CHAR`, `BAA_LITERAL_STRING`, `BAA_LITERAL_NULL`).
  - [ ] Define `BaaLiteralData` struct (with union for values, ensuring `long long` for int, `double` for float).
  - [ ] Implement creation/freeing for literal expression nodes.

- **Basic Variable Expressions (`BAA_EXPR_VARIABLE`):**
  - [ ] Define `BaaVariableExpr` struct (for `wchar_t* name`).
  - [ ] Implement creation/freeing for variable expression nodes.

- **Basic Statement Types:**
  - [ ] `BAA_STMT_EXPR`: Define `BaaExprStmt` struct.
  - [ ] `BAA_STMT_BLOCK`: Define `BaaBlock` struct (dynamic array of statements).
  - [ ] Implement creation/freeing for these statement nodes.

## Phase 2: Core Expressions & Declarations

- **Unary Expressions (`BAA_EXPR_UNARY`):**
  - [ ] Define `BaaUnaryOpKind` enum.
  - [ ] Define `BaaUnaryExpr` struct.
  - [ ] Implement creation/freeing.
- **Binary Expressions (`BAA_EXPR_BINARY`):**
  - [ ] Define `BaaBinaryOpKind` enum.
  - [ ] Define `BaaBinaryExpr` struct.
  - [ ] Implement creation/freeing.
- **Assignment Expressions (`BAA_EXPR_ASSIGN`):**
  - [ ] Define `BaaAssignExpr` struct.
  - [ ] Implement creation/freeing.
- **Variable Declaration Statements (`BAA_STMT_VAR_DECL`):**
  - [ ] Define `BaaVarDeclStmt` struct (name, type reference, initializer).
  - [ ] Implement creation/freeing.

## Phase 3: Control Flow & Function Calls

- **Function Call Expressions (`BAA_EXPR_CALL`):**
  - [ ] Define `BaaCallExpr` struct (callee, arguments, count).
  - [ ] Implement creation/freeing.
- **If Statements (`BAA_STMT_IF`):**
  - [ ] Define `BaaIfStmt` struct (condition, then_statement, else_statement).
  - [ ] Implement creation/freeing.
- **While Statements (`BAA_STMT_WHILE`):**
  - [ ] Define `BaaWhileStmt` struct (condition, body).
  - [ ] Implement creation/freeing.
- **Return Statements (`BAA_STMT_RETURN`):**
  - [ ] Define `BaaReturnStmt` struct (value).
  - [ ] Implement creation/freeing.

## Phase 4: Advanced Structures & Program Representation

- **Function Definitions (`BAA_NODE_FUNCTION`):**
  - [ ] Define `BaaParameter` struct (name, type reference, modifiers).
  - [ ] Define `BaaFunction` struct (name, return_type_ref, parameters, body, modifiers).
  - [ ] Implement creation/freeing for function and parameter nodes.
- **Program Structure (`BAA_NODE_PROGRAM`):**
  - [ ] Define `BaaProgram` struct (array of top-level nodes).
  - [ ] Implement creation/freeing.
- **For Loops (`BAA_STMT_FOR`):**
  - [ ] Define `BaaForStmt` struct (initializer, condition, increment, body).
  - [ ] Implement creation/freeing.
- **Switch/Case Statements (`BAA_STMT_SWITCH`, `BAA_STMT_CASE`, `BAA_STMT_DEFAULT`):**
  - [ ] Define `BaaSwitchStmt`, `BaaCaseStmt`, `BaaDefaultStmt` structs.
  - [ ] Implement creation/freeing.
- **Break/Continue Statements (`BAA_STMT_BREAK`, `BAA_STMT_CONTINUE`):**
  - [ ] Define `BaaBreakStmt`, `BaaContinueStmt` structs.
  - [ ] Implement creation/freeing.

## Phase 5: Type System Representation in AST

- **Type Nodes/References:**
  - [ ] Define `BaaType` struct and `BaaTypeKind` enum (as per `AST.md` and `types.h`).
    - [ ] Ensure `BaaType` struct includes fields/flags for modifiers (e.g., unsigned, long, long_long for integers; double for floats) to distinguish variations of base types like BAA_TYPE_INT and BAA_TYPE_FLOAT.
  - [ ] Ensure AST nodes that refer to types (e.g., `BaaVarDeclStmt`, `BaaFunction` for return/params, `BaaCastExpr`) use pointers to `BaaType` structs.
  - [ ] Implement creation/freeing for `BaaType` nodes if they are dynamically allocated as part of the AST (or manage them via a separate type table/cache).
- **AST Nodes for Type Definitions:**
    - [ ] Plan and define AST node structures for user-defined types (e.g., `BaaStructDefNode`, `BaaUnionDefNode`, `BaaEnumDefNode`, `BaaTypedefNode`) as these language features are tackled by the parser.
- **Array Expressions & Indexing (`BAA_EXPR_ARRAY`, `BAA_EXPR_INDEX`):**
  - [ ] Define `BaaArrayExpr` and `BaaIndexExpr` structs.
  - [ ] Implement creation/freeing.
- **Cast Expressions (`BAA_EXPR_CAST`):**
  - [ ] Define `BaaCastExpr` struct (target_type_ref, expression).
  - [ ] Implement creation/freeing.

## Phase 6: Advanced AST Infrastructure & Utilities

- **AST Node Enhancements:**
  - [ ] Consider adding parent pointers to AST nodes (evaluate trade-offs: ease of navigation vs. complexity of construction/modification).
  - [ ] Design and implement a unique ID system for AST nodes (for debugging, analysis, cross-referencing).
  - [ ] Refine storage for "Node Modifiers" (e.g., `ثابت`, `مضمن`, `مقيد`) in relevant AST nodes (e.g., using a bitmask or dedicated flags struct in `BaaVarDeclStmt`, `BaaFunction`). Ensure all modifiers from `language.md` are covered.
- **AST Traversal & Transformation:**
  - [ ] Design and implement a basic AST Visitor pattern infrastructure for traversals (e.g., for semantic analysis, code generation).
  - [ ] Plan for AST transformation capabilities (e.g., for desugaring complex constructs or early optimization passes).
- **AST Utilities:**
  - [ ] Implement an AST pretty-printer (outputs a textual representation of the AST, useful for debugging parser output).
  - [ ] Implement AST validation utilities (to check for structural integrity, e.g., correct child node types, non-NULL required children).
  - [ ] AST serialization/deserialization (for debugging or inter-tool communication).

## Future Considerations (Longer Term)

- [ ] AST support for `struct`, `union`, `enum` declarations and usage (covered in Phase 5/6 for node definitions).
- [ ] AST nodes for pointer operations (dereference, address-of).
- [ ] AST representation for module system (`BAA_NODE_IMPORT`, `BAA_NODE_MODULE`).
- [ ] Enhanced error state tracking within AST nodes (e.g., a flag if a node or its subtree contains errors).
- [ ] Support for planned advanced language features (lambdas, pattern matching, async/await) by defining corresponding AST nodes.
- [ ] AST Annotations for Semantic Analysis: Define specific fields in expression/identifier nodes to store resolved types and links to symbol table entries post-semantic analysis.
- [ ] Lvalue/Rvalue Distinction: Investigate if AST nodes need explicit flags or distinct types to represent lvalue contexts for expressions, or if this is purely a semantic analysis concern.
- [ ] Memory Management Optimization: Evaluate and potentially implement an arena allocator or a dedicated memory pool for AST nodes to improve allocation performance and simplify freeing the entire tree.


## Documentation & Testing

- [ ] Update `AST.md` continuously as structures are finalized and implemented.
- [ ] Add Doxygen comments to `ast.h` (or equivalent header) for all public structures and functions.
- [ ] Develop unit tests for each AST node type creation, data storage, and freeing.
- [ ] Add tests for memory management and resource leaks in AST operations.
