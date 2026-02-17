/**
 * @file isel_callargs_reject_test.c
 * @brief اختبار رفض معاملات المكدس في النداءات — v0.3.2.8.2
 */

#include <stdio.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/isel.h"
#include "../src/target.h"

static int require(int cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "isel_callargs_reject_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    int ok = 1;

    IRModule* m = ir_module_new("isel_callargs_reject_test");
    ok &= require(m != NULL, "failed to create IR module");
    if (!ok)
        return 1;

    IRBuilder* b = ir_builder_new(m);
    ok &= require(b != NULL, "failed to create IR builder");
    if (!ok)
        return 1;

    // callee: i64 @foo(i64 a,b,c,d,e) { ret a }
    {
        IRFunc* foo = ir_builder_create_func(b, "foo", IR_TYPE_I64_T);
        (void)foo;
        (void)ir_builder_add_param(b, "a", IR_TYPE_I64_T);
        (void)ir_builder_add_param(b, "b", IR_TYPE_I64_T);
        (void)ir_builder_add_param(b, "c", IR_TYPE_I64_T);
        (void)ir_builder_add_param(b, "d", IR_TYPE_I64_T);
        (void)ir_builder_add_param(b, "e", IR_TYPE_I64_T);
        IRBlock* be = ir_builder_create_block(b, "entry");
        ir_builder_set_insert_point(b, be);
        // param0 reg
        ir_builder_emit_ret(b, ir_value_reg(foo->params[0].reg, IR_TYPE_I64_T));
    }

    // caller: i64 @caller() { %r = call foo(1,2,3,4,5); ret %r }
    {
        (void)ir_builder_create_func(b, "caller", IR_TYPE_I64_T);
        IRBlock* be = ir_builder_create_block(b, "entry");
        ir_builder_set_insert_point(b, be);

        IRValue* args[5] = {
            ir_value_const_int(1, IR_TYPE_I64_T),
            ir_value_const_int(2, IR_TYPE_I64_T),
            ir_value_const_int(3, IR_TYPE_I64_T),
            ir_value_const_int(4, IR_TYPE_I64_T),
            ir_value_const_int(5, IR_TYPE_I64_T),
        };
        int r = ir_builder_emit_call(b, "foo", IR_TYPE_I64_T, args, 5);
        ir_builder_emit_ret(b, ir_value_reg(r, IR_TYPE_I64_T));
    }

    ir_builder_free(b);

    // على Windows x64، فقط ٤ معاملات سجلات، لذا يجب أن يرفض isel
    const BaaTarget* win = baa_target_builtin_windows_x86_64();
    MachineModule* mm = isel_run_ex(m, false, win);
    ok &= require(mm == NULL, "expected isel to fail for >4 args on Windows x64");

    ir_module_free(m);

    if (!ok)
        return 1;

    fprintf(stderr, "isel_callargs_reject_test: PASS\n");
    return 0;
}
