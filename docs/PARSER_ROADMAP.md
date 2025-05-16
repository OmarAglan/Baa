# Baa Language Parser Implementation Roadmap (New Design)

**Status: This document outlines the implementation plan for the new Parser following the removal of the previous implementation. All items are planned unless otherwise noted. This roadmap aligns with the new AST design in `docs/AST.md`.**

## Phase 1: Parser Foundation & Basic Expressions/Statements

- **Parser Core Infrastructure:**
  - [ ] Define `BaaParser` state structure (lexer instance, current/previous tokens, error state, source location tracking).
  - [ ] Implement core token handling utilities: `advance()`, `peek()`, `match_token()`, `consume_token()`.
  - [ ] Basic error reporting mechanism (`parser_error()`) - focus on clear messages and accurate `BaaSourceLocation`.
  - [ ] Basic error synchronization strategy (`synchronize()`) - initial strategy: skip to next statement terminator `.` or common statement-starting keyword.
- **Parser Module Setup:**
  - [ ] Create initial C files and headers for core parser modules: `parser.c`/`parser.h` (main), `expression_parser.c`/`expression_parser.h`, `statement_parser.c`/`statement_parser.h`, `declaration_parser.c`/`declaration_parser.h`, `type_parser.c`/`type_parser.h` (as outlined in `PARSER.md`).
- **Primary Expression Parsing:**
  - [ ] Parse Literal Expressions (`BAA_EXPR_LITERAL` for numbers, strings, chars, booleans, null).
  - [ ] Parse Identifier Expressions (as `BAA_EXPR_VARIABLE`).
  - [ ] Parse Grouping Expressions (parentheses).
- **Simple Statement Parsing:**
  - [ ] Parse Expression Statements (`BAA_STMT_EXPR`).
  - [ ] Parse Block Statements (`BAA_STMT_BLOCK`).
- **Top-Level Program Structure:**
  - [ ] Implement `baa_parse_program` to parse a sequence of (initially simple) statements or declarations, returning a `BaaProgram` AST node.

## Phase 2: Operators and Basic Declarations

- **Unary and Binary Expression Parsing:**
  - [ ] Implement parsing for unary operators (e.g., `!`, `-`) with correct precedence (`BAA_EXPR_UNARY`).
  - [ ] Implement parsing for binary operators (e.g., `+`, `-`, `*`, `/`, `==`, `!=`, `<`, `>` etc.) primarily using a cascaded series of functions for precedence levels. Document this chosen strategy in detail in `docs/PARSER.md`, including how operator precedence and associativity from `src/operators/` are handled. (Consider Pratt parsing or Shunting-Yard if cascaded approach proves insufficient for complex cases.) (`BAA_EXPR_BINARY`).
- **Assignment Expression Parsing:**
  - [ ] Parse simple assignment (`=`) (`BAA_EXPR_ASSIGN`).
  - [ ] Parse compound assignment operators (e.g., `+=`, `-=`).
- **Variable Declaration Parsing (`BAA_STMT_VAR_DECL`):**
  - [ ] Parse variable declarations (e.g., `عدد_صحيح اسم_المتغير.`).
  - [ ] Parse variable declarations with initializers.
  - [ ] Handle `ثابت` (const) modifier.

## Phase 3: Control Flow Statements

- **If Statements (`BAA_STMT_IF`):**
  - [ ] Parse `إذا` (if) conditions.
  - [ ] Parse `then` branch (single statement or block).
  - [ ] Parse optional `وإلا` (else) branch.
- **While Loops (`BAA_STMT_WHILE`):**
  - [ ] Parse `طالما` (while) conditions.
  - [ ] Parse loop body (single statement or block).
- **Return Statements (`BAA_STMT_RETURN`):**
  - [ ] Parse `إرجع` (return) statements.
  - [ ] Parse optional return value expression.

## Phase 4: Functions and More Control Flow

- **Function Declaration Parsing (`BAA_NODE_FUNCTION`):**
  - [ ] Parse function name.
  - [ ] Parse parameter list (initially simple types and names - `BaaParameter`).
  - [ ] Parse return type.
  - [ ] Parse function body (`BaaBlock`).
  - [ ] Handle `مضمن` (inline) specifier.
- **Function Call Expression Parsing (`BAA_EXPR_CALL`):**
  - [ ] Parse function call syntax (callee expression, argument list).
- **For Loops (`BAA_STMT_FOR`):**
  - [ ] Parse `لكل` (for) loops: initializer, condition, increment clauses.
  - [ ] Parse loop body.
- **Break and Continue Statements (`BAA_STMT_BREAK`, `BAA_STMT_CONTINUE`):**
  - [ ] Parse `توقف` (break) statements.
  - [ ] Parse `أكمل` (continue) statements.

## Phase 5: Advanced Types and Expressions

- **Type Parsing (`type_parser.c` module - Planned):**
  - [ ] Parse basic type annotations (`عدد_صحيح`, `عدد_حقيقي`, etc.).
  - [ ] Parse array type annotations (e.g., `عدد_صحيح[]`).
- **Array Literal & Indexing Expressions (`BAA_EXPR_ARRAY`, `BAA_EXPR_INDEX`):**
  - [ ] Parse array literal expressions (e.g., `[1, 2, 3]`).
  - [ ] Parse array indexing expressions (e.g., `my_array[0]`).
- **Cast Expressions (`BAA_EXPR_CAST`):**
  - [ ] Parse explicit type cast expressions.
- **Switch Statements (`BAA_STMT_SWITCH`, `BAA_STMT_CASE`, `BAA_STMT_DEFAULT`):**
  - [ ] Parse `اختر` (switch) expression.
  - [ ] Parse `حالة` (case) labels with constant expressions.
  - [ ] Parse `افتراضي` (default) labels.
  - [ ] Parse statement blocks for cases.
- **Specific Language Features from `language.md`:**
    - [ ] Parse Compound Literals (C99-style).
    - [ ] Parse Member Access operators (`::` and `->`) once struct/union parsing is available.


## Phase 6: Parser Infrastructure & Tooling Enhancements

- **Error Handling and Recovery:**
  - [ ] Develop more sophisticated error recovery strategies (e.g., panic mode with synchronization to specific tokens like statement terminators `.`, keywords, or delimiters like `}`).
  - [ ] Design for context-specific and helpful error messages (e.g., "Expected an expression for the 'إذا' condition" instead of generic "Syntax error"). Ensure messages are suitable for Arabic and RTL display.
- **RTL Source Code Considerations for Error Reporting:**
  - [ ] Ensure parser error reporting mechanisms accurately convey `BaaSourceLocation` data so that tools (IDE plugins, etc.) can correctly highlight issues in RTL source code.
- **Semantic Action Integration:**
  - [ ] Define a clear strategy for how and when minimal semantic actions (beyond AST node creation) might be integrated during parsing, if any (e.g., very early symbol table interactions for specific forward declaration scenarios if the language design requires it). Primarily, AST construction is the parser's main semantic action.

## Future Considerations (Longer Term)

- **Advanced Function Features:**
  - [ ] Optional parameters with default values.
  - [ ] Rest parameters.
  - [ ] Named arguments in function calls.
- **Structs, Unions, Enums:**
  - [ ] Parse `بنية` (struct) definitions and member access.
  - [ ] Parse `اتحاد` (union) definitions and member access.
  - [ ] Parse `تعداد` (enum) definitions.
- **Pointer-related syntax** (once pointers are fully designed in the type system).
- **Module System:**
  - [ ] Parse `تضمين` (import) directives if they are to be handled by the parser (currently preprocessor).
  - [ ] Parse module declaration syntax if introduced.
- **Integration with Semantic Analyzer for contextual parsing (e.g., distinguishing type names from variable names).**
- **Parser Performance:**
  - [ ] Profile parser performance on large Baa source files and identify potential bottlenecks for optimization (e.g., memoization for certain non-terminals if grammar becomes complex, though usually not needed for LL(k) grammars).
- **Handling Preprocessor Output:**
  - [ ] Continuously verify that the parser correctly handles various valid (and sometimes complex) token sequences that can result from preprocessor macro expansions.

## Test Strategy

- [ ] Unit tests for individual parsing functions (e.g., `parse_if_statement`, `parse_binary_expression`).
  - [ ] Develop utility functions or a test harness to easily feed specific token sequences to individual parser functions for isolated unit testing.
- [ ] Integration tests: Parse token streams from `lexer_test_suite.baa` and verify the generated AST structure (requires AST pretty-printer).
- [ ] Error case testing: Ensure syntax errors are correctly reported with accurate location information.
- [ ] Test recovery from syntax errors.

## Documentation Approach

- [ ] Update `PARSER.md` continuously as features are implemented.
- [ ] Add Doxygen comments to `parser.h` for all public structures and functions.
- [ ] Document parsing logic and algorithms within source files.
