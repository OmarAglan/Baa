# Baa Language Parser Structure Documentation (New Design)

**Status: This document outlines the new design for the Parser following the removal of the previous implementation. Items here are largely planned unless otherwise noted.**

This document provides a comprehensive reference for the parser implementation in the Baa programming language compiler. The parser's role is to transform token sequences produced by the lexer into an Abstract Syntax Tree (AST) representation, based on the design in `docs/AST.md`.

## 0. Grammar Basis

While a separate formal BNF/EBNF grammar document is not strictly maintained for Baa at this stage, the structure of the recursive descent parsing functions is directly guided by the syntax rules specified in `docs/language.md`. The parser implementation itself serves as an executable form of the language grammar. Consideration may be given to formally documenting the grammar if language complexity significantly increases or if parser generator tools are explored in the future.

## 1. Core Design Principles

1.  **Modularity**: The parser is organized into specialized modules for different language constructs (Expressions, Statements, Declarations, Types).
2.  **Recursive Descent Approach**: The parser uses a top-down, predictive recursive descent parsing technique. Each significant non-terminal in the Baa grammar will correspond to a parsing function.
3.  **Syntactic Focus & AST Integration**: The parser is strictly responsible for syntactic validation and constructing the Abstract Syntax Tree (AST). It achieves this by invoking dedicated AST node creation functions (e.g., `baa_ast_new_binary_expr(...)`) which encapsulate AST construction logic. Semantic checks (e.g., type checking, scope resolution) are deferred to a subsequent Semantic Analysis phase. The parser may collect type *information* (e.g., from type annotations) and store it in the AST, but does not validate its semantic correctness.
4.  **Error Handling**: Robust error detection, reporting, and a defined recovery mechanism are integral to the design.

## 2. Parser Architecture

The parser is structured into the following key components:

```
BaaParser (Main Parser - parser.c)
├── Expression Parser (expression_parser.c)
├── Statement Parser (statement_parser.c)
├── Declaration Parser (declaration_parser.c)
└── Type Parser (type_parser.c)
└── Parser Helpers (parser_utils.c or similar - for common utilities like token consumption, error reporting)
```

## 3. Current Implementation Status (New Design - May 2025)

**Note:** Following the removal of the old parser components, the parser is being rebuilt. Most features listed below are planned for the new implementation. The modular structure is the intended organization.

- 🔲 Basic parser infrastructure with recursive descent approach.
- 🔲 Expression parsing with precedence handling.
- 🔲 Statement parsing for basic control flow.
- 🔲 Declaration parsing for variables, functions.
- 🔲 Basic type information collection from annotations.
- 🔲 Error detection and reporting mechanisms.
- 🔲 Boolean type support (`منطقي`) parsing.
- 🔲 Basic function parameter parsing.
- 🔲 Basic operator parsing.

## 4. Parser State

The parser maintains its state in the `BaaParser` structure (defined in `parser.h` or an internal header):

```c
typedef struct {
    BaaLexer* lexer;           // Pointer to the lexer instance providing tokens
    BaaToken current_token;    // The current token being processed
    BaaToken previous_token;   // The most recently consumed token
    bool had_error;            // Flag: true if any syntax error has been encountered
    bool panic_mode;           // Flag: true if the parser is currently recovering from an error
    // const wchar_t* error_message; // Error messages are typically reported, not stored long-term in parser state
    // BaaSourceLocation location; // Current location is usually derived from current_token.loc
} BaaParser;
```

## 5. Module Organization

### Main Parser (`parser.c`/`parser.h`)
-   Entry point for parsing a complete program (e.g., `baa_parse_program`).
-   Manages the `BaaParser` state and core token consumption utilities (`advance`, `consume`, `match`, `check`, `peek`).
-   Dispatches to specialized parsers for top-level constructs (e.g., declarations, statements).

### Expression Parser (`expression_parser.c`/`expression_parser.h`)
-   Parses all types of expressions, adhering to Baa's operator precedence and associativity rules (defined in `src/operators/`).
-   **Strategy:** Expression parsing will primarily use a cascaded series of functions, each handling a specific level of operator precedence (e.g., `parse_assignment_expr()`, `parse_logical_or_expr()`, ..., `parse_primary_expr()`). This directly models the precedence hierarchy. For complex interactions or future extensibility, techniques like Pratt parsing might be considered if the cascaded approach becomes unwieldy.
-   Supports literals, identifiers (variables), function calls, unary/binary operators, assignments, grouping parentheses, array indexing, etc.

### Statement Parser (`statement_parser.c`/`statement_parser.h`)
-   Parses various statement types.
-   `parse_statement()`: A dispatch function that determines the kind of statement based on the current token (e.g., `إذا`, `طالما`, `{`, identifier for expression statement).
-   Parses control flow statements (if (`إذا`), while (`طالما`), for (`لكل`), return (`إرجع`)). Support for `switch` (`اختر`), `case` (`حالة`), `break` (`توقف`), and `continue` (`أكمل`) is planned.
-   Handles blocks (`{ ... }`) and expression statements.

### Declaration Parser (`declaration_parser.c`/`declaration_parser.h`)
-   Parses declarations for variables, constants, functions, and potentially user-defined types (structs, enums) later.
-   `parse_declaration()`: A dispatch function for different kinds of declarations.
-   **Note:** Initial focus will be on variable and function declarations. The stubbed functions mentioned previously (`baa_parse_function_rest`, etc.) will be implemented or integrated here.

### Type Parser (`type_parser.c`/`type_parser.h`)
-   Parses type annotations found in declarations (variables, function parameters, return types).
-   Constructs preliminary `BaaType` representations (or references to predefined types) to be stored in the AST. Semantic validation of these types occurs later.
-   Handles primitive types (e.g., `عدد_صحيح`), array type syntax (e.g., `type[]`), and potentially pointer/struct/enum type syntax in the future.

## 6. Parsing Process

The parsing process generally follows this flow:
1.  Initialize `BaaParser` with an initialized `BaaLexer`. Fetch the first token.
2.  Call `baa_parse_program()`, which typically parses a sequence of top-level declarations until `BAA_TOKEN_EOF`.
3.  `parse_declaration()` or `parse_statement()` (for top-level script-like code if supported) is called.
4.  These functions recursively call other parsing functions for sub-constructs (e.g., `parse_expression()`, `parse_block_statement()`).
5.  Each successful recognition of a grammar rule results in the creation of an AST node via dedicated AST creation functions (e.g., `baa_ast_new_if_stmt(...)`).
6.  If a syntax error is encountered, `parser_error()` is called, and error recovery (`synchronize()`) is attempted.

## 7. Error Handling and Recovery

The parser employs a "panic mode" error recovery strategy:
1.  **Detection:** An error is detected when an unexpected token is encountered (e.g., by `consume_token` or a failed `match_token` where a specific token was required).
2.  **Reporting:** `parser_error()` is called. This function:
    *   Sets `parser->had_error = true`.
    *   Prints an informative error message in Arabic, including the source location (`previous_token.loc` or `current_token.loc`) and ideally what was expected vs. what was found.
    *   Avoids reporting further errors while already in panic mode to prevent cascades.
3.  **Panic Mode Activation:** `parser->panic_mode = true`. While in panic mode, the parser avoids creating AST nodes for the construct where the error occurred.
4.  **Synchronization (`synchronize()`):**
    *   The parser advances tokens, discarding them, until it reaches a point considered safe to resume parsing.
    *   Synchronization points typically include:
        *   Statement terminators (`.`).
        *   Keywords that unambiguously start new declarations or major statements (e.g., `إذا`, `طالما`, `لكل`, `إرجع`, `ثابت`, type keywords like `عدد_صحيح`).
        *   Delimiters that often end a construct, like `}` (after which a new statement might begin).
    *   The goal is to skip the problematic section and find the start of a subsequent, potentially valid construct.
5.  **Panic Mode Deactivation:** Once a synchronization point is reached (or EOF), `parser->panic_mode` is cleared, allowing parsing to resume normally.

This strategy aims to report multiple distinct syntax errors in a single compilation pass.

## 8. Public API (Example - to be defined in `parser.h`)

```c
// Opaque struct for the parser state
typedef struct BaaParser BaaParser;

// Creates and initializes a new parser instance.
// Takes an initialized lexer. The parser does NOT take ownership of the lexer.
BaaParser* baa_parser_create(BaaLexer* lexer);

// Frees the parser instance. Does not free the lexer.
void baa_parser_free(BaaParser* parser);

// Main parsing function. Parses the entire token stream from the lexer.
// Returns the root of the AST (e.g., a BaaProgram node) or NULL if fatal errors occurred.
BaaProgram* baa_parse_program(BaaParser* parser);

// Error status functions
bool baa_parser_had_error(const BaaParser* parser);
// Specific error messages are typically printed directly; a function to get a list of errors might be added later.
```

This provides a more detailed view of the parser's design and operational principles.
