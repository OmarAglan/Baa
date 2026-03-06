/**
 * @file regalloc.c
 * @brief تخصيص السجلات - تحويل السجلات الافتراضية إلى سجلات فيزيائية (v0.3.2.2)
 *
 * ينفّذ تخصيص السجلات باستخدام خوارزمية المسح الخطي (Linear Scan):
 * 1. ترقيم التعليمات تسلسلياً
 * 2. حساب تحليل الحيوية (def/use/live-in/live-out)
 * 3. بناء فترات الحيوية لكل سجل افتراضي
 * 4. المسح الخطي مع تسريب عند نفاد السجلات
 * 5. إدراج كود التسريب (store/load)
 * 6. إعادة كتابة المعاملات بسجلات فيزيائية
 */

#include "backend_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// أسماء السجلات الفيزيائية (Physical Register Names)
// ============================================================================

/**
 * @brief جدول أسماء السجلات الفيزيائية.
 */
static const char *phys_reg_names[PHYS_REG_COUNT] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp",
    "rsi", "rdi", "r8", "r9", "r10", "r11",
    "r12", "r13", "r14", "r15"};

const char *phys_reg_name(PhysReg reg)
{
    if (reg >= 0 && reg < PHYS_REG_COUNT)
        return phys_reg_names[reg];
    if (reg == PHYS_NONE)
        return "none";
    return "???";
}

bool phys_reg_is_callee_saved(PhysReg reg)
{
    // Windows x64 ABI: السجلات المحفوظة عبر النداء: RBX/RBP/RSI/RDI/R12-R15
    switch (reg)
    {
    case PHYS_RBX:
    case PHYS_RBP:
    case PHYS_RSI:
    case PHYS_RDI:
    case PHYS_R12:
    case PHYS_R13:
    case PHYS_R14:
    case PHYS_R15:
        return true;
    default:
        return false;
    }
}

static const BaaCallingConv* regalloc_cc_or_default(const BaaTarget* target)
{
    if (target && target->cc)
        return target->cc;
    return baa_target_builtin_windows_x86_64()->cc;
}

// سجل خدش محجوز: يُستخدم لإعادة تحميل مؤشرات مسرّبة عند استخدامها كقاعدة في معاملات الذاكرة.
// يجب ألا يُخصص هذا السجل لأي vreg عادي (انظر alloc_order).
#define REGALLOC_VREG_SCRATCH_BASE (-4)

static bool reg_is_caller_saved_cc(const BaaCallingConv* cc, PhysReg reg)
{
    if (!cc) return false;
    if (reg < 0 || reg >= PHYS_REG_COUNT) return false;
    return (cc->caller_saved_mask & (1u << (unsigned)reg)) != 0u;
}

static bool reg_is_callee_saved_cc(const BaaCallingConv* cc, PhysReg reg)
{
    if (!cc) return false;
    if (reg < 0 || reg >= PHYS_REG_COUNT) return false;
    return (cc->callee_saved_mask & (1u << (unsigned)reg)) != 0u;
}

// ============================================================================
// ترتيب تفضيل السجلات للتخصيص
// ============================================================================

/**
 * @brief السجلات المتاحة للتخصيص العام مرتبة بالأفضلية.
 *
 * نفضل السجلات المؤقتة (caller-saved) أولاً لتجنب حفظ/استعادة
 * السجلات المحفوظة (callee-saved) قدر الإمكان.
 */
static const PhysReg alloc_order[] = {
    // سجلات مؤقتة (caller-saved) أولاً
    PHYS_R10,
    // ملاحظة: نحتفظ بـ R11 كسجل خدش (scratch) لتصحيح حالات التسريب
    // عندما يكون سجل القاعدة في معاملات الذاكرة مسرّباً.
    // سجلات عامة
    PHYS_RSI,
    PHYS_RDI,
    // سجلات محفوظة (callee-saved)
    PHYS_RBX,
    PHYS_R12,
    PHYS_R13,
    PHYS_R14,
    PHYS_R15,
    // سجلات ABI (نتجنبها لأنها قد تتعارض مع معاملات الدوال)
    PHYS_RCX,
    PHYS_RDX,
    PHYS_R8,
    PHYS_R9,
};
static const int alloc_order_count = sizeof(alloc_order) / sizeof(alloc_order[0]);

// ============================================================================
// دوال مساعدة لمجموعات البتات (Bitset Helpers)
// ============================================================================

/**
 * @brief حساب عدد كلمات uint64_t اللازمة لتمثيل n بت.
 */
static int bitset_word_count(int n)
{
    return (n + 63) / 64;
}

/**
 * @brief تعيين بت في مجموعة بتات.
 */
static void bitset_set(uint64_t *set, int bit)
{
    if (bit < 0)
        return;
    set[bit / 64] |= (1ULL << (bit % 64));
}

/**
 * @brief اختبار بت في مجموعة بتات.
 */
static bool bitset_test(const uint64_t *set, int bit)
{
    if (bit < 0)
        return false;
    return (set[bit / 64] & (1ULL << (bit % 64))) != 0;
}

/**
 * @brief اتحاد مجموعتين: dst = dst | src
 * @return صحيح إذا تغيرت dst.
 */
static bool bitset_union(uint64_t *dst, const uint64_t *src, int words)
{
    bool changed = false;
    for (int i = 0; i < words; i++)
    {
        uint64_t old = dst[i];
        dst[i] |= src[i];
        if (dst[i] != old)
            changed = true;
    }
    return changed;
}

/**
 * @brief فرق المجموعات: dst = a - b (أي a & ~b)
 */
static void bitset_diff(uint64_t *dst, const uint64_t *a, const uint64_t *b, int words)
{
    for (int i = 0; i < words; i++)
    {
        dst[i] = a[i] & ~b[i];
    }
}

/**
 * @brief تخصيص مجموعة بتات مصفّرة.
 */
static uint64_t *bitset_alloc(int words)
{
    return calloc(words, sizeof(uint64_t));
}

// ============================================================================
// إنشاء وتحرير سياق تخصيص السجلات
// ============================================================================

RegAllocCtx *regalloc_ctx_new(MachineFunc *func)
{
    if (!func)
        return NULL;

    RegAllocCtx *ctx = calloc(1, sizeof(RegAllocCtx));
    if (!ctx)
        return NULL;

    ctx->func = func;
    ctx->cc = NULL;

    // حساب أعلى رقم سجل افتراضي
    ctx->max_vreg = func->next_vreg;
    if (ctx->max_vreg < 1)
        ctx->max_vreg = 1;

    // حجم مصفوفات البتات
    ctx->bitset_words = bitset_word_count(ctx->max_vreg);

    // حساب عدد الكتل
    ctx->block_count = func->block_count;

    // تخصيص خرائط النتائج
    ctx->vreg_to_phys = malloc(ctx->max_vreg * sizeof(PhysReg));
    ctx->vreg_spilled = calloc(ctx->max_vreg, sizeof(bool));
    ctx->vreg_spill_offset = calloc(ctx->max_vreg, sizeof(int));
    for (int i = 0; i < ctx->max_vreg; i++)
    {
        ctx->vreg_to_phys[i] = PHYS_NONE;
    }

    // إزاحة التسريب تبدأ بعد حجم المكدس المحلي الحالي
    ctx->next_spill_offset = func->stack_size;

    // تهيئة callee_saved_used
    memset(ctx->callee_saved_used, 0, sizeof(ctx->callee_saved_used));

    return ctx;
}

void regalloc_ctx_free(RegAllocCtx *ctx)
{
    if (!ctx)
        return;

    // تحرير خريطة التعليمات
    free(ctx->inst_map);

    // تحرير حيوية الكتل
    if (ctx->block_live)
    {
        for (int i = 0; i < ctx->block_count; i++)
        {
            free(ctx->block_live[i].def);
            free(ctx->block_live[i].use);
            free(ctx->block_live[i].live_in);
            free(ctx->block_live[i].live_out);
        }
        free(ctx->block_live);
    }

    // تحرير فترات الحيوية
    free(ctx->intervals);

    // تحرير خرائط النتائج
    free(ctx->vreg_to_phys);
    free(ctx->vreg_spilled);
    free(ctx->vreg_spill_offset);

    free(ctx);
}

// ============================================================================
// ترقيم التعليمات (Instruction Numbering)
// ============================================================================

void regalloc_number_insts(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func)
        return;

    // أولاً: عدّ التعليمات
    int count = 0;
    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            count++;
        }
    }

    ctx->total_insts = count;
    ctx->inst_map = malloc(count * sizeof(MachineInst *));
    if (!ctx->inst_map)
        return;

    // ثانياً: رقّم التعليمات بخطوة 2 (لإتاحة إدراج تعليمات التسريب)
    int idx = 0;
    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            if (idx < count)
            {
                ctx->inst_map[idx] = inst;
            }
            idx++;
        }
    }
}

// ============================================================================
// استخلاص السجلات الافتراضية من معامل (Operand VReg Extraction)
// ============================================================================

/**
 * @brief هل المعامل سجل افتراضي عادي (غير سالب وغير خاص)؟
 */
static bool is_normal_vreg(MachineOperand *op)
{
    return op && op->kind == MACH_OP_VREG && op->data.vreg >= 0;
}

/**
 * @brief استخلاص رقم السجل الافتراضي من معامل ذاكرة.
 *
 * معاملات الذاكرة [base + offset] تحتوي على سجل قاعدة.
 */
static int mem_base_vreg(MachineOperand *op)
{
    if (op && op->kind == MACH_OP_MEM && op->data.mem.base_vreg >= 0)
        return op->data.mem.base_vreg;
    return -1;
}

// ============================================================================
// حساب مجموعات def/use (Compute Def/Use Sets)
// ============================================================================

/**
 * @brief تسجيل الاستخدام: إذا لم يُعرَّف بعد في هذه الكتلة، أضفه لـ use.
 */
static void record_use(BlockLiveness *bl, int vreg, int max_vreg)
{
    if (vreg < 0 || vreg >= max_vreg)
        return;
    if (!bitset_test(bl->def, vreg))
    {
        bitset_set(bl->use, vreg);
    }
}

/**
 * @brief تسجيل التعريف.
 */
static void record_def(BlockLiveness *bl, int vreg, int max_vreg)
{
    if (vreg < 0 || vreg >= max_vreg)
        return;
    bitset_set(bl->def, vreg);
}

void regalloc_compute_def_use(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func)
        return;

    // تخصيص حيوية الكتل
    ctx->block_live = calloc(ctx->block_count, sizeof(BlockLiveness));
    if (!ctx->block_live)
        return;

    int words = ctx->bitset_words;
    int max_v = ctx->max_vreg;
    int bi = 0;

    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        if (bi >= ctx->block_count)
            break;

        BlockLiveness *bl = &ctx->block_live[bi];
        bl->block_id = block->id;
        bl->def = bitset_alloc(words);
        bl->use = bitset_alloc(words);
        bl->live_in = bitset_alloc(words);
        bl->live_out = bitset_alloc(words);

        // المشي على التعليمات بالترتيب
        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            // تسجيل الاستخدامات أولاً (قبل التعريف في نفس التعليمة)
            // src1
            if (is_normal_vreg(&inst->src1))
                record_use(bl, inst->src1.data.vreg, max_v);
            if (inst->src1.kind == MACH_OP_MEM && mem_base_vreg(&inst->src1) >= 0)
                record_use(bl, mem_base_vreg(&inst->src1), max_v);

            // src2
            if (is_normal_vreg(&inst->src2))
                record_use(bl, inst->src2.data.vreg, max_v);
            if (inst->src2.kind == MACH_OP_MEM && mem_base_vreg(&inst->src2) >= 0)
                record_use(bl, mem_base_vreg(&inst->src2), max_v);

            // dst كمصدر (في two-address: op dst, dst, src → dst مستخدم)
            // حالة خاصة: تعليمات مثل add dst, dst, src تستخدم dst كمصدر
            if (inst->op == MACH_ADD || inst->op == MACH_SUB ||
                inst->op == MACH_IMUL || inst->op == MACH_SHL ||
                inst->op == MACH_SHR || inst->op == MACH_SAR ||
                inst->op == MACH_AND || inst->op == MACH_OR || inst->op == MACH_XOR ||
                inst->op == MACH_NEG ||
                inst->op == MACH_NOT)
            {
                if (is_normal_vreg(&inst->dst) &&
                    inst->src1.kind == MACH_OP_VREG &&
                    inst->dst.data.vreg == inst->src1.data.vreg)
                {
                    record_use(bl, inst->dst.data.vreg, max_v);
                }
            }

            // dst كمعامل ذاكرة (للتخزين: store [dst], src)
            if (inst->dst.kind == MACH_OP_MEM && mem_base_vreg(&inst->dst) >= 0)
                record_use(bl, mem_base_vreg(&inst->dst), max_v);

            // تسجيل التعريف
            if (is_normal_vreg(&inst->dst))
                record_def(bl, inst->dst.data.vreg, max_v);
        }

        bi++;
    }
}

#include "regalloc_liveness.c"
#include "regalloc_linear_scan.c"
#include "regalloc_rewrite.c"
#include "regalloc_driver.c"
