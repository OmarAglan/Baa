/**
 * @file isel_tailcall_test.c
 * @brief اختبار اختيار التعليمات للنداء_الذيلي (Tail Call Optimization) — v0.3.2.7.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ir.h"
#include "../src/ir_builder.h"
#include "../src/isel.h"

static int require(int cond, const char *msg)
{
    if (!cond)
    {
        fprintf(stderr, "isel_tailcall_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static int count_mach_op(MachineFunc *f, MachineOp op)
{
    if (!f)
        return 0;
    int c = 0;
    for (MachineBlock *b = f->blocks; b; b = b->next)
    {
        for (MachineInst *i = b->first; i; i = i->next)
        {
            if (i->op == op)
                c++;
        }
    }
    return c;
}

static MachineFunc *find_func(MachineModule *mmod, const char *name)
{
    if (!mmod || !name)
        return NULL;
    for (MachineFunc *f = mmod->funcs; f; f = f->next)
    {
        if (f->name && strcmp(f->name, name) == 0)
            return f;
    }
    return NULL;
}

int main(void)
{
    int ok = 1;

    IRModule *m = ir_module_new("isel_tailcall_test");
    ok &= require(m != NULL, "failed to create IR module");
    if (!ok)
        return 1;

    IRBuilder *b = ir_builder_new(m);
    ok &= require(b != NULL, "failed to create IR builder");
    if (!ok)
        return 1;

    // callee: i64 @foo(i64 %x) { entry: ret %x }
    {
        IRFunc *foo = ir_builder_create_func(b, "foo", IR_TYPE_I64_T);
        (void)ir_builder_add_param(b, "x", IR_TYPE_I64_T);
        IRBlock *be = ir_builder_create_block(b, "entry");
        ir_builder_set_insert_point(b, be);
        ir_builder_emit_ret(b, ir_value_reg(foo->params[0].reg, IR_TYPE_I64_T));
    }

    // caller: i64 @caller() { entry: %r = call foo(123); ret %r }
    {
        (void)ir_builder_create_func(b, "caller", IR_TYPE_I64_T);
        IRBlock *be = ir_builder_create_block(b, "entry");
        ir_builder_set_insert_point(b, be);

        IRValue *args[1] = {ir_value_const_int(123, IR_TYPE_I64_T)};
        int r = ir_builder_emit_call(b, "foo", IR_TYPE_I64_T, args, 1);
        ir_builder_emit_ret(b, ir_value_reg(r, IR_TYPE_I64_T));
    }

    ir_builder_free(b);

    MachineModule *mmod = isel_run_ex(m, true, NULL);
    ok &= require(mmod != NULL, "isel_run_ex failed");
    MachineFunc *caller = find_func(mmod, "caller");
    ok &= require(caller != NULL, "missing lowered function 'caller'");

    if (caller)
    {
        ok &= require(count_mach_op(caller, MACH_TAILJMP) == 1, "expected one MACH_TAILJMP");
        ok &= require(count_mach_op(caller, MACH_CALL) == 0, "expected no MACH_CALL in tailcall");
        ok &= require(count_mach_op(caller, MACH_RET) == 0, "expected no MACH_RET in tailcall");
    }

    mach_module_free(mmod);
    ir_module_free(m);

    if (!ok)
        return 1;

    fprintf(stderr, "isel_tailcall_test: PASS\n");
    return 0;
}
