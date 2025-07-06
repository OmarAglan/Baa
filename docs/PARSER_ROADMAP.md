## Updated `docs/PARSER_ROADMAP.md` (v2 - Detailed)

**Status: ✅ CORE PARSER COMPLETE - This document outlines the implementation status of the new Parser, designed to generate an AST conforming to `AST.md` (v2). Priority 4 (Function Definitions and Calls) completed 2025-07-06.**

The parser will employ a recursive descent strategy, with distinct modules for parsing different language constructs. Its primary output will be a `BaaNode*` representing the root of the Abstract Syntax Tree (typically a `BAA_NODE_KIND_PROGRAM` node).

### Phase 0: Parser Core & Token Utilities

**Goal:** Set up the fundamental parser structure and token manipulation helpers.

1. **Define `BaaParser` Structure:**
    * **File:** `parser_internal.h` (or similar private header).
    * **Description:** Define `BaaParser` struct: `BaaLexer* lexer; BaaToken current_token; BaaToken previous_token; bool had_error; bool panic_mode;`.
    * **Status:** [x] COMPLETED (Includes `source_filename` field now)

2. **Implement Parser Initialization & Teardown:**
    * **File:** `parser.c`.
    * **Description:**
        * `BaaParser* baa_parser_create(BaaLexer* lexer, const char* source_filename);` (allocates, stores filename, initializes `current_token` by calling `advance`).
        * `void baa_parser_free(BaaParser* parser);`.
    * **Status:** [x] COMPLETED

3. **Implement Core Token Handling Utilities:**
    * **File:** `parser_utils.c` (or within `parser.c`).
    * **Description:**
        * `void advance(BaaParser* p);` (consumes `current_token`, makes next token current, updates `previous_token`).
        * `bool check_token(BaaParser* p, BaaTokenType type);` (checks `current_token.type`).
        * `bool match_token(BaaParser* p, BaaTokenType type);` (if `check_token` is true, calls `advance`, returns true; else false).
        * `void consume_token(BaaParser* p, BaaTokenType expected, const wchar_t* error_message_format, ...);` (if `current_token.type` is not `expected`, reports error and enters panic mode; otherwise advances).
    * **Status:** [x] COMPLETED

4. **Implement Basic Error Reporting (`parser_error`):**
    * **File:** `parser_utils.c` (or `parser.c`).
    * **Description:**
        * `void parser_error_at_token(BaaParser* p, BaaToken* token_for_loc, const wchar_t* message_format, ...);` (formats message, uses `source_filename`, sets `had_error`, sets `panic_mode`).
        * Uses `token_for_loc.span` (or constructs a span from it) to report the error location.
        * Avoids cascading errors if `panic_mode` is already true.
    * **Status:** [x] COMPLETED

5. **Implement Basic Error Synchronization (`synchronize`):**
    * **File:** `parser_utils.c` (or `parser.c`).
    * **Description:** `void synchronize(BaaParser* p);`.
        * Initial strategy: Advances tokens until `BAA_TOKEN_DOT` (statement terminator), `BAA_TOKEN_EOF`, or a keyword that typically starts a new statement/declaration (e.g., `إذا`, `عدد_صحيح`). Clears `panic_mode`.
    * **Status:** [x] COMPLETED

### Phase 1: Parsing Minimal Program & Primary Expressions

**Goal:** Parse a very simple program structure and the most basic expressions.

1. **Implement `baa_parse_program` (Entry Point):**
    * **File:** `parser.c`.
    * **Description:** `BaaNode* baa_parse_program(BaaParser* parser);`.
        * Creates a `BAA_NODE_KIND_PROGRAM` node.
        * Loops, calling `parse_statement()` until `BAA_TOKEN_EOF`.
        * Adds successfully parsed top-level nodes to the program node.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can parse an empty file (EOF) and return an empty program node.

2. **Implement `parse_primary_expression`:**
    * **File:** `expression_parser.c`.
    * **Description:** `BaaNode* parse_primary_expression(BaaParser* p);`.
        * Handles `BAA_TOKEN_INT_LIT`, `BAA_TOKEN_STRING_LIT` (creating `BAA_NODE_KIND_LITERAL_EXPR` nodes).
            * Extracts value and full lexeme from token. Uses default types for now.
        * Handles `BAA_TOKEN_IDENTIFIER` (creating `BAA_NODE_KIND_IDENTIFIER_EXPR` nodes).
        * Handles `BAA_TOKEN_LPAREN expression BAA_TOKEN_RPAREN` (grouping - recursively calls `parse_expression`).
        * Reports errors for unexpected tokens.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can parse individual literals, identifiers, and simple parenthesized expressions. Correct AST nodes are created with source spans.

3. **Implement `parse_expression_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** `BaaNode* parse_expression_statement(BaaParser* p);`.
        * Calls `parse_expression()` to get an expression node.
        * Expects and consumes `BAA_TOKEN_DOT`.
        * Creates and returns a `BAA_NODE_KIND_EXPR_STMT` node.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** Can parse `my_var.` or `123.` (assuming these are valid expressions for now).

4. **Implement `parse_statement` (Basic Statement Dispatcher):**
    * **File:** `statement_parser.c`.
    * **Description:** `BaaNode* parse_statement(BaaParser* p);`.
        * Dispatches based on current token type to appropriate statement parsers.
        * Handles `BAA_TOKEN_LBRACE` for block statements.
        * Defaults to expression statements for other tokens.
    * **Status:** [x] COMPLETED - 2025-01-04
    * **Verification:** `baa_parse_program` can now parse a sequence of simple expression statements and block statements.

### Phase 2: Unary, Postfix, and Basic Binary Expressions ✅ PARTIALLY COMPLETED

**Goal:** ✅ Expand expression parsing capabilities.

1. **Implement `parse_postfix_expression`:** ✅ **COMPLETED** (2025-07-06)
    * **File:** `expression_parser.c`.
    * **Description:** ✅ **IMPLEMENTED** `BaaNode* parse_postfix_expression(BaaParser* p, BaaNode* base_expr);`.
        * ✅ Parses postfix operations on a base expression.
        * ✅ Handles postfix operators in a loop:
            * ✅ `BAA_TOKEN_LPAREN` (function call arguments - `BAA_NODE_KIND_CALL_EXPR`). Parses argument list (comma-separated expressions) until `BAA_TOKEN_RPAREN`.
            * `BAA_TOKEN_LBRACKET` (array indexing - `BAA_NODE_KIND_INDEX_EXPR`). Parse expression inside `[]`.
            * `BAA_TOKEN_INCREMENT` / `BAA_TOKEN_DECREMENT` (postfix ++/--).
        * ✅ Builds up nested AST nodes for multiple postfix operations (e.g., `foo()[0]++`).
        * ✅ **Function Call Support**: Complete implementation with `parse_call_expression()`.
    * **Verification:** ✅ Can parse `foo()`, `جمع(أ، ب)`, nested calls like `outer(inner(x), y)`.

2. **Implement `parse_unary_expression`:** ✅ COMPLETED
    * **File:** `expression_parser.c`.
    * **Description:** ✅ `BaaNode* parse_unary_expression(BaaParser* p);`.
        * ✅ Checks for unary operators: `BAA_TOKEN_PLUS`, `BAA_TOKEN_MINUS`, `BAA_TOKEN_BANG` (`!`).
        * ✅ If found, consumes operator, recursively calls `parse_unary_expression` for the operand, and creates a `BAA_NODE_KIND_UNARY_EXPR` node.
        * ✅ If no unary operator, calls `parse_primary_expression`.
    * **Verification:** ✅ Can parse `-x`, `!flag`, `+value`.

3. **Implement Expression Parsing Precedence Levels (Selected Binary Ops):** ✅ COMPLETED
    * **File:** `expression_parser.c`.
    * **Description:** ✅ Implemented precedence climbing algorithm for binary operators.
        * ✅ `parse_binary_expression_rhs(parser, min_precedence, left_expr)` handles all binary operators with proper precedence.
        * ✅ Supports multiplicative (`*`, `/`, `%`), additive (`+`, `-`), comparison (`<`, `<=`, `>`, `>=`), equality (`==`, `!=`), and logical (`&&`, `||`) operators.
        * ✅ Integrated with `parse_expression()` and `parse_unary_expression()`.
    * **Details:** ✅ Uses precedence climbing with proper associativity handling for all binary operators.
    * **Verification:** ✅ Can parse `a * b + c`, `a + b * c` correctly respecting precedence.

### Phase 3: Statements - Control Flow & Blocks

**Goal:** Implement parsing for essential control flow and block structures.

1. **Implement `parse_block_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** `BaaNode* parse_block_statement(BaaParser* p);`.
        * Consumes `BAA_TOKEN_LBRACE`.
        * Loops, calling `parse_declaration_or_statement()` until `BAA_TOKEN_RBRACE` or `BAA_TOKEN_EOF`.
        * Consumes `BAA_TOKEN_RBRACE`.
        * Creates `BAA_NODE_KIND_BLOCK_STMT` node.
    * **Verification:** Can parse `{ stmt1. stmt2. }`.

2. **Implement `parse_statement` (Dispatcher):**
    * **File:** `statement_parser.c`.
    * **Description:** `BaaNode* parse_statement(BaaParser* p);`.
        * Uses `current_token.type` to decide which specific statement parsing function to call (e.g., `if (check_token(p, BAA_TOKEN_IF)) return parse_if_statement(p);`).
        * Defaults to `parse_expression_statement` if no keyword matches.
    * **Verification:** Dispatches correctly based on leading tokens.

3. **Implement `parse_if_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** `BaaNode* parse_if_statement(BaaParser* p);`.
        * Consumes `BAA_TOKEN_IF`, `BAA_TOKEN_LPAREN`.
        * Parses condition expression.
        * Consumes `BAA_TOKEN_RPAREN`.
        * Parses `then` branch (statement or block).
        * Optionally matches `BAA_TOKEN_ELSE` and parses `else` branch.
        * Creates `BAA_NODE_KIND_IF_STMT`.
    * **Verification:** Parses `إذا (cond) then_stmt.` and `إذا (cond) then_stmt. وإلا else_stmt.`.

4. **Implement `parse_return_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** `BaaNode* parse_return_statement(BaaParser* p);`.
        * Consumes `BAA_TOKEN_RETURN`.
        * Optionally parses an expression if not followed immediately by `.`.
        * Consumes `BAA_TOKEN_DOT`.
        * Creates `BAA_NODE_KIND_RETURN_STMT`.
    * **Verification:** Parses `إرجع.` and `إرجع expr.`.

5. **Implement `parse_while_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** Parses `طالما (condition) body_statement`.
    * **Verification:** Parses while loops.

### Phase 4: Type Specifiers & Declarations ✅ COMPLETED

**Goal:** ✅ Enable parsing of type information and variable/function declarations.

1. **Implement `parse_type_specifier` (Basic Primitive Types):** ✅ COMPLETED
    * **File:** `type_parser.c`.
    * **Description:** `BaaNode* parse_type_specifier(BaaParser* p);`.
        * ✅ Checks `current_token.type` for type keywords (e.g., `BAA_TOKEN_TYPE_INT`, `BAA_TOKEN_TYPE_CHAR`).
        * ✅ If a type keyword is found, consumes it and creates a `BAA_NODE_KIND_TYPE` with `BaaTypeAstData` for a primitive type (storing the name).
        * ✅ Reports error if no valid type specifier is found.
        * ✅ Added support for array types with `type[size]` syntax.
    * **Verification:** ✅ Can parse `عدد_صحيح`, `حرف`, and array types like `عدد_صحيح[10]`.

2. **Implement `parse_variable_declaration_statement`:** ✅ COMPLETED
    * **File:** `declaration_parser.c`.
    * **Description:** `BaaNode* parse_variable_declaration_statement(BaaParser* p, BaaAstNodeModifiers initial_modifiers);`.
        * ✅ Handles optional `BAA_TOKEN_CONST` (passed as `initial_modifiers` or parsed here).
        * ✅ Calls `parse_type_specifier()` to get the type node.
        * ✅ Consumes `BAA_TOKEN_IDENTIFIER` for the variable name.
        * ✅ Optionally parses `=` and an initializer expression.
        * ✅ Consumes `BAA_TOKEN_DOT`.
        * ✅ Creates `BAA_NODE_KIND_VAR_DECL_STMT`.
    * **Verification:** ✅ Parses `ثابت عدد_صحيح x = 10.`, `حرف c.` .

3. **Update `parse_declaration_or_statement`:** ✅ COMPLETED
    * **File:** `statement_parser.c`.
    * **Description:** ✅ Modified `parse_statement` to check if `current_token` could start a declaration (e.g., `BAA_TOKEN_CONST` or a type keyword). If so, call `parse_variable_declaration_statement`. Otherwise, call existing statement parsing.
    * **Verification:** ✅ Correctly distinguishes between declarations and other statements.

4. **Implement `parse_parameter_list` and `parse_parameter`:** ✅ **COMPLETED** (2025-07-06)
    * **File:** `declaration_parser.c`.
    * **Description:** ✅ **IMPLEMENTED**
        * ✅ `bool parse_parameter_list(BaaParser* p, BaaNode*** parameters, size_t* parameter_count)`: Parses `( (type_specifier identifier (',' type_specifier identifier)*)? )`. Returns dynamic array of `BAA_NODE_KIND_PARAMETER` nodes.
        * ✅ `BaaNode* parse_parameter(BaaParser* p)`: Parses a single `type_specifier identifier`.
    * **Verification:** ✅ Parses `(عدد_صحيح a, حرف b)`, `()`, `(عدد_صحيح x)` with proper error handling.

5. **Implement `parse_function_definition`:** ✅ **COMPLETED** (2025-07-06)
    * **File:** `declaration_parser.c`.
    * **Description:** ✅ **IMPLEMENTED** `BaaNode* parse_function_definition(BaaParser* p, BaaAstNodeModifiers initial_modifiers);`.
        * ✅ Parses return type using `parse_type_specifier()`.
        * ✅ Consumes `BAA_TOKEN_IDENTIFIER` for function name.
        * ✅ Calls `parse_parameter_list()` with proper memory management.
        * ✅ Parses function body (block statement using `parse_block_statement()`).
        * ✅ Creates `BAA_NODE_KIND_FUNCTION_DEF` with comprehensive error handling.
    * **Verification:** ✅ Parses `فراغ foo() {}`, `عدد_صحيح add(عدد_صحيح a, عدد_صحيح b) { إرجع a + b؛ }` with Arabic syntax support.

### Phase 5: Advanced Control Flow, Expressions, and Array Types

**Goal:** Implement remaining core language constructs.

1. **Implement `parse_for_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** Parses `لكل (initializer; condition; increment) body`.
        * Initializer can be a variable declaration or an expression statement (without the trailing dot).
        * Condition and increment are expressions.
    * **Verification:** Parses C-style for loops.

2. **Implement `parse_break_statement` and `parse_continue_statement`:**
    * **File:** `statement_parser.c`.
    * **Description:** Parse `توقف.` and `استمر.`.
    * **Verification:** Correctly parsed.

3. **Extend `parse_type_specifier` for Arrays:**
    * **File:** `type_parser.c`.
    * **Description:** Modify to handle `type_specifier []` (array type).
        * It should recursively call itself for the element type, then parse `[]`.
        * Creates a `BAA_NODE_KIND_TYPE` with `BaaTypeAstData` for an array.
    * **Verification:** Can parse `عدد_صحيح[]`, `حرف[][]`.

4. **Implement `parse_array_literal_expression`:**
    * **File:** `expression_parser.c`.
    * **Description:** Parses `[ element1, element2, ... ]`. Elements are expressions.
    * **Verification:** Parses `[1, x, foo()]`.

5. **Implement Array Indexing in `parse_postfix_expression`:**
    * **File:** `expression_parser.c`.
    * **Description:** Add logic to handle `expression [ index_expression ]`.
    * **Verification:** Parses `my_array[i+1]`.

6. **Implement `parse_cast_expression` (if syntax is decided):**
    * **File:** `expression_parser.c`.
    * **Description:** Parses `(type_specifier) expression`. This will be a new precedence level, likely higher than unary.
    * **Verification:** Parses `(عدد_حقيقي)my_int`.

### Phase 6: Parser Polish & Advanced Error Handling

**Goal:** Refine error reporting, synchronization, and ensure robustness.

1. **Review and Enhance All Error Messages:**
    * **Description:** Go through all `parser_error` calls. Ensure messages are specific, helpful, and in Arabic. Indicate what was expected vs. found.
    * **Verification:** Error messages are clear and localized.

2. **Fine-tune `synchronize()` Strategy:**
    * **Description:** Test `synchronize()` with various syntax errors. Add more synchronization points if needed (e.g., specific keywords, block delimiters) to improve recovery and allow more errors to be found in one pass.
    * **Verification:** Parser recovers reasonably well from common errors without excessive cascading or skipping valid code.

3. **Test Suite Expansion:**
    * **Description:** Create a comprehensive suite of test Baa files with valid and invalid syntax, covering all language constructs and error conditions.
    * Use an AST pretty-printer (from AST Phase 5) to verify the structure of generated ASTs for valid inputs.
    * Verify correct error messages and locations for invalid inputs.
    * **Verification:** High test coverage for parser logic.

### Future Considerations (Beyond initial redesign)

* Parsing `struct`, `union`, `enum` definitions.
* Parsing pointer type specifiers and dereference/address-of operators.
* Support for designated initializers in array/struct literals.
* Parsing module-related syntax.
* Parsing advanced function features (optional/named params).

This detailed parser roadmap, combined with the AST roadmap, should guide the implementation towards a well-structured and robust front-end for the Baa compiler. Remember to implement unit tests for each parsing function and AST node type as you go.
