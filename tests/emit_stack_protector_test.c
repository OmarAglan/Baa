/**
 * @file emit_stack_protector_test.c
 * @brief اختبار إصدار كناري حماية المكدس في ELF — v0.3.2.8.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/emit.h"
#include "../src/target.h"

// emit.c يعتمد على mach_op_to_string() من isel.c. للاختبار هنا، لا نحتاج
// أي أسماء فعلية لأننا لا نصدر تعليمات غير معروفة.
const char* mach_op_to_string(MachineOp op)
{
    (void)op;
    return "";
}

static int require(int cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "emit_stack_protector_test: FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

static int file_contains(FILE* f, const char* needle)
{
    if (!f || !needle) return 0;
    long pos = ftell(f);
    if (pos < 0) pos = 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n <= 0) { fseek(f, pos, SEEK_SET); return 0; }
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fseek(f, pos, SEEK_SET); return 0; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';

    int ok = (strstr(buf, needle) != NULL);
    free(buf);

    fseek(f, pos, SEEK_SET);
    return ok;
}

int main(void)
{
    int ok = 1;

    // بناء وحدة آلة صغيرة يدوياً: دالة واحدة مع stack_size > 0 و RET.
    MachineModule mod = {0};
    MachineFunc fn = {0};
    MachineBlock bb = {0};
    MachineInst ret = {0};

    fn.name = "test_fn";
    fn.stack_size = 8; // اجعلها غير صفرية لتفعيل -fstack-protector
    fn.blocks = &bb;
    fn.block_count = 1;
    fn.entry = &bb;
    fn.is_prototype = false;

    bb.label = "entry";
    bb.id = 0;
    bb.first = &ret;
    bb.last = &ret;

    ret.op = MACH_RET;

    mod.name = "emit_stack_protector_test";
    mod.funcs = &fn;
    mod.func_count = 1;

    FILE* tmp = tmpfile();
    ok &= require(tmp != NULL, "tmpfile failed");
    if (!ok) return 1;

    BaaCodegenOptions opts = baa_codegen_options_default();
    opts.stack_protector = BAA_STACKPROT_ALL;

    const BaaTarget* t = baa_target_builtin_linux_x86_64();
    ok &= require(emit_module_ex2(&mod, tmp, false, t, opts) == true, "emit_module_ex2 failed");

    ok &= require(file_contains(tmp, "%fs:40"), "missing TLS canary load (%fs:40)");
    ok &= require(file_contains(tmp, "__stack_chk_fail"), "missing __stack_chk_fail call");
    ok &= require(file_contains(tmp, ".note.GNU-stack"), "missing .note.GNU-stack for ELF");

    fclose(tmp);

    if (!ok) return 1;
    fprintf(stderr, "emit_stack_protector_test: PASS\n");
    return 0;
}
