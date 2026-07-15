/**
 * @file emit_nazm.c
 * @brief أول شريحة تنفيذية لمُصدّر باء إلى مصدر نظم العربي.
 */

#include "emit_nazm.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "regalloc.h"

typedef struct
{
    FILE *out;
    bool first_entry;
    unsigned generated_line;
} NazmSourceMapWriter;

static BaaNazmEmitResult nazm_validate_globals(const MachineModule *module);
static unsigned nazm_write_globals(FILE *out, const MachineModule *module);

static const char *const k_nazm_registers[PHYS_REG_COUNT] = {
    "سجل_المركم",
    "سجل_العداد",
    "سجل_البيانات",
    "سجل_القاعدة",
    "مؤشر_المكدس",
    "مؤشر_القاعدة",
    "فهرس_المصدر",
    "فهرس_الوجهة",
    "سجل_عام_٨",
    "سجل_عام_٩",
    "سجل_عام_١٠",
    "سجل_عام_١١",
    "سجل_عام_١٢",
    "سجل_عام_١٣",
    "سجل_عام_١٤",
    "سجل_عام_١٥",
};

static const char *const k_nazm_registers_32[PHYS_REG_COUNT] = {
    "سجل_المركم_٣٢", "سجل_العداد_٣٢", "سجل_البيانات_٣٢", "سجل_القاعدة_٣٢",
    "مؤشر_المكدس_٣٢", "مؤشر_القاعدة_٣٢", "فهرس_المصدر_٣٢", "فهرس_الوجهة_٣٢",
    "سجل_عام_٨_٣٢", "سجل_عام_٩_٣٢", "سجل_عام_١٠_٣٢", "سجل_عام_١١_٣٢",
    "سجل_عام_١٢_٣٢", "سجل_عام_١٣_٣٢", "سجل_عام_١٤_٣٢", "سجل_عام_١٥_٣٢",
};

static const char *const k_nazm_registers_16[PHYS_REG_COUNT] = {
    "سجل_المركم_١٦", "سجل_العداد_١٦", "سجل_البيانات_١٦", "سجل_القاعدة_١٦",
    "مؤشر_المكدس_١٦", "مؤشر_القاعدة_١٦", "فهرس_المصدر_١٦", "فهرس_الوجهة_١٦",
    "سجل_عام_٨_١٦", "سجل_عام_٩_١٦", "سجل_عام_١٠_١٦", "سجل_عام_١١_١٦",
    "سجل_عام_١٢_١٦", "سجل_عام_١٣_١٦", "سجل_عام_١٤_١٦", "سجل_عام_١٥_١٦",
};

static const char *const k_nazm_registers_8[PHYS_REG_COUNT] = {
    "سجل_المركم_٨", "سجل_العداد_٨", "سجل_البيانات_٨", "سجل_القاعدة_٨",
    "مؤشر_المكدس_٨", "مؤشر_القاعدة_٨", "فهرس_المصدر_٨", "فهرس_الوجهة_٨",
    "سجل_عام_٨_٨", "سجل_عام_٩_٨", "سجل_عام_١٠_٨", "سجل_عام_١١_٨",
    "سجل_عام_١٢_٨", "سجل_عام_١٣_٨", "سجل_عام_١٤_٨", "سجل_عام_١٥_٨",
};

static BaaNazmEmitResult nazm_ok(void)
{
    BaaNazmEmitResult result = {0};
    result.status = BAA_NAZM_EMIT_OK;
    result.op = MACH_OP_COUNT;
    return result;
}

static BaaNazmEmitResult nazm_unsupported(const char *blocker_kind,
                                          const char *blocker_detail,
                                          const char *reason,
                                          const MachineInst *inst)
{
    BaaNazmEmitResult result = {0};
    result.status = BAA_NAZM_EMIT_UNSUPPORTED;
    result.op = inst ? inst->op : MACH_OP_COUNT;
    result.reason = reason;
    result.blocker_kind = blocker_kind;
    result.blocker_detail = blocker_detail;
    result.source_file = inst ? inst->src_file : NULL;
    result.source_line = inst ? inst->src_line : 0;
    result.source_col = inst ? inst->src_col : 0;
    return result;
}

static const char *nazm_machine_op_arabic(MachineOp op)
{
    switch (op)
    {
        case MACH_ADD: return "جمع";
        case MACH_SUB: return "طرح";
        case MACH_IMUL: return "ضرب_موقع";
        case MACH_SHL: return "إزاحة_يسار";
        case MACH_SHR: return "إزاحة_يمين_منطقية";
        case MACH_SAR: return "إزاحة_يمين_حسابية";
        case MACH_IDIV: return "قسمة_موقعة";
        case MACH_DIV: return "قسمة_غير_موقعة";
        case MACH_NEG: return "عكس_الإشارة";
        case MACH_CQO: return "توسيع_إشارة_القسمة";
        case MACH_ADDSD: return "جمع_عشري";
        case MACH_SUBSD: return "طرح_عشري";
        case MACH_MULSD: return "ضرب_عشري";
        case MACH_DIVSD: return "قسمة_عشرية";
        case MACH_UCOMISD: return "مقارنة_عشرية";
        case MACH_XORPD: return "خلاف_عشري";
        case MACH_CVTSI2SD: return "تحويل_صحيح_إلى_عشري";
        case MACH_CVTTSD2SI: return "تحويل_عشري_إلى_صحيح";
        case MACH_MOV: return "نقل";
        case MACH_LEA: return "حساب_عنوان";
        case MACH_LOAD: return "تحميل";
        case MACH_STORE: return "تخزين";
        case MACH_CMP: return "مقارنة";
        case MACH_TEST: return "اختبار_بتات";
        case MACH_SETE: return "تعيين_مساو";
        case MACH_SETNE: return "تعيين_غير_مساو";
        case MACH_SETG: return "تعيين_أكبر";
        case MACH_SETL: return "تعيين_أصغر";
        case MACH_SETGE: return "تعيين_أكبر_أو_مساو";
        case MACH_SETLE: return "تعيين_أصغر_أو_مساو";
        case MACH_SETA: return "تعيين_فوق";
        case MACH_SETB: return "تعيين_تحت";
        case MACH_SETAE: return "تعيين_فوق_أو_مساو";
        case MACH_SETBE: return "تعيين_تحت_أو_مساو";
        case MACH_SETP: return "تعيين_تكافؤ";
        case MACH_SETNP: return "تعيين_عدم_تكافؤ";
        case MACH_MOVZX: return "توسيع_بصفر";
        case MACH_MOVSX: return "توسيع_بإشارة";
        case MACH_AND: return "و_بتي";
        case MACH_OR: return "أو_بتي";
        case MACH_NOT: return "عكس_البتات";
        case MACH_XOR: return "خلاف_بتي";
        case MACH_JMP: return "قفز";
        case MACH_JE: return "قفز_مساو";
        case MACH_JNE: return "قفز_غير_مساو";
        case MACH_CALL: return "نداء";
        case MACH_TAILJMP: return "قفز_ذيلي";
        case MACH_RET: return "رجوع";
        case MACH_PUSH: return "دفع";
        case MACH_POP: return "سحب";
        case MACH_NOP: return "لا_عملية";
        case MACH_LABEL: return "وسم";
        case MACH_COMMENT: return "تعليق";
        case MACH_INLINE_ASM: return "نظم_ضمني";
        default: return "غير_معروفة";
    }
}

static const char *nazm_setcc_mnemonic(MachineOp op)
{
    switch (op)
    {
        case MACH_SETE: return "عين_مساو";
        case MACH_SETNE: return "عين_غير_مساو";
        case MACH_SETG: return "عين_أكبر";
        case MACH_SETL: return "عين_أصغر";
        case MACH_SETGE: return "عين_أكبر_أو_مساو";
        case MACH_SETLE: return "عين_أصغر_أو_مساو";
        case MACH_SETA: return "عين_فوق";
        case MACH_SETB: return "عين_تحت";
        case MACH_SETAE: return "عين_فوق_أو_مساو";
        case MACH_SETBE: return "عين_تحت_أو_مساو";
        case MACH_SETP: return "عين_تكافؤ";
        case MACH_SETNP: return "عين_عدم_تكافؤ";
        default: return NULL;
    }
}

static bool nazm_identifier_has_ascii_letter(const char *name)
{
    if (!name) return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
    {
        if ((*p >= (unsigned char)'A' && *p <= (unsigned char)'Z') ||
            (*p >= (unsigned char)'a' && *p <= (unsigned char)'z'))
            return true;
    }
    return false;
}

static bool nazm_width_is_supported(int bits)
{
    return bits == 0 || bits == 8 || bits == 16 || bits == 32 || bits == 64;
}

static const char *nazm_register_name(int reg, int bits)
{
    if (reg < 0 || reg >= PHYS_REG_COUNT) return NULL;
    if (bits == 0 || bits == 64) return k_nazm_registers[reg];
    if (bits == 32) return k_nazm_registers_32[reg];
    if (bits == 16) return k_nazm_registers_16[reg];
    if (bits == 8) return k_nazm_registers_8[reg];
    return NULL;
}

static int nazm_operand_bits(const MachineOperand *operand)
{
    return (!operand || operand->size_bits == 0) ? 64 : operand->size_bits;
}

static bool nazm_immediate_fits_width(int64_t value, int bits)
{
    if (bits == 64) return true;
    if (bits == 32) return value >= INT32_MIN && value <= (int64_t)UINT32_MAX;
    if (bits == 16) return value >= INT16_MIN && value <= (int64_t)UINT16_MAX;
    if (bits == 8) return value >= INT8_MIN && value <= (int64_t)UINT8_MAX;
    return false;
}

static bool nazm_register_is_valid(const MachineOperand *operand)
{
    return operand && operand->kind == MACH_OP_VREG &&
           operand->data.vreg >= 0 && operand->data.vreg < PHYS_REG_COUNT &&
           nazm_width_is_supported(operand->size_bits);
}

static bool nazm_physical_register_index_is_valid(int reg)
{
    return reg >= 0 && reg < PHYS_REG_COUNT;
}

static BaaNazmEmitResult nazm_validate_operand(const MachineOperand *operand,
                                               const BaaTarget *target,
                                               const MachineInst *inst)
{
    (void)target;
    if (!nazm_register_is_valid(operand))
        return nazm_unsupported("عرض_أو_نوع_معامل",
                                NULL,
                                "المعامل ليس سجلا ماديا بعرض ٨ أو ١٦ أو ٣٢ أو ٦٤ بت.",
                                inst);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_memory_operand(const MachineOperand *operand,
                                                      const BaaTarget *target,
                                                      const MachineInst *inst)
{
    if (!operand || operand->kind != MACH_OP_MEM ||
        !nazm_width_is_supported(operand->size_bits) ||
        !nazm_physical_register_index_is_valid(operand->data.mem.base_vreg))
        return nazm_unsupported("عنوان_ذاكرة_غير_مدعوم",
                                NULL,
                                "عنوان الذاكرة ليس قاعدة مادية بعرض ٦٤ بت مع إزاحة ثابتة.",
                                inst);

    (void)target;
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_label_operand(const MachineOperand *operand,
                                                     const MachineInst *inst)
{
    if (!operand || operand->kind != MACH_OP_LABEL || operand->data.label_id < 0)
        return nazm_unsupported("وسم_محلي_غير_صالح",
                                NULL,
                                "معامل القفز ليس وسم كتلة محليا صالحا.",
                                inst);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_symbol_operand(
    const MachineOperand *operand,
    const MachineInst *inst)
{
    if (!operand ||
        (operand->kind != MACH_OP_FUNC && operand->kind != MACH_OP_GLOBAL) ||
        operand->size_bits != 64 || !operand->data.name ||
        !operand->data.name[0])
        return nazm_unsupported("مرجع_دالة_غير_صالح", NULL,
                                "مرجع الدالة ليس اسما صالحا بعرض ٦٤ بت.", inst);
    if (nazm_identifier_has_ascii_letter(operand->data.name))
        return nazm_unsupported("اسم_رمز_غير_عربي", NULL,
                                "رموز الدوال في مصدر نظم وكائنه يجب أن تكون عربية فقط.",
                                inst);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_value_operand(const MachineOperand *operand,
                                                     const BaaTarget *target,
                                                     const MachineInst *inst,
                                                     bool allow_immediate)
{
    if (operand && operand->kind == MACH_OP_VREG)
        return nazm_validate_operand(operand, target, inst);
    if (operand && operand->kind == MACH_OP_MEM)
        return nazm_validate_memory_operand(operand, target, inst);
    if (allow_immediate && operand && operand->kind == MACH_OP_IMM &&
        nazm_width_is_supported(operand->size_bits))
        return nazm_ok();
    return nazm_unsupported("نوع_معامل_قيمة_غير_مدعوم", NULL,
                            "نوع معامل القيمة غير مدعوم في مسار نظم.", inst);
}

static BaaNazmEmitResult nazm_validate_move(const MachineOperand *dst,
                                            const MachineOperand *src,
                                            const BaaTarget *target,
                                            const MachineInst *inst)
{
    BaaNazmEmitResult result = nazm_validate_value_operand(dst, target, inst, false);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    result = nazm_validate_value_operand(src, target, inst, true);
    if (result.status != BAA_NAZM_EMIT_OK) return result;

    int bits = nazm_operand_bits(dst);
    if (src->kind == MACH_OP_IMM)
    {
        if (!nazm_immediate_fits_width(src->data.imm, bits))
            return nazm_unsupported("مدى_قيمة_فورية", "نقل",
                                    "قيمة النقل الفورية لا تدخل في مدى عرض الوجهة.", inst);
    }
    else if (bits != nazm_operand_bits(src))
    {
        return nazm_unsupported("عدم_تطابق_عرض_معاملين", "نقل",
                                "عرض معاملي النقل غير متطابق.", inst);
    }
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_binary(const MachineOperand *dst,
                                              const MachineOperand *src,
                                              const BaaTarget *target,
                                              const MachineInst *inst,
                                              bool allow_memory)
{
    BaaNazmEmitResult result = nazm_validate_operand(dst, target, inst);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    if (src->kind == MACH_OP_MEM && !allow_memory)
        return nazm_unsupported("ذاكرة_غير_مدعومة_لهذه_التعليمة", NULL,
                                "معامل الذاكرة غير مدعوم لهذه التعليمة.", inst);
    result = nazm_validate_value_operand(src, target, inst, true);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    int bits = nazm_operand_bits(dst);
    if (src->kind == MACH_OP_IMM)
    {
        bool fits = nazm_immediate_fits_width(src->data.imm, bits);
        if (bits == 64)
            fits = src->data.imm >= INT32_MIN && src->data.imm <= INT32_MAX;
        if (!fits)
            return nazm_unsupported("مدى_قيمة_فورية",
                                    nazm_machine_op_arabic(inst->op),
                                    "القيمة الفورية لا تدخل في مدى التعليمة.", inst);
    }
    else if (bits != nazm_operand_bits(src))
    {
        return nazm_unsupported("عدم_تطابق_عرض_معاملين",
                                nazm_machine_op_arabic(inst->op),
                                "عرض معاملي التعليمة غير متطابق.", inst);
    }
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_instruction(const MachineInst *inst,
                                                   const BaaTarget *target)
{
    if (!inst)
        return nazm_unsupported("تعليمة_آلة_مفقودة",
                                NULL,
                                "تعليمة آلة مفقودة.",
                                NULL);

    switch (inst->op)
    {
        case MACH_LABEL:
        case MACH_RET:
        case MACH_NOP:
        case MACH_COMMENT:
            return nazm_ok();

        case MACH_MOV:
        case MACH_LOAD:
        case MACH_STORE:
            return nazm_validate_move(&inst->dst, &inst->src1, target, inst);

        case MACH_LEA:
        {
            BaaNazmEmitResult dst = nazm_validate_operand(&inst->dst, target, inst);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;
            if (inst->src1.kind == MACH_OP_FUNC ||
                inst->src1.kind == MACH_OP_GLOBAL)
                return nazm_validate_symbol_operand(&inst->src1, inst);
            return nazm_validate_memory_operand(&inst->src1, target, inst);
        }

        case MACH_ADD:
        case MACH_SUB:
        case MACH_AND:
        case MACH_OR:
        case MACH_XOR:
            return nazm_validate_binary(
                &inst->dst, &inst->src2, target, inst, true);

        case MACH_CMP:
            return nazm_validate_binary(
                &inst->src1, &inst->src2, target, inst, true);

        case MACH_TEST:
            return nazm_validate_binary(
                &inst->src1, &inst->src2, target, inst, false);

        case MACH_IMUL:
            return nazm_validate_binary(
                &inst->dst, &inst->src2, target, inst, false);

        case MACH_SHL:
        case MACH_SHR:
        case MACH_SAR:
        {
            BaaNazmEmitResult dst = nazm_validate_operand(&inst->dst, target, inst);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;
            if (inst->src2.kind == MACH_OP_IMM &&
                inst->src2.data.imm >= 0 && inst->src2.data.imm <= UINT8_MAX)
                return nazm_ok();
            if (inst->src2.kind == MACH_OP_VREG &&
                inst->src2.data.vreg == PHYS_RCX)
                return nazm_ok();
            return nazm_unsupported("معامل_إزاحة_غير_مدعوم", NULL,
                                    "مقدار الإزاحة ليس قيمة ٨ بت أو سجل العداد.", inst);
        }

        case MACH_NEG:
        case MACH_NOT:
            return nazm_validate_operand(&inst->dst, target, inst);

        case MACH_IDIV:
        case MACH_DIV:
            return nazm_validate_operand(&inst->src1, target, inst);

        case MACH_CQO:
            return nazm_ok();

        case MACH_SETE: case MACH_SETNE:
        case MACH_SETG: case MACH_SETL:
        case MACH_SETGE: case MACH_SETLE:
        case MACH_SETA: case MACH_SETB:
        case MACH_SETAE: case MACH_SETBE:
        case MACH_SETP: case MACH_SETNP:
            if (inst->dst.size_bits != 8)
                return nazm_unsupported("عرض_وجهة_تعيين_شرط", NULL,
                                        "وجهة تعيين الشرط يجب أن تكون ٨ بت.", inst);
            return nazm_validate_operand(&inst->dst, target, inst);

        case MACH_MOVZX:
        case MACH_MOVSX:
        {
            BaaNazmEmitResult dst = nazm_validate_operand(&inst->dst, target, inst);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;
            BaaNazmEmitResult src = nazm_validate_operand(&inst->src1, target, inst);
            if (src.status != BAA_NAZM_EMIT_OK) return src;
            int dst_bits = nazm_operand_bits(&inst->dst);
            int src_bits = nazm_operand_bits(&inst->src1);
            bool supported = (src_bits == 8 && dst_bits > src_bits) ||
                             (src_bits == 16 && (dst_bits == 32 || dst_bits == 64)) ||
                             (inst->op == MACH_MOVSX && src_bits == 32 && dst_bits == 64);
            if (!supported)
                return nazm_unsupported("عرض_توسيع_غير_مدعوم", NULL,
                                        "عرضا المصدر والوجهة لا يشكلان توسيعا مدعوما.", inst);
            return nazm_ok();
        }

        case MACH_JMP:
            return nazm_validate_label_operand(&inst->dst, inst);

        case MACH_JE:
        case MACH_JNE:
            return nazm_validate_label_operand(&inst->dst, inst);

        case MACH_CALL:
            if (inst->src1.kind == MACH_OP_VREG &&
                nazm_operand_bits(&inst->src1) == 64)
                return nazm_validate_operand(&inst->src1, target, inst);
            if (inst->src1.kind == MACH_OP_FUNC)
                return nazm_validate_symbol_operand(&inst->src1, inst);
            return nazm_unsupported("هدف_نداء_غير_مدعوم", NULL,
                                    "هدف النداء ليس سجلا أو رمز دالة عربيا.", inst);

        case MACH_PUSH:
            if (nazm_operand_bits(&inst->src1) != 64)
                return nazm_unsupported("عرض_دفع", NULL,
                                        "الدفع يتطلب سجلا بعرض ٦٤ بت.", inst);
            return nazm_validate_operand(&inst->src1, target, inst);

        case MACH_POP:
            if (nazm_operand_bits(&inst->dst) != 64)
                return nazm_unsupported("عرض_سحب", NULL,
                                        "السحب يتطلب سجلا بعرض ٦٤ بت.", inst);
            return nazm_validate_operand(&inst->dst, target, inst);

        default:
            return nazm_unsupported("تعليمة_آلة",
                                    nazm_machine_op_arabic(inst->op),
                                    "تعليمة الآلة غير مدعومة في شريحة نظم التنفيذية الأولى.",
                                    inst);
    }
}

static BaaNazmEmitResult nazm_validate_module(const MachineModule *module,
                                              const BaaTarget *target)
{
    if (!module)
        return nazm_unsupported("وحدة_آلة_مفقودة", NULL, "وحدة الآلة مفقودة.", NULL);
    if (!target || !target->cc)
        return nazm_unsupported("عقد_هدف_مفقود",
                                NULL,
                                "وصف الهدف أو اتفاقية الاستدعاء مفقودة.",
                                NULL);
    BaaNazmEmitResult global_validation = nazm_validate_globals(module);
    if (global_validation.status != BAA_NAZM_EMIT_OK)
        return global_validation;
    if (module->string_count != 0 || module->strings != NULL ||
        module->baa_string_count != 0 || module->baa_strings != NULL)
        return nazm_unsupported("جداول_سلاسل",
                                NULL,
                                "جداول السلاسل غير مدعومة بعد في مسار نظم.",
                                NULL);

    for (const MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (func->is_prototype) continue;
        if (!func->name || !func->name[0])
            return nazm_unsupported("اسم_دالة_مفقود", NULL, "اسم الدالة مفقود.", NULL);
        if (nazm_identifier_has_ascii_letter(func->name))
            return nazm_unsupported("اسم_دالة_غير_عربي",
                                    NULL,
                                    "أسماء الدوال في مصدر نظم يجب أن تكون عربية فقط.",
                                    NULL);
        if (func->stack_size < 0)
            return nazm_unsupported("إطار_مكدس_غير_صالح",
                                    NULL,
                                    "حجم إطار المكدس غير صالح.",
                                    NULL);
        PhysReg callee_regs[PHYS_REG_COUNT];
        if (machine_func_collect_callee_saved(
                func, target, callee_regs, PHYS_REG_COUNT) < 0)
            return nazm_unsupported("حصر_سجلات_محفوظة_فاشل", NULL,
                                    "تعذر حصر السجلات المحفوظة للدالة.", NULL);

        for (const MachineBlock *block = func->blocks; block; block = block->next)
        {
            for (const MachineInst *inst = block->first; inst; inst = inst->next)
            {
                BaaNazmEmitResult result = nazm_validate_instruction(inst, target);
                if (result.status != BAA_NAZM_EMIT_OK) return result;
            }
        }
    }

    return nazm_ok();
}

static void nazm_write_unsigned(FILE *out, uint64_t value)
{
    static const char *const digits[10] = {
        "٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"
    };
    unsigned reversed[32];
    size_t count = 0;

    do
    {
        reversed[count++] = (unsigned)(value % 10u);
        value /= 10u;
    } while (value != 0u);

    while (count > 0)
        fputs(digits[reversed[--count]], out);
}

static void nazm_write_signed(FILE *out, int64_t value)
{
    uint64_t magnitude;
    if (value < 0)
    {
        fputc('-', out);
        magnitude = (uint64_t)(-(value + 1));
        magnitude += 1u;
    }
    else
    {
        magnitude = (uint64_t)value;
    }
    nazm_write_unsigned(out, magnitude);
}

static void nazm_write_operand(FILE *out, const MachineOperand *operand)
{
    if (operand->kind == MACH_OP_IMM)
    {
        nazm_write_signed(out, operand->data.imm);
        return;
    }

    fputs(nazm_register_name(operand->data.vreg, operand->size_bits), out);
}

static void nazm_write_memory_operand(FILE *out, const MachineOperand *operand)
{
    fputc('[', out);
    fputs(nazm_register_name(operand->data.mem.base_vreg, 64), out);
    if (operand->data.mem.offset > 0)
    {
        fputc('+', out);
        nazm_write_unsigned(out, (uint64_t)operand->data.mem.offset);
    }
    else if (operand->data.mem.offset < 0)
    {
        int64_t offset = operand->data.mem.offset;
        fputc('-', out);
        nazm_write_unsigned(out, (uint64_t)(-offset));
    }
    fputc(']', out);
}

static void nazm_write_any_operand(FILE *out, const MachineOperand *operand)
{
    if (operand->kind == MACH_OP_MEM)
        nazm_write_memory_operand(out, operand);
    else
        nazm_write_operand(out, operand);
}

static unsigned nazm_write_move(FILE *out,
                                const MachineOperand *dst,
                                const MachineOperand *src)
{
    if (dst->kind == MACH_OP_MEM && src->kind == MACH_OP_IMM)
    {
        MachineOperand scratch = {0};
        scratch.kind = MACH_OP_VREG;
        scratch.size_bits = nazm_operand_bits(dst);
        scratch.data.vreg = PHYS_R11;
        fputs("    انقل ", out);
        nazm_write_operand(out, &scratch);
        fputs("، ", out);
        nazm_write_operand(out, src);
        fputs("\n    انقل ", out);
        nazm_write_memory_operand(out, dst);
        fputs("، ", out);
        nazm_write_operand(out, &scratch);
        fputc('\n', out);
        return 2;
    }

    if (dst->kind == MACH_OP_MEM && src->kind == MACH_OP_MEM)
    {
        MachineOperand scratch = {0};
        scratch.kind = MACH_OP_VREG;
        scratch.size_bits = nazm_operand_bits(dst);
        scratch.data.vreg = PHYS_RAX;
        fputs("    انقل ", out);
        nazm_write_operand(out, &scratch);
        fputs("، ", out);
        nazm_write_memory_operand(out, src);
        fputs("\n    انقل ", out);
        nazm_write_memory_operand(out, dst);
        fputs("، ", out);
        nazm_write_operand(out, &scratch);
        fputc('\n', out);
        return 2;
    }

    fputs("    انقل ", out);
    nazm_write_any_operand(out, dst);
    fputs("، ", out);
    nazm_write_any_operand(out, src);
    fputc('\n', out);
    return 1;
}

static unsigned nazm_write_binary(FILE *out,
                                  const char *mnemonic,
                                  const MachineOperand *dst,
                                  const MachineOperand *src)
{
    fputs("    ", out);
    fputs(mnemonic, out);
    fputc(' ', out);
    nazm_write_any_operand(out, dst);
    fputs("، ", out);
    nazm_write_any_operand(out, src);
    fputc('\n', out);
    return 1;
}

static void nazm_write_local_label(FILE *out,
                                   unsigned function_id,
                                   int label_id)
{
    fputs("كتلة_", out);
    nazm_write_unsigned(out, function_id);
    fputc('_', out);
    nazm_write_unsigned(out, (uint64_t)label_id);
}

static unsigned nazm_write_source_span(FILE *out, const MachineInst *inst)
{
    if (!inst || inst->src_line <= 0) return 0;
    fputs("    ; موضع باء: السطر ", out);
    nazm_write_unsigned(out, (uint64_t)inst->src_line);
    if (inst->src_col > 0)
    {
        fputs("، العمود ", out);
        nazm_write_unsigned(out, (uint64_t)inst->src_col);
    }
    fputc('\n', out);
    return 1;
}

static void nazm_write_utf8_hex(FILE *out, const char *value)
{
    static const char digits[] = "0123456789abcdef";
    if (!out || !value) return;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p)
    {
        fputc(digits[*p >> 4], out);
        fputc(digits[*p & 0x0fu], out);
    }
}

static bool nazm_source_map_begin(NazmSourceMapWriter *map,
                                  FILE *out,
                                  const char *generated_path)
{
    if (!map) return false;
    map->out = out;
    map->first_entry = true;
    map->generated_line = 0;
    if (!out) return true;
    fputs("{\n  \"schema\": \"baa-nazm-source-map-v1\",\n", out);
    fputs("  \"generated_path_utf8_hex\": \"", out);
    nazm_write_utf8_hex(out, generated_path ? generated_path : "");
    fputs("\",\n  \"entries\": [\n", out);
    return !ferror(out);
}

static void nazm_source_map_entry(NazmSourceMapWriter *map,
                                  unsigned generated_start,
                                  unsigned generated_end,
                                  const MachineInst *inst)
{
    if (!map || !map->out || !inst || !inst->src_file ||
        inst->src_line <= 0 || generated_start == 0 ||
        generated_end < generated_start)
        return;
    fputs(map->first_entry ? "" : ",\n", map->out);
    map->first_entry = false;
    fprintf(map->out,
            "    {\"generated_line_start\": %u, \"generated_line_end\": %u, "
            "\"source_file_utf8_hex\": \"",
            generated_start,
            generated_end);
    nazm_write_utf8_hex(map->out, inst->src_file);
    fprintf(map->out,
            "\", \"source_line\": %d, \"source_column\": %d}",
            inst->src_line,
            inst->src_col > 0 ? inst->src_col : 1);
}

static bool nazm_source_map_end(NazmSourceMapWriter *map)
{
    if (!map || !map->out) return true;
    fputs("\n  ]\n}\n", map->out);
    return !ferror(map->out);
}

static int nazm_frame_size(const MachineFunc *func,
                           const BaaTarget *target,
                           int callee_count)
{
    int total = func->stack_size + target->cc->shadow_space_bytes + callee_count * 8;
    int align = target->cc->stack_align_bytes > 0 ? target->cc->stack_align_bytes : 16;
    if (total > 0 && total % align != 0)
        total = ((total / align) + 1) * align;
    return total;
}

static unsigned nazm_write_epilogue(FILE *out,
                                    bool is_arabic_entry,
                                    const MachineFunc *func,
                                    const BaaTarget *target,
                                    const PhysReg *callee_regs,
                                    int callee_count)
{
    for (int i = callee_count - 1; i >= 0; --i)
    {
        int offset = -(func->stack_size + target->cc->shadow_space_bytes + (i + 1) * 8);
        fputs("    انقل ", out);
        fputs(nazm_register_name(callee_regs[i], 64), out);
        fputs("، [مؤشر_القاعدة", out);
        nazm_write_signed(out, offset);
        fputs("]\n", out);
    }
    fputs("    انقل مؤشر_المكدس، مؤشر_القاعدة\n", out);
    fputs("    اسحب مؤشر_القاعدة\n", out);
    if (is_arabic_entry && target && target->obj_format == BAA_OBJFORMAT_ELF)
    {
        fputs("    انقل فهرس_الوجهة، سجل_المركم\n", out);
        fputs("    انقل سجل_المركم، ٦٠\n", out);
        fputs("    ناد_النظام\n", out);
        return (unsigned)(callee_count + 5);
    }
    fputs("    ارجع\n", out);
    return (unsigned)(callee_count + 3);
}

#include "emit_nazm_data.c"
#include "emit_nazm_function.c"

BaaNazmEmitResult emit_nazm_module_with_source_map(const MachineModule *module,
                                                   FILE *out,
                                                   FILE *source_map,
                                                   const char *generated_path,
                                                   const BaaTarget *target)
{
    if (!out)
    {
        BaaNazmEmitResult result = {0};
        result.status = BAA_NAZM_EMIT_IO_ERROR;
        result.op = MACH_OP_COUNT;
        result.reason = "ملف خرج نظم غير صالح.";
        return result;
    }

    BaaNazmEmitResult validation = nazm_validate_module(module, target);
    if (validation.status != BAA_NAZM_EMIT_OK) return validation;

    NazmSourceMapWriter map = {0};
    if (!nazm_source_map_begin(&map, source_map, generated_path))
    {
        BaaNazmEmitResult result = {0};
        result.status = BAA_NAZM_EMIT_IO_ERROR;
        result.op = MACH_OP_COUNT;
        result.reason = "فشلت كتابة خريطة مصدر نظم.";
        return result;
    }

    fputs("; مصدر نظم مولد من باء\n", out);
    map.generated_line = 1;
    map.generated_line += nazm_write_globals(out, module);
    fputs(".نص\n", out);
    map.generated_line += 1;

    for (const MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (func->is_prototype && func->name && func->name[0] &&
            !nazm_identifier_has_ascii_letter(func->name))
        {
            fputs(".خارجي ", out);
            fputs(func->name, out);
            fputc('\n', out);
            map.generated_line += 1;
        }
    }

    unsigned function_id = 0;
    for (const MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (!func->is_prototype)
            nazm_write_function(out, func, target, function_id++, &map);
    }

    if (!nazm_source_map_end(&map) || ferror(out))
    {
        BaaNazmEmitResult result = {0};
        result.status = BAA_NAZM_EMIT_IO_ERROR;
        result.op = MACH_OP_COUNT;
        result.reason = "فشلت كتابة مصدر نظم أو خريطته.";
        return result;
    }

    return nazm_ok();
}

BaaNazmEmitResult emit_nazm_module(const MachineModule *module,
                                   FILE *out,
                                   const BaaTarget *target)
{
    return emit_nazm_module_with_source_map(module, out, NULL, NULL, target);
}
