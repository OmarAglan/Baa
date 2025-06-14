# Enhanced Error System Migration Guide

This guide shows how to migrate existing error reporting code to use the new enhanced error collection system.

## Migration Pattern

### Before (Old System)
```c
// Direct error_message assignment
if (some_error_condition) {
    *error_message = format_preprocessor_error_at_location(&loc, 
        L"تنسيق #تعريف غير صالح: اسم الماكرو مفقود.");
    success = false;
    goto cleanup;
}
```

### After (Enhanced System)
```c
// Diagnostic collection with recovery
if (some_error_condition) {
    PP_REPORT_ERROR(pp_state, &loc, PP_ERROR_MISSING_MACRO_NAME, "directive",
        L"تنسيق #تعريف غير صالح: اسم الماكرو مفقود.");
    
    PpRecoveryAction action = determine_recovery_action(pp_state, PP_DIAG_ERROR, 
                                                       "directive", &loc);
    if (action == PP_RECOVERY_HALT) {
        success = false;
        goto cleanup;
    }
    
    if (action == PP_RECOVERY_SKIP_DIRECTIVE) {
        // Skip to next line and continue
        sync_to_next_directive(pp_state, &directive_ptr);
        continue; // or appropriate control flow
    }
    
    // Continue processing with recovered state
}
```

## Error Code Assignment

### Error Code Ranges
- **Directive errors**: 1000-1999
- **Macro errors**: 2000-2999  
- **Expression errors**: 3000-3999
- **File errors**: 4000-4999
- **Memory errors**: 5000-5999
- **Syntax errors**: 6000-6999

### Example Error Code Assignments
```c
#define PP_ERROR_UNKNOWN_DIRECTIVE     (PP_ERROR_DIRECTIVE_BASE + 1)
#define PP_ERROR_MISSING_ENDIF         (PP_ERROR_DIRECTIVE_BASE + 2)
#define PP_ERROR_INVALID_INCLUDE       (PP_ERROR_DIRECTIVE_BASE + 3)
#define PP_ERROR_MISSING_MACRO_NAME    (PP_ERROR_DIRECTIVE_BASE + 4)
#define PP_ERROR_INVALID_MACRO_PARAM   (PP_ERROR_DIRECTIVE_BASE + 5)
```

## Severity Guidelines

### Fatal Errors (PP_DIAG_FATAL)
- Memory allocation failures
- Critical system errors
- File permission errors when required

```c
PP_REPORT_FATAL(pp_state, &loc, PP_ERROR_OUT_OF_MEMORY, "memory",
    L"فشل في تخصيص الذاكرة للمعالج المسبق");
```

### Errors (PP_DIAG_ERROR)
- Syntax errors
- Invalid directives
- Macro definition problems
- Include file not found

```c
PP_REPORT_ERROR(pp_state, &loc, PP_ERROR_UNKNOWN_DIRECTIVE, "directive",
    L"توجيه معالج مسبق غير معروف: %ls", directive_name);
```

### Warnings (PP_DIAG_WARNING)
- Macro redefinitions
- Unused macros
- Deprecated features

```c
PP_REPORT_WARNING(pp_state, &loc, PP_ERROR_MACRO_REDEFINITION, "macro",
    L"إعادة تعريف الماكرو '%ls'", macro_name);
```

### Notes (PP_DIAG_NOTE)
- Informational messages
- Expansion traces
- Debug information

```c
PP_REPORT_NOTE(pp_state, &loc, 0, "info",
    L"ملاحظة: تم توسيع الماكرو '%ls' هنا", macro_name);
```

## Category Guidelines

### Categories
- **"directive"**: Preprocessor directive parsing
- **"macro"**: Macro definition and expansion
- **"expression"**: Conditional expression evaluation
- **"file"**: File I/O operations
- **"memory"**: Memory management
- **"syntax"**: General syntax errors

## Recovery Strategy Implementation

### Directive-level Recovery
```c
// Skip invalid directive and continue
if (!parse_directive_successfully) {
    PP_REPORT_ERROR(pp_state, &loc, PP_ERROR_INVALID_DIRECTIVE, "directive",
        L"تنسيق توجيه غير صالح");
    sync_to_next_directive(pp_state, &line_ptr);
    continue;
}
```

### Expression Recovery
```c
// Continue expression evaluation with default value
if (division_by_zero) {
    PP_REPORT_ERROR(pp_state, &loc, PP_ERROR_DIVISION_BY_ZERO, "expression",
        L"قسمة على صفر في التعبير الشرطي");
    result = 0; // Use safe default
    // Continue processing
}
```

### File Recovery
```c
// Skip missing include file
if (!file_found) {
    PP_REPORT_ERROR(pp_state, &loc, PP_ERROR_FILE_NOT_FOUND, "file",
        L"تعذر العثور على ملف التضمين '%hs'", file_path);
    // Skip include and continue processing
    return true;
}
```

## Function Migration Examples

### Example 1: preprocessor_directives.c
```c
// OLD VERSION
if (!macro_name_start) {
    *error_message = format_preprocessor_error_at_location(&directive_loc, 
        L"تنسيق #تعريف غير صالح: اسم الماكرو مفقود.");
    success = false;
    goto cleanup;
}

// NEW VERSION  
if (!macro_name_start) {
    PP_REPORT_ERROR(pp_state, &directive_loc, PP_ERROR_MISSING_MACRO_NAME, "directive",
        L"تنسيق #تعريف غير صالح: اسم الماكرو مفقود.");
    
    if (!should_continue_processing(pp_state)) {
        success = false;
        goto cleanup;
    }
    
    sync_to_next_directive(pp_state, &directive_ptr);
    continue;
}
```

### Example 2: preprocessor_expansion.c
```c
// OLD VERSION
if (arg_count != macro->param_count) {
    *error_message = format_preprocessor_error_at_location(&call_loc,
        L"عدد وسيطات غير صحيح للماكرو '%ls'", macro->name);
    goto parse_error;
}

// NEW VERSION
if (arg_count != macro->param_count) {
    PP_REPORT_ERROR(pp_state, &call_loc, PP_ERROR_MACRO_ARG_MISMATCH, "macro",
        L"عدد وسيطات غير صحيح للماكرو '%ls' (متوقع %zu، تم الحصول على %zu)",
        macro->name, macro->param_count, arg_count);
    
    // Skip this macro expansion and continue
    *invocation_ptr_ref = call_end;
    return NULL; // Indicates failed expansion, but processing continues
}
```

### Example 3: preprocessor_expr_eval.c
```c
// OLD VERSION
if (undefined_identifier) {
    make_error_token(tz, L"معرف غير معرف في التعبير: %ls", identifier);
    return error_token;
}

// NEW VERSION
if (undefined_identifier) {
    PP_REPORT_WARNING(pp_state, &token_loc, PP_ERROR_UNDEFINED_IDENTIFIER, "expression",
        L"معرف غير معرف في التعبير: %ls (يُعامل كصفر)", identifier);
    
    // Continue evaluation with value 0
    return make_int_token(0);
}
```

## Testing Migration

### Unit Test Structure
```c
void test_migrated_error_handling() {
    BaaPreprocessor pp_state = {0};
    init_preprocessor_error_system(&pp_state);
    
    // Test error condition
    // ... trigger error condition ...
    
    // Verify error was collected
    assert(pp_state.error_count > 0);
    assert(pp_state.diagnostic_count > 0);
    
    // Verify processing can continue
    assert(should_continue_processing(&pp_state));
    
    cleanup_preprocessor_error_system(&pp_state);
}
```

## Migration Checklist

For each error reporting site:

- [ ] Replace `*error_message = format_...` with appropriate `PP_REPORT_*` macro
- [ ] Assign appropriate error code from the defined ranges
- [ ] Set correct category ("directive", "macro", "expression", "file", "memory", "syntax")
- [ ] Choose appropriate severity level
- [ ] Implement recovery logic using `determine_recovery_action()`
- [ ] Use synchronization functions where appropriate
- [ ] Update control flow to continue processing when possible
- [ ] Test error scenarios to ensure recovery works correctly
- [ ] Verify error messages are properly formatted and displayed

## Performance Considerations

### Error-Free Fast Path
The enhanced system is designed to have minimal overhead during error-free processing:

```c
// Fast path check
if (LIKELY(!has_any_errors(pp_state))) {
    // Optimized processing without error checking overhead
    return process_fast_path(/* ... */);
} else {
    // Full error handling path
    return process_with_error_handling(/* ... */);
}
```

### Memory Management
- Diagnostic storage uses dynamic arrays with exponential growth
- String interning for common error patterns
- Lazy error message formatting when possible

This migration guide ensures consistent and systematic adoption of the enhanced error collection system across all preprocessor modules.