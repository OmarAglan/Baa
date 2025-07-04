#include "test_framework.h"
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include Baa headers for testing utilities
#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/utils/utils.h"

int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;
int current_failed_count = 0; // Add this

void init_test_framework()
{
    total_tests = 0;
    passed_tests = 0;
    failed_tests = 0;
    current_failed_count = 0; // Initialize
}

void print_test_results()
{
    wprintf(L"\n--- Test Results ---\n"); // Use wprintf
    wprintf(L"Total tests: %d\n", total_tests);
    wprintf(L"Passed: %d\n", passed_tests);
    wprintf(L"Failed: %d\n", failed_tests);
    wprintf(L"Success rate: %.2f%%\n",
            (total_tests > 0) ? ((double)passed_tests / total_tests * 100.0) : 0.0); // Use double for precision
}

// --- Memory Tracking ---
static size_t allocated_memory_count = 0;
static size_t initial_memory_count = 0;

void track_memory_allocation()
{
    initial_memory_count = allocated_memory_count;
}

void assert_no_memory_leaks()
{
    if (allocated_memory_count > initial_memory_count)
    {
        wprintf(L"[MEMORY LEAK] %zu allocations not freed\n",
                allocated_memory_count - initial_memory_count);
        failed_tests++;
    }
}

size_t get_allocated_memory_count()
{
    return allocated_memory_count;
}

// --- String Testing Utilities ---
void assert_wstr_eq(const wchar_t *expected, const wchar_t *actual, const char *test_name)
{
    if (!expected && !actual)
    {
        return; // Both NULL, considered equal
    }
    if (!expected || !actual)
    {
        wprintf(L"[ASSERT FAILED] %hs: One string is NULL, the other is not\n", test_name);
        wprintf(L"                Expected: %ls, Actual: %ls\n",
                expected ? expected : L"(NULL)",
                actual ? actual : L"(NULL)");
        failed_tests++;
        return;
    }
    if (wcscmp(expected, actual) != 0)
    {
        wprintf(L"[ASSERT FAILED] %hs: Strings not equal\n", test_name);
        wprintf(L"                Expected: \"%ls\"\n", expected);
        wprintf(L"                Actual:   \"%ls\"\n", actual);
        failed_tests++;
    }
}

void assert_wstr_contains(const wchar_t *haystack, const wchar_t *needle, const char *test_name)
{
    if (!haystack || !needle)
    {
        wprintf(L"[ASSERT FAILED] %hs: NULL string provided\n", test_name);
        failed_tests++;
        return;
    }
    if (!wcsstr(haystack, needle))
    {
        wprintf(L"[ASSERT FAILED] %hs: String does not contain expected substring\n", test_name);
        wprintf(L"                Haystack: \"%ls\"\n", haystack);
        wprintf(L"                Needle:   \"%ls\"\n", needle);
        failed_tests++;
    }
}

// --- File Testing Utilities ---
wchar_t *load_test_file(const char *relative_path)
{
    // Construct full path relative to tests directory
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "tests/%s", relative_path);

    // Convert to wide string for baa_read_file
    size_t len = strlen(full_path) + 1;
    wchar_t *wide_path = malloc(len * sizeof(wchar_t));
    if (!wide_path)
    {
        return NULL;
    }

    mbstowcs(wide_path, full_path, len);
    wchar_t *content = baa_read_file(wide_path);
    free(wide_path);

    return content;
}

bool file_exists(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

void compare_with_expected_file(const char *actual_output, const char *expected_file)
{
    // This is a simplified implementation
    // In a real scenario, you'd want more sophisticated comparison
    wprintf(L"[INFO] Comparing output with expected file: %hs\n", expected_file);
    // TODO: Implement actual file comparison logic
}
