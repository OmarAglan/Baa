/**
 * @file emit_nazm_symbols.c
 * @brief Private Arabic symbol mapping included by emit_nazm.c.
 *
 * This implementation fragment is intentionally not a standalone translation
 * unit. It owns the closed platform-to-Arabic ABI map and compiler-generated
 * symbol recognizers used by the canonical Nazm emitter.
 */

typedef struct
{
    const char *platform;
    const char *arabic;
} NazmArabicAbiSymbol;

static const NazmArabicAbiSymbol k_nazm_arabic_abi_symbols[] = {
    {"اطبع", "اطبع_منسقا"},
    {"اطبع_صحيح", "اطبع_منسقا"},
    {"printf", "اطبع_منسقا"},
    {"snprintf", "نسق_في_مخزن"},
    {"اقرأ", "اقرأ_منسقا"},
    {"اقرأ_صحيح", "اقرأ_منسقا"},
    {"scanf", "اقرأ_منسقا"},
    {"getchar", "اقرأ_محرف_سي"},
    {"puts", "اطبع_سطر_سي"},
    {"strlen", "طول_سلسلة_سي"},
    {"malloc", "ذاكرة_احجز"},
    {"calloc", "ذاكرة_احجز_مصفرة"},
    {"realloc", "ذاكرة_اعد_الحجز"},
    {"free", "ذاكرة_حرر"},
    {"memcpy", "ذاكرة_انسخ"},
    {"memset", "ذاكرة_املأ"},
    {"sqrt", "جذر_تربيعي_سي"},
    {"pow", "اس_سي"},
    {"sin", "جيب_سي"},
    {"cos", "جيب_تمام_سي"},
    {"tan", "ظل_سي"},
    {"llabs", "مطلق_صحيح_سي"},
    {"rand", "عشوائي_سي"},
    {"getenv", "متغير_بيئة_سي"},
    {"system", "نفذ_نظام_سي"},
    {"time", "وقت_حالي_سي"},
    {"ctime", "وقت_كنص_سي"},
    {"baa_fopen_utf8", "افتح_ملف_بترميز_موحد"},
    {"fclose", "اغلق_ملف_سي"},
    {"fgetc", "اقرأ_محرف_ملف_سي"},
    {"fputc", "اكتب_محرف_ملف_سي"},
    {"fread", "اقرأ_كتلة_ملف_سي"},
    {"fwrite", "اكتب_كتلة_ملف_سي"},
    {"feof", "نهاية_ملف_سي"},
    {"ftello", "موقع_ملف_سي"},
    {"_ftelli64", "موقع_ملف_سي"},
    {"fseeko", "اذهب_لموقع_ملف_سي"},
    {"_fseeki64", "اذهب_لموقع_ملف_سي"},
    {"fputs", "اكتب_سلسلة_ملف_سي"},
    {"exit", "انه_العملية_سي"},
    {"strerror", "نص_خطأ_النظام_سي"},
    {"__errno_location", "موقع_خطأ_النظام_سي"},
    {"_errno", "موقع_خطأ_النظام_سي"},
    {"baa_runtime_process_start", "وقت_تشغيل_ابدأ_عملية"},
    {"baa_runtime_process_poll", "وقت_تشغيل_حالة_عملية"},
    {"baa_runtime_process_wait", "وقت_تشغيل_انتظر_عملية"},
    {"baa_runtime_process_cancel", "وقت_تشغيل_الغ_عملية"},
    {"baa_runtime_process_exit_code", "وقت_تشغيل_كود_خروج_عملية"},
    {"baa_runtime_process_free", "وقت_تشغيل_حرر_عملية"},
    {"baa_runtime_make_dirs", "وقت_تشغيل_انشئ_مجلدات"},
    {"baa_runtime_remove_tree", "وقت_تشغيل_احذف_شجرة"},
    {"baa_runtime_sha256_file", "وقت_تشغيل_تجزئة_ملف"},
};

static const char *nazm_arabic_abi_symbol(const char *name)
{
    if (!name) return NULL;
    for (size_t i = 0;
         i < sizeof(k_nazm_arabic_abi_symbols) /
                 sizeof(k_nazm_arabic_abi_symbols[0]);
         ++i)
    {
        if (strcmp(name, k_nazm_arabic_abi_symbols[i].platform) == 0)
            return k_nazm_arabic_abi_symbols[i].arabic;
    }
    return NULL;
}

static bool nazm_is_generated_static_symbol(const char *name)
{
    static const char prefix[] = "__baa_static_";
    if (!name || strncmp(name, prefix, sizeof(prefix) - 1u) != 0)
        return false;
    const char *suffix = name + sizeof(prefix) - 1u;
    return suffix[0] != '\0' && !nazm_identifier_has_ascii_letter(suffix);
}

static bool nazm_parse_generated_string_label(const char *name,
                                              bool *is_baa_string,
                                              uint64_t *id)
{
    const char *digits = NULL;
    bool is_baa = false;
    if (name && strncmp(name, ".Lstr_", 6) == 0)
    {
        digits = name + 6;
    }
    else if (name && strncmp(name, ".Lbs_", 5) == 0)
    {
        digits = name + 5;
        is_baa = true;
    }
    if (!digits || !*digits) return false;

    uint64_t value = 0;
    for (const unsigned char *p = (const unsigned char *)digits; *p; ++p)
    {
        if (*p < (unsigned char)'0' || *p > (unsigned char)'9') return false;
        uint64_t digit = (uint64_t)(*p - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    if (is_baa_string) *is_baa_string = is_baa;
    if (id) *id = value;
    return true;
}

// بادئة تهريب أسماء المستخدم التي يحجزها نَظْم أو يولّدها هذا المصدر.
static const char k_nazm_escape_prefix[] = "رمز_باء_";

// أسماء السجلات القديمة التي يرفضها نَظْم 0.4 في موضع الرمز
// (تطابق LEGACY_REGISTERS في Nazm/src/lexer/lexer.c).
static const char *const k_nazm_legacy_registers[] = {
    "مجمع", "عداد", "بيانات", "قاعدة_ب", "مكدس", "قاعدة", "مصدر", "وجهة",
    "ر0", "ر1", "ر2", "ر3", "ر4", "ر5", "ر6", "ر7",
    "ر8", "ر9", "ر10", "ر11", "ر12", "ر13", "ر14", "ر15",
    "سجل_عام_8", "سجل_عام_9", "سجل_عام_10", "سجل_عام_11",
    "سجل_عام_12", "سجل_عام_13", "سجل_عام_14", "سجل_عام_15",
};

// بادئات الوسوم التي يولّدها المصدر نفسه؛ اسم مستخدم يبدأ بها قد يتكرر تعريفه.
static const char *const k_nazm_generated_prefixes[] = {
    "سلسلة_باء_", "سلسلة_سي_", "تخزين_ساكن_", "كتلة_", k_nazm_escape_prefix,
};

static bool nazm_name_in_table(const char *name,
                               const char *const *table,
                               size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (strcmp(name, table[i]) == 0) return true;
    }
    return false;
}

// هل يجب تهريب اسم المستخدم قبل كتابته في مصدر نظم؟
// التهريب بإضافة البادئة دالة متباينة: الاسم الذي يبدأ بالبادئة يُهرّب أيضاً،
// فلا يلتقي اسمان مختلفان في الاسم نفسه.
static bool nazm_symbol_needs_escape(const char *name)
{
    if (!name || !*name) return false;
    if (strcmp(name, "مؤشر_التعليمة") == 0) return true;
    if (nazm_name_in_table(name, k_nazm_registers, PHYS_REG_COUNT) ||
        nazm_name_in_table(name, k_nazm_registers_32, PHYS_REG_COUNT) ||
        nazm_name_in_table(name, k_nazm_registers_16, PHYS_REG_COUNT) ||
        nazm_name_in_table(name, k_nazm_registers_8, PHYS_REG_COUNT) ||
        nazm_name_in_table(name, k_nazm_decimal_registers, 16) ||
        nazm_name_in_table(name, k_nazm_legacy_registers,
                           sizeof(k_nazm_legacy_registers) /
                               sizeof(k_nazm_legacy_registers[0])))
        return true;
    for (size_t i = 0;
         i < sizeof(k_nazm_generated_prefixes) /
                 sizeof(k_nazm_generated_prefixes[0]);
         ++i)
    {
        const char *prefix = k_nazm_generated_prefixes[i];
        if (strncmp(name, prefix, strlen(prefix)) == 0) return true;
    }
    return false;
}

static bool nazm_operand_is_memory(const MachineOperand *operand)
{
    return operand &&
        (operand->kind == MACH_OP_MEM || operand->kind == MACH_OP_GLOBAL);
}

static void nazm_write_symbolic_memory_operand(FILE *out, const char *name)
{
    fputs("[مؤشر_التعليمة+", out);
    nazm_write_symbol(out, name);
    fputc(']', out);
}
