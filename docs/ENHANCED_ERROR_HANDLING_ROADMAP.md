# Enhanced Error Handling & Reporting - Implementation Roadmap

## 🎯 Overview

This document outlines the step-by-step implementation plan for enhancing the Baa lexer's error handling and reporting capabilities. The goal is to provide more specific error types, better error context, helpful suggestions, and robust error recovery mechanisms.

## ✅ Step 1: Enhanced Error Token Types (COMPLETED)

### **What was implemented:**

1. **Specific Error Token Types**: Added 8 new specific error token types:
   - `BAA_TOKEN_ERROR_UNTERMINATED_STRING`
   - `BAA_TOKEN_ERROR_UNTERMINATED_CHAR`
   - `BAA_TOKEN_ERROR_UNTERMINATED_COMMENT`
   - `BAA_TOKEN_ERROR_INVALID_ESCAPE`
   - `BAA_TOKEN_ERROR_INVALID_NUMBER`
   - `BAA_TOKEN_ERROR_INVALID_CHARACTER`
   - `BAA_TOKEN_ERROR_NUMBER_OVERFLOW`
   - `BAA_TOKEN_ERROR_INVALID_SUFFIX`

2. **Enhanced Source Spans**: Added `BaaSourceSpan` structure with:
   - Start/end line and column tracking
   - Character offset tracking from source start
   - More precise error location information

3. **Error Context System**: Added `BaaErrorContext` structure with:
   - Error codes for internationalization
   - Error categories for grouping
   - Optional fix suggestions
   - Context before/after error location

4. **Enhanced Token Structure**: Updated `BaaToken` to include:
   - `BaaSourceSpan span` for enhanced location tracking
   - `BaaErrorContext *error` for error-specific information

5. **Utility Functions**: Added:
   - `baa_create_error_context()` and `baa_free_error_context()`
   - `baa_get_error_category_description()` for Arabic category names
   - `baa_get_error_type_description()` for Arabic error type descriptions
   - `baa_token_is_error()` for error token type checking

6. **Memory Management**: Updated `baa_free_token()` to properly handle error contexts

### **Files Modified:**
- `include/baa/lexer/lexer.h` - Added new types and structures
- `src/lexer/lexer_internal.h` - Added enhanced error function declarations
- `src/lexer/lexer.c` - Implemented enhanced error token creation and utilities

---

## 🚀 Step 2: Update Token Scanners with Specific Error Types (NEXT PRIORITY)

### **Objective:**
Update all token scanner functions (`scan_string`, `scan_char_literal`, `scan_number`, etc.) to use the new specific error token types instead of generic `BAA_TOKEN_ERROR`.

### **Implementation Tasks:**

#### 2.1: Update String Literal Scanner (`scan_string`)
**File:** `src/lexer/token_scanners.c`

**Changes needed:**
1. **Unterminated String Errors:**
   ```c
   // CURRENT (line ~464):
   BaaToken *error_token = make_error_token(lexer, L"سلسلة نصية غير منتهية...");
   
   // NEW:
   BaaToken *error_token = make_specific_error_token(lexer, 
       BAA_TOKEN_ERROR_UNTERMINATED_STRING, 
       1001, "string",
       L"أضف علامة اقتباس مزدوجة \" في نهاية السلسلة",
       L"سلسلة نصية غير منتهية (بدأت في السطر %zu، العمود %zu)", 
       start_line, start_col);
   ```

2. **Invalid Escape Sequence Errors:**
   ```c
   // CURRENT (line ~440):
   BaaToken *error_token = make_error_token(lexer, L"تسلسل هروب غير صالح...");
   
   // NEW:
   BaaToken *error_token = make_specific_error_token(lexer,
       BAA_TOKEN_ERROR_INVALID_ESCAPE,
       1002, "escape", 
       L"استخدم تسلسل هروب صالح مثل \\س أو \\م أو \\يXXXX",
       L"تسلسل هروب غير صالح '\\%lc' في سلسلة نصية", 
       baa_escape_char_key);
   ```

#### 2.2: Update Character Literal Scanner (`scan_char_literal`)
**File:** `src/lexer/token_scanners.c`

**Changes needed:**
1. **Unterminated Character Errors:**
   ```c
   // CURRENT (line ~674):
   return make_error_token(lexer, L"قيمة حرفية غير منتهية...");
   
   // NEW:
   return make_specific_error_token(lexer,
       BAA_TOKEN_ERROR_UNTERMINATED_CHAR,
       1003, "character",
       L"أضف علامة اقتباس مفردة ' في نهاية المحرف",
       L"قيمة حرفية غير منتهية (علامة اقتباس أحادية ' مفقودة، بدأت في السطر %zu، العمود %zu)",
       start_line, start_col);
   ```

2. **Invalid Character Literal Errors:**
   ```c
   // CURRENT (line ~649):
   BaaToken *error_token = make_error_token(lexer, L"سطر جديد غير مسموح به...");
   
   // NEW:
   BaaToken *error_token = make_specific_error_token(lexer,
       BAA_TOKEN_ERROR_INVALID_CHARACTER,
       1004, "character",
       L"استخدم تسلسل هروب \\س للسطر الجديد",
       L"سطر جديد غير مسموح به في قيمة حرفية (بدأت في السطر %zu، العمود %zu)",
       start_line, start_col);
   ```

#### 2.3: Update Number Scanner (`scan_number`)
**File:** `src/lexer/token_scanners.c`

**Changes needed:**
1. **Invalid Number Format Errors:**
   ```c
   // CURRENT (line ~78):
   BaaToken *error_token = make_error_token(lexer, L"عدد سداسي عشر غير صالح...");
   
   // NEW:
   BaaToken *error_token = make_specific_error_token(lexer,
       BAA_TOKEN_ERROR_INVALID_NUMBER,
       1005, "number",
       L"أضف رقم سداسي عشري صالح بعد 0x",
       L"عدد سداسي عشر غير صالح: يجب أن يتبع البادئة 0x/0X رقم سداسي عشري (السطر %zu، العمود %zu)",
       lexer->line, lexer->column);
   ```

2. **Invalid Suffix Errors:**
   ```c
   // Add new validation for invalid suffixes like غغ, طططط, etc.
   BaaToken *error_token = make_specific_error_token(lexer,
       BAA_TOKEN_ERROR_INVALID_SUFFIX,
       1006, "number",
       L"استخدم لاحقات صالحة: غ (unsigned)، ط (long)، طط (long long)، ح (float)",
       L"لاحقة رقم غير صالحة '%ls' (السطر %zu، العمود %zu)",
       invalid_suffix, lexer->line, lexer->column);
   ```

#### 2.4: Update Comment Scanners
**File:** `src/lexer/token_scanners.c`

**Changes needed:**
1. **Unterminated Comment Errors:**
   ```c
   // CURRENT (line ~1008):
   return make_error_token(lexer, L"تعليق متعدد الأسطر غير منتهٍ...");
   
   // NEW:
   return make_specific_error_token(lexer,
       BAA_TOKEN_ERROR_UNTERMINATED_COMMENT,
       1007, "comment",
       L"أضف */ لإنهاء التعليق",
       L"تعليق متعدد الأسطر غير منتهٍ (بدأ في السطر %zu، العمود %zu)",
       comment_delimiter_start_line, comment_delimiter_start_col);
   ```

### **Testing for Step 2:**
Create test cases for each new error type:
```c
// Test unterminated string
const wchar_t* test_unterminated_string = L"\"hello world";

// Test invalid escape
const wchar_t* test_invalid_escape = L"\"hello\\q world\"";

// Test unterminated char
const wchar_t* test_unterminated_char = L"'a";

// Test invalid number
const wchar_t* test_invalid_number = L"0x";

// Test unterminated comment
const wchar_t* test_unterminated_comment = L"/* comment";
```

---

## 🛠️ Step 3: Enhanced Error Recovery Mechanisms

### **Objective:**
Implement more sophisticated error recovery strategies that are context-aware and prevent error cascading.

### **Implementation Tasks:**

#### 3.1: Context-Aware Synchronization
**File:** `src/lexer/lexer_internal.h` and `src/lexer/lexer.c`

**Add new synchronization functions:**
```c
// Enhanced synchronization based on error type
void enhanced_synchronize(BaaLexer *lexer, BaaTokenType error_type);

// Specific synchronization strategies
void synchronize_string_error(BaaLexer *lexer);      // Find next quote or newline
void synchronize_number_error(BaaLexer *lexer);      // Find next non-digit character
void synchronize_comment_error(BaaLexer *lexer);     // Find next */ or EOF
void synchronize_general_error(BaaLexer *lexer);     // Current basic strategy
```

#### 3.2: Error Count Limiting
**Add to BaaLexer structure:**
```c
typedef struct {
    // ... existing fields ...
    
    // Error tracking
    size_t error_count;
    size_t max_errors;              // Default: 50
    size_t consecutive_errors;      // Track cascading errors
    size_t max_consecutive_errors;  // Default: 10
    bool error_limit_reached;
} BaaLexer;
```

#### 3.3: Error Recovery Configuration
**Add configuration structure:**
```c
typedef struct {
    size_t max_errors;
    size_t max_consecutive_errors;
    bool stop_on_unterminated_string;
    bool stop_on_invalid_number;
    bool continue_after_comment_errors;
} BaaErrorRecoveryConfig;
```

---

## 📊 Step 4: Error Context Enhancement

### **Objective:**
Enhance error context with source code snippets and more helpful suggestions.

### **Implementation Tasks:**

#### 4.1: Source Context Extraction
**Add to lexer_internal.h:**
```c
// Extract source context around error location
void extract_error_context(BaaLexer *lexer, size_t error_position, 
                          wchar_t **before_context, wchar_t **after_context);

// Calculate error position in source
size_t calculate_error_character_position(BaaLexer *lexer);

// Get line content for error reporting
wchar_t* get_current_line_content(BaaLexer *lexer, size_t line_number);
```

#### 4.2: Smart Suggestions System
**Add suggestion generation:**
```c
// Generate contextual suggestions based on error type and content
wchar_t* generate_error_suggestion(BaaTokenType error_type, 
                                  const wchar_t* error_context);

// Specific suggestion generators
wchar_t* suggest_escape_sequence_fix(wchar_t invalid_escape_char);
wchar_t* suggest_number_format_fix(const wchar_t* invalid_number);
wchar_t* suggest_string_termination_fix(const wchar_t* partial_string);
```

---

## 🧪 Step 5: Comprehensive Testing Framework

### **Objective:**
Create a comprehensive testing framework for all error scenarios.

### **Implementation Tasks:**

#### 5.1: Error Test Suite
**Create:** `tests/unit/lexer/test_enhanced_errors.c`

**Test categories:**
1. **Specific Error Type Tests:**
   - Test each specific error token type is generated correctly
   - Verify error codes and categories are set properly
   - Check Arabic error messages are properly formatted

2. **Error Context Tests:**
   - Verify source spans are calculated correctly
   - Test suggestion generation for different error types
   - Check context extraction functionality

3. **Error Recovery Tests:**
   - Test synchronization after different error types
   - Verify error count limiting works correctly
   - Test cascading error prevention

4. **Integration Tests:**
   - Test complex error scenarios with multiple error types
   - Test error reporting with real Baa source code
   - Test memory management of error contexts

#### 5.2: Error Reporting Test Tool
**Create:** `tools/baa_error_tester.c`

**Features:**
- Load test files with known errors
- Generate error reports in different formats
- Verify error recovery behavior
- Test Arabic error message formatting

---

## 📝 Step 6: Documentation and Examples

### **Objective:**
Document the enhanced error handling system and provide examples.

### **Implementation Tasks:**

#### 6.1: Error Handling Documentation
**Create:** `docs/ERROR_HANDLING.md`

**Content:**
- Error type reference
- Error code reference
- Integration guide for tools
- Best practices for error handling

#### 6.2: Error Examples
**Create:** `docs/examples/ERROR_EXAMPLES.md`

**Content:**
- Examples of each error type
- Before/after comparisons
- Common error patterns and solutions

---

## 🔄 Step 7: Migration and Integration

### **Objective:**
Ensure backward compatibility and smooth integration with existing code.

### **Implementation Tasks:**

#### 7.1: Backward Compatibility
- Ensure existing code using `BAA_TOKEN_ERROR` continues to work
- Provide migration guide for updating error handling code
- Add compatibility layer if needed

#### 7.2: Parser Integration
- Update parser error handling to use new error token types
- Integrate enhanced error context into parser error messages
- Test parser error recovery with enhanced lexer errors

#### 7.3: Tool Integration
- Update `baa_lexer_tester` to display enhanced error information
- Update `baa_preprocessor_tester` if needed
- Ensure error messages are properly displayed in console

---

## 🎯 Priority Order for Implementation

1. **Step 2 (NEXT)**: Update Token Scanners - This provides immediate value
2. **Step 3**: Enhanced Error Recovery - Critical for robustness
3. **Step 4**: Error Context Enhancement - Improves user experience
4. **Step 5**: Testing Framework - Ensures quality
5. **Step 6**: Documentation - Essential for maintenance
6. **Step 7**: Migration and Integration - Final polish

---

## 🧪 Testing Approach

After each step:
1. **Build Test**: Ensure code compiles without errors
2. **Unit Tests**: Run specific unit tests for the implemented functionality
3. **Integration Tests**: Test with real Baa source code
4. **Error Message Tests**: Verify Arabic error messages display correctly
5. **Memory Tests**: Check for memory leaks in error handling paths

---

## 📊 Success Metrics

- ✅ All 8 specific error types properly implemented
- ✅ Error recovery prevents cascading failures
- ✅ Error messages provide helpful suggestions
- ✅ Error context includes source location and snippets
- ✅ Error count limiting prevents infinite error loops
- ✅ Memory management handles error contexts properly
- ✅ Arabic error messages display correctly
- ✅ Integration with parser and tools works smoothly

---

## 🚨 Known Issues and Considerations

1. **Character Encoding**: Ensure Arabic error messages display correctly in all terminals
2. **Memory Management**: Error contexts add memory overhead - monitor carefully
3. **Performance**: Enhanced error handling should not slow down normal tokenization
4. **Backward Compatibility**: Existing code should continue to work unchanged
5. **Testing**: Comprehensive testing needed to ensure reliability

---

## 📞 Next Session Agenda

When you return to continue this implementation:

1. **Test Current Build**: Verify Step 1 implementation compiles and works
2. **Start Step 2**: Begin updating token scanners with specific error types
3. **Focus Area**: Start with `scan_string` function as it has the most error cases
4. **Validation**: Create simple test cases to verify new error tokens are generated correctly

This roadmap provides a clear path forward for implementing enhanced error handling in the Baa lexer, with detailed implementation tasks and testing strategies for each step.
