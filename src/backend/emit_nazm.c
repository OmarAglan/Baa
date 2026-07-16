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
static BaaNazmEmitResult nazm_validate_string_tables(const MachineModule *module);
static unsigned nazm_write_globals(FILE *out, const MachineModule *module);
static unsigned nazm_write_string_tables(FILE *out, const MachineModule *module);
static void nazm_write_symbol(FILE *out, const char *name);
static void nazm_write_symbolic_memory_operand(FILE *out, const char *name);

#include "emit_nazm_names.c"

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

#include "emit_nazm_symbols.c"

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

static bool nazm_decimal_register_is_valid(const MachineOperand *operand)
{
    return operand && operand->kind == MACH_OP_XMM &&
           operand->data.xmm >= 0 && operand->data.xmm < 16 &&
           operand->size_bits == 64;
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
                                inst ? nazm_machine_op_arabic(inst->op) : NULL,
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

static BaaNazmEmitResult nazm_validate_decimal_operand(
    const MachineOperand *operand,
    const MachineInst *inst)
{
    if (!nazm_decimal_register_is_valid(operand))
        return nazm_unsupported("سجل_عشري_غير_صالح",
                                inst ? nazm_machine_op_arabic(inst->op) : NULL,
                                "المعامل ليس سجلا عشريا عربيا صالحا بعرض ٦٤ بت.",
                                inst);
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
    bool valid_width = operand &&
        ((operand->kind == MACH_OP_FUNC && operand->size_bits == 64) ||
         (operand->kind == MACH_OP_GLOBAL &&
          nazm_width_is_supported(operand->size_bits)));
    if (!operand ||
        (operand->kind != MACH_OP_FUNC && operand->kind != MACH_OP_GLOBAL) ||
        !valid_width || !operand->data.name ||
        !operand->data.name[0])
        return nazm_unsupported("مرجع_رمز_غير_صالح", NULL,
                                "مرجع الرمز ليس اسما عربيا صالحا بعرض مدعوم.", inst);
    if (nazm_identifier_has_ascii_letter(operand->data.name) &&
        !nazm_arabic_abi_symbol(operand->data.name) &&
        !nazm_is_generated_static_symbol(operand->data.name) &&
        !nazm_parse_generated_string_label(operand->data.name, NULL, NULL))
        return nazm_unsupported("اسم_رمز_غير_عربي", operand->data.name,
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
    return nazm_unsupported("نوع_معامل_قيمة_غير_مدعوم",
                            inst ? nazm_machine_op_arabic(inst->op) : NULL,
                            "نوع معامل القيمة غير مدعوم في مسار نظم.", inst);
}

static BaaNazmEmitResult nazm_validate_move(const MachineOperand *dst,
                                            const MachineOperand *src,
                                            const BaaTarget *target,
                                            const MachineInst *inst)
{
    if ((dst && dst->kind == MACH_OP_XMM) ||
        (src && src->kind == MACH_OP_XMM))
    {
        if (dst && src &&
            nazm_decimal_register_is_valid(dst) &&
            nazm_decimal_register_is_valid(src))
            return nazm_ok();

        const MachineOperand *decimal =
            dst && dst->kind == MACH_OP_XMM ? dst : src;
        const MachineOperand *storage = decimal == dst ? src : dst;
        BaaNazmEmitResult decimal_result =
            nazm_validate_decimal_operand(decimal, inst);
        if (decimal_result.status != BAA_NAZM_EMIT_OK)
            return decimal_result;

        BaaNazmEmitResult storage_result;
        if (storage && storage->kind == MACH_OP_VREG)
            storage_result = nazm_validate_operand(storage, target, inst);
        else
            storage_result = nazm_validate_memory_operand(storage, target, inst);
        if (storage_result.status != BAA_NAZM_EMIT_OK)
            return nazm_unsupported("نقل_عشري_غير_مدعوم",
                                    "نقل",
                                    "النقل العشري يتطلب سجلا عاما أو ذاكرة بعرض ٦٤ بت.",
                                    inst);
        if (nazm_operand_bits(storage) != 64)
            return nazm_unsupported("عرض_نقل_عشري",
                                    "نقل",
                                    "النقل بين السجل العشري والتخزين يتطلب عرض ٦٤ بت.",
                                    inst);
        return nazm_ok();
    }

    BaaNazmEmitResult result = dst && dst->kind == MACH_OP_GLOBAL
        ? nazm_validate_symbol_operand(dst, inst)
        : nazm_validate_value_operand(dst, target, inst, false);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    result = src && src->kind == MACH_OP_GLOBAL
        ? nazm_validate_symbol_operand(src, inst)
        : nazm_validate_value_operand(src, target, inst, true);
    if (result.status != BAA_NAZM_EMIT_OK) return result;

    int bits = nazm_operand_bits(dst);
    if (src->kind == MACH_OP_IMM)
    {
        if (!nazm_immediate_fits_width(src->data.imm, bits))
            return nazm_unsupported("مدى_قيمة_فورية", "نقل",
                                    "قيمة النقل الفورية لا تدخل في مدى عرض الوجهة.", inst);
    }
    else if (dst->kind != MACH_OP_GLOBAL &&
             src->kind != MACH_OP_GLOBAL &&
             bits != nazm_operand_bits(src))
    {
        return nazm_unsupported("عدم_تطابق_عرض_معاملين", "نقل",
                                "عرض معاملي النقل غير متطابق.", inst);
    }
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_decimal_binary(
    const MachineOperand *dst,
    const MachineOperand *src,
    const MachineInst *inst)
{
    BaaNazmEmitResult result = nazm_validate_decimal_operand(dst, inst);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    return nazm_validate_decimal_operand(src, inst);
}

static BaaNazmEmitResult nazm_validate_int_to_decimal(
    const MachineOperand *dst,
    const MachineOperand *src,
    const BaaTarget *target,
    const MachineInst *inst)
{
    BaaNazmEmitResult result = nazm_validate_decimal_operand(dst, inst);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    result = nazm_validate_operand(src, target, inst);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    int bits = nazm_operand_bits(src);
    if (bits != 32 && bits != 64)
        return nazm_unsupported("عرض_تحويل_صحيح_إلى_عشري",
                                NULL,
                                "التحويل إلى عشري يتطلب سجلا صحيحا بعرض ٣٢ أو ٦٤ بت.",
                                inst);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_decimal_to_int(
    const MachineOperand *dst,
    const MachineOperand *src,
    const BaaTarget *target,
    const MachineInst *inst)
{
    BaaNazmEmitResult result = nazm_validate_operand(dst, target, inst);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    int bits = nazm_operand_bits(dst);
    if (bits != 32 && bits != 64)
        return nazm_unsupported("عرض_تحويل_عشري_إلى_صحيح",
                                NULL,
                                "التحويل من عشري يتطلب سجلا صحيحا بعرض ٣٢ أو ٦٤ بت.",
                                inst);
    return nazm_validate_decimal_operand(src, inst);
}

static BaaNazmEmitResult nazm_validate_binary(const MachineOperand *dst,
                                              const MachineOperand *src,
                                              const BaaTarget *target,
                                              const MachineInst *inst,
                                              bool allow_memory)
{
    BaaNazmEmitResult result = nazm_validate_value_operand(
        dst, target, inst, false);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    if (src->kind == MACH_OP_MEM && !allow_memory)
        return nazm_unsupported("ذاكرة_غير_مدعومة_لهذه_التعليمة",
                                nazm_machine_op_arabic(inst->op),
                                "معامل الذاكرة غير مدعوم لهذه التعليمة.", inst);
    result = nazm_validate_value_operand(src, target, inst, true);
    if (result.status != BAA_NAZM_EMIT_OK) return result;
    int bits = nazm_operand_bits(dst);
    if (src->kind == MACH_OP_IMM)
    {
        bool fits = nazm_immediate_fits_width(src->data.imm, bits);
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
            BaaNazmEmitResult dst = nazm_validate_value_operand(
                &inst->dst, target, inst, false);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;
            if (nazm_operand_bits(&inst->dst) != 64)
                return nazm_unsupported("عرض_وجهة_حساب_عنوان",
                                        NULL,
                                        "وجهة حساب العنوان يجب أن تكون بعرض ٦٤ بت.",
                                        inst);
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
                &inst->src1, &inst->src2, target, inst, true);

        case MACH_IMUL:
        {
            BaaNazmEmitResult dst = nazm_validate_operand(
                &inst->dst, target, inst);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;
            return nazm_validate_binary(
                &inst->dst, &inst->src2, target, inst, true);
        }

        case MACH_SHL:
        case MACH_SHR:
        case MACH_SAR:
        {
            BaaNazmEmitResult dst = nazm_validate_value_operand(
                &inst->dst, target, inst, false);
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
            return nazm_validate_value_operand(
                &inst->dst, target, inst, false);

        case MACH_IDIV:
        case MACH_DIV:
            return nazm_validate_operand(&inst->src1, target, inst);

        case MACH_CQO:
            return nazm_ok();

        case MACH_ADDSD:
        case MACH_SUBSD:
        case MACH_MULSD:
        case MACH_DIVSD:
        case MACH_XORPD:
            return nazm_validate_decimal_binary(
                &inst->dst, &inst->src2, inst);

        case MACH_UCOMISD:
            return nazm_validate_decimal_binary(
                &inst->src1, &inst->src2, inst);

        case MACH_CVTSI2SD:
            return nazm_validate_int_to_decimal(
                &inst->dst, &inst->src1, target, inst);

        case MACH_CVTTSD2SI:
            return nazm_validate_decimal_to_int(
                &inst->dst, &inst->src1, target, inst);

        case MACH_SETE: case MACH_SETNE:
        case MACH_SETG: case MACH_SETL:
        case MACH_SETGE: case MACH_SETLE:
        case MACH_SETA: case MACH_SETB:
        case MACH_SETAE: case MACH_SETBE:
        case MACH_SETP: case MACH_SETNP:
            if (inst->dst.size_bits != 8)
                return nazm_unsupported("عرض_وجهة_تعيين_شرط", NULL,
                                        "وجهة تعيين الشرط يجب أن تكون ٨ بت.", inst);
            return nazm_validate_value_operand(
                &inst->dst, target, inst, false);

        case MACH_MOVZX:
        case MACH_MOVSX:
        {
            BaaNazmEmitResult dst = nazm_validate_value_operand(
                &inst->dst, target, inst, false);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;
            BaaNazmEmitResult src = nazm_validate_value_operand(
                &inst->src1, target, inst, false);
            if (src.status != BAA_NAZM_EMIT_OK) return src;
            int dst_bits = nazm_operand_bits(&inst->dst);
            int src_bits = nazm_operand_bits(&inst->src1);
            bool supported = (src_bits == 8 && dst_bits > src_bits) ||
                             (src_bits == 16 && (dst_bits == 32 || dst_bits == 64)) ||
                             (inst->op == MACH_MOVZX &&
                              src_bits == 32 && dst_bits == 64) ||
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
            if (target && target->cc &&
                target->cc->sysv_set_al_zero_on_call &&
                (inst->sysv_al < -1 || inst->sysv_al > 8))
                return nazm_unsupported(
                    "عدد_سجلات_عشرية_للنداء",
                    NULL,
                    "عدد سجلات المعاملات العشرية لنداء نظام في يجب أن يكون بين ٠ و٨.",
                    inst);
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

        case MACH_INLINE_ASM:
            return nazm_unsupported(
                "ترحيل_التجميع_الضمني",
                "مجمع_جاس_خام",
                "جملة 'مجمع' تحمل نص GAS خاما؛ مسار نظم يرفضها حتى انتقال المصدر إلى عقد 'نظم' العربي المهيكل.",
                inst);

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
    BaaNazmEmitResult string_validation = nazm_validate_string_tables(module);
    if (string_validation.status != BAA_NAZM_EMIT_OK)
        return string_validation;

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

static void nazm_write_generated_string_label(FILE *out,
                                              bool is_baa_string,
                                              uint64_t id)
{
    fputs(is_baa_string ? "سلسلة_باء_" : "سلسلة_سي_", out);
    nazm_write_unsigned(out, id);
}

static void nazm_write_symbol(FILE *out, const char *name)
{
    const char *arabic_abi = nazm_arabic_abi_symbol(name);
    if (arabic_abi)
    {
        fputs(arabic_abi, out);
        return;
    }

    if (nazm_is_generated_static_symbol(name))
    {
        static const char prefix[] = "__baa_static_";
        fputs("تخزين_ساكن_", out);
        for (const unsigned char *p =
                 (const unsigned char *)(name + sizeof(prefix) - 1u);
             *p;
             ++p)
        {
            if (*p >= (unsigned char)'0' && *p <= (unsigned char)'9')
                nazm_write_unsigned(out, (uint64_t)(*p - (unsigned char)'0'));
            else
                fputc((int)*p, out);
        }
        return;
    }

    bool is_baa_string = false;
    uint64_t id = 0;
    if (nazm_parse_generated_string_label(name, &is_baa_string, &id))
    {
        nazm_write_generated_string_label(out, is_baa_string, id);
        return;
    }
    fputs(name, out);
}

static unsigned nazm_write_arabic_abi_externals(FILE *out)
{
    unsigned lines = 0;
    for (size_t i = 0;
         i < sizeof(k_nazm_arabic_abi_symbols) /
                 sizeof(k_nazm_arabic_abi_symbols[0]);
         ++i)
    {
        const char *symbol = k_nazm_arabic_abi_symbols[i].arabic;
        bool seen = false;
        for (size_t previous = 0; previous < i; ++previous)
        {
            if (strcmp(symbol, k_nazm_arabic_abi_symbols[previous].arabic) == 0)
            {
                seen = true;
                break;
            }
        }
        if (seen) continue;
        fputs(".خارجي ", out);
        fputs(symbol, out);
        fputc('\n', out);
        lines += 1;
    }
    return lines;
}

static void nazm_write_operand(FILE *out, const MachineOperand *operand)
{
    if (operand->kind == MACH_OP_IMM)
    {
        nazm_write_signed(out, operand->data.imm);
        return;
    }

    if (operand->kind == MACH_OP_XMM)
    {
        fputs(k_nazm_decimal_registers[operand->data.xmm], out);
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

#include "emit_nazm_lowering.c"

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
    map.generated_line += nazm_write_string_tables(out, module);
    map.generated_line += nazm_write_globals(out, module);
    fputs(".نص\n", out);
    map.generated_line += 1;
    map.generated_line += nazm_write_arabic_abi_externals(out);

    for (const MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (func->is_prototype && func->name && func->name[0] &&
            !nazm_arabic_abi_symbol(func->name) &&
            !nazm_identifier_has_ascii_letter(func->name))
        {
            fputs(".خارجي ", out);
            nazm_write_symbol(out, func->name);
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
