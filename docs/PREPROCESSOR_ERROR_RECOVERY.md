# Baa Preprocessor Error Recovery System

## Overview

The Baa preprocessor now includes a comprehensive error recovery system that allows it to continue processing source code even when errors are encountered. This system improves the user experience by reporting multiple errors in a single compilation pass and attempting to recover from common preprocessing mistakes.

## Key Features

### 1. Diagnostic System
- **Centralized Error Reporting**: All errors and warnings are collected in a unified diagnostic system
- **Precise Location Tracking**: Errors are reported with accurate file, line, and column information
- **Arabic Error Messages**: All error messages are provided in Arabic for consistency with the Baa language
- **Multiple Error Accumulation**: The system can collect and report multiple errors in a single pass

### 2. Error Recovery Mechanisms

#### Directive Recovery
- **Unknown Directive Handling**: When an unknown preprocessor directive is encountered, the system skips to the next line and continues processing
- **Malformed Directive Recovery**: Attempts to find the next valid directive when current directive parsing fails
- **Conditional Stack Validation**: Automatically detects and reports unmatched conditional directives

#### Expression Evaluation Recovery
- **Invalid Expression Handling**: When conditional expressions contain syntax errors, the system reports the error but continues processing
- **Division by Zero Protection**: Prevents crashes from division by zero in preprocessor expressions
- **Undefined Macro Handling**: Gracefully handles undefined macros in expressions (evaluates to 0)

#### Include File Recovery
- **Circular Include Detection**: Prevents infinite recursion from circular includes
- **Alternative File Search**: Attempts to find files with different extensions when includes fail
- **Path Resolution Fallbacks**: Provides helpful error messages when include paths cannot be resolved

### 3. Error Limits and Safeguards

#### Maximum Error Threshold
- **Configurable Error Limits**: Processing stops after a configurable number of errors (default: 25 per file, 50 globally)
- **Prevents Error Flooding**: Avoids overwhelming users with too many error messages
- **Critical Error Detection**: Distinguishes between recoverable and fatal errors

#### Infinite Loop Prevention
- **Macro Expansion Limits**: Prevents runaway macro expansion
- **Conditional Nesting Limits**: Limits the depth of nested conditional directives
- **Include Depth Tracking**: Prevents excessive include file nesting

### 4. Recovery Strategies

#### Synchronization Points
- **Directive Boundaries**: Uses preprocessor directives as natural synchronization points
- **Line-by-Line Recovery**: Falls back to line-by-line processing when directive recovery fails
- **Comment Skipping**: Properly handles and skips comments during error recovery

#### State Restoration
- **Context Preservation**: Maintains file and line number context during recovery
- **Stack Cleanup**: Automatically cleans up conditional and macro expansion stacks
- **Memory Management**: Ensures no memory leaks during error recovery

## Implementation Details

### Core Components

1. **Error Recovery Utilities** (`preprocessor_utils.c`)
   - `skip_to_next_line()`: Advances to the next line for recovery
   - `find_next_directive()`: Locates the next preprocessor directive
   - `attempt_directive_recovery()`: Attempts to recover from directive errors
   - `validate_and_recover_conditional_stack()`: Cleans up unmatched conditionals

2. **Diagnostic System** (`preprocessor_utils.c`)
   - `add_preprocessor_diagnostic()`: Adds errors/warnings to the diagnostic list
   - `format_preprocessor_error_at_location()`: Formats error messages with location info
   - `free_diagnostics_list()`: Manages memory for diagnostic messages

3. **Enhanced Core Processing** (`preprocessor_core.c`)
   - Integrated error checking at each processing step
   - Graceful degradation when errors occur
   - Continued processing after recoverable errors

4. **Expression Evaluator Updates** (`preprocessor_expr_eval.c`)
   - Robust error handling in expression parsing
   - Graceful handling of malformed expressions
   - Proper error reporting with precise locations

### Error Types Handled

1. **Syntax Errors**
   - Malformed preprocessor directives
   - Invalid macro definitions
   - Unterminated string literals in macros

2. **Semantic Errors**
   - Undefined macros in expressions
   - Division by zero in constant expressions
   - Type mismatches in macro arguments

3. **Structural Errors**
   - Unmatched conditional directives
   - Missing #endif statements
   - Circular include dependencies

4. **Resource Errors**
   - File not found errors
   - Memory allocation failures
   - Include path resolution errors

## Usage Examples

### Basic Error Recovery
```c
BaaPpSource source = {
    .type = BAA_PP_SOURCE_STRING,
    .source_name = "<test>",
    .data.source_string = L"#define VALID 1\n"
                         L"#invalid_directive\n"  // Error here
                         L"#define ALSO_VALID 2\n" // But this still processes
};

wchar_t *error_message = NULL;
wchar_t *result = baa_preprocess(&source, NULL, &error_message);

// result contains processed valid parts
// error_message contains details about the invalid directive
```

### Multiple Error Reporting
The system collects multiple errors and reports them all:
```
test.baa:5:1: خطأ: توجيه معالج مسبق غير معروف 'invalid_directive'
test.baa:12:8: خطأ: تعبير غير صالح في التوجيه الشرطي
test.baa:20:1: خطأ: توجيه شرطي غير مطابق - مفقود #نهاية_إذا
```

## Testing

The error recovery system is thoroughly tested with:

1. **Unit Tests** (`test_error_recovery.c`)
   - Tests for each type of error recovery
   - Validation of continued processing
   - Error limit verification

2. **Integration Tests** (`error_recovery_test.baa`)
   - Real-world error scenarios
   - Complex nested error conditions
   - Mixed valid and invalid code

## Benefits

1. **Improved User Experience**
   - Users see multiple errors at once instead of fixing one error at a time
   - More helpful error messages with precise locations
   - Continued processing provides more context about the code

2. **Better IDE Integration**
   - IDEs can show multiple error markers simultaneously
   - Faster development cycle with batch error reporting
   - More robust language server implementation

3. **Robust Processing**
   - Handles malformed code gracefully
   - Prevents crashes from unexpected input
   - Provides partial results even when errors occur

## Future Enhancements

1. **Warning System**
   - Add support for non-fatal warnings
   - Configurable warning levels
   - Warning suppression mechanisms

2. **Enhanced Recovery**
   - More sophisticated synchronization strategies
   - Better handling of deeply nested errors
   - Improved macro expansion error recovery

3. **Performance Optimization**
   - Faster error recovery algorithms
   - Reduced memory overhead for diagnostics
   - Optimized synchronization point detection

## Configuration

The error recovery system can be configured through:

- **Error Limits**: Maximum number of errors before aborting
- **Recovery Strategies**: Choice of recovery mechanisms
- **Diagnostic Verbosity**: Level of detail in error messages
- **Synchronization Points**: Custom synchronization strategies

This comprehensive error recovery system makes the Baa preprocessor more robust and user-friendly, enabling developers to identify and fix multiple issues efficiently.
