#include "baa/lexer/lexer.h"
#include "baa/utils/utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// Helper function to create a test file with UTF-16LE encoding
static void create_test_file(const wchar_t *filename, const wchar_t *content)
{
    FILE *file;
    _wfopen_s(&file, filename, L"wb, ccs=UTF-16LE");
    assert(file != NULL);

    // Write BOM
    const wchar_t bom = 0xFEFF;
    fwrite(&bom, sizeof(wchar_t), 1, file);

    // Write content
    fwrite(content, sizeof(wchar_t), wcslen(content), file);
    fclose(file);
}

void test_file_size(void)
{
    const wchar_t *test_filename = L"test_file.txt";
    const wchar_t *test_content = L"Test content";

    create_test_file(test_filename, test_content);

    FILE *file;
    _wfopen_s(&file, test_filename, L"rb");
    assert(file != NULL);

    long size = baa_file_size(file);
    // Size should include BOM and content
    assert(size == (wcslen(test_content) + 1) * sizeof(wchar_t));

    fclose(file);
    _wremove(test_filename);
}

void test_file_content(void)
{
    const wchar_t *test_filename = L"test_file.txt";
    const wchar_t *test_content = L"Test content\nSecond line";

    create_test_file(test_filename, test_content);

    wchar_t *content = baa_file_content(test_filename);
    assert(content != NULL);
    assert(wcscmp(content, test_content) == 0);

    free(content);
    _wremove(test_filename);
}

void test_nonexistent_file(void)
{
    wchar_t *content = baa_file_content(L"nonexistent_file.txt");
    assert(content == NULL);
}

void test_empty_file(void)
{
    const wchar_t *test_filename = L"empty_file.txt";
    const wchar_t *test_content = L"";

    create_test_file(test_filename, test_content);

    wchar_t *content = baa_file_content(test_filename);
    assert(content != NULL);
    assert(wcscmp(content, test_content) == 0);

    free(content);
    _wremove(test_filename);
}

int main(void)
{
    printf("Running lexer tests...\n");

    test_file_size();
    test_file_content();
    test_nonexistent_file();
    test_empty_file();

    printf("All lexer tests passed!\n");
    return 0;
}
