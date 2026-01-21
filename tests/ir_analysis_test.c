/**
 * @file ir_analysis_test.c
 * @brief Lightweight IR analysis tests (CFG + preds + dominators).
 *
 * This test compiles an existing .baa program to IR, then runs:
 *  - CFG validation
 *  - predecessor rebuild
 *  - dominator + dominance frontier computation
 *
 * It returns exit code 0 on success, non-zero on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/baa.h"
#include "../src/ir_lower.h"
#include "../src/ir_analysis.h"

// -----------------------------------------------------------------------------
// Test-only implementation for read_file() (used by the lexer for includes).
// main.c also defines this, but this test does not link src/main.c.
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

static IRBlock* find_block_by_label(IRFunc* func, const char* label) {
    if (!func || !label) return NULL;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->label && strcmp(b->label, label) == 0) return b;
    }
    return NULL;
}

static int df_contains(IRBlock* block, IRBlock* target) {
    if (!block || !target) return 0;
    for (int i = 0; i < block->dom_frontier_count; i++) {
        if (block->dom_frontier[i] == target) return 1;
    }
    return 0;
}

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "TEST FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void) {
    warning_init();

    const char* input_path = "tests/ir_test.baa";
    char* source = read_file(input_path);

    // Provide source context if errors happen.
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

    // -------------------------------------------------------------------------
    // 1) CFG validation (terminators)
    // -------------------------------------------------------------------------
    if (!require(ir_module_validate_cfg(module), "ir_module_validate_cfg() failed")) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    // -------------------------------------------------------------------------
    // 2) Rebuild preds + 3) dominators
    // -------------------------------------------------------------------------
    ir_module_rebuild_preds(module);
    ir_module_compute_dominators(module);

    IRFunc* main_func = ir_module_find_func(module, "الرئيسية");
    if (!require(main_func != NULL, "Failed to find function @الرئيسية")) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    // Blocks in tests/ir_test.baa have deterministic ASCII-suffixed labels.
    IRBlock* entry = find_block_by_label(main_func, "بداية");
    IRBlock* then_bb = find_block_by_label(main_func, "فرع_صواب_0");
    IRBlock* else_bb = find_block_by_label(main_func, "فرع_خطأ_2");
    IRBlock* merge_bb = find_block_by_label(main_func, "دمج_1");

    IRBlock* while_header = find_block_by_label(main_func, "حلقة_تحقق_3");
    IRBlock* while_body = find_block_by_label(main_func, "حلقة_جسم_4");

    int ok = 1;

    ok &= require(entry != NULL, "Missing entry block 'بداية'");
    ok &= require(then_bb != NULL, "Missing block 'فرع_صواب_0'");
    ok &= require(else_bb != NULL, "Missing block 'فرع_خطأ_2'");
    ok &= require(merge_bb != NULL, "Missing block 'دمج_1'");
    ok &= require(while_header != NULL, "Missing block 'حلقة_تحقق_3'");
    ok &= require(while_body != NULL, "Missing block 'حلقة_جسم_4'");

    if (!ok) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    // Pred checks
    ok &= require(then_bb->pred_count == 1, "then_bb pred_count should be 1");
    ok &= require(else_bb->pred_count == 1, "else_bb pred_count should be 1");
    ok &= require(merge_bb->pred_count == 2, "merge_bb pred_count should be 2");
    ok &= require(while_header->pred_count == 2, "while_header pred_count should be 2 (from merge + body)");

    // Dominator checks (idom)
    ok &= require(entry->idom == entry, "entry idom should be entry");
    ok &= require(merge_bb->idom == entry, "merge idom should be entry");
    ok &= require(while_header->idom == merge_bb, "while_header idom should be merge");
    ok &= require(while_body->idom == while_header, "while_body idom should be while_header");

    // Dominance frontier checks (loop backedge)
    ok &= require(df_contains(while_body, while_header), "DF(while_body) should contain while_header");

    if (!ok) {
        ir_module_free(module);
        free(source);
        return 1;
    }

    ir_module_free(module);
    free(source);

    fprintf(stderr, "ir_analysis_test: PASS\n");
    return 0;
}