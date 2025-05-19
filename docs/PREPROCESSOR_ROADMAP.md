# Baa Language Preprocessor Roadmap

This roadmap outlines the planned improvements and current status of the Baa language preprocessor implementation.

## Core Preprocessor Functionality

- [x] Separate processing stage before lexer
- [x] Recursive file processing
- [x] File encoding detection (UTF-8 with/without BOM, UTF-16LE)
- [x] Dynamic buffer for output generation
- [x] Error reporting via output parameter
- [x] Input source abstraction (file, string). ([ ] Add stdin support)
- [x] Support for UTF-8 input files (with conversion to internal wchar_t)
- [x] Improved error reporting (unified system with original file, line, and column numbers, including through expansions)

## Directive Handling

- **Includes:**
  - [x] `#تضمين "..."` (Relative path resolution)
  - [x] `#تضمين <...>` (Include path searching)
  - [x] Circular include detection
- **Macros:**
  - [x] `#تعريف NAME BODY` (Parameterless macro definition)
  - [x] Simple text substitution for parameterless macros
  - [x] `#الغاء_تعريف NAME` (undef)
  - [x] Function-like macros (with parameters)
  - [x] Stringification (`#`)
  - [x] Token Pasting (`##`) (concatenation)
  - [x] Macro recursion detection
  - [ ] Fully robust argument parsing (complex edge cases with literals/whitespace)
  - [x] **C99 Support**: Implement Variadic Macros (using `وسائط_إضافية` for `...` and `__وسائط_متغيرة__` for `__VA_ARGS__`).
- **Conditional Compilation:**
  - [x] `#إذا_عرف MACRO` (ifdef - checks if macro is defined, i.e., `معرف`)
  - [x] `#إذا_لم_يعرف MACRO` (ifndef - checks if macro is not defined, i.e., `!معرف`)
  - [x] `#وإلا_إذا expression` (elif - constant expression evaluation)
  - [x] `#إلا` (else)
  - [x] `#نهاية_إذا` (endif)
  - [x] Support for bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) in conditional expressions.
  - [x] Support for decimal, hexadecimal (`0x`), and binary (`0b`) integer literals in conditional expressions.

## Key Areas for C99 Compliance and Improvement

- [x] **Macro Expansion Rescanning**: Implement full rescanning of macro expansion results. The output of any macro substitution (from parameter replacement, `#`, or `##` operations) must be rescanned for further macro names to expand. This is critical for C99 compliance.
- [ ] **Full Macro Expansion in Conditional Expressions**: Enhance the expression evaluator for `#إذا` and `#وإلا_إذا` to support full macro expansion (including function-like macros and rescanning of results) before evaluation. Currently, object-like macros expanding to simple integers are handled, and the expression parser itself handles operators and literals. The gap is primarily around expanding *function-like* macros encountered within the expression.
- [x] **`__السطر__` (`__LINE__`) Expansion**: Ensure `__السطر__` (and its C99 equivalent `__LINE__`) expands to an integer constant, not a string literal. (Corrected during rescanning implementation).
- [ ] **Token Pasting (`##`) during Rescanning (Known Issue)**: Enhance the rescanning logic to correctly handle the `##` operator when it *itself* is a result of a macro expansion, or when its operands are complex macros that might interact unexpectedly with the pasting before their full expansion in the rescan phase. (Basic ## works, complex cases or `##` as part of expansion output needs review).
- [ ] **Multi-line Comment Handling**: Implement robust handling for multi-line comments (`/* ... */`) across all preprocessor stages, ensuring they are correctly stripped and do not interfere with parsing or directive recognition.
- [ ] **Macro Redefinition Warnings/Errors**: Implement checks for macro redefinitions. Issue warnings or errors for incompatible redefinitions, as per C99 standard behavior.
- [x] **Predefined `__func__`**: Implement the C99 `__func__` predefined identifier (Baa: `__الدالة__`).
- [x] **Predefined `__STDC_VERSION__` equivalent**: Implement Baa's version macro `__إصدار_المعيار_باء__`.

  - **Other Standard Directives:**
    - [x] `#خطأ message` (Baa: `#خطأ "رسالة الخطأ"`) - Implemented
    - [x] `#تحذير message` (Baa: `#تحذير "رسالة التحذير"`) - Implemented
    - [ ] `#سطر number "filename"` (Baa: `#سطر ١٠٠ "ملف.ب"`)
    - [ ] **C99 Support**: Implement `أمر_براغما` operator (Baa: `أمر_براغما("توجيه")`).
    - [ ] `#براغما directive` (Baa: `#براغما توجيه_خاص`) (Investigate C99 standard pragmas like `STDC FP_CONTRACT`, `STDC FENV_ACCESS`, `STDC CX_LIMITED_RANGE`, and common Baa-specific pragmas like `مرة_واحدة` for `#pragma once`).

## Known Issues / Areas for Refinement

- **Error/Warning Location Precision**: Line and column numbers reported for errors/warnings originating from within directives (e.g., `#إذا`, `#تحذير`) may not always point to the precise location in the original source file, sometimes defaulting to `1:1` or the start of an included file.

## Testing and Validation

- [ ] Unit test coverage for directive parsing
- [ ] Unit tests for include path resolution
- [ ] Unit tests for macro definition and substitution
- [ ] Unit tests for circular include detection
- [ ] Unit tests for conditional compilation logic (when implemented)
- [ ] Integration tests with the overall compiler flow

## Implementation Priorities (Excluding Testing for now)

1. **Robustness & Advanced Features:**
    - [x] Support for UTF-8 input files.
    - [x] Input source abstraction (file, string). (*stdin not yet implemented*)
    - [x] Support for bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) and hex/binary literals in conditional expressions.
    - [x] Predefined macros (`__الملف__`, `__السطر__` (as int), `__التاريخ__`, `__الوقت__`, `__الدالة__` (placeholder), `__إصدار_المعيار_باء__`).
    - [ ] Improve error recovery mechanisms (allow continuation after some errors to find more issues).
2. **Macro Edge Cases:**
        - Address complex edge cases in macro substitution and argument parsing.
