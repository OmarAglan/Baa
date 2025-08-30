# Baa Language Parser Structure Documentation (New Design v2)

**Status: This document outlines the revised design for the Parser. Items here are largely planned unless otherwise noted. This parser will generate an AST conforming to the new `AST.md` (v2) design.**

This document provides a comprehensive reference for the parser implementation in the Baa programming language compiler. The parser's role is to transform token sequences produced by the lexer into an Abstract Syntax Tree (AST), specifically a tree of `BaaNode` structures.

## 1. Core Design Principles

1. **Modularity**: The parser is organized into logical modules/files for different language constructs (e.g., `expression_parser.c`, `statement_parser.c`, `declaration_parser.c`, `type_parser.c`).
2. **Recursive Descent Approach**: The parser uses a top-down, predictive recursive descent parsing technique. Each significant non-terminal in the Baa grammar will typically correspond to a parsing function that returns a `BaaNode*`.
3. **Syntactic Focus & AST Construction**:
    * The parser is strictly responsible for syntactic validation according to Baa's grammar.
    * Upon successful recognition of a grammar rule, it constructs the appropriate `BaaNode` (including its specific `data` struct) using AST creation functions (e.g., `baa_ast_new_binary_expr_node(...)`).
    * Semantic checks (e.g., type checking, scope resolution) are deferred to a subsequent Semantic Analysis phase. The parser collects syntactic information (like type names in declarations) and stores it in the AST.
4. **Error Handling & Recovery**: Robust error detection, reporting (with accurate `BaaSourceSpan`), and a defined recovery mechanism (panic mode with synchronization) are integral.

## 2. Parser Architecture

The parser state and core logic reside in `parser.c`, with specialized parsing functions in separate modules:

``` c

BaaParser (Main Parser - parser.c)
├── Expression Parser (expression_parser.c)
├── Statement Parser (statement_parser.c)
├── Declaration Parser (declaration_parser.c)
└── Type Parser (type_parser.c)
└── Parser Utilities (parser_utils.c - token consumption, error reporting, etc.)

```

Each parsing function (e.g., `parse_if_statement`, `parse_binary_expression`) will return a `BaaNode*` representing the construct parsed, or `NULL` if a syntax error occurred that couldn't be recovered from for that construct.

## 3. Parser State

The parser maintains its state in the `BaaParser` structure (defined in an internal parser header):

```c
typedef struct {
    BaaLexer* lexer;           // Pointer to the lexer instance providing tokens
    BaaToken current_token;    // The current token being processed (lookahead)
    BaaToken previous_token;   // The most recently consumed token (useful for source span)
    const char* source_filename; // Name of the source file being parsed (for error messages)
    bool had_error;            // Flag: true if any syntax error has been encountered
    bool panic_mode;           // Flag: true if the parser is currently recovering from an error
    // DiagnosticContext* diagnostics; // Optional: For collecting multiple parse errors
} BaaParser;
```

The `previous_token.span.start` and `current_token.span.end` (or `previous_token.span.end` after consumption) will be used to construct `BaaSourceSpan` for AST nodes.

## 4. Parsing Process & Key Function Responsibilities

### 4.1 Main Parser (`parser.c`, `parser.h`)

* **`BaaParser* baa_parser_create(BaaLexer* lexer, const char* source_filename)`**: Initializes a new parser.
* **`void baa_parser_free(BaaParser* parser)`**: Frees parser resources.
* **`BaaNode* baa_parse_program(BaaParser* parser)`**: Entry point. Parses a sequence of top-level declarations (function definitions, global variable declarations) until `BAA_TOKEN_EOF`. Returns a `BaaNode*` of kind `BAA_NODE_KIND_PROGRAM`.
* **Token Handling Utilities (internal in `parser_utils.c`):**
  * `advance(BaaParser* p)`: Consumes `current_token`, makes next token current.
  * `consume_token(BaaParser* p, BaaTokenType expected, const wchar_t* error_message)`: Consumes if `current_token.type == expected`, otherwise reports error.
  * `match_token(BaaParser* p, BaaTokenType type)`: Consumes if `current_token.type == type`, returns true/false.
  * `check_token(BaaParser* p, BaaTokenType type)`: Returns true if `current_token.type == type`, no consumption.
  * `peek_token(BaaParser* p)`: Returns `current_token.type` without consumption.

### 4.2 Declaration Parser (`declaration_parser.c`)

* **`BaaNode* parse_declaration_or_statement(BaaParser* parser)`**: ✅ **IMPLEMENTED** - Smart dispatcher that distinguishes between function definitions, variable declarations, and statements using lookahead logic.
* **`BaaNode* parse_function_definition(BaaParser* parser, BaaAstNodeModifiers initial_modifiers)`**: ✅ **IMPLEMENTED** - Parses complete function definitions with return type, name, parameters, and body. Returns `BaaNode*` of kind `BAA_NODE_KIND_FUNCTION_DEF`.
  * **Function Parameters**: Uses `parse_parameter_list()` to parse comma-separated parameter lists, creating `BaaNode*`s of kind `BAA_NODE_KIND_PARAMETER`.
  * **Syntax Support**: `[modifiers] return_type function_name(parameter_list) { body }`
  * **Arabic Example**: `عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) { إرجع أ + ب؛ }`
* **`BaaNode* parse_parameter(BaaParser* parser)`**: ✅ **IMPLEMENTED** - Parses individual function parameters with type and name.
* **`bool parse_parameter_list(BaaParser* parser, BaaNode*** parameters, size_t* parameter_count)`**: ✅ **IMPLEMENTED** - Parses complete parameter lists with proper error handling.
* **`BaaNode* parse_variable_declaration_statement(BaaParser* parser, BaaAstNodeModifiers initial_modifiers)`**: ✅ **IMPLEMENTED** - Parses `ثابت? type_specifier identifier ('=' expression)? '.'`. Returns `BaaNode*` of kind `BAA_NODE_KIND_VAR_DECL_STMT`. Handles `ثابت` modifier.

### 4.3 Type Parser (`type_parser.c`)

* **`BaaNode* parse_type_specifier(BaaParser* parser)`**: Parses a type specification from the token stream (e.g., `عدد_صحيح`, `حرف[]`, `مؤشر<نوع>`).
  * Returns a `BaaNode*` of kind `BAA_NODE_KIND_TYPE`.
  * The `BaaTypeAstData` within this node will capture the structure of the parsed type (e.g., base name, array markers).

### 4.4 Statement Parser (`statement_parser.c`)

* **`BaaNode* parse_statement(BaaParser* parser)`**: Dispatches based on current token (`إذا`, `طالما`, `{`, expression start, etc.).
* **`BaaNode* parse_block_statement(BaaParser* parser)`**: Parses `{ statement* }`. Returns `BaaNode*` of kind `BAA_NODE_KIND_BLOCK_STMT`.
* **`BaaNode* parse_if_statement(BaaParser* parser)`**: Parses `إذا (condition) then_branch (وإلا else_branch)?`. Returns `BaaNode*` of kind `BAA_NODE_KIND_IF_STMT`.
* **`BaaNode* parse_expression_statement(BaaParser* parser)`**: Parses `expression '.'`. Returns `BaaNode*` of kind `BAA_NODE_KIND_EXPR_STMT`.
* *(Similar functions for `while`, `for`, `return`, `break`, `continue`)*

### 4.5 Expression Parser (`expression_parser.c`)

* **`BaaNode* parse_expression(BaaParser* parser)`**: Entry point for parsing any expression. Typically starts by calling the lowest precedence level (e.g., `parse_assignment_expression`).
* **Precedence Climbing using Cascaded Functions:**
  * `BaaNode* parse_assignment_expression(BaaParser* parser)` (handles `=`, `+=`, etc., right-associative)
  * `BaaNode* parse_logical_or_expression(BaaParser* parser)` (handles `||`)
  * `BaaNode* parse_logical_and_expression(BaaParser* parser)` (handles `&&`)
  * ... (levels for bitwise, equality, relational, shift, additive, multiplicative) ...
  * `BaaNode* parse_unary_expression(BaaParser* parser)` (handles `!`, `-` (unary), `~`, `++` (prefix), `--` (prefix))
  * `BaaNode* parse_postfix_expression(BaaParser* parser, BaaNode* base_expr)` ✅ **IMPLEMENTED** - Handles function calls `()`, array indexing `[]`, `++` (postfix), `--` (postfix)
  * `BaaNode* parse_call_expression(BaaParser* parser, BaaNode* callee_expr)` ✅ **IMPLEMENTED** - Parses function call expressions with argument lists
  * `BaaNode* parse_primary_expression(BaaParser* parser)`: Parses literals, identifiers, `( expression )`.
* Each function consumes operators of its precedence level and calls the function for the next higher precedence level to parse operands.
* **Example: `parse_additive_expression`**

    ```c
    // BaaNode* parse_additive_expression(BaaParser* p) {
    //     BaaNode* node = parse_multiplicative_expression(p);
    //     while (match_token(p, BAA_TOKEN_PLUS) || match_token(p, BAA_TOKEN_MINUS)) {
    //         BaaTokenType op_type = p->previous_token.type; // Operator just consumed
    //         BaaNode* right = parse_multiplicative_expression(p);
    //         // node = baa_ast_new_binary_expr_node(op_type_to_binary_kind(op_type), node, right, span);
    //     }
    //     return node;
    // }
    ```

### 4.6 Function Support (✅ COMPLETED - Priority 4)

The parser now has comprehensive support for function definitions and function call expressions:

#### Function Definitions
* **Syntax**: `[modifiers] return_type function_name(parameter_list) { body }`
* **Arabic Example**: `عدد_صحيح جمع(عدد_صحيح أ، عدد_صحيح ب) { إرجع أ + ب؛ }`
* **AST Node**: `BAA_NODE_KIND_FUNCTION_DEF` with `BaaFunctionDefData`
* **Features**:
  - Parameter lists with type and name parsing
  - Return type specification
  - Function body as block statement
  - Modifier support (const, inline, etc.)
  - Empty parameter list support: `void function_name() { ... }`

#### Function Parameters
* **Syntax**: `type_specifier identifier`
* **AST Node**: `BAA_NODE_KIND_PARAMETER` with `BaaParameterData`
* **Features**:
  - Type-safe parameter declarations
  - Multiple parameter support with comma separation
  - Proper error handling for malformed parameters

#### Function Call Expressions
* **Syntax**: `function_name(argument_list)`
* **Arabic Example**: `نتيجة = جمع(٥، ١٠)؛`
* **AST Node**: `BAA_NODE_KIND_CALL_EXPR` with `BaaCallExprData`
* **Features**:
  - Argument list parsing with comma separation
  - Empty argument list support: `function_name()`
  - Nested function calls: `outer(inner(x), y)`
  - Proper precedence as postfix expressions

#### Parser Integration
* **Smart Dispatching**: `parse_declaration_or_statement()` uses lookahead to distinguish function definitions from variable declarations
* **Precedence Handling**: Function calls are handled as postfix expressions with correct precedence
* **Error Recovery**: Comprehensive error handling with proper cleanup of partially constructed AST nodes

## 5. Error Handling and Recovery

The parser will use a "panic mode" error recovery strategy:

1. **Detection**: When an unexpected token is found (e.g., `consume_token` fails), an error is detected.
2. **Reporting (`parser_error(BaaParser* p, const wchar_t* message_format, ...)`):**
    * Sets `p->had_error = true`.
    * Prints an informative error message (in Arabic) using `p->previous_token.span` or `p->current_token.span.start` for location.
    * Avoids cascading reports if already in `panic_mode`.
3. **Panic Mode Activation**: `p->panic_mode = true`. No AST nodes are created for the erroneous construct.
4. **Synchronization (`synchronize(BaaParser* p)`):**
    * Called after reporting an error.
    * Discards tokens by calling `advance()` until a "safe" point is reached or EOF.
    * **Synchronization Points:**
        * Statement terminator (`.`).
        * Keywords that reliably start a new statement or declaration (e.g., `إذا`, `طالما`, `لكل`, `إرجع`, `عدد_صحيح`, `حرف`, `فراغ` when starting a function def, `ثابت`).
        * Opening brace `{` (often starts a new block/scope).
        * Closing brace `}` (often ends a block, look for next statement).
5. **Panic Mode Deactivation**: Once synchronized, `p->panic_mode` is cleared.

## 6. Public API (Example - `parser.h`)

```c
#ifndef BAA_PARSER_H
#define BAA_PARSER_H

#include "baa/lexer/lexer.h" // For BaaLexer, BaaToken
#include "baa/ast/ast.h"     // For BaaNode (once ast.h defines it)

typedef struct BaaParser BaaParser; // Opaque struct, definition in parser_internal.h

BaaParser* baa_parser_create(BaaLexer* lexer, const char* source_filename);
void baa_parser_free(BaaParser* parser);

// Parses the entire token stream, returns the root BaaNode (kind BAA_NODE_KIND_PROGRAM)
// or NULL on unrecoverable errors.
BaaNode* baa_parse_program(BaaParser* parser);

bool baa_parser_had_error(const BaaParser* parser);
// const wchar_t* baa_parser_get_last_error_message(const BaaParser* parser); // Consider a diagnostic list

#endif // BAA_PARSER_H
