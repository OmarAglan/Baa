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

/**
 * @brief خفض عملية حسابية ثنائية (جمع، طرح، ضرب).
 *
 * النمط: %dst = op type lhs, rhs
 * →  mov vreg_dst, lhs
 *    op  vreg_dst, rhs
 *
 * تحسين: إذا كان أحد المعاملين قيمة فورية وكان نفس سجل الوجهة
 * يمكننا استخدامها مباشرة.
 */
static void isel_lower_binop(ISelCtx *ctx, IRInst *inst, MachineOp mop)
{
    if (!inst || inst->operand_count < 2)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // نقل القيمة اليسرى إلى سجل الوجهة
    isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());

    // تنفيذ العملية
    MachineInst *mi = isel_emit(ctx, mop, dst, dst, rhs);
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية القسمة والباقي.
 *
 * القسمة في x86-64 تستخدم idiv:
 *   cqo          ; توسيع الإشارة RAX → RDX:RAX
 *   idiv divisor ; RAX = حاصل القسمة، RDX = الباقي
 *
 * نمثل ذلك بسجلات افتراضية ونترك تخصيص السجلات لاحقاً.
 *
 * النمط: %dst = قسم type lhs, rhs
 * →  mov vreg_dst, lhs
 *    cqo
 *    idiv rhs
 *    (الناتج في vreg_dst لحاصل القسمة أو vreg مؤقت للباقي)
 */
static void isel_lower_div(ISelCtx *ctx, IRInst *inst, bool is_mod)
{
    if (!inst || inst->operand_count < 2)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    IRType *lhs_t = (inst->operands[0] ? inst->operands[0]->type : inst->type);
    IRType *rhs_t = (inst->operands[1] ? inst->operands[1]->type : inst->type);

    MachineOperand lhs0 = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs0 = isel_lower_value(ctx, inst->operands[1]);

    // نُجري القسمة دائماً بعرض 64-بت (idivq/divq) لتبسيط الدعم.
    // ثم نقتطع النتيجة عند النسخ إلى الوجهة إذا كانت أصغر (32-بت مثلاً).
    MachineOperand lhs = isel_extend_to_gpr64(ctx, lhs0, lhs_t);
    MachineOperand divisor = isel_extend_to_gpr64(ctx, rhs0, rhs_t);

    // القسمة تتطلب RAX:RDX ضمنياً
    // نستخدم vreg -2 (RAX) لنقل المقسوم ثم نسخ النتيجة
    MachineOperand rax = mach_op_vreg(-2, 64);
    MachineOperand rdx = mach_op_vreg(-5, 64);

    // نقل المقسوم إلى RAX
    isel_emit_comment(ctx, MACH_MOV, rax, lhs, mach_op_none(),
                      is_mod ? "// باقي: تحضير المقسوم في RAX" : "// قسمة: تحضير المقسوم في RAX");

    bool is_unsigned = false;
    if (inst->type)
    {
        switch (inst->type->kind)
        {
        case IR_TYPE_U8:
        case IR_TYPE_U16:
        case IR_TYPE_U32:
        case IR_TYPE_U64:
            is_unsigned = true;
            break;
        default:
            break;
        }
    }

    if (is_unsigned)
    {
        // div: RDX:RAX يجب أن يكون بدون إشارة → صفّر RDX.
        isel_emit_comment(ctx, MACH_XOR, rdx, rdx, rdx,
                          "// قسمة بدون إشارة: تصفير RDX");
        isel_emit_comment(ctx, MACH_DIV, rax, divisor, mach_op_none(),
                          is_mod ? "// باقي القسمة في RDX (unsigned)" : "// حاصل القسمة في RAX (unsigned)");
    }
    else
    {
        // توسيع الإشارة RAX → RDX:RAX
        isel_emit(ctx, MACH_CQO, mach_op_none(), rax, mach_op_none());

        // القسمة: idivq divisor → حاصل القسمة في RAX، الباقي في RDX
        isel_emit_comment(ctx, MACH_IDIV, rax, divisor, mach_op_none(),
                          is_mod ? "// باقي القسمة في RDX" : "// حاصل القسمة في RAX");
    }

    // نسخ النتيجة إلى سجل الوجهة
    // - القسمة: النتيجة في RAX
    // - الباقي: النتيجة في RDX
    MachineOperand result = is_mod ? mach_op_vreg(-5, bits) : mach_op_vreg(-2, bits);
    MachineInst *mi = isel_emit_comment(ctx, MACH_MOV, dst, result, mach_op_none(),
                                         is_mod ? "// نسخ نتيجة الباقي" : "// نسخ نتيجة القسمة");
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عمليات الإزاحة (shl/shr/sar).
 *
 * قيود x86-64:
 * - عدد الإزاحة إما قيمة فورية 8-بت أو السجل CL.
 */
static void isel_lower_shift(ISelCtx *ctx, IRInst *inst, MachineOp mop)
{
    if (!inst || inst->operand_count < 2)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // نقل القيمة المزاحة إلى الوجهة
    isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());

    MachineOperand sh = rhs;
    if (rhs.kind == MACH_OP_IMM)
    {
        // x86 يستخدم imm8 لعدد الإزاحة
        sh = rhs;
        sh.size_bits = 8;
    }
    else
    {
        // عدد الإزاحة غير الفوري يجب أن يكون في CL (RCX منخفض 8-بت)
        // vreg -6 محجوز لمطابقة RCX في مرحلة regalloc.
        MachineOperand rcx = mach_op_vreg(-6, 64);
        MachineOperand rhs64 = rhs;
        if (rhs64.kind == MACH_OP_VREG || rhs64.kind == MACH_OP_MEM) {
            rhs64.size_bits = 64;
        }
        isel_emit(ctx, MACH_MOV, rcx, rhs64, mach_op_none());
        sh = mach_op_vreg(-6, 8); // %cl
    }

    MachineInst *mi = isel_emit(ctx, mop, dst, dst, sh);
    if (mi)
        mi->ir_reg = inst->dest;
}

static int isel_irtype_is_f64(IRType *t)
{
    return t && t->kind == IR_TYPE_F64;
}

static int isel_irtype_is_signed_int(IRType *t)
{
    if (!t) return 0;
    return t->kind == IR_TYPE_I8 || t->kind == IR_TYPE_I16 || t->kind == IR_TYPE_I32 || t->kind == IR_TYPE_I64 ||
           t->kind == IR_TYPE_CHAR || t->kind == IR_TYPE_I1;
}

static int isel_irtype_is_unsigned_int(IRType *t)
{
    if (!t) return 0;
    return t->kind == IR_TYPE_U8 || t->kind == IR_TYPE_U16 || t->kind == IR_TYPE_U32 || t->kind == IR_TYPE_U64;
}

static MachineOperand isel_materialize_imm_to_gpr64(ISelCtx *ctx, MachineOperand op)
{
    if (!ctx || !ctx->mfunc)
        return op;
    if (op.kind != MACH_OP_IMM)
        return op;

    int tmp = mach_func_alloc_vreg(ctx->mfunc);
    MachineOperand tmp_op = mach_op_vreg(tmp, 64);
    MachineOperand imm = op;
    imm.size_bits = 64;
    isel_emit(ctx, MACH_MOV, tmp_op, imm, mach_op_none());
    return tmp_op;
}

static MachineOperand isel_extend_to_gpr64(ISelCtx *ctx, MachineOperand src, IRType *src_type)
{
    if (!ctx || !ctx->mfunc)
        return src;

    if (src.kind == MACH_OP_IMM)
        return isel_materialize_imm_to_gpr64(ctx, src);

    int sb = src.size_bits;
    if (sb <= 0) sb = 64;
    if (sb >= 64)
    {
        if (src.kind == MACH_OP_VREG) src.size_bits = 64;
        return src;
    }

    int tmp = mach_func_alloc_vreg(ctx->mfunc);
    MachineOperand dst = mach_op_vreg(tmp, 64);
    MachineOp mop = isel_irtype_is_signed_int(src_type) ? MACH_MOVSX : MACH_MOVZX;

    // src يحتفظ بحجمه الأصلي.
    isel_emit(ctx, mop, dst, src, mach_op_none());
    return dst;
}

static void isel_lower_fbinop(ISelCtx *ctx, IRInst *inst, MachineOp mop)
{
    if (!inst || inst->operand_count < 2)
        return;

    MachineOperand dst_gpr = mach_op_vreg(inst->dest, 64);
    MachineOperand lhs0 = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs0 = isel_lower_value(ctx, inst->operands[1]);

    lhs0 = isel_materialize_imm_to_gpr64(ctx, lhs0);
    rhs0 = isel_materialize_imm_to_gpr64(ctx, rhs0);

    MachineOperand xmm0 = mach_op_xmm(0);
    MachineOperand xmm1 = mach_op_xmm(1);

    isel_emit(ctx, MACH_MOV, xmm0, lhs0, mach_op_none());
    isel_emit(ctx, MACH_MOV, xmm1, rhs0, mach_op_none());

    isel_emit(ctx, mop, xmm0, xmm0, xmm1);

    MachineInst *mi = isel_emit(ctx, MACH_MOV, dst_gpr, xmm0, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

static void isel_lower_fneg(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    MachineOperand dst_gpr = mach_op_vreg(inst->dest, 64);
    MachineOperand src0 = isel_lower_value(ctx, inst->operands[0]);
    src0 = isel_materialize_imm_to_gpr64(ctx, src0);

    MachineOperand xmm0 = mach_op_xmm(0);
    MachineOperand xmm1 = mach_op_xmm(1);

    isel_emit(ctx, MACH_MOV, xmm0, src0, mach_op_none());

    // signmask = 0x8000..00
    int tmp = mach_func_alloc_vreg(ctx->mfunc);
    MachineOperand mask_gpr = mach_op_vreg(tmp, 64);
    isel_emit(ctx, MACH_MOV, mask_gpr, mach_op_imm((int64_t)0x8000000000000000ULL, 64), mach_op_none());
    isel_emit(ctx, MACH_MOV, xmm1, mask_gpr, mach_op_none());

    isel_emit(ctx, MACH_XORPD, xmm0, xmm0, xmm1);

    MachineInst *mi = isel_emit(ctx, MACH_MOV, dst_gpr, xmm0, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

static void isel_lower_fcmp(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    MachineOperand lhs0 = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs0 = isel_lower_value(ctx, inst->operands[1]);
    lhs0 = isel_materialize_imm_to_gpr64(ctx, lhs0);
    rhs0 = isel_materialize_imm_to_gpr64(ctx, rhs0);

    MachineOperand xmm0 = mach_op_xmm(0);
    MachineOperand xmm1 = mach_op_xmm(1);
    isel_emit(ctx, MACH_MOV, xmm0, lhs0, mach_op_none());
    isel_emit(ctx, MACH_MOV, xmm1, rhs0, mach_op_none());

    // ucomisd xmm1, xmm0
    isel_emit(ctx, MACH_UCOMISD, mach_op_none(), xmm0, xmm1);

    MachineOperand dst8 = mach_op_vreg(inst->dest, 8);

    // اختيار setcc؛ مع معالجة unordered عند الحاجة.
    switch (inst->cmp_pred)
    {
    case IR_CMP_EQ:
        isel_emit(ctx, MACH_SETE, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETNP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_AND, dst8, dst8, t8);
        }
        break;

    case IR_CMP_NE:
        isel_emit(ctx, MACH_SETNE, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_OR, dst8, dst8, t8);
        }
        break;

    case IR_CMP_LT:
        isel_emit(ctx, MACH_SETB, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETNP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_AND, dst8, dst8, t8);
        }
        break;

    case IR_CMP_LE:
        isel_emit(ctx, MACH_SETBE, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETNP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_AND, dst8, dst8, t8);
        }
        break;

    case IR_CMP_GT:
        isel_emit(ctx, MACH_SETA, dst8, mach_op_none(), mach_op_none());
        break;

    case IR_CMP_GE:
        isel_emit(ctx, MACH_SETAE, dst8, mach_op_none(), mach_op_none());
        break;

    default:
        isel_emit(ctx, MACH_SETE, dst8, mach_op_none(), mach_op_none());
        break;
    }

    MachineOperand dst64 = mach_op_vreg(inst->dest, 64);
    MachineInst *mi = isel_emit(ctx, MACH_MOVZX, dst64, dst8, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية السالب (negation).
 *
 * النمط: %dst = سالب type operand
 * →  mov vreg_dst, operand
 *    neg vreg_dst
 */
static void isel_lower_neg(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    MachineInst *mi = isel_emit(ctx, MACH_NEG, dst, dst, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية تخصيص المكدس (alloca).
 *
 * النمط: %dst = حجز type
 * →  lea vreg_dst, [rbp - offset]
 *
 * نحسب الإزاحة بناءً على حجم النوع ونضيفها لحجم المكدس.
 */
static void isel_lower_alloca(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    const IRDataLayout *dl = (ctx && ctx->target) ? ctx->target->data_layout : NULL;

    // تعليمة alloca تنتج مؤشراً إلى نوع pointee، لذلك حجم التخصيص يعتمد على pointee وليس على حجم المؤشر.
    IRType *pointee = NULL;
    if (inst->type && inst->type->kind == IR_TYPE_PTR)
        pointee = inst->type->data.pointee;
    if (!pointee)
        pointee = IR_TYPE_I64_T;

    int alloc_size = ir_type_store_size(dl, pointee);
    int align = ir_type_alignment(dl, pointee);

    // تبسيط: نضمن 8 بايت كحد أدنى للتوافق مع معظم أنماط الحمل/الخزن الحالية.
    if (alloc_size < 8) alloc_size = 8;
    if (align < 8) align = 8;

    // محاذاة تصاعدية: offset النهائي يجب أن يحقق محاذاة النوع.
    int s = ctx->mfunc->stack_size + alloc_size;
    int rem = (align > 0) ? (s % align) : 0;
    if (rem != 0) s += (align - rem);
    ctx->mfunc->stack_size = s;

    MachineOperand dst = mach_op_vreg(inst->dest, 64); // المؤشر دائماً 64 بت
    MachineOperand mem = mach_op_mem(-1, -(int32_t)ctx->mfunc->stack_size, 64);
    // base_vreg = -1 يعني RBP (سيُحل في مرحلة إصدار الكود)

    MachineInst *mi = isel_emit(ctx, MACH_LEA, dst, mem, mach_op_none());
    if (mi)
    {
        mi->ir_reg = inst->dest;
        mi->comment = "// حجز مكان في المكدس";
    }
}

/**
 * @brief خفض عملية التحميل (load).
 *
 * النمط: %dst = حمل type, ptr
 * →  mov vreg_dst, [ptr]
 */
static void isel_lower_load(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand ptr = isel_lower_value(ctx, inst->operands[0]);

    // إذا كان المعامل سجلاً، نولد load من الذاكرة
    if (ptr.kind == MACH_OP_VREG)
    {
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        MachineInst *mi = isel_emit(ctx, MACH_LOAD, dst, mem, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
    else if (ptr.kind == MACH_OP_GLOBAL)
    {
        // تحميل من متغير عام
        MachineInst *mi = isel_emit(ctx, MACH_LOAD, dst, ptr, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
    else
    {
        // حالة احتياطية: نقل مباشر
        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, ptr, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
}

/**
 * @brief خفض عملية التخزين (store).
 *
 * النمط: خزن value, ptr
 * →  mov [ptr], value
 */
static void isel_lower_store(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    MachineOperand val = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand ptr = isel_lower_value(ctx, inst->operands[1]);
    int bits = 64;
    if (inst->operands[0] && inst->operands[0]->type)
        bits = isel_type_bits(inst->operands[0]->type);

    // إذا كانت القيمة فورية، يمكن لـ x86-64 تخزين imm32 مباشرة في الذاكرة.
    // أما imm64 الكامل فلا يمكن تخزينه مباشرة إلى الذاكرة: نحتاج سجل وسيط.
    MachineOperand store_val = val;
    if (val.kind == MACH_OP_IMM && ptr.kind == MACH_OP_VREG)
    {
        bool fits_imm32 = (val.data.imm >= INT32_MIN && val.data.imm <= INT32_MAX);
        if (!(bits == 64 && !fits_imm32))
        {
            MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
            isel_emit(ctx, MACH_STORE, mem, store_val, mach_op_none());
            return;
        }

        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, val, mach_op_none());
        store_val = tmp_op;
    }

    if (ptr.kind == MACH_OP_VREG)
    {
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, store_val, mach_op_none());
    }
    else if (ptr.kind == MACH_OP_GLOBAL)
    {
        isel_emit(ctx, MACH_STORE, ptr, store_val, mach_op_none());
    }
    else
    {
        // حالة احتياطية
        isel_emit(ctx, MACH_STORE, ptr, store_val, mach_op_none());
    }
}

/**
 * @brief خفض عملية المقارنة.
 *
 * النمط: %dst = قارن pred type lhs, rhs
 * →  cmp lhs, rhs
 *    setCC tmp8
 *    movzx vreg_dst, tmp8
 *
 * تحسين: إذا كان المعامل الأيمن قيمة فورية، نستخدمها مباشرة مع cmp.
 */
static void isel_lower_cmp(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    // عشري: ucomisd + setcc
    if (inst->operands[0] && inst->operands[0]->type && inst->operands[0]->type->kind == IR_TYPE_F64)
    {
        isel_lower_fcmp(ctx, inst);
        return;
    }

    int bits = isel_type_bits(inst->operands[0] ? inst->operands[0]->type : NULL);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // إذا كان الطرف الأيسر قيمة فورية، ننقله لسجل مؤقت
    // (cmp لا يقبل فوري كمعامل أول)
    if (lhs.kind == MACH_OP_IMM)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, bits);
        isel_emit(ctx, MACH_MOV, tmp_op, lhs, mach_op_none());
        lhs = tmp_op;
    }

    // تنفيذ المقارنة
    isel_emit(ctx, MACH_CMP, mach_op_none(), lhs, rhs);

    // اختيار تعليمة setCC المناسبة
    MachineOp setcc;
    switch (inst->cmp_pred)
    {
    case IR_CMP_EQ:
        setcc = MACH_SETE;
        break;
    case IR_CMP_NE:
        setcc = MACH_SETNE;
        break;
    case IR_CMP_GT:
        setcc = MACH_SETG;
        break;
    case IR_CMP_LT:
        setcc = MACH_SETL;
        break;
    case IR_CMP_GE:
        setcc = MACH_SETGE;
        break;
    case IR_CMP_LE:
        setcc = MACH_SETLE;
        break;
    case IR_CMP_UGT:
        setcc = MACH_SETA;
        break;
    case IR_CMP_ULT:
        setcc = MACH_SETB;
        break;
    case IR_CMP_UGE:
        setcc = MACH_SETAE;
        break;
    case IR_CMP_ULE:
        setcc = MACH_SETBE;
        break;
    default:
        setcc = MACH_SETE;
        break;
    }

    // setCC يكتب بايت واحد
    MachineOperand dst8 = mach_op_vreg(inst->dest, 8);
    isel_emit(ctx, setcc, dst8, mach_op_none(), mach_op_none());

    // توسيع بالأصفار إلى حجم كامل
    MachineOperand dst64 = mach_op_vreg(inst->dest, 64);
    MachineInst *mi = isel_emit(ctx, MACH_MOVZX, dst64, dst8, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عمليات منطقية (AND, OR, NOT).
 */
static void isel_lower_logical(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    // العمليات البتية تستخدم عرض النوع الناتج.
    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    if (inst->op == IR_OP_NOT)
    {
        // نفي: not dst
        if (inst->operand_count < 1)
            return;
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        // ضمان ٦٤ بت للمعامل (بعد التوسيع بالأصفار من المقارنات)
        if (src.kind == MACH_OP_VREG && src.size_bits < bits)
            src.size_bits = bits;
        isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        MachineInst *mi = isel_emit(ctx, MACH_NOT, dst, dst, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
    else
    {
        // AND أو OR
        if (inst->operand_count < 2)
            return;
        MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
        MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);
        // ضمان عرض النوع للمعاملات.
        if (lhs.kind == MACH_OP_VREG && lhs.size_bits < bits)
            lhs.size_bits = bits;
        if (rhs.kind == MACH_OP_VREG && rhs.size_bits < bits)
            rhs.size_bits = bits;
        MachineOp mop = MACH_OR;
        if (inst->op == IR_OP_AND) mop = MACH_AND;
        else if (inst->op == IR_OP_XOR) mop = MACH_XOR;
        isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());
        MachineInst *mi = isel_emit(ctx, mop, dst, dst, rhs);
        if (mi)
            mi->ir_reg = inst->dest;
    }
}

/**
 * @brief خفض عملية القفز غير المشروط.
 *
 * النمط: قفز target
 * →  jmp label
 */
static void isel_lower_br(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    MachineOperand target = isel_lower_value(ctx, inst->operands[0]);
    isel_emit(ctx, MACH_JMP, target, mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية القفز المشروط.
 *
 * النمط: قفز_شرط cond, if_true, if_false
 * →  test cond, cond    (أو cmp cond, 0)
 *    jne  if_true
 *    jmp  if_false
 */
static void isel_lower_br_cond(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 3)
        return;

    MachineOperand cond = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand if_true = isel_lower_value(ctx, inst->operands[1]);
    MachineOperand if_false = isel_lower_value(ctx, inst->operands[2]);

    // إذا كان الشرط قيمة فورية، ننقله لسجل
    if (cond.kind == MACH_OP_IMM)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, cond, mach_op_none());
        cond = tmp_op;
    }

    // فحص الشرط
    isel_emit(ctx, MACH_TEST, mach_op_none(), cond, cond);

    // قفز إلى الكتلة الصحيحة
    isel_emit(ctx, MACH_JNE, if_true, mach_op_none(), mach_op_none());
    isel_emit(ctx, MACH_JMP, if_false, mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية الرجوع.
 *
 * النمط: رجوع value
 * →  mov vreg_ret, value   (إذا كانت هناك قيمة)
 *    ret
 */
static void isel_lower_ret(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    const BaaCallingConv *cc = isel_cc_or_default(ctx);

    if (inst->operand_count > 0 && inst->operands[0])
    {
        IRType *rt = inst->operands[0] ? inst->operands[0]->type : NULL;
        MachineOperand val = isel_lower_value(ctx, inst->operands[0]);

        if (rt && rt->kind == IR_TYPE_F64)
        {
            // القيمة المرجعة العشرية → XMM0
            val = isel_materialize_imm_to_gpr64(ctx, val);
            MachineOperand xmm0 = mach_op_xmm(0);
            isel_emit_comment(ctx, MACH_MOV, xmm0, val, mach_op_none(),
                              "// قيمة الإرجاع → XMM0");
        }
        else
        {
            // نضع القيمة المرجعة في سجل خاص (سيصبح RAX في تخصيص السجلات)
            MachineOperand ret_reg = mach_op_vreg(isel_abi_ret_vreg(cc), isel_type_bits(rt));
            isel_emit_comment(ctx, MACH_MOV, ret_reg, val, mach_op_none(),
                              "// قيمة الإرجاع → RAX");
        }
    }

    isel_emit(ctx, MACH_RET, mach_op_none(), mach_op_none(), mach_op_none());
}

typedef struct
{
    int fixed_vreg;
    int bits;
    IRValue *addr;
} InlineAsmOutputBinding;

static void isel_inline_asm_error(ISelCtx *ctx, IRInst *inst, const char *message)
{
    if (ctx) ctx->had_error = true;
    if (!message) message = "خطأ غير معروف في خفض 'مجمع'.";

    if (inst && inst->src_file)
    {
        fprintf(stderr, "خطأ (ISel): %s:%d:%d: %s\n",
                inst->src_file,
                inst->src_line > 0 ? inst->src_line : 1,
                inst->src_col > 0 ? inst->src_col : 1,
                message);
    }
    else
    {
        fprintf(stderr, "خطأ (ISel): %s\n", message);
    }
}

static bool isel_inline_asm_parse_count(IRValue *v, int *out_value)
{
    if (!out_value) return false;
    *out_value = 0;
    if (!v || v->kind != IR_VAL_CONST_INT) return false;
    if (v->data.const_int < 0 || v->data.const_int > INT32_MAX) return false;
    *out_value = (int)v->data.const_int;
    return true;
}

static bool isel_inline_asm_parse_fixed_vreg(const char *constraint, bool is_output, int *out_vreg)
{
    if (!constraint || !out_vreg) return false;
    *out_vreg = 0;

    const char *p = constraint;
    if (is_output)
    {
        if (p[0] != '=') return false;
        p++;
    }
    else if (p[0] == '=')
    {
        return false;
    }

    if (p[0] == '\0' || p[1] != '\0') return false;

    switch (p[0])
    {
    case 'a':
        *out_vreg = -2; // RAX
        return true;
    case 'd':
        *out_vreg = -5; // RDX
        return true;
    case 'c':
        *out_vreg = -6; // RCX
        return true;
    default:
        return false;
    }
}

static int isel_inline_asm_output_bits(IRValue *addr)
{
    if (!addr || !addr->type || addr->type->kind != IR_TYPE_PTR || !addr->type->data.pointee)
        return 64;
    return isel_type_bits(addr->type->data.pointee);
}

static void isel_inline_asm_store_output(ISelCtx *ctx, IRInst *inst,
                                         IRValue *addr_val, MachineOperand src, int bits)
{
    if (!ctx || !addr_val) return;
    if (bits <= 0) bits = 64;
    src.size_bits = bits;

    MachineOperand addr = isel_lower_value(ctx, addr_val);
    if (addr.kind == MACH_OP_VREG)
    {
        MachineOperand mem = mach_op_mem(addr.data.vreg, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
        return;
    }
    if (addr.kind == MACH_OP_GLOBAL)
    {
        addr.size_bits = bits;
        isel_emit(ctx, MACH_STORE, addr, src, mach_op_none());
        return;
    }
    if (addr.kind == MACH_OP_MEM)
    {
        int tmp_v = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp = mach_op_vreg(tmp_v, 64);
        isel_emit(ctx, MACH_MOV, tmp, addr, mach_op_none());
        MachineOperand mem = mach_op_mem(tmp_v, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
        return;
    }

    isel_inline_asm_error(ctx, inst, "تعذر خفض عنوان خرج 'مجمع'.");
}

static bool isel_lower_inline_asm_call(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || !inst->call_target) return false;
    if (strcmp(inst->call_target, ISEL_INLINE_ASM_PSEUDO_CALL) != 0) return false;

    if (!inst->call_args || inst->call_arg_count < 3)
    {
        isel_inline_asm_error(ctx, inst, "بيانات 'مجمع' داخل IR غير مكتملة.");
        return true;
    }

    int template_count = 0;
    int output_count = 0;
    int input_count = 0;
    if (!isel_inline_asm_parse_count(inst->call_args[0], &template_count) ||
        !isel_inline_asm_parse_count(inst->call_args[1], &output_count) ||
        !isel_inline_asm_parse_count(inst->call_args[2], &input_count))
    {
        isel_inline_asm_error(ctx, inst, "ترويسة بيانات 'مجمع' غير صالحة.");
        return true;
    }

    int expected = 3 + template_count + (output_count * 2) + (input_count * 2);
    if (inst->call_arg_count != expected)
    {
        isel_inline_asm_error(ctx, inst, "عدد معاملات 'مجمع' في IR غير متطابق.");
        return true;
    }

    InlineAsmOutputBinding *outputs = NULL;
    if (output_count > 0)
    {
        outputs = (InlineAsmOutputBinding *)calloc((size_t)output_count, sizeof(InlineAsmOutputBinding));
        if (!outputs)
        {
            isel_inline_asm_error(ctx, inst, "نفدت الذاكرة أثناء خفض مخارج 'مجمع'.");
            return true;
        }
    }

    int out_idx = 0;
    int idx = 3 + template_count;
    for (int i = 0; i < output_count; i++)
    {
        IRValue *constraint_val = inst->call_args[idx++];
        IRValue *addr_val = inst->call_args[idx++];

        if (!constraint_val || constraint_val->kind != IR_VAL_CONST_STR || !constraint_val->data.const_str.data)
        {
            isel_inline_asm_error(ctx, inst, "قيد خرج 'مجمع' يجب أن يكون نصاً ثابتاً.");
            free(outputs);
            return true;
        }

        int fixed_vreg = 0;
        if (!isel_inline_asm_parse_fixed_vreg(constraint_val->data.const_str.data, true, &fixed_vreg))
        {
            isel_inline_asm_error(ctx, inst, "قيد خرج غير مدعوم في خفض 'مجمع'.");
            free(outputs);
            return true;
        }

        outputs[out_idx].fixed_vreg = fixed_vreg;
        outputs[out_idx].bits = isel_inline_asm_output_bits(addr_val);
        outputs[out_idx].addr = addr_val;
        out_idx++;
    }

    for (int i = 0; i < input_count; i++)
    {
        IRValue *constraint_val = inst->call_args[idx++];
        IRValue *value_val = inst->call_args[idx++];

        if (!constraint_val || constraint_val->kind != IR_VAL_CONST_STR || !constraint_val->data.const_str.data)
        {
            isel_inline_asm_error(ctx, inst, "قيد إدخال 'مجمع' يجب أن يكون نصاً ثابتاً.");
            free(outputs);
            return true;
        }

        int fixed_vreg = 0;
        if (!isel_inline_asm_parse_fixed_vreg(constraint_val->data.const_str.data, false, &fixed_vreg))
        {
            isel_inline_asm_error(ctx, inst, "قيد إدخال غير مدعوم في خفض 'مجمع'.");
            free(outputs);
            return true;
        }

        MachineOperand src = isel_lower_value(ctx, value_val);
        int bits = isel_type_bits(value_val ? value_val->type : NULL);
        if (bits <= 0) bits = 64;
        MachineOperand dst = mach_op_vreg(fixed_vreg, bits);
        isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    }

    for (int i = 0; i < template_count; i++)
    {
        IRValue *tv = inst->call_args[3 + i];
        if (!tv || tv->kind != IR_VAL_CONST_STR || !tv->data.const_str.data)
        {
            isel_inline_asm_error(ctx, inst, "سطر 'مجمع' في IR يجب أن يكون نصاً ثابتاً.");
            free(outputs);
            return true;
        }

        MachineInst *mi = isel_emit(ctx, MACH_INLINE_ASM, mach_op_none(), mach_op_none(), mach_op_none());
        if (mi) mi->comment = tv->data.const_str.data;
    }

    for (int i = 0; i < output_count; i++)
    {
        MachineOperand src = mach_op_vreg(outputs[i].fixed_vreg, outputs[i].bits);
        isel_inline_asm_store_output(ctx, inst, outputs[i].addr, src, outputs[i].bits);
    }

    free(outputs);
    return true;
}

/**
 * @brief خفض عملية الاستدعاء.
 *
 * اتفاقية Windows x64 ABI:
 * - المعاملات الأربعة الأولى: RCX, RDX, R8, R9
 * - المعاملات الإضافية: على المكدس
 * - القيمة المرجعة: RAX
 * - Shadow space: 32 بايت
 *
 * النمط: %dst = نداء ret_type @target(args...)
 * →  mov arg_reg1, arg1     (أول 4 معاملات في سجلات)
 *    push argN              (المعاملات الإضافية)
 *    call target
 *    mov vreg_dst, ret_reg  (إذا كانت هناك قيمة مرجعة)
 */
static void isel_lower_call(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    if (isel_lower_inline_asm_call(ctx, inst))
        return;

    const BaaCallingConv *cc = isel_cc_or_default(ctx);
    bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);

    int gpr_max = (cc && cc->int_arg_reg_count > 0) ? cc->int_arg_reg_count : 4;
    int xmm_max = is_win ? 4 : 8;

    typedef enum { ARG_GPR, ARG_XMM, ARG_STACK } ArgLocKind;
    typedef struct { ArgLocKind kind; int idx; } ArgLoc;

    int n = inst->call_arg_count;
    ArgLoc *locs = NULL;
    if (n > 0)
    {
        locs = (ArgLoc *)calloc((size_t)n, sizeof(ArgLoc));
        if (!locs) return;
    }

    int gpr_i = 0;
    int xmm_i = 0;
    int stack_i = 0;

    for (int i = 0; i < n; i++)
    {
        IRType *at = (inst->call_args && inst->call_args[i]) ? inst->call_args[i]->type : NULL;
        bool is_f64 = (at && at->kind == IR_TYPE_F64);

        if (is_win)
        {
            if (i < 4)
            {
                if (is_f64)
                    locs[i] = (ArgLoc){ARG_XMM, i};
                else
                    locs[i] = (ArgLoc){ARG_GPR, i};
            }
            else
            {
                locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
        }
        else
        {
            if (is_f64)
            {
                if (xmm_i < xmm_max)
                    locs[i] = (ArgLoc){ARG_XMM, xmm_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
            else
            {
                if (gpr_i < gpr_max)
                    locs[i] = (ArgLoc){ARG_GPR, gpr_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
        }
    }

    int shadow = (cc) ? cc->shadow_space_bytes : 0;
    int align = (cc && cc->stack_align_bytes > 0) ? cc->stack_align_bytes : 16;

    int call_frame = shadow + stack_i * 8;
    int call_frame_aligned = call_frame;
    if (align > 0 && call_frame_aligned % align != 0)
        call_frame_aligned = ((call_frame_aligned / align) + 1) * align;

    if (call_frame_aligned > 0)
    {
        MachineOperand sp = mach_op_vreg(isel_abi_sp_vreg(), 64);
        MachineOperand imm = mach_op_imm(call_frame_aligned, 64);
        isel_emit_comment(ctx, MACH_SUB, sp, sp, imm, "// حجز إطار نداء");
    }

    // 1) معاملات السجلات
    for (int i = 0; i < n; i++)
    {
        MachineOperand arg = isel_lower_value(ctx, inst->call_args[i]);
        IRType *at = (inst->call_args && inst->call_args[i]) ? inst->call_args[i]->type : NULL;
        bool is_f64 = (at && at->kind == IR_TYPE_F64);

        if (locs[i].kind == ARG_GPR)
        {
            int bits = at ? isel_type_bits(at) : 64;
            MachineOperand param_reg = mach_op_vreg(isel_abi_arg_vreg(cc, locs[i].idx), bits);
            isel_emit(ctx, MACH_MOV, param_reg, arg, mach_op_none());
        }
        else if (locs[i].kind == ARG_XMM)
        {
            (void)is_f64;
            arg = isel_materialize_imm_to_gpr64(ctx, arg);
            MachineOperand xmm = mach_op_xmm(locs[i].idx);
            isel_emit(ctx, MACH_MOV, xmm, arg, mach_op_none());
        }
    }

    // 2) Windows: home/shadow space لمعاملات السجلات (بما فيها XMM)
    if (cc && cc->home_reg_args_on_call && shadow >= 32)
    {
        int n_home = (n < 4) ? n : 4;
        for (int i = 0; i < n_home; i++)
        {
            MachineOperand mem = mach_op_mem(isel_abi_sp_vreg(), (int32_t)(i * 8), 64);
            if (locs[i].kind == ARG_XMM)
            {
                MachineOperand src = mach_op_xmm(i);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
            else
            {
                MachineOperand src = mach_op_vreg(isel_abi_arg_vreg(cc, i), 64);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
        }
    }

    // 3) معاملات المكدس
    for (int i = 0; i < n; i++)
    {
        if (locs[i].kind != ARG_STACK)
            continue;

        int32_t off = (int32_t)(shadow + locs[i].idx * 8);
        MachineOperand mem = mach_op_mem(isel_abi_sp_vreg(), off, 64);

        MachineOperand arg = isel_lower_value(ctx, inst->call_args[i]);
        arg = isel_materialize_imm_to_gpr64(ctx, arg);

        if (arg.kind == MACH_OP_MEM || arg.kind == MACH_OP_GLOBAL)
        {
            int tmp_v = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand tmp = mach_op_vreg(tmp_v, 64);
            isel_emit(ctx, MACH_MOV, tmp, arg, mach_op_none());
            arg = tmp;
        }

        isel_emit(ctx, MACH_STORE, mem, arg, mach_op_none());
    }

    // استدعاء
    MachineOperand target = mach_op_none();
    if (inst->call_target) {
        target = mach_op_func(inst->call_target);
    } else if (inst->call_callee) {
        target = isel_lower_value(ctx, inst->call_callee);
        target = isel_materialize_imm_to_gpr64(ctx, target);
    } else {
        // احتياطي: لا هدف.
        target = mach_op_func("???");
    }
    MachineInst *calli = isel_emit(ctx, MACH_CALL, mach_op_none(), target, mach_op_none());
    if (calli)
    {
        // SysV varargs rule: عدد سجلات XMM المستخدمة.
        calli->sysv_al = is_win ? -1 : xmm_i;
    }

    // تحرير إطار النداء
    if (call_frame_aligned > 0)
    {
        MachineOperand sp = mach_op_vreg(isel_abi_sp_vreg(), 64);
        MachineOperand imm = mach_op_imm(call_frame_aligned, 64);
        isel_emit_comment(ctx, MACH_ADD, sp, sp, imm, "// تحرير إطار نداء");
    }

    // نقل القيمة المرجعة
    if (inst->dest >= 0 && inst->type && inst->type->kind != IR_TYPE_VOID)
    {
        if (inst->type->kind == IR_TYPE_F64)
        {
            MachineOperand dst = mach_op_vreg(inst->dest, 64);
            MachineOperand xmm0 = mach_op_xmm(0);
            MachineInst *mi = isel_emit_comment(ctx, MACH_MOV, dst, xmm0, mach_op_none(),
                                                "// القيمة المرجعة من XMM0");
            if (mi) mi->ir_reg = inst->dest;
        }
        else
        {
            int bits = isel_type_bits(inst->type);
            MachineOperand dst = mach_op_vreg(inst->dest, bits);
            MachineOperand ret_reg = mach_op_vreg(isel_abi_ret_vreg(cc), bits);
            MachineInst *mi = isel_emit_comment(ctx, MACH_MOV, dst, ret_reg, mach_op_none(),
                                                "// القيمة المرجعة من RAX");
            if (mi) mi->ir_reg = inst->dest;
        }
    }

    free(locs);
}

// -----------------------------------------------------------------------------
// تحسين النداء_الذيلي (Tail Call Optimization) — v0.3.2.7.3
// -----------------------------------------------------------------------------

static int isel_is_tailcall_pair(ISelCtx *ctx, IRInst *call, IRInst *ret)
{
    if (!ctx || !ctx->enable_tco)
        return 0;
    if (!call || !ret)
        return 0;
    if (call->op != IR_OP_CALL)
        return 0;
    if (ret->op != IR_OP_RET)
        return 0;
    if (!call->call_target)
        return 0;

    // النداء_الذيلي مع معاملات مكدس قد يكون ممكناً، لكن سيتم التحقق منه في مرحلة الخفض.

    // 1) رجوع فراغ: يجب أن يكون النداء أيضاً بلا قيمة.
    if (ret->operand_count == 0 || !ret->operands[0])
    {
        if (call->dest >= 0 && call->type && call->type->kind != IR_TYPE_VOID)
            return 0;
        return 1;
    }

    // 2) رجوع بقيمة: يجب أن يرجع نفس سجل نتيجة النداء.
    if (call->dest < 0)
        return 0;
    if (ret->operand_count < 1 || !ret->operands[0])
        return 0;
    if (ret->operands[0]->kind != IR_VAL_REG)
        return 0;
    if (ret->operands[0]->data.reg_num != call->dest)
        return 0;

    return 1;
}

static bool isel_lower_tailcall(ISelCtx *ctx, IRInst *call)
{
    if (!ctx || !call)
        return false;

    const BaaCallingConv *cc = isel_cc_or_default(ctx);
    bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
    int gpr_max = (cc && cc->int_arg_reg_count > 0) ? cc->int_arg_reg_count : 4;
    int xmm_max = is_win ? 4 : 8;

    typedef enum { ARG_GPR, ARG_XMM, ARG_STACK } ArgLocKind;
    typedef struct { ArgLocKind kind; int idx; } ArgLoc;

    int n = call->call_arg_count;
    ArgLoc *locs = NULL;
    if (n > 0)
    {
        locs = (ArgLoc *)calloc((size_t)n, sizeof(ArgLoc));
        if (!locs) return false;
    }

    int gpr_i = 0;
    int xmm_i = 0;
    int stack_i = 0;

    for (int i = 0; i < n; i++)
    {
        IRType *at = (call->call_args && call->call_args[i]) ? call->call_args[i]->type : NULL;
        bool is_f64 = (at && at->kind == IR_TYPE_F64);

        if (is_win)
        {
            if (i < 4)
                locs[i] = (ArgLoc){is_f64 ? ARG_XMM : ARG_GPR, i};
            else
                locs[i] = (ArgLoc){ARG_STACK, stack_i++};
        }
        else
        {
            if (is_f64)
            {
                if (xmm_i < xmm_max)
                    locs[i] = (ArgLoc){ARG_XMM, xmm_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
            else
            {
                if (gpr_i < gpr_max)
                    locs[i] = (ArgLoc){ARG_GPR, gpr_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
        }
    }

    int out_stack = stack_i;
    int in_stack = 0;
    if (ctx->ir_func && ctx->ir_func->param_count > 0)
    {
        int in_gpr = 0;
        int in_xmm = 0;
        int in_stack_i = 0;

        for (int i = 0; i < ctx->ir_func->param_count; i++)
        {
            IRType *pt = ctx->ir_func->params ? ctx->ir_func->params[i].type : NULL;
            bool is_f64 = (pt && pt->kind == IR_TYPE_F64);

            if (is_win)
            {
                if (i >= 4) in_stack_i++;
            }
            else
            {
                if (is_f64)
                {
                    if (in_xmm < xmm_max) in_xmm++;
                    else in_stack_i++;
                }
                else
                {
                    if (in_gpr < gpr_max) in_gpr++;
                    else in_stack_i++;
                }
            }
        }

        in_stack = in_stack_i;
    }

    // لا نُطبّق TCO إذا كانت معاملات المكدس المطلوبة أكبر مما وفره المستدعي لنا.
    if (out_stack > in_stack)
    {
        free(locs);
        return false;
    }

    // تحضير معاملات ABI (نفس مسار CALL لكن بدون call/ret/mov من RAX).
    for (int i = 0; i < n; i++)
    {
        MachineOperand arg = isel_lower_value(ctx, call->call_args[i]);
        IRType *at = (call->call_args && call->call_args[i]) ? call->call_args[i]->type : NULL;

        if (locs[i].kind == ARG_GPR)
        {
            int bits = at ? isel_type_bits(at) : 64;
            MachineOperand param_reg = mach_op_vreg(isel_abi_arg_vreg(cc, locs[i].idx), bits);
            isel_emit(ctx, MACH_MOV, param_reg, arg, mach_op_none());
        }
        else if (locs[i].kind == ARG_XMM)
        {
            arg = isel_materialize_imm_to_gpr64(ctx, arg);
            MachineOperand xmm = mach_op_xmm(locs[i].idx);
            isel_emit(ctx, MACH_MOV, xmm, arg, mach_op_none());
        }
    }

    int shadow = (cc) ? cc->shadow_space_bytes : 0;

    // Windows: home/shadow space لمعاملات السجلات قبل leave.
    // بعد leave تصبح هذه الخانات عند 8(%rsp).
    if (cc && cc->home_reg_args_on_call && shadow >= 32)
    {
        int n_home = (n < 4) ? n : 4;
        for (int i = 0; i < n_home; i++)
        {
            MachineOperand mem = mach_op_mem(-1, (int32_t)(16 + i * 8), 64);
            if (locs[i].kind == ARG_XMM)
            {
                MachineOperand src = mach_op_xmm(locs[i].idx);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
            else
            {
                MachineOperand src = mach_op_vreg(isel_abi_arg_vreg(cc, i), 64);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
        }
    }

    // معاملات المكدس للنداء_الذيلي: نكتبها في منطقة معاملات الدالة الحالية فوق return address.
    // هذا آمن فقط إذا كانت دالتنا نفسها تتلقى نفس/أكثر عدد معاملات مكدس.
    if (out_stack > 0)
    {
        for (int i = 0; i < n; i++)
        {
            if (locs[i].kind != ARG_STACK)
                continue;

            int32_t off = (int32_t)(16 + shadow + locs[i].idx * 8);
            MachineOperand mem = mach_op_mem(-1, off, 64); // base -1 => RBP

            MachineOperand arg = isel_lower_value(ctx, call->call_args[i]);
            arg = isel_materialize_imm_to_gpr64(ctx, arg);

            if (arg.kind == MACH_OP_MEM || arg.kind == MACH_OP_GLOBAL)
            {
                int tmp_v = mach_func_alloc_vreg(ctx->mfunc);
                MachineOperand tmp = mach_op_vreg(tmp_v, 64);
                isel_emit(ctx, MACH_MOV, tmp, arg, mach_op_none());
                arg = tmp;
            }

            isel_emit(ctx, MACH_STORE, mem, arg, mach_op_none());
        }
    }

    // SysV varargs rule: AL = عدد سجلات XMM المستخدمة.
    if (cc && cc->sysv_set_al_zero_on_call)
    {
        MachineOperand eax = mach_op_vreg(isel_abi_ret_vreg(cc), 32);
        MachineOperand imm = mach_op_imm((int64_t)xmm_i, 32);
        isel_emit(ctx, MACH_MOV, eax, imm, mach_op_none());
    }

    MachineOperand target = mach_op_func(call->call_target);
    isel_emit_comment(ctx, MACH_TAILJMP, mach_op_none(), target, mach_op_none(),
                      "// نداء_ذيلي: قفز إلى الدالة الهدف");

    free(locs);
    return true;
}

/**
 * @brief خفض عملية النسخ (copy).
 *
 * النمط: %dst = نسخ type src
 * →  mov vreg_dst, src
 */
static void isel_lower_copy(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض تعليمة إزاحة_مؤشر (ptr.offset).
 *
 * النمط: %dst = إزاحة_مؤشر ptr<T> base, index
 *
 * حيث index هو فهرس عناصر ويُضرب ضمنياً في حجم T.
 */
static void isel_lower_ptr_offset(ISelCtx *ctx, IRInst *inst)
{
    if (!ctx || !ctx->mfunc || !inst || inst->operand_count < 2)
        return;

    const IRDataLayout *dl = (ctx->target) ? ctx->target->data_layout : NULL;

    IRType *elem_t = NULL;
    if (inst->type && inst->type->kind == IR_TYPE_PTR)
        elem_t = inst->type->data.pointee;

    int elem_size = ir_type_store_size(dl, elem_t ? elem_t : IR_TYPE_I8_T);
    if (elem_size <= 0) elem_size = 1;

    // base: يجب أن يكون عنواناً في سجل
    MachineOperand base = isel_lower_value(ctx, inst->operands[0]);
    if (base.kind == MACH_OP_GLOBAL)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_LEA, tmp_op, base, mach_op_none());
        base = tmp_op;
    }
    else if (base.kind != MACH_OP_VREG)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, base, mach_op_none());
        base = tmp_op;
    }

    // index: قد يكون فوري/سجل/ذاكرة -> نضمن تمثيله كقيمة 64 بت
    MachineOperand idx = isel_lower_value(ctx, inst->operands[1]);

    MachineOperand scaled = idx;

    if (idx.kind == MACH_OP_IMM)
    {
        int64_t v = idx.data.imm;
        int64_t s = v * (int64_t)elem_size;
        scaled = mach_op_imm(s, 64);
    }
    else
    {
        // ننسخ index إلى سجل ثم نُطبّق التحجيم عليه
        if (idx.kind != MACH_OP_VREG)
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t_op = mach_op_vreg(t, 64);
            isel_emit(ctx, MACH_MOV, t_op, idx, mach_op_none());
            idx = t_op;
        }

        if (elem_size != 1)
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t_op = mach_op_vreg(t, 64);
            isel_emit(ctx, MACH_MOV, t_op, idx, mach_op_none());

            int sh = 0;
            if (isel_is_pow2_i64((int64_t)elem_size, &sh) && sh > 0)
            {
                isel_emit(ctx, MACH_SHL, t_op, t_op, mach_op_imm((int64_t)sh, 8));
            }
            else
            {
                isel_emit(ctx, MACH_IMUL, t_op, t_op, mach_op_imm((int64_t)elem_size, 64));
            }

            scaled = t_op;
        }
        else
        {
            scaled = idx;
        }
    }

    MachineOperand dst = mach_op_vreg(inst->dest, 64);
    isel_emit(ctx, MACH_MOV, dst, base, mach_op_none());
    MachineInst *mi = isel_emit(ctx, MACH_ADD, dst, dst, scaled);
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عقدة فاي (phi).
 *
 * عقد فاي تُحل عادةً أثناء خروج SSA أو تخصيص السجلات.
 * حالياً، نولد تعليمات mov مشروحة كتعليقات.
 *
 * ملاحظة: في التنفيذ الكامل، يجب إدراج النسخ في الكتل السابقة.
 */
static void isel_lower_phi(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    // نولد تعليمة NOP كعنصر نائب لعقدة فاي
    // سيتم حل هذا في مرحلة خروج SSA (v0.3.2.2+)
    MachineInst *mi = isel_emit_comment(ctx, MACH_NOP, dst, mach_op_none(), mach_op_none(),
                                        "// فاي - سيُحل في تخصيص السجلات");
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية التحويل (cast).
 *
 * النمط: %dst = تحويل from_type to to_type value
 * →  mov/movzx/movsx vreg_dst, value
 */
static void isel_lower_cast(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    IRType *dst_t = inst->type;
    IRType *src_t = (inst->operands[0]) ? inst->operands[0]->type : NULL;

    // عشري <-> عشري: نسخ بتات.
    if (isel_irtype_is_f64(dst_t) && isel_irtype_is_f64(src_t))
    {
        MachineOperand dst = mach_op_vreg(inst->dest, 64);
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        src = isel_materialize_imm_to_gpr64(ctx, src);
        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
        return;
    }

    // int -> عشري
    if (isel_irtype_is_f64(dst_t) && !isel_irtype_is_f64(src_t))
    {
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        src = isel_extend_to_gpr64(ctx, src, src_t);

        MachineOperand xmm0 = mach_op_xmm(0);
        isel_emit(ctx, MACH_CVTSI2SD, xmm0, src, mach_op_none());

        MachineOperand dst = mach_op_vreg(inst->dest, 64);
        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, xmm0, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
        return;
    }

    // عشري -> int
    if (!isel_irtype_is_f64(dst_t) && isel_irtype_is_f64(src_t))
    {
        int dst_bits = isel_type_bits(dst_t);
        MachineOperand dst = mach_op_vreg(inst->dest, dst_bits);

        MachineOperand src_bits = isel_lower_value(ctx, inst->operands[0]);
        src_bits = isel_materialize_imm_to_gpr64(ctx, src_bits);

        MachineOperand xmm0 = mach_op_xmm(0);
        isel_emit(ctx, MACH_MOV, xmm0, src_bits, mach_op_none());

        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp64 = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_CVTTSD2SI, tmp64, xmm0, mach_op_none());

        // اقتطاع إلى الحجم النهائي إن كان أصغر.
        MachineOperand src = tmp64;
        if (dst_bits > 0 && dst_bits < 64)
            src.size_bits = dst_bits;

        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
        return;
    }

    // تحويلات الأعداد الصحيحة (sign/zero extend + trunc)
    {
        int dst_bits = isel_type_bits(dst_t);
        MachineOperand dst = mach_op_vreg(inst->dest, dst_bits);
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

        int src_bits = src.size_bits;

        if (src_bits < dst_bits && src_bits > 0)
        {
            // توسيع: بالأصفار للأنواع unsigned/i1، وبالإشارة للأنواع signed.
            bool zext = true;
            if (isel_irtype_is_signed_int(src_t))
                zext = false;
            MachineOp mop = zext ? MACH_MOVZX : MACH_MOVSX;
            MachineInst *mi = isel_emit(ctx, mop, dst, src, mach_op_none());
            if (mi) mi->ir_reg = inst->dest;
        }
        else
        {
            // نسخ مباشر (نفس الحجم أو تقليص). عند التقليص، نحدد حجم المصدر.
            if (src_bits > dst_bits && dst_bits > 0)
                src.size_bits = dst_bits;
            MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
            if (mi) mi->ir_reg = inst->dest;
        }
    }
}

// ============================================================================
// خفض تعليمة IR واحدة
// ============================================================================

/**
 * @brief خفض تعليمة IR واحدة إلى تعليمات آلة.
 *
 * هذه هي الدالة المحورية لاختيار التعليمات. تقوم بتوزيع
 * كل عملية IR إلى دالة الخفض المناسبة.
 */
static void isel_lower_inst(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    switch (inst->op)
    {
    // عمليات حسابية
    case IR_OP_ADD:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_ADDSD);
        else isel_lower_binop(ctx, inst, MACH_ADD);
        break;
    case IR_OP_SUB:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_SUBSD);
        else isel_lower_binop(ctx, inst, MACH_SUB);
        break;
    case IR_OP_MUL:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_MULSD);
        else isel_lower_mul_strength_reduce(ctx, inst);
        break;
    case IR_OP_SHL:
        isel_lower_shift(ctx, inst, MACH_SHL);
        break;
    case IR_OP_SHR:
        isel_lower_shift(ctx, inst,
                         isel_irtype_is_unsigned_int(inst->type) ? MACH_SHR : MACH_SAR);
        break;
    case IR_OP_DIV:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_DIVSD);
        else isel_lower_div(ctx, inst, false);
        break;
    case IR_OP_MOD:
        isel_lower_div(ctx, inst, true);
        break;
    case IR_OP_NEG:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fneg(ctx, inst);
        else isel_lower_neg(ctx, inst);
        break;

    // عمليات الذاكرة
    case IR_OP_ALLOCA:
        isel_lower_alloca(ctx, inst);
        break;
    case IR_OP_LOAD:
        isel_lower_load(ctx, inst);
        break;
    case IR_OP_STORE:
        isel_lower_store(ctx, inst);
        break;
    case IR_OP_PTR_OFFSET:
        isel_lower_ptr_offset(ctx, inst);
        break;

    // عملية المقارنة
    case IR_OP_CMP:
        isel_lower_cmp(ctx, inst);
        break;

    // عمليات منطقية
    case IR_OP_AND:
    case IR_OP_OR:
    case IR_OP_XOR:
    case IR_OP_NOT:
        isel_lower_logical(ctx, inst);
        break;

    // عمليات التحكم
    case IR_OP_BR:
        isel_lower_br(ctx, inst);
        break;
    case IR_OP_BR_COND:
        isel_lower_br_cond(ctx, inst);
        break;
    case IR_OP_RET:
        isel_lower_ret(ctx, inst);
        break;
    case IR_OP_CALL:
        isel_lower_call(ctx, inst);
        break;

    // عمليات SSA
    case IR_OP_PHI:
        isel_lower_phi(ctx, inst);
        break;
    case IR_OP_COPY:
        isel_lower_copy(ctx, inst);
        break;

    // تحويل الأنواع
    case IR_OP_CAST:
        isel_lower_cast(ctx, inst);
        break;

    // لا عملية
    case IR_OP_NOP:
        isel_emit(ctx, MACH_NOP, mach_op_none(), mach_op_none(), mach_op_none());
        break;

    default:
        // عملية غير معروفة - نولد تعليق
        isel_emit_comment(ctx, MACH_NOP, mach_op_none(), mach_op_none(), mach_op_none(),
                          "// عملية IR غير مدعومة");
        break;
    }
}

// ============================================================================
// خفض كتلة IR
// ============================================================================

/**
 * @brief خفض كتلة IR إلى كتلة آلية.
 */
static MachineBlock *isel_lower_block(ISelCtx *ctx, IRBlock *ir_block)
{
    if (!ir_block)
        return NULL;

    MachineBlock *mblock = mach_block_new(ir_block->label, ir_block->id);
    if (!mblock)
        return NULL;

    ctx->ir_block = ir_block;
    ctx->mblock = mblock;

    // إصدار تسمية الكتلة
    MachineInst *label = mach_inst_new(MACH_LABEL, mach_op_label(ir_block->id),
                                       mach_op_none(), mach_op_none());
    if (label)
    {
        label->comment = ir_block->label;
        mach_block_append(mblock, label);
    }

    // خفض كل تعليمة في الكتلة
    IRInst *inst = ir_block->first;
    while (inst)
    {
        ctx->ir_inst = inst;

        // نمط النداء_الذيلي: CALL ثم RET مباشرة.
        if (inst->op == IR_OP_CALL)
        {
            IRInst *next = inst->next;
            if (next && isel_is_tailcall_pair(ctx, inst, next))
            {
                if (isel_lower_tailcall(ctx, inst))
                {
                    inst = next->next; // تخطَّ RET
                    continue;
                }
            }
        }

        isel_lower_inst(ctx, inst);
        inst = inst->next;
    }

    ctx->ir_inst = NULL;

    // نسخ معلومات الخلفاء
    mblock->succ_count = ir_block->succ_count;
    // (سيتم ربط الخلفاء بالكتل الآلية لاحقاً)

    return mblock;
}

// ============================================================================
// خفض دالة IR
// ============================================================================

/**
 * @brief خفض دالة IR إلى دالة آلية.
 */
static MachineFunc *isel_lower_func(ISelCtx *ctx, IRFunc *ir_func)
{
    if (!ir_func)
        return NULL;

    MachineFunc *mfunc = mach_func_new(ir_func->name);
    if (!mfunc)
        return NULL;

    mfunc->is_prototype = ir_func->is_prototype;
    mfunc->param_count = ir_func->param_count;

    // نبدأ عداد السجلات الافتراضية من حيث توقف IR
    mfunc->next_vreg = ir_func->next_reg;

    ctx->ir_func = ir_func;
    ctx->mfunc = mfunc;

    // تخطي النماذج الأولية
    if (ir_func->is_prototype)
        return mfunc;

    // تحليل الحلقات لاستخدامه في تحسينات داخل isel.
    ctx->loop_info = ir_loop_analyze_func(ir_func);

    // خفض كل كتلة
    for (IRBlock *block = ir_func->blocks; block; block = block->next)
    {
        MachineBlock *mblock = isel_lower_block(ctx, block);
        if (mblock)
        {
            mach_func_add_block(mfunc, mblock);
        }
    }

    if (ctx->loop_info)
    {
        ir_loop_info_free(ctx->loop_info);
        ctx->loop_info = NULL;
    }

    // ──────────────────────────────────────────────────────────────────────
    // ربط CFG: خلفاء الكتل الآلية (MachineBlock.succs)
    //
    // ملاحظة مهمة (v0.3.2.6.4): تحليل الحيوية في regalloc يعتمد على succs[]
    // لحساب live_out عبر الحواف، بما فيها حواف الرجوع في الحلقات (back-edges).
    // كان لدينا سابقاً succ_count فقط بدون ربط succs، مما يجعل liveness عبر
    // الحلقات خاطئاً تحت ضغط سجلات عالٍ.
    // ──────────────────────────────────────────────────────────────────────
    {
        int max_id = -1;
        for (IRBlock *b = ir_func->blocks; b; b = b->next)
        {
            if (b->id > max_id)
                max_id = b->id;
        }

        if (max_id >= 0)
        {
            MachineBlock **by_id = (MachineBlock **)calloc((size_t)(max_id + 1), sizeof(MachineBlock *));
            if (by_id)
            {
                for (MachineBlock *mb = mfunc->blocks; mb; mb = mb->next)
                {
                    if (mb->id >= 0 && mb->id <= max_id)
                        by_id[mb->id] = mb;
                }

                for (IRBlock *ib = ir_func->blocks; ib; ib = ib->next)
                {
                    if (ib->id < 0 || ib->id > max_id)
                        continue;
                    MachineBlock *mb = by_id[ib->id];
                    if (!mb)
                        continue;

                    mb->succ_count = 0;
                    mb->succs[0] = NULL;
                    mb->succs[1] = NULL;

                    for (int s = 0; s < ib->succ_count && s < 2; s++)
                    {
                        IRBlock *succ = ib->succs[s];
                        if (!succ)
                            continue;
                        if (succ->id < 0 || succ->id > max_id)
                            continue;
                        MachineBlock *msucc = by_id[succ->id];
                        if (!msucc)
                            continue;
                        mb->succs[mb->succ_count++] = msucc;
                    }
                }

                free(by_id);
            }
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // نسخ معاملات الدالة من سجلات ABI إلى سجلات المعاملات الافتراضية
    // ──────────────────────────────────────────────────────────────────────
    if (ir_func->param_count > 0 && mfunc->entry)
    {
        // نُنشئ تعليمات النسخ ونحشرها في بداية كتلة الدخول
        const BaaCallingConv *cc = isel_cc_or_default(ctx);
        bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
        int gpr_max = (cc && cc->int_arg_reg_count > 0) ? cc->int_arg_reg_count : 4;
        int xmm_max = is_win ? 4 : 8;

        typedef enum { IN_GPR, IN_XMM, IN_STACK } InLocKind;
        typedef struct { InLocKind kind; int idx; } InLoc;

        InLoc *locs = (InLoc *)calloc((size_t)ir_func->param_count, sizeof(InLoc));
        if (!locs)
            return mfunc;

        int gpr_i = 0;
        int xmm_i = 0;
        int stack_i = 0;

        for (int i = 0; i < ir_func->param_count; i++)
        {
            IRType *pt = ir_func->params ? ir_func->params[i].type : NULL;
            bool is_f64 = (pt && pt->kind == IR_TYPE_F64);

            if (is_win)
            {
                if (i < 4)
                {
                    locs[i] = (InLoc){is_f64 ? IN_XMM : IN_GPR, i};
                }
                else
                {
                    locs[i] = (InLoc){IN_STACK, stack_i++};
                }
            }
            else
            {
                if (is_f64)
                {
                    if (xmm_i < xmm_max)
                        locs[i] = (InLoc){IN_XMM, xmm_i++};
                    else
                        locs[i] = (InLoc){IN_STACK, stack_i++};
                }
                else
                {
                    if (gpr_i < gpr_max)
                        locs[i] = (InLoc){IN_GPR, gpr_i++};
                    else
                        locs[i] = (InLoc){IN_STACK, stack_i++};
                }
            }
        }

        // نبني سلسلة التعليمات المؤقتة
        MachineInst *param_first = NULL;
        MachineInst *param_last = NULL;

        for (int i = 0; i < ir_func->param_count; i++)
        {
            MachineInst *mi = NULL;
            IRType *pt = ir_func->params ? ir_func->params[i].type : NULL;
            MachineOperand param_dst = mach_op_vreg(ir_func->params[i].reg, 64);

            if (locs[i].kind == IN_GPR)
            {
                MachineOperand abi_reg = mach_op_vreg(isel_abi_arg_vreg(cc, locs[i].idx), 64);
                mi = mach_inst_new(MACH_MOV, param_dst, abi_reg, mach_op_none());
            }
            else if (locs[i].kind == IN_XMM)
            {
                (void)pt;
                MachineOperand xmm = mach_op_xmm(locs[i].idx);
                mi = mach_inst_new(MACH_MOV, param_dst, xmm, mach_op_none());
            }
            else
            {
                // معاملات المكدس: تُقرأ من مساحة المستدعي فوق return address.
                // ملاحظة: على Windows توجد shadow space (32) بين home slots ومعاملات المكدس.
                int shadow = (cc) ? cc->shadow_space_bytes : 0;
                int32_t off = (int32_t)(16 + shadow + locs[i].idx * 8);
                MachineOperand mem = mach_op_mem(-1, off, 64);
                mi = mach_inst_new(MACH_LOAD, param_dst, mem, mach_op_none());
            }

            if (!mi)
                continue;

            if (!param_first)
            {
                param_first = mi;
                param_last = mi;
            }
            else
            {
                param_last->next = mi;
                mi->prev = param_last;
                param_last = mi;
            }
        }

        // حشر السلسلة في بداية كتلة الدخول
        if (param_first && mfunc->entry->first)
        {
            param_last->next = mfunc->entry->first;
            mfunc->entry->first->prev = param_last;
            mfunc->entry->first = param_first;
        }
        else if (param_first)
        {
            mfunc->entry->first = param_first;
            mfunc->entry->last = param_last;
        }

        free(locs);
    }

    return mfunc;
}

// ============================================================================
// نقطة الدخول الرئيسية لاختيار التعليمات
// ============================================================================

MachineModule *isel_run_ex(IRModule *ir_module, bool enable_tco, const BaaTarget* target)
{
    if (!ir_module)
        return NULL;

    MachineModule *mmod = mach_module_new(ir_module->name);
    if (!mmod)
        return NULL;

    // نسخ مراجع المتغيرات العامة وجدول النصوص
    mmod->globals = ir_module->globals;
    mmod->global_count = ir_module->global_count;
    mmod->strings = ir_module->strings;
    mmod->string_count = ir_module->string_count;
    mmod->baa_strings = ir_module->baa_strings;
    mmod->baa_string_count = ir_module->baa_string_count;

    ISelCtx ctx = {0};
    ctx.mmod = mmod;
    ctx.ir_module = ir_module;
    ctx.enable_tco = enable_tco;
    ctx.target = target ? target : baa_target_builtin_windows_x86_64();

    // خفض كل دالة
    for (IRFunc *func = ir_module->funcs; func; func = func->next)
    {
        MachineFunc *mfunc = isel_lower_func(&ctx, func);
        if (mfunc)
        {
            mach_module_add_func(mmod, mfunc);
        }
    }

    if (ctx.had_error)
    {
        mach_module_free(mmod);
        return NULL;
    }

    return mmod;
}

MachineModule *isel_run(IRModule *ir_module)
{
    return isel_run_ex(ir_module, false, baa_target_builtin_windows_x86_64());
}

// ============================================================================
// طباعة تمثيل الآلة (للتنقيح)
// ============================================================================

const char *mach_op_to_string(MachineOp op)
{
    switch (op)
    {
    case MACH_ADD:
        return "add";
    case MACH_SUB:
        return "sub";
    case MACH_IMUL:
        return "imul";
    case MACH_SHL:
        return "shl";
    case MACH_SHR:
        return "shr";
    case MACH_SAR:
        return "sar";
    case MACH_IDIV:
        return "idiv";
    case MACH_DIV:
        return "div";
    case MACH_NEG:
        return "neg";
    case MACH_CQO:
        return "cqo";
    case MACH_ADDSD:
        return "addsd";
    case MACH_SUBSD:
        return "subsd";
    case MACH_MULSD:
        return "mulsd";
    case MACH_DIVSD:
        return "divsd";
    case MACH_UCOMISD:
        return "ucomisd";
    case MACH_XORPD:
        return "xorpd";
    case MACH_CVTSI2SD:
        return "cvtsi2sd";
    case MACH_CVTTSD2SI:
        return "cvttsd2si";
    case MACH_MOV:
        return "mov";
    case MACH_LEA:
        return "lea";
    case MACH_LOAD:
        return "load";
    case MACH_STORE:
        return "store";
    case MACH_CMP:
        return "cmp";
    case MACH_TEST:
        return "test";
    case MACH_SETE:
        return "sete";
    case MACH_SETNE:
        return "setne";
    case MACH_SETG:
        return "setg";
    case MACH_SETL:
        return "setl";
    case MACH_SETGE:
        return "setge";
    case MACH_SETLE:
        return "setle";
    case MACH_SETA:
        return "seta";
    case MACH_SETB:
        return "setb";
    case MACH_SETAE:
        return "setae";
    case MACH_SETBE:
        return "setbe";
    case MACH_SETP:
        return "setp";
    case MACH_SETNP:
        return "setnp";
    case MACH_MOVZX:
        return "movzx";
    case MACH_MOVSX:
        return "movsx";
    case MACH_AND:
        return "and";
    case MACH_OR:
        return "or";
    case MACH_NOT:
        return "not";
    case MACH_XOR:
        return "xor";
    case MACH_JMP:
        return "jmp";
    case MACH_JE:
        return "je";
    case MACH_JNE:
        return "jne";
    case MACH_CALL:
        return "call";
    case MACH_TAILJMP:
        return "tailjmp";
    case MACH_RET:
        return "ret";
    case MACH_PUSH:
        return "push";
    case MACH_POP:
        return "pop";
    case MACH_NOP:
        return "nop";
    case MACH_LABEL:
        return "label";
    case MACH_COMMENT:
        return "comment";
    case MACH_INLINE_ASM:
        return "inline_asm";
    default:
        return "???";
    }
}

void mach_operand_print(MachineOperand *op, FILE *out)
{
    if (!op || !out)
        return;

    switch (op->kind)
    {
    case MACH_OP_NONE:
        break;
    case MACH_OP_VREG:
        if (op->data.vreg == -2)
        {
            fprintf(out, "%%ret");
        }
        else if (op->data.vreg == -6)
        {
            fprintf(out, "%%rcx");
        }
        else if (op->data.vreg <= -10 && op->data.vreg >= -13)
        {
            // سجلات معاملات ABI
            const char *abi_regs[] = {"%%rcx", "%%rdx", "%%r8", "%%r9"};
            int idx = -(op->data.vreg + 10);
            fprintf(out, "%s", abi_regs[idx]);
        }
        else
        {
            fprintf(out, "%%v%d", op->data.vreg);
        }
        break;
    case MACH_OP_IMM:
        fprintf(out, "$%lld", (long long)op->data.imm);
        break;
    case MACH_OP_MEM:
        if (op->data.mem.base_vreg == -1)
        {
            fprintf(out, "%d(%%rbp)", op->data.mem.offset);
        }
        else
        {
            if (op->data.mem.offset != 0)
                fprintf(out, "%d(%%v%d)", op->data.mem.offset, op->data.mem.base_vreg);
            else
                fprintf(out, "(%%v%d)", op->data.mem.base_vreg);
        }
        break;
    case MACH_OP_LABEL:
        fprintf(out, ".L%d", op->data.label_id);
        break;
    case MACH_OP_GLOBAL:
        fprintf(out, "%s", op->data.name ? op->data.name : "???");
        break;
    case MACH_OP_FUNC:
        fprintf(out, "@%s", op->data.name ? op->data.name : "???");
        break;
    case MACH_OP_XMM:
        fprintf(out, "%%xmm%d", op->data.xmm);
        break;
    }
}

void mach_inst_print(MachineInst *inst, FILE *out)
{
    if (!inst || !out)
        return;

    if (inst->op == MACH_LABEL)
    {
        // طباعة تسمية
        fprintf(out, ".L%d:", inst->dst.data.label_id);
        if (inst->comment)
            fprintf(out, "  # %s", inst->comment);
        fprintf(out, "\n");
        return;
    }

    if (inst->op == MACH_COMMENT)
    {
        if (inst->comment)
            fprintf(out, "    # %s\n", inst->comment);
        return;
    }

    if (inst->op == MACH_INLINE_ASM)
    {
        if (inst->comment)
            fprintf(out, "    %s\n", inst->comment);
        return;
    }

    fprintf(out, "    %-8s", mach_op_to_string(inst->op));

    // طباعة المعاملات
    int has_dst = (inst->dst.kind != MACH_OP_NONE);
    int has_src1 = (inst->src1.kind != MACH_OP_NONE);
    int has_src2 = (inst->src2.kind != MACH_OP_NONE);

    if (has_dst)
    {
        fprintf(out, " ");
        mach_operand_print(&inst->dst, out);
    }
    if (has_src1)
    {
        fprintf(out, "%s", has_dst ? ", " : " ");
        mach_operand_print(&inst->src1, out);
    }
    if (has_src2)
    {
        fprintf(out, ", ");
        mach_operand_print(&inst->src2, out);
    }

    if (inst->comment)
        fprintf(out, "  # %s", inst->comment);
    fprintf(out, "\n");
}

void mach_block_print(MachineBlock *block, FILE *out)
{
    if (!block || !out)
        return;

    for (MachineInst *inst = block->first; inst; inst = inst->next)
    {
        mach_inst_print(inst, out);
    }
}

void mach_func_print(MachineFunc *func, FILE *out)
{
    if (!func || !out)
        return;

    if (func->is_prototype)
    {
        fprintf(out, "# prototype: %s\n", func->name ? func->name : "???");
        return;
    }

    fprintf(out, "# function: %s (stack=%d, vregs=%d)\n",
            func->name ? func->name : "???",
            func->stack_size, func->next_vreg);

    for (MachineBlock *block = func->blocks; block; block = block->next)
    {
        mach_block_print(block, out);
    }
    fprintf(out, "\n");
}

void mach_module_print(MachineModule *module, FILE *out)
{
    if (!module || !out)
        return;

    fprintf(out, "# ============================================\n");
    fprintf(out, "# Machine IR: %s\n", module->name ? module->name : "???");
    fprintf(out, "# Functions: %d\n", module->func_count);
    fprintf(out, "# ============================================\n\n");

    for (MachineFunc *func = module->funcs; func; func = func->next)
    {
        mach_func_print(func, out);
    }
}
