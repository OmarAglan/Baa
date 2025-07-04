#ifndef BAA_TEST_FRAMEWORK_H
#define BAA_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <wchar.h> // Include for wprintf
#include <math.h>  // Include for fabs in double equal

// --- Test Suite Macros ---
#define TEST_SUITE_BEGIN()     \
    int main(void)             \
    {                          \
        init_test_framework(); \
        wprintf(L"Starting Test Suite...\n\n");

#define TEST_CASE(name)                                                           \
    do                                                                            \
    {                                                                             \
        wprintf(L"--- Running test: %ls ---\n", L"" #name);                       \
        total_tests++;                                                            \
        name();                                                                   \
        if (failed_tests == current_failed_count)                                 \
        { /* Check if test function incremented failed_tests */                   \
            wprintf(L"--- Test PASSED: %ls ---\n\n", L"" #name);                  \
            passed_tests++;                                                       \
        }                                                                         \
        else                                                                      \
        {                                                                         \
            wprintf(L"--- Test FAILED: %ls ---\n\n", L"" #name);                  \
            current_failed_count = failed_tests; /* Update count for next test */ \
        }                                                                         \
    } while (0);

#define TEST_SUITE_END()       \
    print_test_results();      \
    return (failed_tests > 0); \
    }

// --- Assertion Macros ---
#define ASSERT(condition)                                                          \
    do                                                                             \
    {                                                                              \
        if (!(condition))                                                          \
        {                                                                          \
            wprintf(L"[ASSERT FAILED] Condition: %ls\n", L"" #condition);          \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__); \
            failed_tests++;                                                        \
            /* return; // Don't return from main, just record failure */           \
        }                                                                          \
    } while (0)

#define ASSERT_TRUE(condition, message)                                                   \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
        {                                                                                 \
            wprintf(L"[ASSERT FAILED] Expected true: %ls\n", (const wchar_t *)(message)); \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);        \
            failed_tests++;                                                               \
        }                                                                                 \
    } while (0)

// Generic ASSERT_EQUAL using _Generic (C11) if possible, otherwise needs specific types
// This is a simplified version. A real framework might have type-specific ones.
#define ASSERT_EQUAL(expected, actual, message)                                                                                                     \
    do                                                                                                                                              \
    {                                                                                                                                               \
        if ((expected) != (actual))                                                                                                                 \
        {                                                                                                                                           \
            /* Use format specifiers appropriate for common types */                                                                                \
            if (sizeof(expected) == sizeof(long long))                                                                                              \
            {                                                                                                                                       \
                wprintf(L"[ASSERT FAILED] %ls: Expected %lld, Got %lld\n", (const wchar_t *)(message), (long long)(expected), (long long)(actual)); \
            }                                                                                                                                       \
            else if (sizeof(expected) == sizeof(int))                                                                                               \
            {                                                                                                                                       \
                wprintf(L"[ASSERT FAILED] %ls: Expected %d, Got %d\n", (const wchar_t *)(message), (int)(expected), (int)(actual));                 \
            }                                                                                                                                       \
            else                                                                                                                                    \
            {                                                                                                                                       \
                wprintf(L"[ASSERT FAILED] %ls: Values not equal (non-standard size)\n", (const wchar_t *)(message));                                \
            }                                                                                                                                       \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);                                                                  \
            failed_tests++;                                                                                                                         \
        }                                                                                                                                           \
    } while (0)

// Macro similar to the original ASSERT_EQ for specific integer types if needed
#define ASSERT_EQ(expected, actual) ASSERT_EQUAL(expected, actual, L"Values not equal")

#define ASSERT_STR_EQ(expected, actual)                                                                                                   \
    do                                                                                                                                    \
    {                                                                                                                                     \
        if (wcscmp((expected), (actual)) != 0)                                                                                            \
        {                                                                                                                                 \
            wprintf(L"[ASSERT FAILED] Expected String: \"%ls\", Got: \"%ls\"\n", (const wchar_t *)(expected), (const wchar_t *)(actual)); \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);                                                        \
            failed_tests++;                                                                                                               \
        }                                                                                                                                 \
    } while (0)

#define ASSERT_NOT_NULL(ptr, message)                                                         \
    do                                                                                        \
    {                                                                                         \
        if ((ptr) == NULL)                                                                    \
        {                                                                                     \
            wprintf(L"[ASSERT FAILED] Expected non-NULL: %ls\n", (const wchar_t *)(message)); \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);            \
            failed_tests++;                                                                   \
        }                                                                                     \
    } while (0)

#define ASSERT_NULL(ptr, message)                                                         \
    do                                                                                    \
    {                                                                                     \
        if ((ptr) != NULL)                                                                \
        {                                                                                 \
            wprintf(L"[ASSERT FAILED] Expected NULL: %ls\n", (const wchar_t *)(message)); \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);        \
            failed_tests++;                                                               \
        }                                                                                 \
    } while (0)

#define ASSERT_DOUBLE_EQUAL(expected, actual, tolerance, message)                                                                                                            \
    do                                                                                                                                                                       \
    {                                                                                                                                                                        \
        if (fabs((double)(expected) - (double)(actual)) > (double)(tolerance))                                                                                               \
        {                                                                                                                                                                    \
            wprintf(L"[ASSERT FAILED] %ls: Expected %lf, Got %lf (Tolerance %lf)\n", (const wchar_t *)(message), (double)(expected), (double)(actual), (double)(tolerance)); \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);                                                                                           \
            failed_tests++;                                                                                                                                                  \
        }                                                                                                                                                                    \
    } while (0)

// --- Framework Globals ---
extern int total_tests;
extern int passed_tests;
extern int failed_tests;
// Helper variable for TEST_CASE
extern int current_failed_count;

// --- Framework Functions ---
void init_test_framework();
void print_test_results();

// --- Enhanced Testing Utilities ---

// Forward declarations for Baa types
struct BaaNode;
struct BaaToken;
struct BaaLexer;
typedef struct BaaNode BaaNode;
typedef struct BaaToken BaaToken;
typedef struct BaaLexer BaaLexer;

// AST Testing Utilities
BaaNode *create_test_ast_node(int kind, const char *test_data);
void assert_ast_structure(BaaNode *root, const char *expected_structure);
void print_ast_debug(BaaNode *root, int indent_level);
void free_test_ast(BaaNode *root);

// Parser Testing Utilities
BaaNode *parse_test_string(const wchar_t *source);
void assert_parse_success(const wchar_t *source, const char *test_name);
void assert_parse_error(const wchar_t *source, int expected_error, const char *test_name);

// Lexer Testing Utilities
BaaToken *tokenize_test_string(const wchar_t *source);
void assert_token_sequence(const wchar_t *source, int *expected_types, size_t count, const char *test_name);
void free_test_tokens(BaaToken *tokens, size_t count);

// File Testing Utilities
wchar_t *load_test_file(const char *relative_path);
void compare_with_expected_file(const char *actual_output, const char *expected_file);
bool file_exists(const char *path);

// Memory Testing Utilities
void track_memory_allocation();
void assert_no_memory_leaks();
size_t get_allocated_memory_count();

// String Testing Utilities
void assert_wstr_eq(const wchar_t *expected, const wchar_t *actual, const char *test_name);
void assert_wstr_contains(const wchar_t *haystack, const wchar_t *needle, const char *test_name);

// Enhanced Assertion Macros
#define ASSERT_NE(expected, actual)                                                             \
    do                                                                                          \
    {                                                                                           \
        if ((expected) == (actual))                                                             \
        {                                                                                       \
            wprintf(L"[ASSERT FAILED] Expected values to be different, but both were equal\n"); \
            wprintf(L"                File: %hs, Line: %d\n", __FILE__, __LINE__);              \
            failed_tests++;                                                                     \
        }                                                                                       \
    } while (0)

#define ASSERT_WSTR_EQ(expected, actual) \
    assert_wstr_eq((expected), (actual), __func__)

#define ASSERT_WSTR_CONTAINS(haystack, needle) \
    assert_wstr_contains((haystack), (needle), __func__)

#define ASSERT_PARSE_SUCCESS(source) \
    assert_parse_success((source), __func__)

#define ASSERT_PARSE_ERROR(source, expected_error) \
    assert_parse_error((source), (expected_error), __func__)

#define ASSERT_TOKEN_SEQUENCE(source, expected_types, count) \
    assert_token_sequence((source), (expected_types), (count), __func__)

#define ASSERT_AST_STRUCTURE(root, expected) \
    assert_ast_structure((root), (expected))

#define ASSERT_NO_MEMORY_LEAKS() \
    assert_no_memory_leaks()

#define TRACK_MEMORY() \
    track_memory_allocation()

// Test organization macros
#define TEST_SETUP()    \
    do                  \
    {                   \
        TRACK_MEMORY(); \
    } while (0)

#define TEST_TEARDOWN()           \
    do                            \
    {                             \
        ASSERT_NO_MEMORY_LEAKS(); \
    } while (0)

#endif // BAA_TEST_FRAMEWORK_H
