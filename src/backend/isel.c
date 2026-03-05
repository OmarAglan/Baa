/**
 * @file isel.c
 * @brief اختيار التعليمات - تحويل IR إلى تعليمات الآلة (v0.3.2.1)
 *
 * يحوّل كل تعليمة IR إلى تعليمة آلة أو أكثر لمعمارية x86-64.
 * يدعم:
 * - تضمين القيم الفورية (immediates) في التعليمات مباشرة
 * - اختيار أنماط التعليمات المثلى لكل عملية IR
 * - التعامل مع اتفاقية الاستدعاء عبر هدف (Windows x64 / SystemV AMD64)
 * - القسمة باستخدام CQO + IDIV
 * - المقارنات باستخدام CMP + SETcc + MOVZX
 */

#include "isel.h"

#include "ir_loop.h"
#include "target.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define ISEL_INLINE_ASM_PSEUDO_CALL "__baa_inline_asm_v0406"

// ============================================================================
// بناء المعاملات (Operand Construction)
// ============================================================================

MachineOperand mach_op_none(void)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_NONE;
    op.size_bits = 0;
    return op;
}

MachineOperand mach_op_vreg(int vreg, int bits)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_VREG;
    op.size_bits = bits;
    op.data.vreg = vreg;
    return op;
}

MachineOperand mach_op_imm(int64_t imm, int bits)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_IMM;
    op.size_bits = bits;
    op.data.imm = imm;
    return op;
}

MachineOperand mach_op_mem(int base_vreg, int32_t offset, int bits)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_MEM;
    op.size_bits = bits;
    op.data.mem.base_vreg = base_vreg;
    op.data.mem.offset = offset;
    return op;
}

MachineOperand mach_op_label(int label_id)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_LABEL;
    op.size_bits = 0;
    op.data.label_id = label_id;
    return op;
}

MachineOperand mach_op_global(const char *name)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_GLOBAL;
    op.size_bits = 64;
    op.data.name = name ? strdup(name) : NULL;
    return op;
}

MachineOperand mach_op_func(const char *name)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_FUNC;
    op.size_bits = 64;
    op.data.name = name ? strdup(name) : NULL;
    return op;
}

MachineOperand mach_op_xmm(int xmm)
{
    MachineOperand op = {0};
    op.kind = MACH_OP_XMM;
    op.size_bits = 64;
    op.data.xmm = xmm;
    return op;
}

// ============================================================================
// بناء التعليمات (Instruction Construction)
// ============================================================================

MachineInst *mach_inst_new(MachineOp op, MachineOperand dst,
                           MachineOperand src1, MachineOperand src2)
{
    MachineInst *inst = calloc(1, sizeof(MachineInst));
    if (!inst)
        return NULL;
    inst->op = op;
    inst->dst = dst;
    inst->src1 = src1;
    inst->src2 = src2;
    inst->ir_reg = -1;
    inst->comment = NULL;
    inst->src_file = NULL;
    inst->src_line = 0;
    inst->src_col = 0;
    inst->ir_inst_id = -1;
    inst->dbg_name = NULL;
    inst->sysv_al = -1;
    inst->prev = NULL;
    inst->next = NULL;
    return inst;
}

/**
 * @brief تحرير اسم المعامل إذا كان مخصصاً ديناميكياً.
 */
static void mach_operand_cleanup(MachineOperand *op)
{
    if (!op)
        return;
    if ((op->kind == MACH_OP_GLOBAL || op->kind == MACH_OP_FUNC) && op->data.name)
    {
        free(op->data.name);
        op->data.name = NULL;
    }
}

void mach_inst_free(MachineInst *inst)
{
    if (!inst)
        return;
    mach_operand_cleanup(&inst->dst);
    mach_operand_cleanup(&inst->src1);
    mach_operand_cleanup(&inst->src2);
    // لا نحرر comment لأنه عادة سلسلة ثابتة
    free(inst);
}

// ============================================================================
// بناء الكتل (Block Construction)
// ============================================================================

MachineBlock *mach_block_new(const char *label, int id)
{
    MachineBlock *block = calloc(1, sizeof(MachineBlock));
    if (!block)
        return NULL;
    block->label = label ? strdup(label) : NULL;
    block->id = id;
    block->first = NULL;
    block->last = NULL;
    block->inst_count = 0;
    block->succ_count = 0;
    block->succs[0] = NULL;
    block->succs[1] = NULL;
    block->next = NULL;
    return block;
}

void mach_block_append(MachineBlock *block, MachineInst *inst)
{
    if (!block || !inst)
        return;
    inst->prev = block->last;
    inst->next = NULL;
    if (block->last)
    {
        block->last->next = inst;
    }
    else
    {
        block->first = inst;
    }
    block->last = inst;
    block->inst_count++;
}

void mach_block_free(MachineBlock *block)
{
    if (!block)
        return;
    // تحرير جميع التعليمات
    MachineInst *inst = block->first;
    while (inst)
    {
        MachineInst *next = inst->next;
        mach_inst_free(inst);
        inst = next;
    }
    free(block->label);
    free(block);
}

// ============================================================================
// بناء الدوال (Function Construction)
// ============================================================================

MachineFunc *mach_func_new(const char *name)
{
    MachineFunc *func = calloc(1, sizeof(MachineFunc));
    if (!func)
        return NULL;
    func->name = name ? strdup(name) : NULL;
    func->is_prototype = false;
    func->entry = NULL;
    func->blocks = NULL;
    func->block_count = 0;
    func->next_vreg = 0;
    func->stack_size = 0;
    func->param_count = 0;
    func->next = NULL;
    return func;
}

int mach_func_alloc_vreg(MachineFunc *func)
{
    if (!func)
        return -1;
    return func->next_vreg++;
}

void mach_func_add_block(MachineFunc *func, MachineBlock *block)
{
    if (!func || !block)
        return;
    // إضافة إلى نهاية القائمة المترابطة
    if (!func->blocks)
    {
        func->blocks = block;
        func->entry = block;
    }
    else
    {
        MachineBlock *cur = func->blocks;
        while (cur->next)
            cur = cur->next;
        cur->next = block;
    }
    func->block_count++;
}

void mach_func_free(MachineFunc *func)
{
    if (!func)
        return;
    MachineBlock *block = func->blocks;
    while (block)
    {
        MachineBlock *next = block->next;
        mach_block_free(block);
        block = next;
    }
    free(func->name);
    free(func);
}

// ============================================================================
// بناء الوحدة (Module Construction)
// ============================================================================

MachineModule *mach_module_new(const char *name)
{
    MachineModule *module = calloc(1, sizeof(MachineModule));
    if (!module)
        return NULL;
    module->name = name ? strdup(name) : NULL;
    module->funcs = NULL;
    module->func_count = 0;
    module->globals = NULL;
    module->global_count = 0;
    module->strings = NULL;
    module->string_count = 0;
    module->baa_strings = NULL;
    module->baa_string_count = 0;
    return module;
}

void mach_module_add_func(MachineModule *module, MachineFunc *func)
{
    if (!module || !func)
        return;
    if (!module->funcs)
    {
        module->funcs = func;
    }
    else
    {
        MachineFunc *cur = module->funcs;
        while (cur->next)
            cur = cur->next;
        cur->next = func;
    }
    module->func_count++;
}

void mach_module_free(MachineModule *module)
{
    if (!module)
        return;
    MachineFunc *func = module->funcs;
    while (func)
    {
        MachineFunc *next = func->next;
        mach_func_free(func);
        func = next;
    }
    // لا نحرر globals و strings لأنها مملوكة من وحدة IR
    free(module->name);
    free(module);
}

// ============================================================================
// سياق اختيار التعليمات (ISel Context)
// ============================================================================

/**
 * @struct ISelCtx
 * @brief سياق داخلي لعملية اختيار التعليمات.
 */
typedef struct
{
    MachineModule *mmod;  // الوحدة الآلية الناتجة
    MachineFunc *mfunc;   // الدالة الآلية الحالية
    MachineBlock *mblock; // الكتلة الآلية الحالية

    IRModule *ir_module; // وحدة IR المصدر
    IRFunc *ir_func;     // دالة IR الحالية
    IRBlock *ir_block;   // كتلة IR الحالية

    // تعليمة IR الحالية (لنسخ معلومات الديبغ إلى MachineInst)
    IRInst *ir_inst;

    // معلومات الحلقات (للتحسينات داخل الحلقات مثل تقليل القوة)
    IRLoopInfo *loop_info;

    // تفعيل تحسين النداء_الذيلي (Tail Call Optimization)
    bool enable_tco;

    // الهدف الحالي (Windows x64 / SysV ...)
    const BaaTarget *target;

    // حالة فشل داخل الخلفية (مثلاً: معاملات مكدس غير مدعومة بعد)
    bool had_error;
} ISelCtx;

static const BaaCallingConv *isel_cc_or_default(ISelCtx *ctx)
{
    if (ctx && ctx->target && ctx->target->cc)
        return ctx->target->cc;
    return baa_target_builtin_windows_x86_64()->cc;
}

static int isel_abi_sp_vreg(void)
{
    // اتفاقية ثابتة في Machine IR: vreg -3 -> RSP
    return -3;
}

static int isel_abi_arg_vreg(const BaaCallingConv *cc, int i)
{
    int base = (cc) ? cc->abi_arg_vreg0 : -10;
    return base - i;
}

static int isel_abi_ret_vreg(const BaaCallingConv *cc)
{
    return (cc) ? cc->abi_ret_vreg : -2;
}


// -----------------------------------------------------------------------------
// تصريحات مسبقة (Forward Declarations)
// -----------------------------------------------------------------------------

static MachineInst *isel_emit(ISelCtx *ctx, MachineOp op,
                              MachineOperand dst, MachineOperand src1,
                              MachineOperand src2);

static int isel_type_bits(IRType *type);
static MachineOperand isel_lower_value(ISelCtx *ctx, IRValue *val);
static MachineOperand isel_extend_to_gpr64(ISelCtx *ctx, MachineOperand src, IRType *src_type);
static void isel_lower_binop(ISelCtx *ctx, IRInst *inst, MachineOp mop);
static void isel_lower_shift(ISelCtx *ctx, IRInst *inst, MachineOp mop);

static int isel_block_in_any_loop(ISelCtx *ctx)
{
    if (!ctx || !ctx->loop_info || !ctx->ir_block)
        return 0;

    int n = ir_loop_info_count(ctx->loop_info);
    for (int i = 0; i < n; i++)
    {
        IRLoop *L = ir_loop_info_get(ctx->loop_info, i);
        if (L && ir_loop_contains(L, ctx->ir_block))
            return 1;
    }
    return 0;
}

static int isel_is_pow2_i64(int64_t v, int *out_shift)
{
    if (out_shift)
        *out_shift = 0;

    if (v <= 0)
        return 0;
    if ((v & (v - 1)) != 0)
        return 0;

    int sh = 0;
    while (v > 1)
    {
        v >>= 1;
        sh++;
        if (sh > 63)
            return 0;
    }

    if (out_shift)
        *out_shift = sh;
    return 1;
}

static void isel_lower_mul_strength_reduce(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    // نطبق تقليل القوة داخل الحلقات فقط (v0.3.2.7.1).
    if (!isel_block_in_any_loop(ctx))
    {
        isel_lower_binop(ctx, inst, MACH_IMUL);
        return;
    }

    IRValue *a = inst->operands[0];
    IRValue *b = inst->operands[1];
    if (!a || !b)
    {
        isel_lower_binop(ctx, inst, MACH_IMUL);
        return;
    }

    // نبحث عن ثابت قوة-٢.
    IRValue *var = NULL;
    int shift = 0;
    if (b->kind == IR_VAL_CONST_INT && isel_is_pow2_i64(b->data.const_int, &shift))
    {
        var = a;
    }
    else if (a->kind == IR_VAL_CONST_INT && isel_is_pow2_i64(a->data.const_int, &shift))
    {
        var = b;
    }

    // إذا لم يكن هناك ثابت مناسب: استخدم imul.
    if (!var || shift <= 0)
    {
        isel_lower_binop(ctx, inst, MACH_IMUL);
        return;
    }

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, var);
    MachineOperand sh = mach_op_imm((int64_t)shift, 8);

    // mov dst, var
    isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());

    // shl dst, $shift
    MachineInst *mi = isel_emit(ctx, MACH_SHL, dst, dst, sh);
    if (mi)
        mi->ir_reg = inst->dest;
}

// ============================================================================
// دوال مساعدة لاختيار التعليمات
// ============================================================================

/**
 * @brief تحديد حجم البتات لنوع IR.
 */
static int isel_type_bits(IRType *type)
{
    if (!type)
        return 64;
    switch (type->kind)
    {
    case IR_TYPE_I1:
        return 8; // نستخدم بايت كامل للمنطقي
    case IR_TYPE_I8:
    case IR_TYPE_U8:
        return 8;
    case IR_TYPE_I16:
    case IR_TYPE_U16:
        return 16;
    case IR_TYPE_I32:
    case IR_TYPE_U32:
        return 32;
    case IR_TYPE_I64:
    case IR_TYPE_U64:
        return 64;
    case IR_TYPE_CHAR:
        return 64;
    case IR_TYPE_F64:
        return 64;
    case IR_TYPE_PTR:
        return 64;
    case IR_TYPE_FUNC:
        return 64;
    default:
        return 64;
    }
}

static const char* isel_translate_func_name(const char* name)
{
    if (!name) return name;
    if (strcmp(name, "الرئيسية") == 0) return "main";
    if (strcmp(name, "اطبع") == 0 || strcmp(name, "اطبع_صحيح") == 0) return "printf";
    if (strcmp(name, "اقرأ") == 0 || strcmp(name, "اقرأ_صحيح") == 0) return "scanf";
    return name;
}

/**
 * @brief تحويل قيمة IR إلى معامل آلة.
 *
 * إذا كانت القيمة ثابتاً، يتم تضمينها كقيمة فورية.
 * إذا كانت سجلاً، يتم استخدام سجل افتراضي.
 */
static MachineOperand isel_lower_value(ISelCtx *ctx, IRValue *val)
{
    if (!val)
        return mach_op_none();

    int bits = isel_type_bits(val->type);

    switch (val->kind)
    {
    case IR_VAL_CONST_INT:
        // تضمين القيمة الفورية مباشرة
        return mach_op_imm(val->data.const_int, bits);

    case IR_VAL_REG:
        return mach_op_vreg(val->data.reg_num, bits);

    case IR_VAL_GLOBAL:
        return mach_op_global(val->data.global_name);

    case IR_VAL_FUNC:
        // مرجع دالة كقيمة: نُحوّله إلى عنوان في سجل عبر LEA (RIP-relative).
        {
            const char* nm = isel_translate_func_name(val->data.global_name);
            if (!ctx || !ctx->mfunc || !ctx->mblock)
            {
                return mach_op_global(nm ? nm : "???");
            }

            int tmp = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand dst = mach_op_vreg(tmp, 64);
            MachineOperand src = mach_op_global(nm ? nm : "???");
            isel_emit(ctx, MACH_LEA, dst, src, mach_op_none());
            return dst;
        }

    case IR_VAL_BLOCK:
        if (val->data.block)
            return mach_op_label(val->data.block->id);
        return mach_op_none();

    case IR_VAL_CONST_STR:
        // النصوص في IR تمثل "مؤشر" إلى جدول النصوص (.Lstr_N)
        // لذا نحتاج تحميل "العنوان" باستخدام LEA، وليس تحميل 8 بايت من محتوى السلسلة.
        {
            if (!ctx || !ctx->mfunc || !ctx->mblock)
            {
                // احتياطي: في حال عدم توفر السياق (لا يُفترض أن يحدث)
                char label_buf[64];
                snprintf(label_buf, sizeof(label_buf), ".Lstr_%d", val->data.const_str.id);
                return mach_op_global(label_buf);
            }

            int tmp = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand dst = mach_op_vreg(tmp, 64);

            char label_buf[64];
            snprintf(label_buf, sizeof(label_buf), ".Lstr_%d", val->data.const_str.id);
            MachineOperand src = mach_op_global(label_buf);

            // tmp = &.Lstr_N
            isel_emit(ctx, MACH_LEA, dst, src, mach_op_none());
            return dst;
        }

    case IR_VAL_BAA_STR:
        // النصوص من نوع باء تمثل "مؤشر" إلى جدول نصوص باء (.Lbs_N)
        {
            if (!ctx || !ctx->mfunc || !ctx->mblock)
            {
                char label_buf[64];
                snprintf(label_buf, sizeof(label_buf), ".Lbs_%d", val->data.const_str.id);
                return mach_op_global(label_buf);
            }

            int tmp = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand dst = mach_op_vreg(tmp, 64);

            char label_buf[64];
            snprintf(label_buf, sizeof(label_buf), ".Lbs_%d", val->data.const_str.id);
            MachineOperand src = mach_op_global(label_buf);

            isel_emit(ctx, MACH_LEA, dst, src, mach_op_none());
            return dst;
        }

    default:
        return mach_op_none();
    }
}

/**
 * @brief إصدار تعليمة آلة في الكتلة الحالية.
 */
static MachineInst *isel_emit(ISelCtx *ctx, MachineOp op,
                              MachineOperand dst, MachineOperand src1,
                              MachineOperand src2)
{
    MachineInst *inst = mach_inst_new(op, dst, src1, src2);
    if (!inst)
        return NULL;

    // نسخ معلومات الديبغ من تعليمة IR الحالية (إن وُجدت)
    if (ctx && ctx->ir_inst) {
        inst->src_file = ctx->ir_inst->src_file;
        inst->src_line = ctx->ir_inst->src_line;
        inst->src_col = ctx->ir_inst->src_col;
        inst->ir_inst_id = ctx->ir_inst->id;
        inst->dbg_name = ctx->ir_inst->dbg_name;
    }

    mach_block_append(ctx->mblock, inst);
    return inst;
}

/**
 * @brief إصدار تعليمة آلة مع تعليق.
 */
static MachineInst *isel_emit_comment(ISelCtx *ctx, MachineOp op,
                                      MachineOperand dst, MachineOperand src1,
                                      MachineOperand src2, const char *comment)
{
    MachineInst *inst = isel_emit(ctx, op, dst, src1, src2);
    if (inst)
        inst->comment = comment;
    return inst;
}

// ============================================================================
// اختيار التعليمات لكل عملية IR
// ============================================================================


#include "isel_arith.c"
#include "isel_memory.c"
#include "isel_control.c"
#include "isel_calls.c"
#include "isel_misc.c"
#include "isel_driver.c"
