/**
 * @file emit_nazm_names.c
 * @brief Private canonical Arabic register and instruction names.
 *
 * This implementation fragment is intentionally included by emit_nazm.c. It
 * keeps the closed Nazm spelling tables separate from validation and writing.
 */

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

static const char *const k_nazm_decimal_registers[16] = {
    "سجل_عشري_٠", "سجل_عشري_١", "سجل_عشري_٢", "سجل_عشري_٣",
    "سجل_عشري_٤", "سجل_عشري_٥", "سجل_عشري_٦", "سجل_عشري_٧",
    "سجل_عشري_٨", "سجل_عشري_٩", "سجل_عشري_١٠", "سجل_عشري_١١",
    "سجل_عشري_١٢", "سجل_عشري_١٣", "سجل_عشري_١٤", "سجل_عشري_١٥",
};

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
        case MACH_CPU_NOP: return "لا_تفعل";
        case MACH_RDTSC: return "قراءة_عداد_الزمن";
        case MACH_LABEL: return "وسم";
        case MACH_COMMENT: return "تعليق";
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
