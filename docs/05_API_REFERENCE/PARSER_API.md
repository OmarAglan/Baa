# Baa Parser API Reference

This document provides a comprehensive API reference for the Baa language parser component.

## 🎯 Overview

The Baa parser implements a recursive descent parser that converts token streams from the lexer into Abstract Syntax Trees (AST). It supports Arabic syntax and provides comprehensive error recovery.

## 📋 Core Data Structures

### BaaParser

Main parser context structure.

```c
typedef struct BaaParser {
    BaaLexer* lexer;              // Token source
    BaaToken current_token;       // Current token being processed
    BaaToken peek_token;          // Next token (lookahead)
    BaaErrorReporter* error_reporter;
    BaaVector* error_recovery_tokens; // Tokens for error synchronization
    int max_errors;               // Maximum errors before stopping
    bool panic_mode;              // Error recovery state
} BaaParser;
```

### BaaParseResult

Result of parsing operations.

```c
typedef struct BaaParseResult {
    BaaNode* ast;                 // Resulting AST node
    BaaResult status;             // Success/error status
    int error_count;              // Number of errors encountered
} BaaParseResult;
```

## 🔧 Core Functions

### Initialization and Cleanup

#### `baa_parser_create`
```c
BaaParser* baa_parser_create(BaaLexer* lexer, BaaErrorReporter* error_reporter);
```
**Description:** Creates a new parser instance.
**Parameters:**
- `lexer`: Lexer instance for token input
- `error_reporter`: Error reporting context
**Returns:** New parser instance or NULL on failure
**Example:**
```c
BaaLexer* lexer = baa_lexer_create(input);
BaaErrorReporter* reporter = baa_error_reporter_create();
BaaParser* parser = baa_parser_create(lexer, reporter);
```

#### `baa_parser_destroy`
```c
void baa_parser_destroy(BaaParser* parser);
```
**Description:** Destroys parser instance and frees memory.
**Parameters:**
- `parser`: Parser instance to destroy

### Main Parsing Functions

#### `baa_parser_parse_program`
```c
BaaParseResult baa_parser_parse_program(BaaParser* parser);
```
**Description:** Parses a complete program (translation unit).
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with AST root node
**Example:**
```c
BaaParseResult result = baa_parser_parse_program(parser);
if (result.status == BAA_RESULT_SUCCESS) {
    // Process AST
    process_ast(result.ast);
    baa_node_destroy(result.ast);
}
```

#### `baa_parser_parse_statement`
```c
BaaParseResult baa_parser_parse_statement(BaaParser* parser);
```
**Description:** Parses a single statement.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with statement AST node

#### `baa_parser_parse_expression`
```c
BaaParseResult baa_parser_parse_expression(BaaParser* parser);
```
**Description:** Parses an expression using precedence climbing.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with expression AST node

### Declaration Parsing

#### `baa_parser_parse_function_definition`
```c
BaaParseResult baa_parser_parse_function_definition(BaaParser* parser);
```
**Description:** Parses a function definition.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with function definition AST node
**Example:**
```c
// Parses: دالة عدد_صحيح الرئيسية() { ... }
BaaParseResult result = baa_parser_parse_function_definition(parser);
```

#### `baa_parser_parse_variable_declaration`
```c
BaaParseResult baa_parser_parse_variable_declaration(BaaParser* parser);
```
**Description:** Parses a variable declaration.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with variable declaration AST node
**Example:**
```c
// Parses: عدد_صحيح x = ١٠؛
BaaParseResult result = baa_parser_parse_variable_declaration(parser);
```

### Statement Parsing

#### `baa_parser_parse_if_statement`
```c
BaaParseResult baa_parser_parse_if_statement(BaaParser* parser);
```
**Description:** Parses an if statement.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with if statement AST node

#### `baa_parser_parse_while_statement`
```c
BaaParseResult baa_parser_parse_while_statement(BaaParser* parser);
```
**Description:** Parses a while loop statement.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with while statement AST node

#### `baa_parser_parse_for_statement`
```c
BaaParseResult baa_parser_parse_for_statement(BaaParser* parser);
```
**Description:** Parses a for loop statement.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with for statement AST node

#### `baa_parser_parse_return_statement`
```c
BaaParseResult baa_parser_parse_return_statement(BaaParser* parser);
```
**Description:** Parses a return statement.
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with return statement AST node

### Expression Parsing

#### `baa_parser_parse_primary_expression`
```c
BaaParseResult baa_parser_parse_primary_expression(BaaParser* parser);
```
**Description:** Parses primary expressions (literals, identifiers, parenthesized expressions).
**Parameters:**
- `parser`: Parser instance
**Returns:** Parse result with primary expression AST node

#### `baa_parser_parse_function_call`
```c
BaaParseResult baa_parser_parse_function_call(BaaParser* parser, BaaNode* function_expr);
```
**Description:** Parses a function call expression.
**Parameters:**
- `parser`: Parser instance
- `function_expr`: Function expression being called
**Returns:** Parse result with function call AST node

### Utility Functions

#### `baa_parser_current_token_is`
```c
bool baa_parser_current_token_is(BaaParser* parser, BaaTokenType type);
```
**Description:** Checks if current token matches specified type.
**Parameters:**
- `parser`: Parser instance
- `type`: Token type to check
**Returns:** `true` if token matches, `false` otherwise

#### `baa_parser_advance`
```c
BaaResult baa_parser_advance(BaaParser* parser);
```
**Description:** Advances to the next token.
**Parameters:**
- `parser`: Parser instance
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_parser_expect_token`
```c
BaaResult baa_parser_expect_token(BaaParser* parser, BaaTokenType expected);
```
**Description:** Expects and consumes a specific token type.
**Parameters:**
- `parser`: Parser instance
- `expected`: Expected token type
**Returns:** `BAA_RESULT_SUCCESS` or error code

### Error Recovery

#### `baa_parser_synchronize`
```c
void baa_parser_synchronize(BaaParser* parser);
```
**Description:** Synchronizes parser after an error by skipping to recovery points.
**Parameters:**
- `parser`: Parser instance

#### `baa_parser_set_max_errors`
```c
void baa_parser_set_max_errors(BaaParser* parser, int max_errors);
```
**Description:** Sets maximum number of errors before stopping.
**Parameters:**
- `parser`: Parser instance
- `max_errors`: Maximum error count (default: 10)

## 🌍 Arabic Syntax Support

### Arabic Keywords

The parser recognizes these Arabic keywords:

| Arabic Keyword | English Equivalent | Usage |
|----------------|-------------------|-------|
| `دالة` | `function` | Function definitions |
| `إذا` | `if` | Conditional statements |
| `وإلا` | `else` | Else clauses |
| `بينما` | `while` | While loops |
| `لـ` | `for` | For loops |
| `إرجاع` | `return` | Return statements |
| `كسر` | `break` | Break statements |
| `متابعة` | `continue` | Continue statements |
| `عدد_صحيح` | `int` | Integer type |
| `عدد_حقيقي` | `float` | Float type |
| `حرف` | `char` | Character type |
| `منطقي` | `bool` | Boolean type |
| `فراغ` | `void` | Void type |

### Arabic Operators

| Arabic Operator | English Equivalent | Description |
|-----------------|-------------------|-------------|
| `و` | `&&` | Logical AND |
| `أو` | `\|\|` | Logical OR |
| `ليس` | `!` | Logical NOT |

## 📝 Usage Examples

### Basic Parsing

```c
#include "baa_parser.h"

int main() {
    // Create lexer and parser
    BaaLexer* lexer = baa_lexer_create_from_file("program.baa");
    BaaErrorReporter* reporter = baa_error_reporter_create();
    BaaParser* parser = baa_parser_create(lexer, reporter);
    
    // Parse program
    BaaParseResult result = baa_parser_parse_program(parser);
    
    if (result.status == BAA_RESULT_SUCCESS) {
        printf("Parsing successful! AST created.\n");
        
        // Process the AST
        baa_ast_print(result.ast);
        
        // Clean up
        baa_node_destroy(result.ast);
    } else {
        printf("Parsing failed with %d errors.\n", result.error_count);
    }
    
    // Cleanup
    baa_parser_destroy(parser);
    baa_lexer_destroy(lexer);
    baa_error_reporter_destroy(reporter);
    
    return 0;
}
```

### Parsing Specific Constructs

```c
// Parse a single expression
BaaParseResult expr_result = baa_parser_parse_expression(parser);
if (expr_result.status == BAA_RESULT_SUCCESS) {
    // Handle expression AST
    handle_expression(expr_result.ast);
    baa_node_destroy(expr_result.ast);
}

// Parse a function definition
BaaParseResult func_result = baa_parser_parse_function_definition(parser);
if (func_result.status == BAA_RESULT_SUCCESS) {
    // Handle function AST
    handle_function(func_result.ast);
    baa_node_destroy(func_result.ast);
}
```

### Error Handling

```c
// Set maximum errors
baa_parser_set_max_errors(parser, 5);

// Parse with error recovery
BaaParseResult result = baa_parser_parse_program(parser);

if (result.error_count > 0) {
    printf("Parsing completed with %d errors:\n", result.error_count);
    
    // Print error details
    BaaVector* errors = baa_error_reporter_get_errors(reporter);
    for (size_t i = 0; i < baa_vector_size(errors); i++) {
        BaaError* error = baa_vector_get(errors, i);
        printf("Error at line %d: %s\n", error->line, error->message);
    }
}
```

## ⚠️ Error Handling

Common error scenarios and recovery strategies:

- **Syntax Errors**: Parser synchronizes at statement boundaries
- **Missing Tokens**: Parser inserts expected tokens and continues
- **Unexpected Tokens**: Parser skips tokens until synchronization point
- **Arabic Encoding Issues**: Parser reports encoding errors with context

## 🔗 Related APIs

- **[Lexer API](LEXER_API.md)** - Token input for parser
- **[AST API](../02_COMPILER_ARCHITECTURE/AST.md)** - AST node manipulation
- **[Utilities API](UTILITIES_API.md)** - Error handling utilities

---

**For more information, see the [Parser Documentation](../02_COMPILER_ARCHITECTURE/PARSER.md).**
