## Updated `docs/PARSER_ROADMAP.md` (v3 - Current Status)

**Status: ✅ CORE PARSER COMPLETE - This document outlines the implementation status of the Parser. Priority 4 (Function Definitions and Calls) was completed in July 2025.**

The parser uses a recursive descent strategy to generate an AST conforming to `AST_ROADMAP.md` (v3).

### Phase 0: Parser Core & Token Utilities (✅ COMPLETED)

**Goal:** Set up the fundamental parser structure and token manipulation helpers.

1. **Parser Structure & Initialization:**
    * **Files:** `parser_internal.h`, `parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** `BaaParser` struct defined with lexer, current/prev tokens, error flags. Creation/destruction logic implemented.

2. **Token Handling Utilities:**
    * **File:** `parser_utils.h` (implemented in `parser.c`)
    * **Status:** ✅ **COMPLETED**
    * **Details:** `advance`, `check_token`, `match_token`, and `consume_token` are fully functional.

3. **Error Reporting & Synchronization:**
    * **File:** `parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** `baa_parser_error_at_token` handles panic mode. `baa_parser_synchronize` recovers at statement boundaries (keywords or `.`).

### Phase 1: Minimal Program & Primary Expressions (✅ COMPLETED)

**Goal:** Parse a simple program structure.

1. **Program Entry Point (`baa_parse_program`):**
    * **File:** `parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses top-level declarations/statements until EOF and returns `BAA_NODE_KIND_PROGRAM`.

2. **Primary Expressions:**
    * **File:** `expression_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Handles Literals (Int, String), Identifiers, and Grouping `( ... )`.

3. **Expression Statements:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses expression followed by `.` terminator.

4. **Statement Dispatcher:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** `parse_statement` routes to specific handlers based on token type.

### Phase 2: Declarations & Complex Expressions (✅ COMPLETED)

**Goal:** Expand parsing to variables and math logic.

1. **Type Specifiers:**
    * **File:** `type_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses primitives (`عدد_صحيح`) and array types (`type[]`).

2. **Variable Declarations:**
    * **File:** `declaration_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Handles `[const] type name [= init].`.

3. **Binary & Unary Expressions:**
    * **File:** `expression_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Implements precedence climbing for arithmetic, comparison, and logical operators.

4. **Postfix Expressions (Calls/Index):**
    * **File:** `expression_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Handles function calls `foo(args)` and array indexing `arr[i]`.

### Phase 3: Control Flow Statements (✅ COMPLETED)

**Goal:** Implement flow control logic.

1. **Block Statement:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `{ ... }`.

2. **If Statement:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `إذا ... [وإلا ...]`.

3. **While Statement:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `طالما ...`.

4. **For Statement:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `لكل (init; cond; inc) ...`.

5. **Return, Break, Continue:**
    * **File:** `statement_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `إرجع`, `توقف`, `استمر`.

### Phase 4: Function Definitions (Priority 4 - ✅ COMPLETED)

**Goal:** Implement parsing for function signatures and bodies.

1. **Parameter Parsing:**
    * **File:** `declaration_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `(type name, ...)`.

2. **Function Definition:**
    * **File:** `declaration_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses `type name(params) { body }`. Handles lookahead to distinguish functions from variables.

3. **Call Expressions:**
    * **File:** `expression_parser.c`
    * **Status:** ✅ **COMPLETED**
    * **Details:** Parses argument lists in function calls.

### Phase 5: Advanced Features (Priority 5 - Planned)

**Goal:** Implement advanced language features (Arrays, Structs).

1. **Array Literal Parsing:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Parse `[elem1, elem2]`.

2. **Struct/Union/Enum Definitions:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Parse definitions using `بنية` (struct), etc.

3. **Member Access Parsing:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Handle `obj.member` or `ptr->member` in expression parser.

4. **Cast Expressions:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Parse `(type)expr`.

### Phase 6: Refinement & Stability (Ongoing)

1. **Error Messages:**
    * **Status:** 🔄 **IN PROGRESS**
    * **Description:** Ensure all error messages are in Arabic and provide context.

2. **Synchronization:**
    * **Status:** 🔄 **IN PROGRESS**
    * **Description:** Fine-tune `synchronize()` to recover gracefully from syntax errors inside deep structures.

3. **Fuzz Testing:**
    * **Status:** 📅 **PLANNED**
    * **Description:** Feed random/malformed input to ensure parser robustness (no crashes).
