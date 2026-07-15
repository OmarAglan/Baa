/**
 * @file emit_nazm_data.c
 * @brief Private Arabic global-data validation and writing for emit_nazm.c.
 *
 * This implementation fragment is intentionally included after the canonical
 * numeral and operand helpers. It is not a standalone translation unit.
 */

static int nazm_data_type_bits(const IRType *type)
{
    if (!type) return 0;
    switch (type->kind)
    {
        case IR_TYPE_I1:
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
        case IR_TYPE_CHAR:
        case IR_TYPE_F64:
        case IR_TYPE_PTR:
        case IR_TYPE_FUNC:
            return 64;
        default:
            return 0;
    }
}

static const char *nazm_data_directive(int bits)
{
    switch (bits)
    {
        case 8: return ".عدد٨";
        case 16: return ".عدد١٦";
        case 32: return ".عدد٣٢";
        case 64: return ".عدد٦٤";
        default: return NULL;
    }
}

static bool nazm_data_value_is_zero(const IRValue *value)
{
    return !value ||
           (value->kind == IR_VAL_CONST_INT && value->data.const_int == 0);
}

static BaaNazmEmitResult nazm_validate_data_symbol(const char *name,
                                                   const char *kind)
{
    if (!name || !name[0])
        return nazm_unsupported(kind, NULL, "اسم رمز البيانات مفقود.", NULL);
    if (nazm_identifier_has_ascii_letter(name) &&
        !nazm_arabic_abi_symbol(name) &&
        !nazm_is_generated_static_symbol(name))
        return nazm_unsupported("اسم_رمز_غير_عربي",
                                name,
                                "رموز البيانات في مصدر نظم وكائنه يجب أن تكون عربية فقط.",
                                NULL);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_data_value(const IRValue *value,
                                                  int bits)
{
    if (!value) return nazm_ok();
    if (value->kind == IR_VAL_CONST_INT)
    {
        if (!nazm_immediate_fits_width(value->data.const_int, bits))
            return nazm_unsupported("مدى_تهيئة_عامة",
                                    NULL,
                                    "قيمة تهيئة المتغير العام لا تدخل في عرض التخزين.",
                                    NULL);
        return nazm_ok();
    }
    if ((value->kind == IR_VAL_FUNC || value->kind == IR_VAL_GLOBAL) &&
        bits == 64)
        return nazm_validate_data_symbol(value->data.global_name,
                                         "مرجع_تهيئة_عامة");
    if ((value->kind == IR_VAL_CONST_STR || value->kind == IR_VAL_BAA_STR) &&
        bits == 64 && value->data.const_str.id >= 0)
        return nazm_ok();

    return nazm_unsupported("نوع_تهيئة_عامة",
                            NULL,
                            "نوع تهيئة المتغير العام غير مدعوم في مسار نظم.",
                            NULL);
}

static BaaNazmEmitResult nazm_validate_global(const IRGlobal *global)
{
    BaaNazmEmitResult name_result = nazm_validate_data_symbol(
        global ? global->name : NULL, "اسم_متغير_عام");
    if (name_result.status != BAA_NAZM_EMIT_OK) return name_result;
    if (!global->type)
        return nazm_unsupported("نوع_متغير_عام_مفقود",
                                NULL,
                                "نوع المتغير العام مفقود.",
                                NULL);
    if (global->is_extern) return nazm_ok();

    if (global->type->kind == IR_TYPE_ARRAY)
    {
        int bits = nazm_data_type_bits(global->type->data.array.element);
        int count = global->type->data.array.count;
        if (bits == 0 || count < 0 || global->init_elem_count < 0 ||
            global->init_elem_count > count)
            return nazm_unsupported("نوع_مصفوفة_عامة",
                                    NULL,
                                    "نوع المصفوفة العامة أو حجمها غير مدعوم.",
                                    NULL);
        for (int i = 0; i < global->init_elem_count; ++i)
        {
            BaaNazmEmitResult value_result = nazm_validate_data_value(
                global->init_elems ? global->init_elems[i] : NULL, bits);
            if (value_result.status != BAA_NAZM_EMIT_OK) return value_result;
        }
        return nazm_ok();
    }

    int bits = nazm_data_type_bits(global->type);
    if (bits == 0)
        return nazm_unsupported("نوع_متغير_عام",
                                NULL,
                                "نوع المتغير العام غير مدعوم في مسار نظم.",
                                NULL);
    return nazm_validate_data_value(global->init, bits);
}

static BaaNazmEmitResult nazm_validate_globals(const MachineModule *module)
{
    if (!module) return nazm_ok();
    int count = 0;
    for (const IRGlobal *global = module->globals; global; global = global->next)
    {
        BaaNazmEmitResult result = nazm_validate_global(global);
        if (result.status != BAA_NAZM_EMIT_OK) return result;
        count += 1;
    }
    if (count != module->global_count)
        return nazm_unsupported("عدد_متغيرات_عامة_غير_متسق",
                                NULL,
                                "عدد المتغيرات العامة لا يطابق قائمتها.",
                                NULL);
    return nazm_ok();
}

static BaaNazmEmitResult nazm_validate_string_tables(const MachineModule *module)
{
    if (!module) return nazm_ok();

    int string_count = 0;
    for (const IRStringEntry *string = module->strings;
         string;
         string = string->next)
    {
        if (string->id < 0 || !string->content)
            return nazm_unsupported("جدول_سلاسل_سي_غير_صالح",
                                    NULL,
                                    "جدول سلاسل سي يحتوي مدخلا غير صالح.",
                                    NULL);
        string_count += 1;
    }
    if (string_count != module->string_count)
        return nazm_unsupported("عدد_سلاسل_سي_غير_متسق",
                                NULL,
                                "عدد سلاسل سي لا يطابق قائمتها.",
                                NULL);

    int baa_string_count = 0;
    for (const IRBaaStringEntry *string = module->baa_strings;
         string;
         string = string->next)
    {
        if (string->id < 0 || !string->content)
            return nazm_unsupported("جدول_سلاسل_باء_غير_صالح",
                                    NULL,
                                    "جدول سلاسل باء يحتوي مدخلا غير صالح.",
                                    NULL);
        baa_string_count += 1;
    }
    if (baa_string_count != module->baa_string_count)
        return nazm_unsupported("عدد_سلاسل_باء_غير_متسق",
                                NULL,
                                "عدد سلاسل باء لا يطابق قائمتها.",
                                NULL);

    return nazm_ok();
}

static void nazm_write_data_value(FILE *out, const IRValue *value)
{
    if (!value || value->kind == IR_VAL_NONE)
    {
        fputs("٠", out);
        return;
    }
    if (value->kind == IR_VAL_CONST_INT)
    {
        nazm_write_signed(out, value->data.const_int);
        return;
    }
    if (value->kind == IR_VAL_CONST_STR || value->kind == IR_VAL_BAA_STR)
    {
        nazm_write_generated_string_label(
            out,
            value->kind == IR_VAL_BAA_STR,
            (uint64_t)value->data.const_str.id);
        return;
    }
    nazm_write_symbol(out, value->data.global_name);
}

static uint64_t nazm_pack_baa_character(const unsigned char **cursor)
{
    const unsigned char *p = *cursor;
    unsigned char first = p[0];
    int length = 0;
    if ((first & 0x80u) == 0x00u) length = 1;
    else if ((first & 0xe0u) == 0xc0u) length = 2;
    else if ((first & 0xf0u) == 0xe0u) length = 3;
    else if ((first & 0xf8u) == 0xf0u) length = 4;

    unsigned char bytes[4] = {0, 0, 0, 0};
    bool valid = length > 0;
    for (int i = 0; valid && i < length; ++i)
    {
        unsigned char byte = p[i];
        if (byte == 0 || (i > 0 && (byte & 0xc0u) != 0x80u))
            valid = false;
        else
            bytes[i] = byte;
    }

    if (!valid)
    {
        bytes[0] = 0xefu;
        bytes[1] = 0xbfu;
        bytes[2] = 0xbdu;
        length = 3;
        *cursor = p + 1;
    }
    else
    {
        *cursor = p + (size_t)length;
    }

    uint64_t bytes_field = (uint64_t)bytes[0] |
                           ((uint64_t)bytes[1] << 8) |
                           ((uint64_t)bytes[2] << 16) |
                           ((uint64_t)bytes[3] << 24);
    return bytes_field | ((uint64_t)(unsigned)length << 32);
}

static unsigned nazm_write_string_tables(FILE *out,
                                         const MachineModule *module)
{
    if (!module || (!module->strings && !module->baa_strings)) return 0;

    fputs(".بيانات_للقراءة\n", out);
    unsigned lines = 1;
    for (const IRStringEntry *string = module->strings;
         string;
         string = string->next)
    {
        nazm_write_generated_string_label(out, false, (uint64_t)string->id);
        fputs(":\n", out);
        lines += 1;
        for (const unsigned char *p = (const unsigned char *)string->content;
             *p;
             ++p)
        {
            fputs("    .عدد٨ ", out);
            nazm_write_unsigned(out, (uint64_t)*p);
            fputc('\n', out);
            lines += 1;
        }
        fputs("    .عدد٨ ٠\n", out);
        lines += 1;
    }

    for (const IRBaaStringEntry *string = module->baa_strings;
         string;
         string = string->next)
    {
        fputs("    .محاذاة ٨\n", out);
        nazm_write_generated_string_label(out, true, (uint64_t)string->id);
        fputs(":\n", out);
        lines += 2;
        const unsigned char *cursor = (const unsigned char *)string->content;
        while (*cursor)
        {
            uint64_t packed = nazm_pack_baa_character(&cursor);
            fputs("    .عدد٦٤ ", out);
            nazm_write_unsigned(out, packed);
            fputc('\n', out);
            lines += 1;
        }
        fputs("    .عدد٦٤ ٠\n", out);
        lines += 1;
    }
    return lines;
}

static unsigned nazm_write_global(FILE *out, const IRGlobal *global)
{
    fputs(global->is_internal ? ".محلي " : ".عام ", out);
    nazm_write_symbol(out, global->name);
    fputc('\n', out);
    unsigned lines = 1;

    if (global->type->kind == IR_TYPE_ARRAY)
    {
        int count = global->type->data.array.count;
        int bits = nazm_data_type_bits(global->type->data.array.element);
        bool all_zero = true;
        for (int i = 0; i < global->init_elem_count; ++i)
        {
            if (global->init_elems &&
                !nazm_data_value_is_zero(global->init_elems[i]))
            {
                all_zero = false;
                break;
            }
        }

        nazm_write_symbol(out, global->name);
        if (all_zero)
        {
            fputs(": .مساحة_صفرية ", out);
            nazm_write_unsigned(out,
                (uint64_t)(unsigned)count * (uint64_t)(unsigned)(bits / 8));
            fputc('\n', out);
            return lines + 1;
        }

        fputs(":\n", out);
        lines += 1;
        for (int i = 0; i < count; ++i)
        {
            const IRValue *value =
                global->init_elems && i < global->init_elem_count
                    ? global->init_elems[i]
                    : NULL;
            fputs("    ", out);
            fputs(nazm_data_directive(bits), out);
            fputc(' ', out);
            nazm_write_data_value(out, value);
            fputc('\n', out);
            lines += 1;
        }
        return lines;
    }

    int bits = nazm_data_type_bits(global->type);
    nazm_write_symbol(out, global->name);
    fputs(": ", out);
    fputs(nazm_data_directive(bits), out);
    fputc(' ', out);
    nazm_write_data_value(out, global->init);
    fputc('\n', out);
    return lines + 1;
}

static bool nazm_module_defines_global(const MachineModule *module,
                                       const char *name)
{
    if (!module || !name) return false;
    for (const IRGlobal *global = module->globals; global; global = global->next)
    {
        if (!global->is_extern && global->name &&
            strcmp(global->name, name) == 0)
            return true;
    }
    return false;
}

static unsigned nazm_write_globals(FILE *out, const MachineModule *module)
{
    unsigned lines = 0;
    bool has_storage = false;

    for (const IRGlobal *global = module->globals; global; global = global->next)
    {
        if (global->is_extern &&
            !nazm_module_defines_global(module, global->name))
        {
            fputs(".خارجي ", out);
            nazm_write_symbol(out, global->name);
            fputc('\n', out);
            lines += 1;
        }
        else
        {
            has_storage = true;
        }
    }

    if (!has_storage) return lines;
    fputs(".بيانات\n", out);
    lines += 1;
    for (const IRGlobal *global = module->globals; global; global = global->next)
    {
        if (!global->is_extern)
            lines += nazm_write_global(out, global);
    }
    return lines;
}
