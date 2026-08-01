static bool driver_is_utf8_continuation(unsigned char byte)
{
    return (byte & 0xC0u) == 0x80u;
}

static int driver_utf8_sequence_length(const unsigned char* text, size_t remaining)
{
    if (!text || remaining == 0u || text[0] == 0u) return 0;
    const unsigned char b0 = text[0];
    if ((b0 & 0x80u) == 0u) return 1;

    if ((b0 & 0xE0u) == 0xC0u)
    {
        if (remaining < 2u) return 0;
        const unsigned char b1 = text[1];
        if (b1 == 0u || !driver_is_utf8_continuation(b1)) return 0;
        uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        return cp >= 0x80u ? 2 : 0;
    }
    if ((b0 & 0xF0u) == 0xE0u)
    {
        if (remaining < 3u) return 0;
        const unsigned char b1 = text[1];
        const unsigned char b2 = text[2];
        if (b1 == 0u || b2 == 0u ||
            !driver_is_utf8_continuation(b1) || !driver_is_utf8_continuation(b2)) return 0;
        uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                      ((uint32_t)(b1 & 0x3Fu) << 6) |
                      (uint32_t)(b2 & 0x3Fu);
        return cp >= 0x800u && !(cp >= 0xD800u && cp <= 0xDFFFu) ? 3 : 0;
    }
    if ((b0 & 0xF8u) == 0xF0u)
    {
        if (remaining < 4u) return 0;
        const unsigned char b1 = text[1];
        const unsigned char b2 = text[2];
        const unsigned char b3 = text[3];
        if (b1 == 0u || b2 == 0u || b3 == 0u ||
            !driver_is_utf8_continuation(b1) ||
            !driver_is_utf8_continuation(b2) ||
            !driver_is_utf8_continuation(b3)) return 0;
        uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) |
                      ((uint32_t)(b1 & 0x3Fu) << 12) |
                      ((uint32_t)(b2 & 0x3Fu) << 6) |
                      (uint32_t)(b3 & 0x3Fu);
        return cp >= 0x10000u && cp <= 0x10FFFFu ? 4 : 0;
    }
    return 0;
}

static bool driver_validate_stdin_utf8(const char* source,
                                       const char* logical_path)
{
    if (!source) return false;

    int line = 1;
    int column = 1;
    size_t remaining = strlen(source);
    const unsigned char* current = (const unsigned char*)source;
    while (*current)
    {
        const int length = driver_utf8_sequence_length(current, remaining);
        if (length == 0)
        {
            Token token;
            memset(&token, 0, sizeof(token));
            token.type = TOKEN_INVALID;
            token.filename = logical_path ? logical_path : "unknown";
            token.line = line;
            token.col = column;
            token.length = 1;
            error_init(source);
            error_register_source(token.filename, source);
            error_report(token,
                         "خطأ لفظي: تسلسل UTF-8 غير صالح عند البايت 0x%02X.",
                         (unsigned int)*current);
            return false;
        }

        for (int i = 0; i < length; ++i)
        {
            if (*current == '\n')
            {
                ++line;
                column = 1;
            }
            else
            {
                ++column;
            }
            ++current;
            --remaining;
        }
    }
    return true;
}

// ============================================================================
// خط الأنابيب لكل ملف
// ============================================================================

typedef struct
{
    char* name;
    char* source;
    const char* kind;
} DriverOneDefinitionSymbol;

typedef struct
{
    DriverOneDefinitionSymbol* symbols;
    size_t count;
    size_t capacity;
} DriverOneDefinitionRegistry;

static void driver_odr_registry_init(DriverOneDefinitionRegistry* registry)
{
    if (!registry) return;
    registry->symbols = NULL;
    registry->count = 0;
    registry->capacity = 0;
}

static void driver_odr_registry_free(DriverOneDefinitionRegistry* registry)
{
    if (!registry) return;
    for (size_t i = 0; i < registry->count; ++i)
    {
        free(registry->symbols[i].name);
        free(registry->symbols[i].source);
    }
    free(registry->symbols);
    registry->symbols = NULL;
    registry->count = 0;
    registry->capacity = 0;
}

static bool driver_odr_registry_reserve(DriverOneDefinitionRegistry* registry)
{
    if (!registry) return false;
    if (registry->count < registry->capacity) return true;

    size_t new_capacity = registry->capacity ? registry->capacity * 2u : 32u;
    DriverOneDefinitionSymbol* grown =
        (DriverOneDefinitionSymbol*)realloc(registry->symbols,
                                            new_capacity * sizeof(*grown));
    if (!grown) return false;

    registry->symbols = grown;
    registry->capacity = new_capacity;
    return true;
}

static bool driver_odr_record_symbol(DriverOneDefinitionRegistry* registry,
                                     const char* name,
                                     const char* source,
                                     const char* kind)
{
    if (!registry || !name || !name[0]) return true;
    if (!source) source = "<unknown>";
    if (!kind) kind = "رمز";

    for (size_t i = 0; i < registry->count; ++i)
    {
        DriverOneDefinitionSymbol* prev = &registry->symbols[i];
        if (prev->name && strcmp(prev->name, name) == 0)
        {
            fprintf(stderr, "خطأ: تعريف متعدد للرمز العام '%s'.\n", name);
            fprintf(stderr, "  التعريف الأول: %s في %s\n",
                    prev->kind ? prev->kind : "رمز",
                    prev->source ? prev->source : "<unknown>");
            fprintf(stderr, "  التعريف الثاني: %s في %s\n", kind, source);
            fprintf(stderr,
                    "مساعدة: اجعل أحد التعريفين تصريحاً 'خارجي' في ملف .baahd، "
                    "أو استخدم 'ساكن' للرموز المحلية.\n");
            return false;
        }
    }

    if (!driver_odr_registry_reserve(registry))
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة أثناء فحص التعريفات العامة المتعددة.\n");
        return false;
    }

    DriverOneDefinitionSymbol* slot = &registry->symbols[registry->count++];
    slot->name = driver_strdup_alloc(name);
    slot->source = driver_strdup_alloc(source);
    slot->kind = kind;
    if (!slot->name || !slot->source)
    {
        fprintf(stderr, "خطأ: نفدت الذاكرة أثناء تسجيل تعريف عام.\n");
        return false;
    }
    return true;
}

static bool driver_record_exported_definitions(DriverOneDefinitionRegistry* registry,
                                               IRModule* module,
                                               const char* source)
{
    if (!registry || !module) return true;

    for (IRFunc* fn = module->funcs; fn; fn = fn->next)
    {
        if (!fn->is_prototype)
        {
            if (!driver_odr_record_symbol(registry, fn->name, source, "دالة"))
                return false;
        }
    }

    for (IRGlobal* global = module->globals; global; global = global->next)
    {
        if (!global->is_extern && !global->is_internal)
        {
            if (!driver_odr_record_symbol(registry, global->name, source, "متغير عام"))
                return false;
        }
    }

    return true;
}
