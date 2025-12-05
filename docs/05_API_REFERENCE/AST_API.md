# Baa AST API Reference

**Status:** ✅ Complete  
**Last Updated:** 2025-12-05  
**Version Compatibility:** v0.1.27.0+

## Overview

The Baa AST (Abstract Syntax Tree) module provides the data structures and functions for representing parsed Baa source code. It implements a unified `BaaNode` architecture where all AST elements share a common base structure.

## Core Data Structures

### BaaNode

The fundamental building block of the AST.

```c
typedef struct BaaNode {
    BaaNodeKind kind;       // Type of node
    BaaAstSourceSpan span;  // Source location
    void *data;             // Kind-specific data pointer
} BaaNode;
```

### BaaAstSourceSpan

Tracks source code location for error reporting.

```c
typedef struct BaaAstSourceSpan {
    BaaSourceLocation start;  // Start position (file, line, column)
    BaaSourceLocation end;    // End position
} BaaAstSourceSpan;
```

### BaaNodeKind

Enumeration of all AST node types:

| Kind | Description |
|------|-------------|
| `BAA_NODE_KIND_PROGRAM` | Root node containing all declarations |
| `BAA_NODE_KIND_FUNCTION_DEF` | Function definition |
| `BAA_NODE_KIND_PARAMETER` | Function parameter |
| `BAA_NODE_KIND_VAR_DECL_STMT` | Variable declaration |
| `BAA_NODE_KIND_BLOCK_STMT` | Block of statements `{ }` |
| `BAA_NODE_KIND_IF_STMT` | If statement (إذا) |
| `BAA_NODE_KIND_WHILE_STMT` | While loop (طالما) |
| `BAA_NODE_KIND_FOR_STMT` | For loop (لكل) |
| `BAA_NODE_KIND_RETURN_STMT` | Return statement (إرجع) |
| `BAA_NODE_KIND_BREAK_STMT` | Break statement |
| `BAA_NODE_KIND_CONTINUE_STMT` | Continue statement |
| `BAA_NODE_KIND_EXPR_STMT` | Expression statement |
| `BAA_NODE_KIND_LITERAL_EXPR` | Literal value (int, string, etc.) |
| `BAA_NODE_KIND_IDENTIFIER_EXPR` | Identifier expression |
| `BAA_NODE_KIND_BINARY_EXPR` | Binary expression (a + b) |
| `BAA_NODE_KIND_UNARY_EXPR` | Unary expression (-a, !b) |
| `BAA_NODE_KIND_CALL_EXPR` | Function call |
| `BAA_NODE_KIND_TYPE` | Type specification |

## Core Functions

### Node Lifecycle

#### `baa_ast_new_node`
```c
BaaNode *baa_ast_new_node(BaaNodeKind kind, BaaAstSourceSpan span);
```
Creates a new generic AST node. The `data` field is initialized to NULL.

#### `baa_ast_free_node`
```c
void baa_ast_free_node(BaaNode *node);
```
Recursively frees a node and all its children. Safe to call with NULL.

### Expression Nodes

#### `baa_ast_new_literal_int_node`
```c
BaaNode *baa_ast_new_literal_int_node(BaaAstSourceSpan span, long long value, BaaType *type);
```
Creates an integer literal node.

#### `baa_ast_new_literal_string_node`
```c
BaaNode *baa_ast_new_literal_string_node(BaaAstSourceSpan span, const wchar_t *value, BaaType *type);
```
Creates a string literal node. The string is duplicated.

#### `baa_ast_new_identifier_expr_node`
```c
BaaNode *baa_ast_new_identifier_expr_node(BaaAstSourceSpan span, const wchar_t *name);
```
Creates an identifier expression node.

#### `baa_ast_new_binary_expr_node`
```c
BaaNode *baa_ast_new_binary_expr_node(BaaAstSourceSpan span, BaaNode *left, BaaNode *right, BaaBinaryOperatorKind op);
```
Creates a binary expression node.

#### `baa_ast_new_unary_expr_node`
```c
BaaNode *baa_ast_new_unary_expr_node(BaaAstSourceSpan span, BaaNode *operand, BaaUnaryOperatorKind op);
```
Creates a unary expression node.

#### `baa_ast_new_call_expr_node`
```c
BaaNode *baa_ast_new_call_expr_node(BaaAstSourceSpan span, BaaNode *callee);
```
Creates a function call expression node with empty arguments.

#### `baa_ast_add_call_argument`
```c
bool baa_ast_add_call_argument(BaaNode *call_node, BaaNode *argument);
```
Adds an argument to a call expression.

### Statement Nodes

#### `baa_ast_new_block_stmt_node`
```c
BaaNode *baa_ast_new_block_stmt_node(BaaAstSourceSpan span);
```
Creates an empty block statement.

#### `baa_ast_add_stmt_to_block`
```c
bool baa_ast_add_stmt_to_block(BaaNode *block, BaaNode *statement);
```
Adds a statement to a block.

#### `baa_ast_new_if_stmt_node`
```c
BaaNode *baa_ast_new_if_stmt_node(BaaAstSourceSpan span, BaaNode *condition, BaaNode *then_stmt, BaaNode *else_stmt);
```
Creates an if statement. `else_stmt` can be NULL.

#### `baa_ast_new_while_stmt_node`
```c
BaaNode *baa_ast_new_while_stmt_node(BaaAstSourceSpan span, BaaNode *condition, BaaNode *body);
```
Creates a while loop statement.

#### `baa_ast_new_for_stmt_node`
```c
BaaNode *baa_ast_new_for_stmt_node(BaaAstSourceSpan span, BaaNode *init, BaaNode *condition, BaaNode *increment, BaaNode *body);
```
Creates a for loop statement. `init`, `condition`, and `increment` can be NULL.

#### `baa_ast_new_return_stmt_node`
```c
BaaNode *baa_ast_new_return_stmt_node(BaaAstSourceSpan span, BaaNode *value);
```
Creates a return statement. `value` can be NULL for void returns.

### Declaration Nodes

#### `baa_ast_new_var_decl_node`
```c
BaaNode *baa_ast_new_var_decl_node(BaaAstSourceSpan span, const wchar_t *name, 
                                   BaaAstNodeModifiers modifiers, BaaNode *type, BaaNode *init);
```
Creates a variable declaration. `init` can be NULL.

#### `baa_ast_new_function_def_node`
```c
BaaNode *baa_ast_new_function_def_node(BaaAstSourceSpan span, const wchar_t *name,
                                        BaaAstNodeModifiers modifiers, BaaNode *return_type,
                                        BaaNode *body, bool is_variadic);
```
Creates a function definition with empty parameters.

#### `baa_ast_add_function_parameter`
```c
bool baa_ast_add_function_parameter(BaaNode *func, BaaNode *param);
```
Adds a parameter to a function definition.

#### `baa_ast_new_program_node`
```c
BaaNode *baa_ast_new_program_node(BaaAstSourceSpan span);
```
Creates the program root node.

#### `baa_ast_add_declaration_to_program`
```c
bool baa_ast_add_declaration_to_program(BaaNode *program, BaaNode *decl);
```
Adds a top-level declaration to the program.

### Type Nodes

#### `baa_ast_new_primitive_type_node`
```c
BaaNode *baa_ast_new_primitive_type_node(BaaAstSourceSpan span, const wchar_t *type_name);
```
Creates a primitive type node (e.g., `عدد_صحيح`).

#### `baa_ast_new_array_type_node`
```c
BaaNode *baa_ast_new_array_type_node(BaaAstSourceSpan span, BaaNode *element_type, BaaNode *size);
```
Creates an array type node. `size` can be NULL for dynamic arrays.

## Operator Enumerations

### BaaBinaryOperatorKind

| Operator | Description |
|----------|-------------|
| `BAA_BINARY_OP_ADD` | Addition (+) |
| `BAA_BINARY_OP_SUBTRACT` | Subtraction (-) |
| `BAA_BINARY_OP_MULTIPLY` | Multiplication (*) |
| `BAA_BINARY_OP_DIVIDE` | Division (/) |
| `BAA_BINARY_OP_MODULO` | Modulo (%) |
| `BAA_BINARY_OP_EQUAL` | Equality (==) |
| `BAA_BINARY_OP_NOT_EQUAL` | Inequality (!=) |
| `BAA_BINARY_OP_LESS_THAN` | Less than (<) |
| `BAA_BINARY_OP_GREATER_THAN` | Greater than (>) |
| `BAA_BINARY_OP_LOGICAL_AND` | Logical AND (&&) |
| `BAA_BINARY_OP_LOGICAL_OR` | Logical OR (\|\|) |

### BaaUnaryOperatorKind

| Operator | Description |
|----------|-------------|
| `BAA_UNARY_OP_PLUS` | Unary plus (+a) |
| `BAA_UNARY_OP_MINUS` | Unary minus (-a) |
| `BAA_UNARY_OP_LOGICAL_NOT` | Logical NOT (!a) |

## Usage Example

```c
#include "baa/ast/ast.h"

// Create: عدد_صحيح x = ١٠.
BaaAstSourceSpan span = { /* ... */ };

// Type node
BaaNode *type = baa_ast_new_primitive_type_node(span, L"عدد_صحيح");

// Literal value
BaaNode *value = baa_ast_new_literal_int_node(span, 10, baa_type_int);

// Variable declaration
BaaNode *decl = baa_ast_new_var_decl_node(span, L"x", BAA_MOD_NONE, type, value);

// Clean up
baa_ast_free_node(decl);  // Frees type and value too
```

## Memory Management

- **Ownership**: When a node is added to another (e.g., `baa_ast_add_stmt_to_block`), the parent takes ownership
- **String duplication**: String parameters (names, values) are always duplicated
- **Recursive freeing**: `baa_ast_free_node` frees all child nodes automatically
- **NULL safety**: All functions handle NULL parameters gracefully

## See Also

- [Architecture Overview](../02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)
- [AST Documentation](../02_COMPILER_ARCHITECTURE/AST.md)
- [Parser API](PARSER_API.md)

---

*This document is part of the [Baa Language Documentation](../index.md)*
