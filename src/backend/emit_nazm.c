/**
 * @file emit_nazm.c
 * @brief أول شريحة تنفيذية لمُصدّر باء إلى مصدر نظم العربي.
 */

#include "emit_nazm.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "regalloc.h"

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

static BaaNazmEmitResult nazm_ok(void)
{
    BaaNazmEmitResult result = {0};
    result.status = BAA_NAZM_EMIT_OK;
    result.op = MACH_OP_COUNT;
    return result;
}

static BaaNazmEmitResult nazm_unsupported(const char *reason,
                                          const MachineInst *inst)
{
    BaaNazmEmitResult result = {0};
    result.status = BAA_NAZM_EMIT_UNSUPPORTED;
    result.op = inst ? inst->op : MACH_OP_COUNT;
    result.reason = reason;
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

static bool nazm_width_is_64(int bits)
{
    return bits == 0 || bits == 64;
}

static bool nazm_register_is_valid(const MachineOperand *operand)
{
    return operand && operand->kind == MACH_OP_VREG &&
           operand->data.vreg >= 0 && operand->data.vreg < PHYS_REG_COUNT &&
           nazm_width_is_64(operand->size_bits);
}

static bool nazm_register_is_callee_saved(const MachineOperand *operand,
                                          const BaaTarget *target)
{
    if (!nazm_register_is_valid(operand) || !target || !target->cc) return false;
    return (target->cc->callee_saved_mask & (1u << (unsigned)operand->data.vreg)) != 0u;
}

static BaaNazmEmitResult nazm_validate_operand(const MachineOperand *operand,
                                               const BaaTarget *target,
                                               const MachineInst *inst)
{
    if (!nazm_register_is_valid(operand))
        return nazm_unsupported("تدعم الشريحة الأولى سجلات ٦٤ بت المادية فقط.", inst);
    if (nazm_register_is_callee_saved(operand, target))
        return nazm_unsupported("حفظ السجلات المحفوظة عبر الاستدعاء غير مدعوم بعد في مسار نظم.", inst);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_instruction(const MachineInst *inst,
                                                   const BaaTarget *target)
{
    if (!inst) return nazm_unsupported("تعليمة آلة مفقودة.", NULL);

    switch (inst->op)
    {
        case MACH_LABEL:
        case MACH_RET:
        case MACH_NOP:
        case MACH_COMMENT:
            return nazm_ok();

        case MACH_MOV:
        {
            BaaNazmEmitResult dst = nazm_validate_operand(&inst->dst, target, inst);
            if (dst.status != BAA_NAZM_EMIT_OK) return dst;

            if (inst->src1.kind == MACH_OP_IMM)
            {
                if (!nazm_width_is_64(inst->src1.size_bits))
                    return nazm_unsupported("القيمة الفورية ليست بعرض ٦٤ بت.", inst);
                return nazm_ok();
            }

            return nazm_validate_operand(&inst->src1, target, inst);
        }

        default:
            return nazm_unsupported("تعليمة الآلة غير مدعومة في شريحة نظم التنفيذية الأولى.", inst);
    }
}

static BaaNazmEmitResult nazm_validate_module(const MachineModule *module,
                                              const BaaTarget *target)
{
    if (!module) return nazm_unsupported("وحدة الآلة مفقودة.", NULL);
    if (!target || !target->cc)
        return nazm_unsupported("وصف الهدف أو اتفاقية الاستدعاء مفقودة.", NULL);
    if (module->global_count != 0 || module->globals != NULL)
        return nazm_unsupported("المتغيرات العامة غير مدعومة بعد في مسار نظم.", NULL);
    if (module->string_count != 0 || module->strings != NULL ||
        module->baa_string_count != 0 || module->baa_strings != NULL)
        return nazm_unsupported("جداول السلاسل غير مدعومة بعد في مسار نظم.", NULL);

    for (const MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (func->is_prototype) continue;
        if (!func->name || !func->name[0])
            return nazm_unsupported("اسم الدالة مفقود.", NULL);
        if (nazm_identifier_has_ascii_letter(func->name))
            return nazm_unsupported("أسماء الدوال في مصدر نظم يجب أن تكون عربية فقط.", NULL);
        if (func->stack_size < 0)
            return nazm_unsupported("حجم إطار المكدس غير صالح.", NULL);

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

    fputs(k_nazm_registers[operand->data.vreg], out);
}

static void nazm_write_source_span(FILE *out, const MachineInst *inst)
{
    if (!inst || inst->src_line <= 0) return;
    fputs("    ; موضع باء: السطر ", out);
    nazm_write_unsigned(out, (uint64_t)inst->src_line);
    if (inst->src_col > 0)
    {
        fputs("، العمود ", out);
        nazm_write_unsigned(out, (uint64_t)inst->src_col);
    }
    fputc('\n', out);
}

static int nazm_frame_size(const MachineFunc *func, const BaaTarget *target)
{
    int total = func->stack_size + target->cc->shadow_space_bytes;
    int align = target->cc->stack_align_bytes > 0 ? target->cc->stack_align_bytes : 16;
    if (total > 0 && total % align != 0)
        total = ((total / align) + 1) * align;
    return total;
}

static void nazm_write_epilogue(FILE *out,
                                bool is_arabic_entry,
                                const BaaTarget *target)
{
    fputs("    انقل مؤشر_المكدس، مؤشر_القاعدة\n", out);
    fputs("    اسحب مؤشر_القاعدة\n", out);
    if (is_arabic_entry && target && target->obj_format == BAA_OBJFORMAT_ELF)
    {
        fputs("    انقل فهرس_الوجهة، سجل_المركم\n", out);
        fputs("    انقل سجل_المركم، ٦٠\n", out);
        fputs("    ناد_النظام\n", out);
        return;
    }
    fputs("    ارجع\n", out);
}

static void nazm_write_function(FILE *out,
                                const MachineFunc *func,
                                const BaaTarget *target,
                                unsigned function_id)
{
    bool is_arabic_entry = strcmp(func->name, "الرئيسية") == 0;
    fputs("\n.عام ", out);
    fputs(func->name, out);
    fputc('\n', out);
    fputs(func->name, out);
    fputs(":\n", out);
    fputs("    ادفع مؤشر_القاعدة\n", out);
    fputs("    انقل مؤشر_القاعدة، مؤشر_المكدس\n", out);

    int frame_size = nazm_frame_size(func, target);
    if (frame_size > 0)
    {
        fputs("    اطرح مؤشر_المكدس، ", out);
        nazm_write_unsigned(out, (uint64_t)frame_size);
        fputc('\n', out);
    }

    bool has_return = false;
    for (const MachineBlock *block = func->blocks; block; block = block->next)
    {
        for (const MachineInst *inst = block->first; inst; inst = inst->next)
        {
            nazm_write_source_span(out, inst);
            switch (inst->op)
            {
                case MACH_LABEL:
                    fputs("كتلة_", out);
                    nazm_write_unsigned(out, function_id);
                    fputc('_', out);
                    nazm_write_unsigned(out, (uint64_t)inst->dst.data.label_id);
                    fputs(":\n", out);
                    break;

                case MACH_MOV:
                    fputs("    انقل ", out);
                    nazm_write_operand(out, &inst->dst);
                    fputs("، ", out);
                    nazm_write_operand(out, &inst->src1);
                    fputc('\n', out);
                    break;

                case MACH_RET:
                    nazm_write_epilogue(out, is_arabic_entry, target);
                    has_return = true;
                    break;

                case MACH_NOP:
                case MACH_COMMENT:
                    break;

                default:
                    break;
            }
        }
    }

    if (!has_return)
    {
        if (strcmp(func->name, "الرئيسية") == 0)
            fputs("    انقل سجل_المركم، ٠\n", out);
        nazm_write_epilogue(out, is_arabic_entry, target);
    }
}

BaaNazmEmitResult emit_nazm_module(const MachineModule *module,
                                   FILE *out,
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

    fputs("; مصدر نظم مولد من باء\n", out);
    fputs(".نص\n", out);

    unsigned function_id = 0;
    for (const MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (!func->is_prototype)
            nazm_write_function(out, func, target, function_id++);
    }

    if (ferror(out))
    {
        BaaNazmEmitResult result = {0};
        result.status = BAA_NAZM_EMIT_IO_ERROR;
        result.op = MACH_OP_COUNT;
        result.reason = "فشلت كتابة مصدر نظم.";
        return result;
    }

    return nazm_ok();
}
