/**
 * @file ir_loop_test.c
 * @brief اختبار تحليل الحلقات (Loop Detection) — حواف الرجوع والحلقات الطبيعية.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/baa.h"
#include "../src/ir_lower.h"
#include "../src/ir_loop.h"

// -----------------------------------------------------------------------------
// تنفيذ read_file() للاختبار فقط (يستخدمه الـ lexer لدعم include).
// -----------------------------------------------------------------------------
char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Test Error: Could not open file '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc((size_t)length + 1);
    if (!buffer) {
        fprintf(stderr, "Test Error: Memory allocation failed\n");
        exit(1);
    }

    fread(buffer, 1, (size_t)length, f);
    buffer[length] = '\0';
    fclose(f);
    return buffer;
}

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static int starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return 0;
    size_t n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static IRBlock* find_block_by_prefix(IRFunc* func, const char* prefix) {
    if (!func || !prefix) return NULL;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->label && starts_with(b->label, prefix)) return b;
    }
    return NULL;
}

static IRLoop* find_loop_by_header(IRLoopInfo* info, IRBlock* header) {
    if (!info || !header) return NULL;
    int n = ir_loop_info_count(info);
    for (int i = 0; i < n; i++) {
        IRLoop* L = ir_loop_info_get(info, i);
        if (ir_loop_header(L) == header) return L;
    }
    return NULL;
}

int main(void) {
    warning_init();

    const char* input_path = "tests/ir_test.baa";
    char* source = read_file(input_path);

    error_init(source);

    Lexer lexer;
    lexer_init(&lexer, source, input_path);
    Node* ast = parse(&lexer);

    if (error_has_occurred()) {
        fprintf(stderr, "TEST FAIL: parser errors while parsing %s\n", input_path);
        free(source);
        return 1;
    }

    if (!analyze(ast)) {
        fprintf(stderr, "TEST FAIL: semantic errors while analyzing %s\n", input_path);
        free(source);
        return 1;
    }

    IRModule* module = ir_lower_program(ast, input_path);
    if (!module) {
        fprintf(stderr, "TEST FAIL: IR lowering returned NULL\n");
        free(source);
        return 1;
    }

    IRFunc* main_func = ir_module_find_func(module, "الرئيسية");
    if (!require(main_func != NULL, "Failed to find function @الرئيسية")) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    // توقع وجود حلقة طالما وحلقة لكل في tests/ir_test.baa
    IRBlock* while_header = find_block_by_prefix(main_func, "حلقة_تحقق_");
    IRBlock* while_body = find_block_by_prefix(main_func, "حلقة_جسم_");

    IRBlock* for_header = find_block_by_prefix(main_func, "لكل_تحقق_");
    IRBlock* for_body = find_block_by_prefix(main_func, "لكل_جسم_");
    IRBlock* for_inc = find_block_by_prefix(main_func, "لكل_زيادة_");

    int ok = 1;
    ok &= require(while_header != NULL, "Missing while header block (حلقة_تحقق_*)");
    ok &= require(while_body != NULL, "Missing while body block (حلقة_جسم_*)");
    ok &= require(for_header != NULL, "Missing for header block (لكل_تحقق_*)");
    ok &= require(for_body != NULL, "Missing for body block (لكل_جسم_*)");
    ok &= require(for_inc != NULL, "Missing for increment block (لكل_زيادة_*)");

    if (!ok) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    IRLoopInfo* info = ir_loop_analyze_func(main_func);
    ok &= require(info != NULL, "ir_loop_analyze_func() returned NULL");
    if (!ok) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    ok &= require(ir_loop_info_count(info) >= 2, "Expected at least 2 loops in @الرئيسية");

    IRLoop* while_loop = find_loop_by_header(info, while_header);
    IRLoop* for_loop = find_loop_by_header(info, for_header);
    ok &= require(while_loop != NULL, "Failed to find while loop by header");
    ok &= require(for_loop != NULL, "Failed to find for loop by header");

    if (while_loop) {
        ok &= require(ir_loop_contains(while_loop, while_header), "while loop should contain its header");
        ok &= require(ir_loop_contains(while_loop, while_body), "while loop should contain its body");
    }

    if (for_loop) {
        ok &= require(ir_loop_contains(for_loop, for_header), "for loop should contain its header");
        ok &= require(ir_loop_contains(for_loop, for_body), "for loop should contain its body");
        ok &= require(ir_loop_contains(for_loop, for_inc), "for loop should contain its increment block");
    }

    ir_loop_info_free(info);
    ir_module_free(module);
    free(source);

    if (!ok) return 1;
    fprintf(stderr, "ir_loop_test: PASS\n");
    return 0;
}
