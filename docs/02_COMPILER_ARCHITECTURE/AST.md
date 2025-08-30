# Baa Language AST Structure Documentation (New Design v2)

**Status: ✅ CORE AST COMPLETE - This document outlines the implemented design for the Abstract Syntax Tree (AST). Priority 4 (Function Definitions and Calls) completed 2025-07-06. All core AST node types are implemented and functional.**

This document provides a comprehensive reference for the Abstract Syntax Tree (AST) structure to be implemented for the Baa programming language compiler. The goal is to establish a clear standard for all AST components to ensure consistency across the compiler implementation.

## 1. Core Design Principles

1. **Unified Node Structure**: All AST elements (program, declarations, statements, expressions, types) are represented by a single base `BaaNode` structure.
2. **Discriminated Union via `void* data`**: `BaaNode->kind` (an enum `BaaNodeKind`) determines the actual type of the node. `BaaNode->data` points to a specific data structure for that kind.
3. **Type-Safe Accessors**: Macros will be provided for safely casting `BaaNode->data` to the correct specific data type based on `BaaNode->kind`.
4. **Clear Memory Ownership**:
    * All `BaaNode`s and their associated `data` structs are dynamically allocated.
    * The `baa_ast_free_node(BaaNode* node)` function is responsible for recursively freeing a node, its specific data, any owned child nodes, and any duplicated strings within its data.
5. **String Handling**: Strings (like identifiers, string literals) stored in AST data structs are always duplicated (e.g., using `baa_strdup`) and are owned by the AST node. They must be freed when the node is freed.
6. **Source Location Spanning**: Each `BaaNode` stores a `BaaSourceSpan` indicating its start and end location in the source code for accurate error reporting and tooling.

## 2. Base Node Structure and Core Types

### BaaSourceLocation

Defines a point in the source code.

```c
typedef struct {
    const wchar_t* filename; // Pointer to the (canonical) filename string
    size_t line;             // 1-based line number
    size_t column;           // 1-based column number
} BaaSourceLocation;
```

### BaaSourceSpan

Defines a span of source code from a start to an end location.

```c
typedef struct {
    BaaSourceLocation start;
    BaaSourceLocation end;
} BaaSourceSpan;
```

### BaaNodeKind (Illustrative - To be Expanded)

A unified enumeration for all types of AST nodes.

```c
typedef enum {
    // Program Structure
    BAA_NODE_KIND_PROGRAM,          // Top-level program (data: BaaProgramData)
    BAA_NODE_KIND_FUNCTION_DEF,     // Function definition (data: BaaFunctionDefData)
    BAA_NODE_KIND_PARAMETER,        // Function parameter (data: BaaParameterData) - used within BaaFunctionDefData

    // Statements
    BAA_NODE_KIND_EXPR_STMT,        // Expression statement (data: BaaExprStmtData)
    BAA_NODE_KIND_BLOCK_STMT,       // Block of statements (data: BaaBlockStmtData)
    BAA_NODE_KIND_IF_STMT,          // If statement (data: BaaIfStmtData)
    BAA_NODE_KIND_WHILE_STMT,       // While loop (data: BaaWhileStmtData)
    BAA_NODE_KIND_FOR_STMT,         // For loop (data: BaaForStmtData)
    BAA_NODE_KIND_RETURN_STMT,      // Return statement (data: BaaReturnStmtData)
    BAA_NODE_KIND_VAR_DECL_STMT,    // Variable declaration statement (data: BaaVarDeclData)
    BAA_NODE_KIND_BREAK_STMT,       // Break statement (data: NULL or BaaBreakStmtData if labels are supported)
    BAA_NODE_KIND_CONTINUE_STMT,    // Continue statement (data: NULL or BaaContinueStmtData if labels)
    // Future: Switch, Case, Default statements

    // Expressions
    BAA_NODE_KIND_LITERAL_EXPR,     // Literal value (data: BaaLiteralExprData)
    BAA_NODE_KIND_IDENTIFIER_EXPR,  // Identifier/Variable reference (data: BaaIdentifierExprData)
    BAA_NODE_KIND_UNARY_EXPR,       // Unary operation (data: BaaUnaryExprData)
    BAA_NODE_KIND_BINARY_EXPR,      // Binary operation (data: BaaBinaryExprData)
    BAA_NODE_KIND_CALL_EXPR,        // Function call (data: BaaCallExprData)
    BAA_NODE_KIND_ASSIGN_EXPR,      // Assignment (data: BaaAssignExprData)
    BAA_NODE_KIND_CAST_EXPR,        // Type cast (data: BaaCastExprData)
    BAA_NODE_KIND_ARRAY_LITERAL_EXPR, // Array literal e.g., [1,2,3] (data: BaaArrayLiteralExprData)
    BAA_NODE_KIND_INDEX_EXPR,       // Array indexing e.g., arr[idx] (data: BaaIndexExprData)
    // Future: Grouping (parenthesized) expr, member access, etc.

    // Type Representations (parsed type syntax)
    BAA_NODE_KIND_TYPE,             // Represents a type specification (data: BaaTypeAstData)

    // Utility
    BAA_NODE_KIND_UNKNOWN,          // Placeholder or error node
} BaaNodeKind;
```

### BaaNode

The fundamental building block of the AST.

```c
typedef struct BaaNode {
    BaaNodeKind kind;       // The specific kind of this AST node
    BaaSourceSpan span;     // Source location span for this node
    void* data;             // Pointer to kind-specific data structure
    // Future: Maybe a pointer to its resolved BaaType* (from types.c) after semantic analysis
    // BaaType* resolved_type;
} BaaNode;
```

### AST Node Modifiers

Modifiers like `ثابت` (const), `مستقر` (static), `مضمن` (inline), `مقيد` (restrict) will be stored as a bitmask in the relevant specific data structures (e.g., `BaaVarDeclData`, `BaaFunctionDefData`).

```c
typedef uint32_t BaaAstNodeModifiers;
#define BAA_MOD_NONE     (0)
#define BAA_MOD_CONST    (1 << 0) // ثابت
#define BAA_MOD_STATIC   (1 << 1) // مستقر
#define BAA_MOD_INLINE   (1 << 2) // مضمن
#define BAA_MOD_RESTRICT (1 << 3) // مقيد
// Add more as needed
```

## 3. Type-Safe Accessor Macros (Examples)

These macros will be defined in a header like `ast_utils.h`.

```c
// Example: Accessing data for a literal expression node
#define BaaNodeGetLiteralData(node) \
    (((node) && (node)->kind == BAA_NODE_KIND_LITERAL_EXPR) ? (BaaLiteralExprData*)((node)->data) : NULL)

// Example: Accessing data for an if statement node
#define BaaNodeGetIfStmtData(node) \
    (((node) && (node)->kind == BAA_NODE_KIND_IF_STMT) ? (BaaIfStmtData*)((node)->data) : NULL)

// Add similar macros for all specific BaaNodeKind data types.
```

## 4. Specific Node Data Structures (Illustrative Examples)

Below are examples of what the `data` field of a `BaaNode` might point to, based on its `kind`. These will be defined in detail (e.g., in `ast_structs.h` or similar).

### 4.1 Program Structure Data

**BaaProgramData (for `BAA_NODE_KIND_PROGRAM`)**

```c
typedef struct {
    BaaNode** top_level_declarations; // Dynamic array of BaaNode* (function defs, global var decls)
    size_t count;
    size_t capacity;
} BaaProgramData;
```

**BaaFunctionDefData (for `BAA_NODE_KIND_FUNCTION_DEF`)**

```c
typedef struct {
    wchar_t* name;                      // Duplicated function name
    BaaAstNodeModifiers modifiers;      // e.g., static, inline
    BaaNode* return_type_node;          // BaaNode* of kind BAA_NODE_KIND_TYPE
    BaaNode** parameters;               // Dynamic array of BaaNode* (each of kind BAA_NODE_KIND_PARAMETER)
    size_t parameter_count;
    size_t parameter_capacity;
    BaaNode* body;                      // BaaNode* of kind BAA_NODE_KIND_BLOCK_STMT
    bool is_variadic;                   // For C-style varargs (...)
    // Future: BaaSymbol* symbol_entry; // Link to symbol table after resolution
} BaaFunctionDefData;
```

**BaaParameterData (for `BAA_NODE_KIND_PARAMETER`)**

```c
typedef struct {
    wchar_t* name;              // Duplicated parameter name
    BaaNode* type_node;         // BaaNode* of kind BAA_NODE_KIND_TYPE
    // Future: BaaNode* default_value_expr; // For optional parameters
} BaaParameterData;
```

### 4.2 Statement Data Structures

**BaaExprStmtData (for `BAA_NODE_KIND_EXPR_STMT`)**

```c
typedef struct {
    BaaNode* expression; // The expression (BaaNode* with an expression kind)
} BaaExprStmtData;
```

**BaaBlockStmtData (for `BAA_NODE_KIND_BLOCK_STMT`)**

```c
typedef struct {
    BaaNode** statements;   // Dynamic array of BaaNode* (statement kinds)
    size_t count;
    size_t capacity;
    // Future: BaaScope* scope; // Link to its scope in symbol table
} BaaBlockStmtData;
```

**BaaVarDeclData (for `BAA_NODE_KIND_VAR_DECL_STMT`)**

```c
typedef struct {
    wchar_t* name;              // Duplicated variable name
    BaaAstNodeModifiers modifiers; // e.g., const, static
    BaaNode* type_node;         // BaaNode* of kind BAA_NODE_KIND_TYPE (the declared type syntax)
    BaaNode* initializer_expr;  // Optional initializer expression (BaaNode* with an expression kind)
    // Future: BaaSymbol* symbol_entry; // Link to symbol table after resolution
    // BaaType* resolved_canonical_type; // Pointer to canonical BaaType after semantic analysis
} BaaVarDeclData;
```

**BaaIfStmtData (for `BAA_NODE_KIND_IF_STMT`)**

```c
typedef struct {
    BaaNode* condition_expr;    // Condition expression
    BaaNode* then_stmt;         // Statement or block for the 'then' branch
    BaaNode* else_stmt;         // Optional statement or block for the 'else' branch (can be NULL)
} BaaIfStmtData;
```

*(Similar data structures for `BaaWhileStmtData`, `BaaForStmtData`, `BaaReturnStmtData` etc.)*

### 4.3 Expression Data Structures

**BaaLiteralExprData (for `BAA_NODE_KIND_LITERAL_EXPR`)**

```c
typedef enum {
    BAA_LITERAL_KIND_BOOL,
    BAA_LITERAL_KIND_INT,
    BAA_LITERAL_KIND_FLOAT,
    BAA_LITERAL_KIND_CHAR,
    BAA_LITERAL_KIND_STRING,
    BAA_LITERAL_KIND_NULL,
} BaaLiteralKind;

typedef struct {
    BaaLiteralKind literal_kind;
    union {
        bool bool_value;
        long long int_value;      // To accommodate all integer sizes
        double float_value;     // To accommodate all float sizes
        wchar_t char_value;
        wchar_t* string_value;    // Duplicated string
    } value;
    BaaType* determined_type;   // Canonical BaaType* set by parser based on literal form & suffixes
    // wchar_t* original_lexeme; // Optional: for precise error reporting or debugging
} BaaLiteralExprData;
```

**BaaIdentifierExprData (for `BAA_NODE_KIND_IDENTIFIER_EXPR`)**

```c
typedef struct {
    wchar_t* name;              // Duplicated identifier name
    // Future: BaaSymbol* resolved_symbol; // Link to symbol table entry after resolution
} BaaIdentifierExprData;
```

**BaaUnaryExprData (for `BAA_NODE_KIND_UNARY_EXPR`)**

```c
typedef enum { /* UNARY_PLUS, UNARY_MINUS, LOGICAL_NOT, BITWISE_NOT, ... */ } BaaUnaryOperatorKind;
typedef struct {
    BaaUnaryOperatorKind op_kind;
    BaaNode* operand_expr;
} BaaUnaryExprData;
```

**BaaBinaryExprData (for `BAA_NODE_KIND_BINARY_EXPR`)**

```c
typedef enum { /* ADD, SUB, MUL, DIV, EQUAL, LESS_THAN, LOGICAL_AND, ... */ } BaaBinaryOperatorKind;
typedef struct {
    BaaBinaryOperatorKind op_kind;
    BaaNode* left_expr;
    BaaNode* right_expr;
} BaaBinaryExprData;
```

*(Similar data structures for `BaaAssignExprData`, `BaaCallExprData`, etc.)*

### 4.4 Type Representation Node Data

**BaaTypeAstData (for `BAA_NODE_KIND_TYPE`)**
This structure represents the type *as parsed from the source code*. Semantic analysis will resolve this to a canonical `BaaType*` from the `types.c` system.

```c
typedef enum {
    BAA_TYPE_AST_KIND_PRIMITIVE, // e.g., "عدد_صحيح", "حرف"
    BAA_TYPE_AST_KIND_ARRAY,
    BAA_TYPE_AST_KIND_POINTER,   // Future
    BAA_TYPE_AST_KIND_USER_DEFINED, // Future, for struct/enum names
} BaaTypeAstKind;

typedef struct {
    BaaTypeAstKind type_ast_kind;
    union {
        struct { // For BAA_TYPE_AST_KIND_PRIMITIVE
            wchar_t* name; // Duplicated (e.g., L"عدد_صحيح")
                           // Alternatively, an enum for known primitive type keywords
        } primitive;
        struct { // For BAA_TYPE_AST_KIND_ARRAY
            BaaNode* element_type_node; // BaaNode* of kind BAA_NODE_KIND_TYPE
            BaaNode* size_expr;         // Optional: BaaNode* expression for array size
        } array;
        // Future: struct for pointer, struct/union/enum names
    } specifier;
    // BaaType* resolved_canonical_type; // Populated by semantic analyzer
} BaaTypeAstData;
```

## 5. Memory Management Functions (API Sketch)

Defined in `ast.h` or similar.

```c
// Creation functions
BaaNode* baa_ast_new_program_node(BaaSourceSpan span);
BaaNode* baa_ast_new_literal_int_node(BaaSourceSpan span, long long value, BaaType* determined_type);
// ... one creation function per BaaNodeKind, taking necessary data and span.
// These functions will allocate BaaNode and its specific data struct, initialize them,
// and duplicate any strings.

// Destruction function
void baa_ast_free_node(BaaNode* node); // Master free function
```

`baa_ast_free_node` will internally dispatch to specific data-freeing functions based on `node->kind`.

## 6. AST Traversal (Visitor Pattern - API Sketch)

A visitor pattern will be defined for traversing the AST (e.g., for semantic analysis, code generation, pretty-printing).

```c
// Forward declare
typedef struct BaaASTVisitor BaaASTVisitor;

// The accept function, implemented by each node type implicitly via a master dispatch
// or each specific node data struct could have an accept method if we used a more OO C style.
// For now, a master dispatch in ast_traversal.c is likely.
// void baa_ast_node_accept(BaaNode* node, BaaASTVisitor* visitor, void* user_data);

// Visitor struct with function pointers for each node kind
struct BaaASTVisitor {
    // Return bool: true to continue traversal, false to stop
    // Pre-visit result controls if children are visited. Post-visit for cleanup/aggregation.
    bool (*pre_visit_node)(BaaNode* node, void* user_data, BaaASTVisitor* self); // Generic pre-visit
    void (*post_visit_node)(BaaNode* node, void* user_data, BaaASTVisitor* self); // Generic post-visit

    // Optional: Specific visit functions if generic pre/post is not enough.
    // bool (*visit_literal_expr_data)(BaaLiteralExprData* data, void* user_data, BaaASTVisitor* self);
    // ... and so on for all *Data structs ...

    // OR, just one visit function per node if pre/post are not desired
    // void (*visit_node_kind_xxxx)(BaaNode* node, void* user_data);
};

// Traversal function
void baa_ast_traverse(BaaNode* root_node, BaaASTVisitor* visitor, void* user_data);
```

The `baa_ast_traverse` function would implement a depth-first traversal, calling `pre_visit_node`, then recursively traversing children (if `pre_visit_node` returns true), and finally calling `post_visit_node`.
