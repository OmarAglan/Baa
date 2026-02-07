/**
 * @file isel.h
 * @brief اختيار التعليمات - تحويل IR إلى تعليمات الآلة (v0.3.2.1)
 *
 * يعرّف هياكل بيانات التعليمات المنخفضة المستوى (Machine Instructions)
 * وواجهة اختيار التعليمات التي تحوّل IR SSA إلى تمثيل آلي مجرد
 * لمعمارية x86-64.
 *
 * المراحل:
 *   IR → اختيار التعليمات → تعليمات آلة (MachineInst)
 *   → تخصيص السجلات (v0.3.2.2) → إصدار الكود (v0.3.2.3)
 */

#ifndef BAA_ISEL_H
#define BAA_ISEL_H

#include <stdbool.h>
#include <stdint.h>
#include "ir.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// أكواد عمليات الآلة (Machine Opcodes)
// ============================================================================

/**
 * @enum MachineOp
 * @brief أكواد عمليات تعليمات x86-64 المدعومة.
 *
 * تمثل مجموعة فرعية من تعليمات x86-64 الكافية لتغطية جميع
 * عمليات IR الحالية.
 */
typedef enum {
    // --------------------------------------------------------------------
    // عمليات حسابية (Arithmetic)
    // --------------------------------------------------------------------
    MACH_ADD,       // add dst, src
    MACH_SUB,       // sub dst, src
    MACH_IMUL,      // imul dst, src
    MACH_IDIV,      // idiv src (يقسم RDX:RAX على src)
    MACH_NEG,       // neg dst (سالب)
    MACH_CQO,       // cqo - توسيع الإشارة RAX → RDX:RAX (لعملية القسمة)

    // --------------------------------------------------------------------
    // عمليات نقل البيانات (Data Movement)
    // --------------------------------------------------------------------
    MACH_MOV,       // mov dst, src
    MACH_LEA,       // lea dst, [mem] (تحميل العنوان الفعّال)
    MACH_LOAD,      // mov dst, [src] (تحميل من الذاكرة)
    MACH_STORE,     // mov [dst], src (تخزين في الذاكرة)

    // --------------------------------------------------------------------
    // عمليات المقارنة (Comparison)
    // --------------------------------------------------------------------
    MACH_CMP,       // cmp op1, op2
    MACH_TEST,      // test op1, op2
    MACH_SETE,      // sete dst  (يساوي)
    MACH_SETNE,     // setne dst (لا يساوي)
    MACH_SETG,      // setg dst  (أكبر - signed)
    MACH_SETL,      // setl dst  (أصغر - signed)
    MACH_SETGE,     // setge dst (أكبر أو يساوي)
    MACH_SETLE,     // setle dst (أصغر أو يساوي)
    MACH_MOVZX,     // movzx dst, src (توسيع بالأصفار)

    // --------------------------------------------------------------------
    // عمليات منطقية (Logical)
    // --------------------------------------------------------------------
    MACH_AND,       // and dst, src
    MACH_OR,        // or dst, src
    MACH_NOT,       // not dst
    MACH_XOR,       // xor dst, src

    // --------------------------------------------------------------------
    // عمليات التحكم (Control Flow)
    // --------------------------------------------------------------------
    MACH_JMP,       // jmp label (قفز غير مشروط)
    MACH_JE,        // je label  (قفز إذا يساوي)
    MACH_JNE,       // jne label (قفز إذا لا يساوي)
    MACH_CALL,      // call target (استدعاء دالة)
    MACH_RET,       // ret (رجوع من دالة)

    // --------------------------------------------------------------------
    // عمليات المكدس (Stack)
    // --------------------------------------------------------------------
    MACH_PUSH,      // push src
    MACH_POP,       // pop dst

    // --------------------------------------------------------------------
    // خاصة (Special)
    // --------------------------------------------------------------------
    MACH_NOP,       // nop (لا عملية)
    MACH_LABEL,     // تسمية (ليست تعليمة حقيقية)
    MACH_COMMENT,   // تعليق (للتوثيق في الخرج)

    MACH_OP_COUNT   // عدد أكواد العمليات
} MachineOp;

// ============================================================================
// أنواع المعاملات (Operand Types)
// ============================================================================

/**
 * @enum MachineOperandKind
 * @brief أنواع معاملات تعليمات الآلة.
 */
typedef enum {
    MACH_OP_NONE,       // لا معامل
    MACH_OP_VREG,       // سجل افتراضي (%م<n>)
    MACH_OP_IMM,        // قيمة فورية (ثابت)
    MACH_OP_MEM,        // عنوان ذاكرة [base + offset]
    MACH_OP_LABEL,      // تسمية كتلة (للقفز)
    MACH_OP_GLOBAL,     // متغير عام (@name)
    MACH_OP_FUNC,       // مرجع دالة (@name)
} MachineOperandKind;

/**
 * @struct MachineOperand
 * @brief يمثل معاملاً واحداً في تعليمة آلة.
 */
typedef struct MachineOperand {
    MachineOperandKind kind;
    int size_bits;              // حجم المعامل بالبتات (8, 16, 32, 64)
    union {
        int vreg;               // رقم السجل الافتراضي (MACH_OP_VREG)
        int64_t imm;            // قيمة فورية (MACH_OP_IMM)
        struct {
            int base_vreg;      // سجل القاعدة (MACH_OP_MEM)
            int32_t offset;     // الإزاحة
        } mem;
        int label_id;           // معرف الكتلة (MACH_OP_LABEL)
        char* name;             // اسم المتغير/الدالة (MACH_OP_GLOBAL, MACH_OP_FUNC)
    } data;
} MachineOperand;

// ============================================================================
// تعليمات الآلة (Machine Instructions)
// ============================================================================

/**
 * @struct MachineInst
 * @brief تمثل تعليمة آلة واحدة.
 *
 * التعليمات مخزنة كقائمة مترابطة مزدوجة داخل كتلة آلية.
 */
typedef struct MachineInst {
    MachineOp op;               // كود العملية
    MachineOperand dst;         // المعامل الوجهة
    MachineOperand src1;        // المعامل المصدر الأول
    MachineOperand src2;        // المعامل المصدر الثاني (اختياري)

    // معلومات تعقب المصدر
    int ir_reg;                 // سجل IR الأصلي (للربط مع IR)
    const char* comment;        // تعليق اختياري (لسهولة القراءة)

    // القائمة المترابطة المزدوجة
    struct MachineInst* prev;
    struct MachineInst* next;
} MachineInst;

// ============================================================================
// كتل الآلة (Machine Blocks)
// ============================================================================

/**
 * @struct MachineBlock
 * @brief تمثل كتلة أساسية في تمثيل الآلة.
 *
 * كل كتلة آلية تتوافق مع كتلة IR أساسية واحدة.
 */
typedef struct MachineBlock {
    char* label;                // تسمية الكتلة
    int id;                     // معرف رقمي

    // التعليمات (قائمة مترابطة مزدوجة)
    MachineInst* first;
    MachineInst* last;
    int inst_count;

    // الخلفاء والأسلاف
    struct MachineBlock* succs[2];
    int succ_count;

    // القائمة المترابطة للكتل في الدالة
    struct MachineBlock* next;
} MachineBlock;

// ============================================================================
// دوال الآلة (Machine Functions)
// ============================================================================

/**
 * @struct MachineFunc
 * @brief تمثل دالة في تمثيل الآلة.
 *
 * تحتوي على كتل آلية وتتبع حالة السجلات الافتراضية.
 */
typedef struct MachineFunc {
    char* name;                 // اسم الدالة
    bool is_prototype;          // هل هو نموذج أولي فقط؟

    // الكتل الآلية (قائمة مترابطة)
    MachineBlock* entry;        // كتلة الدخول
    MachineBlock* blocks;       // جميع الكتل
    int block_count;

    // عداد السجلات الافتراضية
    int next_vreg;              // السجل الافتراضي التالي المتاح

    // معلومات المكدس
    int stack_size;             // حجم المكدس المحلي (بالبايت)
    int param_count;            // عدد المعاملات

    // القائمة المترابطة للدوال في الوحدة
    struct MachineFunc* next;
} MachineFunc;

// ============================================================================
// وحدة الآلة (Machine Module)
// ============================================================================

/**
 * @struct MachineModule
 * @brief تمثل وحدة ترجمة كاملة في تمثيل الآلة.
 */
typedef struct MachineModule {
    char* name;                 // اسم الوحدة

    // الدوال
    MachineFunc* funcs;
    int func_count;

    // المتغيرات العامة (مرجع من IR)
    IRGlobal* globals;
    int global_count;

    // جدول النصوص (مرجع من IR)
    IRStringEntry* strings;
    int string_count;
} MachineModule;

// ============================================================================
// بناء المعاملات (Operand Construction)
// ============================================================================

/**
 * @brief إنشاء معامل سجل افتراضي.
 * @param vreg رقم السجل الافتراضي.
 * @param bits حجم المعامل بالبتات.
 * @return معامل آلة.
 */
MachineOperand mach_op_vreg(int vreg, int bits);

/**
 * @brief إنشاء معامل قيمة فورية.
 * @param imm القيمة الفورية.
 * @param bits حجم المعامل بالبتات.
 * @return معامل آلة.
 */
MachineOperand mach_op_imm(int64_t imm, int bits);

/**
 * @brief إنشاء معامل ذاكرة [base + offset].
 * @param base_vreg سجل القاعدة.
 * @param offset الإزاحة.
 * @param bits حجم المعامل بالبتات.
 * @return معامل آلة.
 */
MachineOperand mach_op_mem(int base_vreg, int32_t offset, int bits);

/**
 * @brief إنشاء معامل تسمية كتلة.
 * @param label_id معرف الكتلة.
 * @return معامل آلة.
 */
MachineOperand mach_op_label(int label_id);

/**
 * @brief إنشاء معامل متغير عام.
 * @param name اسم المتغير.
 * @return معامل آلة.
 */
MachineOperand mach_op_global(const char* name);

/**
 * @brief إنشاء معامل مرجع دالة.
 * @param name اسم الدالة.
 * @return معامل آلة.
 */
MachineOperand mach_op_func(const char* name);

/**
 * @brief إنشاء معامل فارغ (لا معامل).
 * @return معامل آلة فارغ.
 */
MachineOperand mach_op_none(void);

// ============================================================================
// بناء التعليمات (Instruction Construction)
// ============================================================================

/**
 * @brief إنشاء تعليمة آلة جديدة.
 * @param op كود العملية.
 * @param dst معامل الوجهة.
 * @param src1 المعامل المصدر الأول.
 * @param src2 المعامل المصدر الثاني.
 * @return مؤشر للتعليمة الجديدة.
 */
MachineInst* mach_inst_new(MachineOp op, MachineOperand dst,
                           MachineOperand src1, MachineOperand src2);

/**
 * @brief تحرير تعليمة آلة.
 * @param inst التعليمة المراد تحريرها.
 */
void mach_inst_free(MachineInst* inst);

// ============================================================================
// بناء الكتل (Block Construction)
// ============================================================================

/**
 * @brief إنشاء كتلة آلية جديدة.
 * @param label تسمية الكتلة.
 * @param id المعرف الرقمي.
 * @return مؤشر للكتلة الجديدة.
 */
MachineBlock* mach_block_new(const char* label, int id);

/**
 * @brief إضافة تعليمة إلى نهاية كتلة.
 * @param block الكتلة.
 * @param inst التعليمة.
 */
void mach_block_append(MachineBlock* block, MachineInst* inst);

/**
 * @brief تحرير كتلة آلية وجميع تعليماتها.
 * @param block الكتلة المراد تحريرها.
 */
void mach_block_free(MachineBlock* block);

// ============================================================================
// بناء الدوال (Function Construction)
// ============================================================================

/**
 * @brief إنشاء دالة آلية جديدة.
 * @param name اسم الدالة.
 * @return مؤشر للدالة الجديدة.
 */
MachineFunc* mach_func_new(const char* name);

/**
 * @brief تخصيص سجل افتراضي جديد.
 * @param func الدالة.
 * @return رقم السجل الافتراضي الجديد.
 */
int mach_func_alloc_vreg(MachineFunc* func);

/**
 * @brief إضافة كتلة إلى الدالة.
 * @param func الدالة.
 * @param block الكتلة.
 */
void mach_func_add_block(MachineFunc* func, MachineBlock* block);

/**
 * @brief تحرير دالة آلية وجميع كتلها.
 * @param func الدالة المراد تحريرها.
 */
void mach_func_free(MachineFunc* func);

// ============================================================================
// بناء الوحدة (Module Construction)
// ============================================================================

/**
 * @brief إنشاء وحدة آلية جديدة.
 * @param name اسم الوحدة.
 * @return مؤشر للوحدة الجديدة.
 */
MachineModule* mach_module_new(const char* name);

/**
 * @brief إضافة دالة إلى الوحدة.
 * @param module الوحدة.
 * @param func الدالة.
 */
void mach_module_add_func(MachineModule* module, MachineFunc* func);

/**
 * @brief تحرير وحدة آلية وجميع دوالها.
 * @param module الوحدة المراد تحريرها.
 */
void mach_module_free(MachineModule* module);

// ============================================================================
// اختيار التعليمات (Instruction Selection)
// ============================================================================

/**
 * @brief تحويل وحدة IR كاملة إلى تمثيل آلي.
 *
 * يمشي على كل دالة وكتلة وتعليمة في IR ويحولها إلى
 * تعليمات آلة x86-64 مكافئة مع:
 * - تضمين القيم الفورية حيثما أمكن
 * - اختيار أنماط التعليمات المثلى
 * - الحفاظ على اتفاقية الاستدعاء Windows x64 ABI
 *
 * @param ir_module وحدة IR المصدر (بعد التحسين).
 * @return وحدة آلية جديدة، أو NULL عند الفشل.
 */
MachineModule* isel_run(IRModule* ir_module);

// ============================================================================
// طباعة تمثيل الآلة (للتنقيح)
// ============================================================================

/**
 * @brief الحصول على الاسم النصي لكود عملية الآلة.
 * @param op كود العملية.
 * @return سلسلة نصية.
 */
const char* mach_op_to_string(MachineOp op);

/**
 * @brief طباعة معامل آلة إلى ملف.
 * @param op المعامل.
 * @param out ملف الخرج.
 */
void mach_operand_print(MachineOperand* op, FILE* out);

/**
 * @brief طباعة تعليمة آلة إلى ملف.
 * @param inst التعليمة.
 * @param out ملف الخرج.
 */
void mach_inst_print(MachineInst* inst, FILE* out);

/**
 * @brief طباعة كتلة آلية إلى ملف.
 * @param block الكتلة.
 * @param out ملف الخرج.
 */
void mach_block_print(MachineBlock* block, FILE* out);

/**
 * @brief طباعة دالة آلية إلى ملف.
 * @param func الدالة.
 * @param out ملف الخرج.
 */
void mach_func_print(MachineFunc* func, FILE* out);

/**
 * @brief طباعة وحدة آلية كاملة إلى ملف.
 * @param module الوحدة.
 * @param out ملف الخرج.
 */
void mach_module_print(MachineModule* module, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // BAA_ISEL_H
