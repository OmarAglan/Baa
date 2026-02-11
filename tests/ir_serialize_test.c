// اختبار تسلسل الـIR النصي: كتابة -> قراءة -> كتابة ثم مقارنة

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_text.h"

static int require(int cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "ir_serialize_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static char* read_entire_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) { free(buf); return NULL; }
    buf[n] = '\0';
    if (out_size) *out_size = n;
    return buf;
}

static IRModule* build_sample_module(void) {
    IRModule* m = ir_module_new("ir_serialize_test");
    if (!m) return NULL;

    // جدول النصوص
    int s0 = ir_module_add_string(m, "Hello \"Baa\"\\n\n");
    if (s0 < 0) return NULL;

    // global ثابت
    IRGlobal* g = ir_global_new("g", IR_TYPE_I64_T, 1);
    if (!g) return NULL;
    ir_global_set_init(g, ir_value_const_int(7, IR_TYPE_I64_T));
    ir_module_add_global(m, g);

    // func @foo(i64 %r0) -> i64
    IRFunc* foo = ir_func_new("foo", IR_TYPE_I64_T);
    if (!foo) return NULL;
    ir_func_add_param(foo, NULL, IR_TYPE_I64_T);
    IRBlock* foo_b0 = ir_func_new_block(foo, NULL);
    if (!foo_b0) return NULL;
    int r_add = ir_func_alloc_reg(foo);
    IRValue* arg0 = ir_value_reg(foo->params[0].reg, IR_TYPE_I64_T);
    IRInst* add = ir_inst_binary(IR_OP_ADD, IR_TYPE_I64_T, r_add, arg0, ir_value_const_int(1, IR_TYPE_I64_T));
    ir_block_append(foo_b0, add);
    ir_block_append(foo_b0, ir_inst_ret(ir_value_reg(r_add, IR_TYPE_I64_T)));
    ir_module_add_func(m, foo);

    // func @main() -> i64
    IRFunc* mainf = ir_func_new("main", IR_TYPE_I64_T);
    if (!mainf) return NULL;
    IRBlock* b0 = ir_func_new_block(mainf, NULL);
    IRBlock* b1 = ir_func_new_block(mainf, NULL);
    IRBlock* b2 = ir_func_new_block(mainf, NULL);
    IRBlock* b3 = ir_func_new_block(mainf, NULL);
    if (!b0 || !b1 || !b2 || !b3) return NULL;

    int r_ptr = ir_func_alloc_reg(mainf);
    IRInst* alloca_i = ir_inst_alloca(IR_TYPE_I64_T, r_ptr);
    ir_block_append(b0, alloca_i);

    IRValue* ptrv = ir_value_reg(r_ptr, ir_type_ptr(IR_TYPE_I64_T));
    ir_block_append(b0, ir_inst_store(ir_value_const_int(1, IR_TYPE_I64_T), ptrv));

    int r_ld = ir_func_alloc_reg(mainf);
    ir_block_append(b0, ir_inst_load(IR_TYPE_I64_T, r_ld, ptrv));

    int r_cmp = ir_func_alloc_reg(mainf);
    IRInst* cmp = ir_inst_cmp(IR_CMP_GT, r_cmp, ir_value_reg(r_ld, IR_TYPE_I64_T), ir_value_const_int(0, IR_TYPE_I64_T));
    ir_block_append(b0, cmp);

    ir_block_append(b0, ir_inst_br_cond(ir_value_reg(r_cmp, IR_TYPE_I1_T), b1, b2));
    ir_block_add_succ(b0, b1);
    ir_block_add_succ(b0, b2);

    // b1: call i64 @foo(%r_ld) ثم br b3
    int r_call1 = ir_func_alloc_reg(mainf);
    {
        IRValue* args[1];
        args[0] = ir_value_reg(r_ld, IR_TYPE_I64_T);
        IRInst* c1 = ir_inst_call("foo", IR_TYPE_I64_T, r_call1, args, 1);
        ir_block_append(b1, c1);
        ir_block_append(b1, ir_inst_br(b3));
        ir_block_add_succ(b1, b3);
    }

    // b2: call i64 @foo(2) ثم br b3
    int r_call2 = ir_func_alloc_reg(mainf);
    {
        IRValue* args[1];
        args[0] = ir_value_const_int(2, IR_TYPE_I64_T);
        IRInst* c2 = ir_inst_call("foo", IR_TYPE_I64_T, r_call2, args, 1);
        ir_block_append(b2, c2);
        ir_block_append(b2, ir_inst_br(b3));
        ir_block_add_succ(b2, b3);
    }

    // b3: phi ثم اطبع نص ثم ret
    int r_phi = ir_func_alloc_reg(mainf);
    IRInst* phi = ir_inst_phi(IR_TYPE_I64_T, r_phi);
    ir_inst_phi_add(phi, ir_value_reg(r_call1, IR_TYPE_I64_T), b1);
    ir_inst_phi_add(phi, ir_value_reg(r_call2, IR_TYPE_I64_T), b2);
    ir_block_append(b3, phi);

    {
        const char* str0 = ir_module_get_string(m, s0);
        IRValue* a = ir_value_const_str(str0, s0);
        IRValue* args[1];
        args[0] = a;
        IRInst* c = ir_inst_call("اطبع", IR_TYPE_VOID_T, -1, args, 1);
        ir_block_append(b3, c);
    }

    ir_block_append(b3, ir_inst_ret(ir_value_reg(r_phi, IR_TYPE_I64_T)));
    ir_module_add_func(m, mainf);

    return m;
}

int main(void) {
    const char* f1 = "ir_serialize_tmp1.ir";
    const char* f2 = "ir_serialize_tmp2.ir";

    IRModule* m1 = build_sample_module();
    if (!require(m1 != NULL, "failed to build sample module")) return 1;

    if (!require(ir_text_dump_module(m1, f1) == 1, "failed to write first text")) return 1;

    IRModule* m2 = ir_text_read_module_file(f1);
    if (!require(m2 != NULL, "failed to read text back")) return 1;

    if (!require(ir_text_dump_module(m2, f2) == 1, "failed to write second text")) return 1;

    size_t n1 = 0, n2 = 0;
    char* t1 = read_entire_file(f1, &n1);
    char* t2 = read_entire_file(f2, &n2);
    if (!require(t1 && t2, "failed to read output files")) return 1;

    int same = (n1 == n2) && (memcmp(t1, t2, n1) == 0);
    if (!require(same, "round-trip text mismatch")) {
        fprintf(stderr, "--- first ---\n%s\n--- second ---\n%s\n", t1, t2);
        return 1;
    }

    free(t1);
    free(t2);
    ir_module_free(m1);
    ir_module_free(m2);

    remove(f1);
    remove(f2);

    fprintf(stderr, "ir_serialize_test: PASS\n");
    return 0;
}
