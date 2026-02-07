/**
 * @file regalloc.h
 * @brief تخصيص السجلات - تحويل السجلات الافتراضية إلى سجلات فيزيائية (v0.3.2.2)
 *
 * يعرّف هياكل بيانات تخصيص السجلات وواجهة المخصص الذي يحوّل
 * السجلات الافتراضية في تعليمات الآلة إلى سجلات فيزيائية x86-64
 * باستخدام خوارزمية المسح الخطي (Linear Scan).
 *
 * المراحل:
 *   تعليمات آلة (MachineInst) → تحليل الحيوية (Liveness)
 *   → تخصيص السجلات (Linear Scan) → إدراج التسريب (Spill)
 *   → إعادة كتابة المعاملات (Rewrite)
 */

#ifndef BAA_REGALLOC_H
#define BAA_REGALLOC_H

#include <stdbool.h>
#include <stdint.h>
#include "isel.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// سجلات x86-64 الفيزيائية (Physical Registers)
// ============================================================================

/**
 * @enum PhysReg
 * @brief ترقيم السجلات الفيزيائية لمعمارية x86-64.
 *
 * يتبع ترتيب ترميز x86-64 القياسي.
 */
typedef enum {
    PHYS_RAX = 0,   // سجل المراكم / القيمة المرجعة
    PHYS_RCX = 1,   // المعامل الأول (Windows x64 ABI)
    PHYS_RDX = 2,   // المعامل الثاني / باقي القسمة
    PHYS_RBX = 3,   // سجل محفوظ (callee-saved)
    PHYS_RSP = 4,   // مؤشر المكدس (محجوز)
    PHYS_RBP = 5,   // مؤشر الإطار (محجوز)
    PHYS_RSI = 6,   // سجل عام
    PHYS_RDI = 7,   // سجل عام
    PHYS_R8  = 8,   // المعامل الثالث (Windows x64 ABI)
    PHYS_R9  = 9,   // المعامل الرابع (Windows x64 ABI)
    PHYS_R10 = 10,  // سجل مؤقت (caller-saved)
    PHYS_R11 = 11,  // سجل مؤقت (caller-saved)
    PHYS_R12 = 12,  // سجل محفوظ (callee-saved)
    PHYS_R13 = 13,  // سجل محفوظ (callee-saved)
    PHYS_R14 = 14,  // سجل محفوظ (callee-saved)
    PHYS_R15 = 15,  // سجل محفوظ (callee-saved)

    PHYS_REG_COUNT = 16,  // عدد السجلات الفيزيائية
    PHYS_NONE = -1        // لا سجل مخصص
} PhysReg;

/**
 * @brief الحصول على الاسم النصي لسجل فيزيائي.
 * @param reg رقم السجل الفيزيائي.
 * @return سلسلة نصية (مثل "rax", "rcx", ...).
 */
const char* phys_reg_name(PhysReg reg);

/**
 * @brief التحقق مما إذا كان السجل محفوظاً من قبل المستدعى (callee-saved).
 * @param reg رقم السجل.
 * @return صحيح إذا كان callee-saved.
 */
bool phys_reg_is_callee_saved(PhysReg reg);

// ============================================================================
// عدد السجلات المتاحة للتخصيص
// ============================================================================

/**
 * السجلات المتاحة للتخصيص العام (بعد استبعاد RSP و RBP):
 * RAX, RCX, RDX, RBX, RSI, RDI, R8-R15 = 14 سجلاً
 *
 * لكن بعض السجلات لها استخدامات خاصة:
 * - RAX: القيمة المرجعة + حاصل القسمة
 * - RCX, RDX, R8, R9: معاملات الدالة (Windows x64 ABI)
 * - RDX: باقي القسمة
 *
 * السجلات المتاحة بحرية: RBX, RSI, RDI, R10-R15 = 8 سجلات
 */
#define REGALLOC_NUM_ALLOCABLE 14  // عدد السجلات القابلة للتخصيص (كل شيء عدا RSP/RBP)

// ============================================================================
// فترة الحيوية (Live Interval)
// ============================================================================

/**
 * @struct LiveInterval
 * @brief تمثل فترة حيوية سجل افتراضي.
 *
 * الفترة تمتد من أول تعريف (def) إلى آخر استخدام (use)
 * ممثلة بأرقام تسلسلية لمواقع التعليمات.
 */
typedef struct LiveInterval {
    int vreg;           // رقم السجل الافتراضي
    int start;          // موقع البداية (أول تعريف)
    int end;            // موقع النهاية (آخر استخدام)

    // نتيجة التخصيص
    PhysReg phys_reg;   // السجل الفيزيائي المخصص (PHYS_NONE إذا سُرّب)
    bool spilled;       // هل تم تسريبه إلى المكدس؟
    int spill_offset;   // إزاحة التسريب في المكدس (بالنسبة لـ RBP)
} LiveInterval;

// ============================================================================
// مجموعات الحيوية لكل كتلة (Block Liveness Sets)
// ============================================================================

/**
 * @struct BlockLiveness
 * @brief مجموعات الحيوية لكتلة آلية واحدة.
 *
 * تحتوي على مجموعات def/use/live-in/live-out لكل كتلة.
 * المجموعات ممثلة كمصفوفات بتية (bitsets) للكفاءة.
 */
typedef struct BlockLiveness {
    int block_id;       // معرف الكتلة

    // مجموعات البتات (كل بت يمثل سجل افتراضي)
    uint64_t* def;      // السجلات المعرّفة في هذه الكتلة
    uint64_t* use;      // السجلات المستخدمة قبل تعريفها في هذه الكتلة
    uint64_t* live_in;  // السجلات الحية عند دخول الكتلة
    uint64_t* live_out; // السجلات الحية عند خروج الكتلة
} BlockLiveness;

// ============================================================================
// سياق تخصيص السجلات (Register Allocation Context)
// ============================================================================

/**
 * @struct RegAllocCtx
 * @brief سياق رئيسي لعملية تخصيص السجلات لدالة واحدة.
 */
typedef struct RegAllocCtx {
    MachineFunc* func;          // الدالة الآلية المعالَجة

    // ترقيم التعليمات
    int total_insts;            // عدد التعليمات الإجمالي
    MachineInst** inst_map;     // خريطة رقم → تعليمة

    // حيوية الكتل
    BlockLiveness* block_live;  // مصفوفة حيوية لكل كتلة
    int block_count;            // عدد الكتل

    // فترات الحيوية
    LiveInterval* intervals;    // مصفوفة فترات الحيوية
    int interval_count;         // عدد الفترات
    int interval_capacity;      // سعة المصفوفة

    // حجم مصفوفات البتات (بعدد الكلمات uint64_t)
    int bitset_words;           // عدد كلمات uint64_t في كل مجموعة بتات
    int max_vreg;               // أعلى رقم سجل افتراضي + 1

    // نتائج التخصيص
    PhysReg* vreg_to_phys;      // خريطة vreg → سجل فيزيائي
    bool* vreg_spilled;         // هل تم تسريب كل vreg؟
    int* vreg_spill_offset;     // إزاحة التسريب لكل vreg

    // معلومات التسريب
    int next_spill_offset;      // الإزاحة التالية المتاحة للتسريب
    int spill_count;            // عدد السجلات المسرّبة

    // السجلات المحفوظة (callee-saved) المستخدمة
    bool callee_saved_used[PHYS_REG_COUNT];
} RegAllocCtx;

// ============================================================================
// واجهة تخصيص السجلات (Register Allocation API)
// ============================================================================

/**
 * @brief تشغيل تخصيص السجلات على وحدة آلية كاملة.
 *
 * يعالج كل دالة في الوحدة:
 * 1. يرقّم التعليمات تسلسلياً
 * 2. يحسب تحليل الحيوية (def/use/live-in/live-out)
 * 3. يبني فترات الحيوية لكل سجل افتراضي
 * 4. يشغل خوارزمية المسح الخطي لتخصيص السجلات
 * 5. يدرج كود التسريب عند الحاجة
 * 6. يعيد كتابة كل معاملات VREG بسجلات فيزيائية
 *
 * @param module الوحدة الآلية (تُعدّل في المكان).
 * @return صحيح عند النجاح، خطأ عند الفشل.
 */
bool regalloc_run(MachineModule* module);

/**
 * @brief تشغيل تخصيص السجلات على دالة واحدة.
 *
 * @param func الدالة الآلية (تُعدّل في المكان).
 * @return صحيح عند النجاح، خطأ عند الفشل.
 */
bool regalloc_func(MachineFunc* func);

// ============================================================================
// دوال تحليل الحيوية (Liveness Analysis API)
// ============================================================================

/**
 * @brief إنشاء سياق تخصيص السجلات لدالة.
 * @param func الدالة الآلية.
 * @return سياق جديد، أو NULL عند الفشل.
 */
RegAllocCtx* regalloc_ctx_new(MachineFunc* func);

/**
 * @brief تحرير سياق تخصيص السجلات.
 * @param ctx السياق المراد تحريره.
 */
void regalloc_ctx_free(RegAllocCtx* ctx);

/**
 * @brief ترقيم التعليمات تسلسلياً وبناء خريطة التعليمات.
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_number_insts(RegAllocCtx* ctx);

/**
 * @brief حساب مجموعات def/use لكل كتلة.
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_compute_def_use(RegAllocCtx* ctx);

/**
 * @brief حساب مجموعات live-in/live-out بالتكرار حتى الثبات.
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_compute_liveness(RegAllocCtx* ctx);

/**
 * @brief بناء فترات الحيوية من مجموعات الحيوية.
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_build_intervals(RegAllocCtx* ctx);

// ============================================================================
// دوال التخصيص (Allocation API)
// ============================================================================

/**
 * @brief تشغيل خوارزمية المسح الخطي لتخصيص السجلات.
 *
 * ترتّب فترات الحيوية حسب نقطة البداية، ثم تمشي عليها
 * وتخصص سجلاً فيزيائياً لكل فترة. عند نفاد السجلات،
 * تسرّب الفترة الأطول.
 *
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_linear_scan(RegAllocCtx* ctx);

/**
 * @brief إدراج تعليمات التسريب (store بعد def، load قبل use).
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_insert_spill_code(RegAllocCtx* ctx);

/**
 * @brief إعادة كتابة كل معاملات VREG بسجلات فيزيائية.
 * @param ctx سياق تخصيص السجلات.
 */
void regalloc_rewrite(RegAllocCtx* ctx);

// ============================================================================
// دوال مساعدة للطباعة (Debug Printing)
// ============================================================================

/**
 * @brief طباعة فترات الحيوية لجميع السجلات الافتراضية.
 * @param ctx سياق تخصيص السجلات.
 * @param out ملف الخرج.
 */
void regalloc_print_intervals(RegAllocCtx* ctx, FILE* out);

/**
 * @brief طباعة نتائج التخصيص (vreg → phys reg).
 * @param ctx سياق تخصيص السجلات.
 * @param out ملف الخرج.
 */
void regalloc_print_allocation(RegAllocCtx* ctx, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // BAA_REGALLOC_H
