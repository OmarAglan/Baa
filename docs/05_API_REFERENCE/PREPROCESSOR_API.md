# Baa Preprocessor API Reference

This document provides a comprehensive reference for the public Baa preprocessor API as implemented in `include/baa/preprocessor/preprocessor.h`.

## 🎯 Overview

The Baa preprocessor handles Arabic directives, macro expansion, file inclusion, and conditional compilation. It processes source files before lexical analysis and supports C99-compatible behavior with Arabic syntax.

## 📋 Public API Types

### Forward declarations

```c
typedef struct BaaPreprocessor BaaPreprocessor; // Opaque internal state (not exposed)
```

### BaaMacro

Public structure representing a preprocessor macro definition.

```c
typedef struct
{
    wchar_t *name;         // Macro name
    wchar_t *body;         // Replacement text
    bool is_function_like; // Function-like vs object-like
    size_t param_count;    // Number of named parameters
    wchar_t **param_names; // Parameter names (NULL if not function-like)
    bool is_variadic;      // Variadic macro accepts extra args
} BaaMacro;
```

### BaaPpSourceType and BaaPpSource

Abstraction for input source (file or in-memory string).

```c
typedef enum {
    BAA_PP_SOURCE_FILE,
    BAA_PP_SOURCE_STRING,
} BaaPpSourceType;

typedef struct {
    BaaPpSourceType type;
    const char* source_name;   // For error messages
    union {
        const char* file_path;       // When type = FILE
        const wchar_t* source_string; // When type = STRING
    } data;
} BaaPpSource;
```

## 🔧 Public Functions

### `baa_preprocess`

```c
wchar_t* baa_preprocess(
    const BaaPpSource* source,
    const char** include_paths,
    wchar_t** error_message
);
```

- Processes a file or string through the preprocessor and returns a newly allocated UTF-16LE wide-character buffer containing the fully processed source. Returns NULL on failure and sets `*error_message` to a newly allocated diagnostic string.
- `include_paths` is a NULL-terminated array of UTF-8 directory paths used to resolve `#تضمين <...>`; may be NULL.
- Caller must free both the returned buffer and `*error_message` (if set).

#### Example (file input)

```c
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h>

const char* include_dirs[] = {"./include", NULL};
BaaPpSource src = {
    .type = BAA_PP_SOURCE_FILE,
    .source_name = "program.ب",
    .data.file_path = "program.ب",
};
wchar_t* err = NULL;
wchar_t* out = baa_preprocess(&src, include_dirs, &err);
if (!out) {
    if (err) { fwprintf(stderr, L"%ls\n", err); free(err); }
} else {
    // use out ...
    free(out);
}
```

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

## 📝 Usage Example (string input)

```c
#include "baa/preprocessor/preprocessor.h"
#include <wchar.h>

const wchar_t* code = L"#تعريف X 1\nX\n";
BaaPpSource src = {
    .type = BAA_PP_SOURCE_STRING,
    .source_name = "<string>",
    .data.source_string = code,
};
wchar_t* err = NULL;
wchar_t* out = baa_preprocess(&src, NULL, &err);
if (out) { /* ... */ free(out); }
if (err) { free(err); }
```

## ⚠️ Error Handling

`baa_preprocess` returns NULL on failure and sets `*error_message` to a newly allocated, human-readable diagnostic string (UTF-16LE). Always free it after use.

## 🔗 Related APIs

- **[Lexer API](LEXER_API.md)** - Next stage after preprocessing
- **[Utilities API](UTILITIES_API.md)** - Error handling and memory utilities
- **[Parser API](PARSER_API.md)** - Next stage after preprocessing

---

**For more information, see the [Preprocessor Documentation](../02_COMPILER_ARCHITECTURE/PREPROCESSOR.md).**
