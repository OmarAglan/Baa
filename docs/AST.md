# Baa Language AST Structure Documentation (New Design)

**Status: This document outlines the new design for the Abstract Syntax Tree (AST) following the removal of the previous implementation. Items here are largely planned unless otherwise noted.**

This document provides a comprehensive reference for the Abstract Syntax Tree (AST) structure to be implemented for the Baa programming language compiler. The goal is to establish a clear standard for all AST components to ensure consistency across the compiler implementation.

## Core Design Principles

1. **Standardized Node Structure**: All AST components (expressions, statements, declarations) follow a consistent structure.
2. **Data Pointer Approach**: Instead of embedding data in unions, we use a `void* data` pointer to specific data structures.
3. **Type Enumeration**: All node types are represented by enumeration values (BaaExprKind, BaaStmtKind, BaaNodeKind).
4. **Memory Ownership**: Clear rules for memory allocation, transfer, and deallocation.
5. **String Handling**: Consistent approach to string duplication for literals and identifiers.

## Node Hierarchy

The AST uses a hierarchy of node types:

```
BaaNode
├── BaaExpr (expressions)
└── BaaStmt (statements)
```

## Base Node Structure

### BaaNode

```c
typedef struct BaaNode {
    BaaNodeKind kind;     // The type of node
    BaaSourceLocation loc; // Source location information
    void* data;           // Pointer to the actual node data
} BaaNode;
```

### BaaExpr (Expression)

```c
typedef struct BaaExpr {
    BaaExprKind kind;     // The type of expression
    BaaSourceLocation loc; // Source location information
    void* data;           // Pointer to expression-specific data
} BaaExpr;
```

### BaaStmt (Statement)

```c
typedef struct BaaStmt {
    BaaStmtKind kind;     // The type of statement
    BaaSourceLocation loc; // Source location information
    void* data;           // Pointer to statement-specific data
} BaaStmt;
```

### Node Modifiers

Nodes, particularly declarations like functions or variables, can have modifier flags associated with them. These are typically stored as bitfields within the relevant node's data structure (e.g., `BaaFunction`, `BaaVarDeclStmt`).

- Constant (ثابت)
- Static (ساكن)
- Extern (خارجي)
- Export (تصدير)
- Async (متزامن)
- Generator (مولد)
- Abstract (مجرد)
- Final (نهائي)
- Override (تجاوز)

## Node Type Enumerations

### BaaNodeKind

```c
typedef enum {
    BAA_NODE_PROGRAM,      // Top-level program
    BAA_NODE_FUNCTION,     // Function declaration
    BAA_NODE_EXPRESSION,   // Expression node
    BAA_NODE_STATEMENT,    // Statement node
    BAA_NODE_IMPORT,       // Import directive
    BAA_NODE_MODULE        // Module declaration
} BaaNodeKind;
```

### BaaExprKind

```c
typedef enum {
    BAA_EXPR_LITERAL,     // Literal value (number, text, etc.)
    BAA_EXPR_VARIABLE,    // Variable reference
    BAA_EXPR_UNARY,       // Unary operation
    BAA_EXPR_BINARY,      // Binary operation
    BAA_EXPR_CALL,        // Function call
    BAA_EXPR_CAST,        // Type cast expression
    BAA_EXPR_ASSIGN,      // Assignment
    BAA_EXPR_ARRAY,       // Array creation
    BAA_EXPR_INDEX        // Array indexing
} BaaExprKind;
```

### BaaStmtKind

```c
typedef enum {
    BAA_STMT_EXPR,         // Expression statement
    BAA_STMT_BLOCK,        // Block of statements
    BAA_STMT_IF,           // If statement
    BAA_STMT_WHILE,        // While loop
    BAA_STMT_FOR,          // For loop
    BAA_STMT_RETURN,       // Return statement
    BAA_STMT_VAR_DECL,     // Variable declaration
    BAA_STMT_SWITCH,       // Switch statement
    BAA_STMT_CASE,         // Case statement
    BAA_STMT_DEFAULT,      // Default case statement
    BAA_STMT_BREAK,        // Break statement
    BAA_STMT_CONTINUE      // Continue statement
} BaaStmtKind;
```

## Type System

The Baa language type system is represented by the following structures:

```c
// Type kinds
typedef enum {
    BAA_TYPE_VOID,    // فراغ
    BAA_TYPE_INT,     // عدد_صحيح (Covers int, long, long long via modifiers)
    BAA_TYPE_FLOAT,   // عدد_حقيقي (Covers float, double via modifiers)
    BAA_TYPE_CHAR,    // حرف
    // BAA_TYPE_STRING,  // نص - Strings are typically handled as arrays of char or pointer to char
    BAA_TYPE_BOOL,    // منطقي
    // BAA_TYPE_NULL,    // فارغ - Null is usually a literal, not a distinct type itself
    BAA_TYPE_ERROR,   // خطأ (For internal error representation)
    BAA_TYPE_ARRAY,   // مصفوفة
    BAA_TYPE_POINTER, // مؤشر (Planned)
    BAA_TYPE_STRUCT,  // بنية (Planned)
    BAA_TYPE_UNION,   // اتحاد (Planned)
    BAA_TYPE_ENUM     // تعداد (Planned)
} BaaTypeKind;

// Type structure (To be refined, should align with types.h if possible)
typedef struct BaaType {
    BaaTypeKind kind;         // Type kind
    const wchar_t* name;      // Type name in Arabic (e.g., "عدد_صحيح")
    uint32_t size;           // Size in bytes (determined by target and modifiers)
    // Modifiers for basic types (e.g., for int: unsigned, long, long long)
    // bool is_unsigned;
    // bool is_long;
    // bool is_long_long;
    // bool is_double; // For float type
    // Consider a flags field for modifiers.
    struct BaaType* next;    // For linked list of types or for pointer/array base types

    // Array type information (only valid if kind == BAA_TYPE_ARRAY)
    struct BaaType* element_type; // Type of array elements
    size_t array_size;           // Number of elements (0 for dynamic arrays / incomplete)

    // Pointer type information (only valid if kind == BAA_TYPE_POINTER)
    // struct BaaType* pointed_to_type;

    // Struct/Union/Enum information (Planned)
    // struct BaaSymbolTable* members; // For structs/unions
    // struct BaaEnumItem* enum_items; // For enums
} BaaType;
```

## Expression Data Structures

### BaaLiteralData (BAA_EXPR_LITERAL)

```c
typedef enum {
    BAA_LITERAL_BOOL,     // Boolean value
    BAA_LITERAL_INT,      // Integer value
    BAA_LITERAL_FLOAT,    // Floating-point value
    BAA_LITERAL_CHAR,     // Character value
    BAA_LITERAL_STRING,   // String value
    BAA_LITERAL_NULL      // Null value
} BaaLiteralKind;

typedef struct {
    BaaLiteralKind kind;   // Type of literal

    // Value storage (only one is used based on kind)
    union {
        bool bool_value;
        long long int_value; // To accommodate all integer sizes, including those with suffixes
        double float_value;  // To accommodate all float sizes (float, potentially double later)
        wchar_t char_value;
        wchar_t* string_value; // string_value should be duplicated
    };
    // Add fields for suffixes if they affect type/value interpretation at AST level,
    // or handle this during semantic analysis based on full lexeme.
    // For now, assuming lexer provides full lexeme, and parser/semantic analyzer interprets.
} BaaLiteralData;
```

### BaaVariableExpr (BAA_EXPR_VARIABLE)

```c
typedef struct {
    wchar_t* name;         // Variable name (dynamically allocated)
} BaaVariableExpr;
```

### BaaBinaryExpr (BAA_EXPR_BINARY)

```c
typedef enum {
    BAA_OP_ADD,           // Addition (+)
    BAA_OP_SUB,           // Subtraction (-)
    BAA_OP_MUL,           // Multiplication (*)
    BAA_OP_DIV,           // Division (/)
    BAA_OP_MOD,           // Modulo (%)
    BAA_OP_EQ,            // Equality (==)
    BAA_OP_NE,            // Inequality (!=)
    BAA_OP_LT,            // Less than (<)
    BAA_OP_LE,            // Less than or equal (<=)
    BAA_OP_GT,            // Greater than (>)
    BAA_OP_GE,            // Greater than or equal (>=)
    BAA_OP_AND,           // Logical AND (&&)
    BAA_OP_OR,            // Logical OR (||)
    BAA_OP_BIT_AND,       // Bitwise AND (&)
    BAA_OP_BIT_OR,        // Bitwise OR (|)
    BAA_OP_BIT_XOR,       // Bitwise XOR (^)
    BAA_OP_BIT_SHL,       // Bitwise shift left (<<)
    BAA_OP_BIT_SHR        // Bitwise shift right (>>)
} BaaBinaryOpKind;

typedef struct {
    BaaBinaryOpKind op;   // Operator type
    BaaExpr* left;        // Left operand
    BaaExpr* right;       // Right operand
} BaaBinaryExpr;
```

### BaaUnaryExpr (BAA_EXPR_UNARY)

```c
typedef enum {
    BAA_OP_UNARY_MINUS,   // Unary minus (-)
    BAA_OP_NOT,           // Logical NOT (!)
    BAA_OP_BIT_NOT        // Bitwise NOT (~)
} BaaUnaryOpKind;

typedef struct {
    BaaUnaryOpKind op;    // Operator type
    BaaExpr* operand;     // The expression being operated on
} BaaUnaryExpr;
```

### BaaCallExpr (BAA_EXPR_CALL)

```c
typedef struct {
    BaaExpr* callee;      // The function being called
    BaaExpr** arguments;  // Array of argument expressions
    size_t argument_count; // Number of arguments
    wchar_t** named_args; // Names for named arguments (NULL for positional)
    bool is_method_call;  // Whether this is a method call (obj.method())
} BaaCallExpr;
```

### BaaAssignExpr (BAA_EXPR_ASSIGN)

```c
typedef struct {
    BaaExpr* target;      // The target of the assignment (lvalue)
    BaaExpr* value;       // The value being assigned
} BaaAssignExpr;
```

### BaaArrayExpr (BAA_EXPR_ARRAY)

```c
typedef struct {
    BaaExpr** elements;   // Array elements
    size_t element_count; // Number of elements
    BaaType* element_type; // Type of array elements
} BaaArrayExpr;
```

### BaaIndexExpr (BAA_EXPR_INDEX)

```c
typedef struct {
    BaaExpr* array;       // Array being indexed
    BaaExpr* index;       // Index expression
} BaaIndexExpr;
```

## Statement Data Structures

### BaaExprStmt (BAA_STMT_EXPR)

```c
typedef struct {
    BaaExpr* expression;  // The expression to evaluate
} BaaExprStmt;
```

### BaaBlock (BAA_STMT_BLOCK)

```c
typedef struct {
    BaaStmt** statements;  // Array of statements
    size_t count;          // Number of statements
    size_t capacity;       // Capacity of statements array
} BaaBlock;
```

### BaaIfStmt (BAA_STMT_IF)

```c
typedef struct {
    BaaExpr* condition;    // Condition expression
    BaaStmt* then_statement; // Statement to execute if condition is true
    BaaStmt* else_statement; // Statement to execute if condition is false (can be NULL)
} BaaIfStmt;
```

### BaaWhileStmt (BAA_STMT_WHILE)

```c
typedef struct {
    BaaExpr* condition;    // Loop condition
    BaaStmt* body;         // Loop body
} BaaWhileStmt;
```

### BaaForStmt (BAA_STMT_FOR)

```c
typedef struct {
    BaaStmt* initializer;   // Initialization statement (can be expression or variable declaration)
    BaaExpr* condition;     // Loop condition expression
    BaaExpr* increment;     // Increment expression
    BaaBlock* body;         // Loop body
} BaaForStmt;
```

### BaaReturnStmt (BAA_STMT_RETURN)

```c
typedef struct {
    BaaExpr* value;        // Return value (can be NULL for void returns)
} BaaReturnStmt;
```

### BaaVarDeclStmt (BAA_STMT_VAR_DECL)

```c
typedef struct {
    wchar_t* name;         // Variable name
    BaaType* type;         // Variable type
    BaaExpr* initializer;  // Initial value (can be NULL)
} BaaVarDeclStmt;
```

### BaaSwitchStmt (BAA_STMT_SWITCH)

```c
typedef struct {
    BaaExpr* expression;   // Switch expression
    BaaCaseStmt** cases;   // Array of case statements
    size_t case_count;     // Number of cases
    BaaDefaultStmt* default_case; // Default case (can be NULL)
} BaaSwitchStmt;
```

### BaaCaseStmt (BAA_STMT_CASE)

```c
typedef struct {
    BaaExpr* value;        // Case value expression
    BaaBlock* body;        // Case body
    bool has_fallthrough;  // Whether this case falls through to the next one
} BaaCaseStmt;
```

### BaaDefaultStmt (BAA_STMT_DEFAULT)

```c
typedef struct {
    BaaBlock* body;        // Default case body
} BaaDefaultStmt;
```

### BaaBreakStmt (BAA_STMT_BREAK)

```c
typedef struct {
    int loop_depth;       // How many levels to break out of (default: 1)
    bool is_switch_break; // Whether this break is for a switch statement
} BaaBreakStmt;
```

### BaaContinueStmt (BAA_STMT_CONTINUE)

```c
typedef struct {
    int loop_depth;       // How many levels to continue (default: 1)
} BaaContinueStmt;
```

## Top-Level Structure

### BaaProgram

```c
typedef struct {
    BaaNode** nodes;       // Array of top-level nodes
    size_t node_count;     // Number of nodes
    size_t node_capacity;  // Capacity of nodes array
} BaaProgram;
```

### BaaFunction

```c
typedef struct {
    wchar_t* name;              // Function name
    size_t name_length;         // Function name length
    BaaType* return_type;       // Return type
    BaaParameter* parameters;   // Array of parameters
    size_t parameter_count;     // Number of parameters
    size_t parameter_capacity;  // Capacity of parameters array
    BaaBlock* body;             // Function body
    bool is_variadic;           // Whether function accepts variable arguments
    bool is_extern;             // Whether function is externally defined
    bool is_method;             // Whether function is a method on a type
    wchar_t* module_name;       // Module the function belongs to (NULL for current module)
    BaaNode* ast_node;          // Reference to AST node, if any
    wchar_t* documentation;     // Documentation comment for the function
} BaaFunction;
```

### BaaParameter

```c
typedef struct {
    wchar_t* name;          // Parameter name
    size_t name_length;     // Parameter name length
    BaaType* type;          // Parameter type
    bool is_mutable;        // Whether parameter can be modified
    bool is_optional;       // Whether parameter is optional
    BaaExpr* default_value; // Optional default value (NULL if none)
    bool is_rest;           // Whether this is a rest parameter (...args)
} BaaParameter;
```

## Memory Management

### Creation Functions

All node creation functions follow this pattern:
- Allocate memory for the specific node type
- Initialize values (typically with default values or NULL)
- Set up the node structure
- Return the created node

```c
BaaExpr* baa_create_expr(BaaExprKind kind, BaaSourceLocation loc);
BaaStmt* baa_create_stmt(BaaStmtKind kind, BaaSourceLocation loc);
BaaNode* baa_create_node(BaaNodeKind kind, BaaSourceLocation loc);
```

### Destruction Functions

All destruction functions follow this pattern:
- Check if the pointer is NULL (early return if it is)
- Free any child nodes recursively
- Free any owned memory (like strings)
- Free the node itself

```c
void baa_free_expr(BaaExpr* expr);
void baa_free_stmt(BaaStmt* stmt);
void baa_free_node(BaaNode* node);
```

## String Handling

All strings in the AST are handled through these principles:
- Strings are always duplicated when assigned to AST nodes
- Wide character strings (wchar_t*) are used for Unicode support
- The owner of a string is responsible for freeing it

## Usage Examples

### Creating a Literal Expression

```c
BaaSourceLocation loc = baa_get_current_location(parser);
BaaExpr* expr = baa_create_expr(BAA_EXPR_LITERAL, loc);

BaaLiteralData* literal_data = baa_malloc(sizeof(BaaLiteralData));
literal_data->kind = BAA_LITERAL_INT;
literal_data->int_value = 42;

expr->data = literal_data;
```

### Creating an If Statement

```c
BaaSourceLocation loc = baa_get_current_location(parser);
BaaStmt* stmt = baa_create_stmt(BAA_STMT_IF, loc);

BaaIfStmt* if_stmt = baa_malloc(sizeof(BaaIfStmt));
if_stmt->condition = condition_expr;
if_stmt->then_statement = then_stmt;
if_stmt->else_statement = else_stmt;

stmt->data = if_stmt;
```

## Best Practices

1. **Always initialize**: Always initialize all fields in newly created structures
2. **Check for NULL**: Always check return values from creation functions
3. **Free consistently**: Call the appropriate free function for each allocated node
4. **Ownership transfer**: Document when ownership of a node is transferred
5. **String duplication**: Always duplicate strings when storing them in the AST
6. **Type safety**: Cast void* pointers to the appropriate type before using them

## LLVM Code Generation

The code generator traverses the AST and generates LLVM IR based on the node types:

1. It uses a visitor pattern to process each node type
2. For each node, it checks the `kind` field to determine the node type
3. It casts the `data` pointer to the appropriate structure type
4. It generates the corresponding LLVM IR code

For example, with a literal expression:

```c
if (expr->kind == BAA_EXPR_LITERAL) {
    BaaLiteralData* literal_data = (BaaLiteralData*)expr->data;

    switch (literal_data->kind) {
        case BAA_LITERAL_INT:
            return LLVMConstInt(LLVMInt32TypeInContext(context),
                               literal_data->int_value, true);
        // ...other literal types
    }
}
```

## Conclusion

Following this standardized AST structure ensures:
1. Consistency across the compiler
2. Clear memory ownership
3. Simplified code generation
4. Easier maintenance and evolution

Always refer to this documentation when making changes to the AST structure to maintain consistency throughout the codebase.
