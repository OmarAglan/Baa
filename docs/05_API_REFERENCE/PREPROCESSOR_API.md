# Baa Preprocessor API Reference

This document provides a comprehensive API reference for the Baa language preprocessor component.

## 🎯 Overview

The Baa preprocessor handles Arabic directives, macro expansion, file inclusion, and conditional compilation. It processes source files before lexical analysis and supports C99-compatible features with Arabic syntax.

## 📋 Core Data Structures

### BaaPreprocessor

Main preprocessor context structure.

```c
typedef struct BaaPreprocessor {
    BaaHashMap* macros;           // Macro definitions
    BaaVector* include_paths;     // Include search paths
    BaaVector* include_stack;     // Include file stack
    BaaVector* conditional_stack; // Conditional compilation stack
    BaaErrorReporter* error_reporter;
    int max_include_depth;
    bool enable_warnings;
} BaaPreprocessor;
```

### BaaMacro

Represents a preprocessor macro definition.

```c
typedef struct BaaMacro {
    char* name;                   // Macro name
    BaaVector* parameters;        // Parameter names (for function-like macros)
    BaaVector* replacement;       // Replacement token sequence
    bool is_function_like;        // Function-like vs object-like
    bool is_variadic;            // Has variadic parameters
    BaaSourceSpan definition_span; // Where macro was defined
} BaaMacro;
```

## 🔧 Core Functions

### Initialization and Cleanup

#### `baa_preprocessor_create`
```c
BaaPreprocessor* baa_preprocessor_create(BaaErrorReporter* error_reporter);
```
**Description:** Creates a new preprocessor instance.
**Parameters:**
- `error_reporter`: Error reporting context
**Returns:** New preprocessor instance or NULL on failure
**Example:**
```c
BaaErrorReporter* reporter = baa_error_reporter_create();
BaaPreprocessor* pp = baa_preprocessor_create(reporter);
```

#### `baa_preprocessor_destroy`
```c
void baa_preprocessor_destroy(BaaPreprocessor* preprocessor);
```
**Description:** Destroys preprocessor instance and frees memory.
**Parameters:**
- `preprocessor`: Preprocessor instance to destroy

### File Processing

#### `baa_preprocessor_process_file`
```c
BaaResult baa_preprocessor_process_file(
    BaaPreprocessor* preprocessor,
    const char* filename,
    char** output,
    size_t* output_length
);
```
**Description:** Processes a source file through the preprocessor.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `filename`: Input file path
- `output`: Pointer to store processed output
- `output_length`: Pointer to store output length
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_preprocessor_process_string`
```c
BaaResult baa_preprocessor_process_string(
    BaaPreprocessor* preprocessor,
    const char* input,
    const char* filename,
    char** output,
    size_t* output_length
);
```
**Description:** Processes a string through the preprocessor.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `input`: Input string to process
- `filename`: Virtual filename for error reporting
- `output`: Pointer to store processed output
- `output_length`: Pointer to store output length
**Returns:** `BAA_RESULT_SUCCESS` or error code

### Macro Management

#### `baa_preprocessor_define_macro`
```c
BaaResult baa_preprocessor_define_macro(
    BaaPreprocessor* preprocessor,
    const char* name,
    const char* replacement,
    const char** parameters,
    int parameter_count,
    bool is_variadic
);
```
**Description:** Defines a new macro.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `name`: Macro name
- `replacement`: Replacement text
- `parameters`: Parameter names (NULL for object-like macros)
- `parameter_count`: Number of parameters
- `is_variadic`: Whether macro accepts variable arguments
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_preprocessor_undefine_macro`
```c
BaaResult baa_preprocessor_undefine_macro(
    BaaPreprocessor* preprocessor,
    const char* name
);
```
**Description:** Undefines a macro.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `name`: Macro name to undefine
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_preprocessor_is_macro_defined`
```c
bool baa_preprocessor_is_macro_defined(
    BaaPreprocessor* preprocessor,
    const char* name
);
```
**Description:** Checks if a macro is defined.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `name`: Macro name to check
**Returns:** `true` if macro is defined, `false` otherwise

### Include Path Management

#### `baa_preprocessor_add_include_path`
```c
BaaResult baa_preprocessor_add_include_path(
    BaaPreprocessor* preprocessor,
    const char* path
);
```
**Description:** Adds an include search path.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `path`: Directory path to add
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_preprocessor_clear_include_paths`
```c
void baa_preprocessor_clear_include_paths(BaaPreprocessor* preprocessor);
```
**Description:** Clears all include search paths.
**Parameters:**
- `preprocessor`: Preprocessor instance

### Configuration

#### `baa_preprocessor_set_max_include_depth`
```c
void baa_preprocessor_set_max_include_depth(
    BaaPreprocessor* preprocessor,
    int max_depth
);
```
**Description:** Sets maximum include nesting depth.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `max_depth`: Maximum nesting depth (default: 200)

#### `baa_preprocessor_enable_warnings`
```c
void baa_preprocessor_enable_warnings(
    BaaPreprocessor* preprocessor,
    bool enable
);
```
**Description:** Enables or disables preprocessor warnings.
**Parameters:**
- `preprocessor`: Preprocessor instance
- `enable`: Whether to enable warnings

## 🌍 Arabic Directive Support

### Supported Directives

| Arabic Directive | English Equivalent | Description |
|------------------|-------------------|-------------|
| `#تضمين` | `#include` | File inclusion |
| `#تعريف` | `#define` | Macro definition |
| `#الغاء_تعريف` | `#undef` | Macro undefinition |
| `#إذا` | `#if` | Conditional compilation |
| `#إذا_عرف` | `#ifdef` | If defined |
| `#إذا_لم_يعرف` | `#ifndef` | If not defined |
| `#وإلا_إذا` | `#elif` | Else if |
| `#إلا` | `#else` | Else |
| `#نهاية_إذا` | `#endif` | End if |
| `#خطأ` | `#error` | Error directive |
| `#تحذير` | `#warning` | Warning directive |

### Predefined Macros

| Arabic Macro | Description | Example Value |
|--------------|-------------|---------------|
| `__الملف__` | Current filename | `"program.baa"` |
| `__السطر__` | Current line number | `42` |
| `__التاريخ__` | Compilation date | `"Jan  9 2025"` |
| `__الوقت__` | Compilation time | `"12:34:56"` |
| `__الدالة__` | Current function | `"main"` |
| `__إصدار_المعيار_باء__` | Baa standard version | `202501L` |

## 📝 Usage Examples

### Basic Usage

```c
#include "baa_preprocessor.h"

int main() {
    // Create error reporter
    BaaErrorReporter* reporter = baa_error_reporter_create();
    
    // Create preprocessor
    BaaPreprocessor* pp = baa_preprocessor_create(reporter);
    
    // Add include path
    baa_preprocessor_add_include_path(pp, "/usr/include/baa");
    
    // Define a macro
    baa_preprocessor_define_macro(pp, "PI", "٣.١٤١٥٩", NULL, 0, false);
    
    // Process file
    char* output;
    size_t output_length;
    BaaResult result = baa_preprocessor_process_file(
        pp, "input.baa", &output, &output_length
    );
    
    if (result == BAA_RESULT_SUCCESS) {
        printf("Processed output:\n%s\n", output);
        free(output);
    }
    
    // Cleanup
    baa_preprocessor_destroy(pp);
    baa_error_reporter_destroy(reporter);
    
    return 0;
}
```

### Function-like Macro

```c
// Define function-like macro
const char* params[] = {"x", "y"};
baa_preprocessor_define_macro(
    pp, "MAX", "((x) > (y) ? (x) : (y))", 
    params, 2, false
);
```

### Variadic Macro

```c
// Define variadic macro
const char* params[] = {"format"};
baa_preprocessor_define_macro(
    pp, "DEBUG_PRINT", "printf(format, وسائط_إضافية)", 
    params, 1, true
);
```

## ⚠️ Error Handling

All functions return `BaaResult` values or use the error reporter for detailed error information. Common error codes:

- `BAA_RESULT_SUCCESS`: Operation successful
- `BAA_RESULT_ERROR_FILE_NOT_FOUND`: Include file not found
- `BAA_RESULT_ERROR_MACRO_REDEFINITION`: Macro redefinition conflict
- `BAA_RESULT_ERROR_INCLUDE_DEPTH_EXCEEDED`: Too many nested includes
- `BAA_RESULT_ERROR_INVALID_DIRECTIVE`: Invalid preprocessor directive

## 🔗 Related APIs

- **[Lexer API](LEXER_API.md)** - Next stage after preprocessing
- **[Error Reporting API](ERROR_REPORTING_API.md)** - Error handling utilities
- **[Memory Management API](MEMORY_API.md)** - Memory allocation utilities

---

**For more information, see the [Preprocessor Documentation](../02_COMPILER_ARCHITECTURE/PREPROCESSOR.md).**
