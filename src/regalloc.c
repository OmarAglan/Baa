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

#include "regalloc.h"
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
    // Windows x64 ABI: RBX, RBP, RDI, RSI, RSP, R12-R15 هي callee-saved
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

/**
 * @brief هل السجل من نوع caller-saved حسب Windows x64 ABI؟
 *
 * هذا مهم لأن استدعاءات الدوال (CALL) قد تُدمّر هذه السجلات،
 * لذا لا يجوز الاحتفاظ بقيم "حية" عبر CALL داخلها.
 */
static bool phys_reg_is_caller_saved(PhysReg reg)
{
    // caller-saved: RAX, RCX, RDX, R8, R9, R10, R11
    switch (reg)
    {
    case PHYS_RAX:
    case PHYS_RCX:
    case PHYS_RDX:
    case PHYS_R8:
    case PHYS_R9:
    case PHYS_R10:
    case PHYS_R11:
        return true;
    default:
        return false;
    }
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
    PHYS_R11,
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
    PHYS_RAX,
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
                inst->op == MACH_IMUL || inst->op == MACH_AND ||
                inst->op == MACH_OR || inst->op == MACH_NEG ||
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

// ============================================================================
// حساب الحيوية (Liveness: live-in / live-out)
// ============================================================================

void regalloc_compute_liveness(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->block_live)
        return;

    int words = ctx->bitset_words;
    int bc = ctx->block_count;

    // نحتاج مصفوفة مؤقتة للحسابات
    uint64_t *temp = bitset_alloc(words);
    if (!temp)
        return;

    // بناء خريطة block_id → فهرس في block_live
    // وخريطة الكتل الآلية
    MachineBlock **block_arr = malloc(bc * sizeof(MachineBlock *));
    if (!block_arr)
    {
        free(temp);
        return;
    }

    int bi = 0;
    for (MachineBlock *b = ctx->func->blocks; b; b = b->next)
    {
        if (bi < bc)
            block_arr[bi] = b;
        bi++;
    }

    // التكرار حتى الثبات (fixpoint)
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 100)
    {
        changed = false;
        iterations++;

        // المشي على الكتل بترتيب عكسي (أفضل للتقارب)
        for (int i = bc - 1; i >= 0; i--)
        {
            BlockLiveness *bl = &ctx->block_live[i];
            MachineBlock *mb = block_arr[i];

            // live_out = اتحاد live_in لكل خلف
            for (int s = 0; s < mb->succ_count; s++)
            {
                MachineBlock *succ = mb->succs[s];
                if (!succ)
                    continue;

                // إيجاد فهرس الخلف في المصفوفة
                for (int j = 0; j < bc; j++)
                {
                    if (block_arr[j] == succ)
                    {
                        changed |= bitset_union(bl->live_out,
                                                ctx->block_live[j].live_in, words);
                        break;
                    }
                }
            }

            // live_in = use ∪ (live_out - def)
            bitset_diff(temp, bl->live_out, bl->def, words);
            bitset_union(temp, bl->use, words);

            // التحقق من التغيير
            for (int w = 0; w < words; w++)
            {
                if (bl->live_in[w] != temp[w])
                {
                    changed = true;
                    bl->live_in[w] = temp[w];
                }
            }
        }
    }

    free(temp);
    free(block_arr);
}

// ============================================================================
// بناء فترات الحيوية (Build Live Intervals)
// ============================================================================

/**
 * @brief إضافة أو تحديث فترة حيوية لسجل افتراضي.
 */
static void update_interval(RegAllocCtx *ctx, int vreg, int pos)
{
    if (vreg < 0 || vreg >= ctx->max_vreg)
        return;

    // البحث عن فترة موجودة
    for (int i = 0; i < ctx->interval_count; i++)
    {
        if (ctx->intervals[i].vreg == vreg)
        {
            if (pos < ctx->intervals[i].start)
                ctx->intervals[i].start = pos;
            if (pos > ctx->intervals[i].end)
                ctx->intervals[i].end = pos;
            return;
        }
    }

    // إنشاء فترة جديدة
    if (ctx->interval_count >= ctx->interval_capacity)
    {
        int new_cap = ctx->interval_capacity ? ctx->interval_capacity * 2 : 32;
        LiveInterval *new_arr = realloc(ctx->intervals, new_cap * sizeof(LiveInterval));
        if (!new_arr)
            return;
        ctx->intervals = new_arr;
        ctx->interval_capacity = new_cap;
    }

    LiveInterval *li = &ctx->intervals[ctx->interval_count++];
    li->vreg = vreg;
    li->start = pos;
    li->end = pos;
    li->phys_reg = PHYS_NONE;
    li->spilled = false;
    li->spill_offset = 0;
}

void regalloc_build_intervals(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func)
        return;

    // المشي على كل تعليمة بترتيب تسلسلي
    int pos = 0;
    int block_idx = 0;
    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        // السجلات الحية عند دخول الكتلة تبدأ فتراتها هنا
        if (block_idx < ctx->block_count && ctx->block_live)
        {
            BlockLiveness *bl = &ctx->block_live[block_idx];
            for (int v = 0; v < ctx->max_vreg; v++)
            {
                if (bitset_test(bl->live_in, v))
                {
                    update_interval(ctx, v, pos);
                }
            }
        }

        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            // تسجيل الاستخدامات
            if (is_normal_vreg(&inst->src1))
                update_interval(ctx, inst->src1.data.vreg, pos);
            if (is_normal_vreg(&inst->src2))
                update_interval(ctx, inst->src2.data.vreg, pos);
            if (inst->src1.kind == MACH_OP_MEM && mem_base_vreg(&inst->src1) >= 0)
                update_interval(ctx, mem_base_vreg(&inst->src1), pos);
            if (inst->src2.kind == MACH_OP_MEM && mem_base_vreg(&inst->src2) >= 0)
                update_interval(ctx, mem_base_vreg(&inst->src2), pos);
            if (inst->dst.kind == MACH_OP_MEM && mem_base_vreg(&inst->dst) >= 0)
                update_interval(ctx, mem_base_vreg(&inst->dst), pos);

            // تسجيل التعريف
            if (is_normal_vreg(&inst->dst))
                update_interval(ctx, inst->dst.data.vreg, pos);

            pos++;
        }

        // السجلات الحية عند خروج الكتلة تمتد فتراتها إلى هنا
        if (block_idx < ctx->block_count && ctx->block_live)
        {
            BlockLiveness *bl = &ctx->block_live[block_idx];
            for (int v = 0; v < ctx->max_vreg; v++)
            {
                if (bitset_test(bl->live_out, v))
                {
                    update_interval(ctx, v, pos - 1);
                }
            }
        }

        block_idx++;
    }
}

// ============================================================================
// مقارنة فترات الحيوية للترتيب
// ============================================================================

/**
 * @brief مقارنة فترتي حيوية حسب نقطة البداية.
 */
static int interval_cmp_start(const void *a, const void *b)
{
    const LiveInterval *ia = (const LiveInterval *)a;
    const LiveInterval *ib = (const LiveInterval *)b;
    if (ia->start != ib->start)
        return ia->start - ib->start;
    return ia->vreg - ib->vreg;
}

/**
 * @brief هل الفترة تعبر موقع CALL؟
 * @param li مؤشر إلى فترة الحيوية
 * @param call_pos مصفوفة مواقع CALL
 * @param call_count عدد مواقع CALL
 * @return true إذا كانت الفترة تعبر CALL
 */
static bool interval_crosses_call(const LiveInterval *li, const int *call_pos, int call_count)
{
    if (!li || !call_pos || call_count == 0)
        return false;
    for (int c = 0; c < call_count; c++)
    {
        int p = call_pos[c];
        if (li->start < p && li->end > p)
            return true;
    }
    return false;
}

// ============================================================================
// خوارزمية المسح الخطي (Linear Scan Algorithm)
// ============================================================================

/**
 * @struct ActiveInterval
 * @brief عنصر في قائمة الفترات النشطة مرتبة حسب نقطة النهاية.
 */
typedef struct
{
    int interval_idx; // فهرس في مصفوفة الفترات
    PhysReg reg;      // السجل الفيزيائي المخصص
} ActiveEntry;

void regalloc_linear_scan(RegAllocCtx *ctx)
{
    if (!ctx || ctx->interval_count == 0)
        return;

    // ترتيب الفترات حسب نقطة البداية
    qsort(ctx->intervals, ctx->interval_count,
          sizeof(LiveInterval), interval_cmp_start);

    // قائمة الفترات النشطة
    ActiveEntry *active = malloc(ctx->interval_count * sizeof(ActiveEntry));
    if (!active)
        return;
    int active_count = 0;

    // بناء قائمة مواقع CALL (حسب ترتيب inst_map)
    int *call_pos = NULL;
    int call_count = 0;
    if (ctx->inst_map && ctx->total_insts > 0)
    {
        call_pos = malloc((size_t)ctx->total_insts * sizeof(int));
        if (call_pos)
        {
            for (int p = 0; p < ctx->total_insts; p++)
            {
                MachineInst *inst = ctx->inst_map[p];
                if (inst && inst->op == MACH_CALL)
                {
                    call_pos[call_count++] = p;
                }
            }
        }
    }

    // مصفوفة تتبع توفر السجلات الفيزيائية
    bool reg_free[PHYS_REG_COUNT];
    for (int i = 0; i < PHYS_REG_COUNT; i++)
        reg_free[i] = true;

    // RSP و RBP محجوزان دائماً
    reg_free[PHYS_RSP] = false;
    reg_free[PHYS_RBP] = false;

    // سجلات ABI الثابتة لا نخصصها لفترات عادية:
    // - RAX: قيمة الإرجاع
    // - RCX/RDX/R8/R9: معاملات الاستدعاء
    // لأننا نستخدم vregs سالبة لفرض هذه السجلات، ولا بد ألا تتعارض
    // مع تخصيص السجلات العادي.
    reg_free[PHYS_RAX] = false;
    reg_free[PHYS_RCX] = false;
    reg_free[PHYS_RDX] = false;
    reg_free[PHYS_R8] = false;
    reg_free[PHYS_R9] = false;

    // المسح الخطي
    for (int i = 0; i < ctx->interval_count; i++)
    {
        LiveInterval *cur = &ctx->intervals[i];
        bool cur_crosses_call = interval_crosses_call(cur, call_pos, call_count);

        // 1. انتهاء الفترات النشطة التي انتهت قبل نقطة بداية الفترة الحالية
        int new_active_count = 0;
        for (int j = 0; j < active_count; j++)
        {
            LiveInterval *act = &ctx->intervals[active[j].interval_idx];
            if (act->end < cur->start)
            {
                // الفترة انتهت: تحرير السجل
                reg_free[active[j].reg] = true;
            }
            else
            {
                // الفترة لا تزال نشطة
                active[new_active_count++] = active[j];
            }
        }
        active_count = new_active_count;

        // 2. محاولة تخصيص سجل فيزيائي
        PhysReg assigned = PHYS_NONE;

        // البحث عن سجل متاح بترتيب الأفضلية
        for (int k = 0; k < alloc_order_count; k++)
        {
            PhysReg r = alloc_order[k];

            // إذا كانت الفترة تعبر CALL، يجب أن تكون في سجل callee-saved
            // حتى لا تُدمّر قيمتها أثناء الاستدعاء.
            if (cur_crosses_call && phys_reg_is_caller_saved(r))
            {
                continue;
            }

            if (reg_free[r])
            {
                assigned = r;
                reg_free[r] = false;
                break;
            }
        }

        if (assigned != PHYS_NONE)
        {
            // نجح التخصيص
            cur->phys_reg = assigned;
            cur->spilled = false;

            // تسجيل استخدام سجل callee-saved
            if (phys_reg_is_callee_saved(assigned))
            {
                ctx->callee_saved_used[assigned] = true;
            }

            // إضافة إلى القائمة النشطة (مرتبة حسب نقطة النهاية)
            int insert_pos = active_count;
            for (int j = 0; j < active_count; j++)
            {
                if (ctx->intervals[active[j].interval_idx].end > cur->end)
                {
                    insert_pos = j;
                    break;
                }
            }
            // إزاحة العناصر
            for (int j = active_count; j > insert_pos; j--)
            {
                active[j] = active[j - 1];
            }
            active[insert_pos].interval_idx = i;
            active[insert_pos].reg = assigned;
            active_count++;
        }
        else
        {
            // 3. لا سجل متاح: تسريب
            // نسرّب الفترة الأطول (بما في ذلك الحالية)
            int spill_idx = i; // افتراضياً نسرّب الحالية
            int longest_end = cur->end;

            for (int j = 0; j < active_count; j++)
            {
                LiveInterval *act = &ctx->intervals[active[j].interval_idx];

                // إذا كانت الفترة الحالية تعبر CALL، نحتاج تحرير سجل callee-saved فقط.
                // تحرير سجل caller-saved (مثل r10/r11) لن يحل المشكلة.
                if (cur_crosses_call && phys_reg_is_caller_saved(active[j].reg))
                {
                    continue;
                }

                if (act->end > longest_end)
                {
                    longest_end = act->end;
                    spill_idx = active[j].interval_idx;
                }
            }

            if (spill_idx != i)
            {
                // تسريب فترة نشطة بدلاً من الحالية
                LiveInterval *to_spill = &ctx->intervals[spill_idx];
                PhysReg freed_reg = to_spill->phys_reg;

                // تسريب الفترة القديمة
                to_spill->spilled = true;
                to_spill->phys_reg = PHYS_NONE;
                ctx->next_spill_offset += 8;
                to_spill->spill_offset = -(int)ctx->next_spill_offset;
                ctx->spill_count++;

                // تحديث خرائط النتائج
                if (to_spill->vreg >= 0 && to_spill->vreg < ctx->max_vreg)
                {
                    ctx->vreg_spilled[to_spill->vreg] = true;
                    ctx->vreg_spill_offset[to_spill->vreg] = to_spill->spill_offset;
                    ctx->vreg_to_phys[to_spill->vreg] = PHYS_NONE;
                }

                // إزالته من القائمة النشطة
                int remove_pos = -1;
                for (int j = 0; j < active_count; j++)
                {
                    if (active[j].interval_idx == spill_idx)
                    {
                        remove_pos = j;
                        break;
                    }
                }
                if (remove_pos >= 0)
                {
                    for (int j = remove_pos; j < active_count - 1; j++)
                    {
                        active[j] = active[j + 1];
                    }
                    active_count--;
                }

                // تخصيص السجل المحرر للفترة الحالية
                cur->phys_reg = freed_reg;
                cur->spilled = false;

                if (phys_reg_is_callee_saved(freed_reg))
                {
                    ctx->callee_saved_used[freed_reg] = true;
                }

                // إضافة الحالية إلى القائمة النشطة
                int insert_pos = active_count;
                for (int j = 0; j < active_count; j++)
                {
                    if (ctx->intervals[active[j].interval_idx].end > cur->end)
                    {
                        insert_pos = j;
                        break;
                    }
                }
                for (int j = active_count; j > insert_pos; j--)
                {
                    active[j] = active[j - 1];
                }
                active[insert_pos].interval_idx = i;
                active[insert_pos].reg = freed_reg;
                active_count++;
            }
            else
            {
                // تسريب الفترة الحالية
                cur->spilled = true;
                cur->phys_reg = PHYS_NONE;
                ctx->next_spill_offset += 8;
                cur->spill_offset = -(int)ctx->next_spill_offset;
                ctx->spill_count++;
            }
        }

        // تحديث خرائط النتائج
        if (cur->vreg >= 0 && cur->vreg < ctx->max_vreg)
        {
            ctx->vreg_to_phys[cur->vreg] = cur->phys_reg;
            ctx->vreg_spilled[cur->vreg] = cur->spilled;
            if (cur->spilled)
            {
                ctx->vreg_spill_offset[cur->vreg] = cur->spill_offset;
            }
        }
    }

    // تحديث حجم المكدس
    ctx->func->stack_size = ctx->next_spill_offset;

    free(call_pos);
    free(active);
}

// ============================================================================
// تحليل السجلات الخاصة (Special VReg Resolution)
// ============================================================================

/**
 * @brief تحويل السجلات الخاصة (السالبة) إلى سجلات فيزيائية.
 *
 * الاتفاقيات:
 *   vreg -1  → RBP (مؤشر الإطار)
 *   vreg -2  → RAX (القيمة المرجعة)
 *   vreg -10 → RCX (المعامل الأول)
 *   vreg -11 → RDX (المعامل الثاني)
 *   vreg -12 → R8  (المعامل الثالث)
 *   vreg -13 → R9  (المعامل الرابع)
 */
static PhysReg resolve_special_vreg(int vreg)
{
    switch (vreg)
    {
    case -1:
        return PHYS_RBP;
    case -2:
        return PHYS_RAX;
    case -10:
        return PHYS_RCX;
    case -11:
        return PHYS_RDX;
    case -12:
        return PHYS_R8;
    case -13:
        return PHYS_R9;
    default:
        return PHYS_NONE;
    }
}

// ============================================================================
// إعادة كتابة المعاملات (Operand Rewrite)
// ============================================================================

/**
 * @brief إعادة كتابة معامل واحد بسجل فيزيائي.
 *
 * يحوّل MACH_OP_VREG إلى سجل فيزيائي، أو إذا كان مسرّباً
 * يحوّله إلى معامل ذاكرة [RBP + offset].
 */
static void rewrite_operand(RegAllocCtx *ctx, MachineOperand *op)
{
    if (!op)
        return;

    if (op->kind == MACH_OP_VREG)
    {
        int vreg = op->data.vreg;

        // السجلات الخاصة (السالبة)
        if (vreg < 0)
        {
            PhysReg phys = resolve_special_vreg(vreg);
            if (phys != PHYS_NONE)
            {
                op->data.vreg = phys;
                // نبقي kind كـ MACH_OP_VREG لكن الرقم الآن هو السجل الفيزيائي
                // سيتم تفسيره كسجل فيزيائي في مرحلة إصدار الكود
            }
            return;
        }

        // السجلات العادية
        if (vreg >= 0 && vreg < ctx->max_vreg)
        {
            if (ctx->vreg_spilled[vreg])
            {
                // السجل مسرّب: تحويل إلى معامل ذاكرة
                op->kind = MACH_OP_MEM;
                op->data.mem.base_vreg = PHYS_RBP;
                op->data.mem.offset = ctx->vreg_spill_offset[vreg];
            }
            else if (ctx->vreg_to_phys[vreg] != PHYS_NONE)
            {
                // السجل مخصص: استبدال الرقم
                op->data.vreg = ctx->vreg_to_phys[vreg];
            }
        }
    }
    else if (op->kind == MACH_OP_MEM)
    {
        // إعادة كتابة سجل القاعدة في معاملات الذاكرة
        int base = op->data.mem.base_vreg;
        if (base < 0)
        {
            PhysReg phys = resolve_special_vreg(base);
            if (phys != PHYS_NONE)
            {
                op->data.mem.base_vreg = phys;
            }
        }
        else if (base >= 0 && base < ctx->max_vreg)
        {
            if (ctx->vreg_to_phys[base] != PHYS_NONE)
            {
                op->data.mem.base_vreg = ctx->vreg_to_phys[base];
            }
        }
    }
}

void regalloc_rewrite(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func)
        return;

    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            rewrite_operand(ctx, &inst->dst);
            rewrite_operand(ctx, &inst->src1);
            rewrite_operand(ctx, &inst->src2);
        }
    }
}

// ============================================================================
// إدراج كود التسريب (Spill Code Insertion)
// ============================================================================

void regalloc_insert_spill_code(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func || ctx->spill_count == 0)
        return;

    // للسجلات المسرّبة، نحتاج لإدراج:
    // - store بعد كل تعريف (def)
    // - load قبل كل استخدام (use)
    //
    // ملاحظة: في التنفيذ الحالي، إعادة الكتابة (rewrite) تحوّل
    // معاملات السجلات المسرّبة إلى معاملات ذاكرة مباشرة [RBP + offset].
    // هذا يعمل لمعظم التعليمات في x86-64 لأنها تقبل معامل ذاكرة واحد.
    //
    // الحالات التي تحتاج معالجة خاصة (معاملان ذاكرة) ستُعالج
    // في مرحلة إصدار الكود (v0.3.2.3) بإدراج سجل مؤقت.

    // حالياً: إعادة الكتابة كافية لأن تحويل VREG → MEM يعمل مباشرة.
    // التسريب الفعلي يحدث ضمنياً عبر إعادة الكتابة.
}

// ============================================================================
// تشغيل تخصيص السجلات لدالة واحدة
// ============================================================================

bool regalloc_func(MachineFunc *func)
{
    if (!func || func->is_prototype)
        return true;
    if (func->block_count == 0)
        return true;

    // إنشاء السياق
    RegAllocCtx *ctx = regalloc_ctx_new(func);
    if (!ctx)
        return false;

    // 1. ترقيم التعليمات
    regalloc_number_insts(ctx);

    // 2. حساب def/use
    regalloc_compute_def_use(ctx);

    // 3. حساب الحيوية (live-in / live-out)
    regalloc_compute_liveness(ctx);

    // 4. بناء فترات الحيوية
    regalloc_build_intervals(ctx);

    // 5. المسح الخطي
    regalloc_linear_scan(ctx);

    // 6. إدراج كود التسريب
    regalloc_insert_spill_code(ctx);

    // 7. إعادة كتابة المعاملات
    regalloc_rewrite(ctx);

    // تحرير السياق
    regalloc_ctx_free(ctx);

    return true;
}

// ============================================================================
// تشغيل تخصيص السجلات على وحدة كاملة
// ============================================================================

bool regalloc_run(MachineModule *module)
{
    if (!module)
        return false;

    for (MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (!regalloc_func(func))
        {
            return false;
        }
    }

    return true;
}

// ============================================================================
// طباعة فترات الحيوية (Debug Printing)
// ============================================================================

void regalloc_print_intervals(RegAllocCtx *ctx, FILE *out)
{
    if (!ctx || !out)
        return;

    fprintf(out, "# فترات الحيوية (Live Intervals):\n");
    for (int i = 0; i < ctx->interval_count; i++)
    {
        LiveInterval *li = &ctx->intervals[i];
        fprintf(out, "#   vreg %d: [%d, %d]", li->vreg, li->start, li->end);
        if (li->spilled)
        {
            fprintf(out, " → spilled [rbp%+d]", li->spill_offset);
        }
        else if (li->phys_reg != PHYS_NONE)
        {
            fprintf(out, " → %s", phys_reg_name(li->phys_reg));
        }
        else
        {
            fprintf(out, " → unassigned");
        }
        fprintf(out, "\n");
    }
}

void regalloc_print_allocation(RegAllocCtx *ctx, FILE *out)
{
    if (!ctx || !out)
        return;

    fprintf(out, "# نتائج تخصيص السجلات (Register Allocation Results):\n");
    fprintf(out, "#   عدد السجلات الافتراضية: %d\n", ctx->max_vreg);
    fprintf(out, "#   عدد الفترات: %d\n", ctx->interval_count);
    fprintf(out, "#   عدد المسرّبة: %d\n", ctx->spill_count);
    fprintf(out, "#   حجم المكدس: %d\n", ctx->func->stack_size);

    for (int i = 0; i < ctx->max_vreg; i++)
    {
        if (ctx->vreg_to_phys[i] != PHYS_NONE)
        {
            fprintf(out, "#   v%d → %s\n", i, phys_reg_name(ctx->vreg_to_phys[i]));
        }
        else if (ctx->vreg_spilled[i])
        {
            fprintf(out, "#   v%d → spilled [rbp%+d]\n", i, ctx->vreg_spill_offset[i]);
        }
    }

    // طباعة السجلات المحفوظة المستخدمة
    fprintf(out, "#   السجلات المحفوظة المستخدمة:");
    bool first = true;
    for (int i = 0; i < PHYS_REG_COUNT; i++)
    {
        if (ctx->callee_saved_used[i])
        {
            fprintf(out, "%s %s", first ? "" : ",", phys_reg_name(i));
            first = false;
        }
    }
    fprintf(out, "%s\n", first ? " (لا شيء)" : "");
}
