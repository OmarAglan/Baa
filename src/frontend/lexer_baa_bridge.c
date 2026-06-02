extern void محللباءهيئ(unsigned char* source);
extern void محللباءنظف(void);
extern int64_t محللباءالتالي_مؤشر(int64_t* type,
                                  int64_t* start,
                                  int64_t* length,
                                  int64_t* line,
                                  int64_t* column,
                                  int64_t* value_ptr,
                                  int64_t* value_length,
                                  int64_t* value_mode);
extern int64_t محللباءتشخيص(int64_t* code,
                            int64_t* line,
                            int64_t* column,
                            int64_t* length);
extern unsigned char* محللباءمصدر_حالي(void);

#define BAA_BRIDGE_VALUE_MODE_DIRECT 1
#define BAA_BRIDGE_VALUE_MODE_NUMBER 2
#define BAA_BRIDGE_DIAG_UNCLOSED_CONDITION 1
#define BAA_BRIDGE_DIAG_ELSE_WITHOUT_CONDITION 2
#define BAA_BRIDGE_DIAG_DUPLICATE_ELSE 3
#define BAA_BRIDGE_DIAG_END_WITHOUT_CONDITION 4
#define BAA_BRIDGE_DIAG_UNKNOWN_DIRECTIVE 5
#define BAA_BRIDGE_DIAG_INCLUDE_MISSING 6
#define BAA_BRIDGE_DIAG_BAD_STRING_ESCAPE 8
#define BAA_BRIDGE_DIAG_UNCLOSED_STRING 9
#define BAA_BRIDGE_DIAG_BAD_CHAR_ESCAPE 10
#define BAA_BRIDGE_DIAG_UNCLOSED_CHAR 11
#define BAA_BRIDGE_DIAG_INVALID_STRING_UTF8 12
#define BAA_BRIDGE_DIAG_INVALID_CHAR_UTF8 13
#define BAA_BRIDGE_DIAG_UNKNOWN_BYTE 14

static Lexer* lex_baa_active_lexer = NULL;

static const unsigned char* lex_baa_skip_utf8_bom_ptr(const char* source)
{
    if (!source) return NULL;

    const unsigned char* s = (const unsigned char*)source;
    if (s[0] == 0xEFu && s[1] == 0xBBu && s[2] == 0xBFu) {
        return s + 3;
    }
    return s;
}

static const unsigned char* lex_baa_source_start(const Lexer* l, int source_index)
{
    if (!l || source_index < 0 || (size_t)source_index >= l->baa_source_count) {
        return NULL;
    }
    return lex_baa_skip_utf8_bom_ptr(l->baa_sources[source_index].source);
}

static int lex_baa_register_source(Lexer* l,
                                   char* source,
                                   char* filename,
                                   int parent,
                                   bool owns_source,
                                   bool owns_filename)
{
    if (!l || !source || !filename || l->baa_source_count >= BAA_LEXER_BRIDGE_MAX_SOURCES) {
        return -1;
    }

    size_t index = l->baa_source_count;
    l->baa_sources[index].source = source;
    l->baa_sources[index].filename = filename;
    l->baa_sources[index].length = strlen(source);
    l->baa_sources[index].parent = parent;
    l->baa_sources[index].owns_source = owns_source;
    l->baa_sources[index].owns_filename = owns_filename;
    l->baa_source_count++;
    return (int)index;
}

static int lex_baa_find_source(const Lexer* l, const unsigned char* ptr)
{
    if (!l || !ptr) return -1;
    uintptr_t value = (uintptr_t)ptr;
    for (size_t i = 0; i < l->baa_source_count; ++i) {
        uintptr_t begin = (uintptr_t)l->baa_sources[i].source;
        uintptr_t end = begin + l->baa_sources[i].length + 1u;
        if (value >= begin && value < end) {
            return (int)i;
        }
    }
    return -1;
}

static void lex_baa_span_location(const Lexer* l,
                                  int source_index,
                                  const unsigned char* ptr,
                                  int* out_line,
                                  int* out_col)
{
    int line = 1;
    int col = 1;
    if (l && source_index >= 0 && (size_t)source_index < l->baa_source_count && ptr) {
        const unsigned char* p = lex_baa_source_start(l, source_index);
        const unsigned char* source = (const unsigned char*)l->baa_sources[source_index].source;
        if (!p || ptr < p) {
            p = source;
        }
        while (p < ptr && *p) {
            if (*p == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            p++;
        }
    }
    if (out_line) *out_line = line;
    if (out_col) *out_col = col;
}

static void lex_baa_set_c_location(Lexer* l, int source_index, int line, int col)
{
    if (!l) return;
    if (source_index >= 0 && (size_t)source_index < l->baa_source_count) {
        l->state.source = l->baa_sources[source_index].source;
        l->state.filename = l->baa_sources[source_index].filename;
    }
    l->state.line = (line > 0) ? line : 1;
    l->state.col = (col > 0) ? col : 1;
    l->state.cur_char = (char*)lex_baa_source_start(l, source_index);
    if (!l->state.cur_char) {
        l->state.cur_char = l->state.source;
    }
}

static void lex_baa_prepare_include_stack(Lexer* l, int current_index)
{
    if (!l) return;
    l->stack_depth = 0;
    int chain[10];
    int depth = 0;
    int index = current_index;

    while (index >= 0 && (size_t)index < l->baa_source_count && depth < 10) {
        chain[depth++] = index;
        index = l->baa_sources[index].parent;
    }

    for (int i = depth - 1; i > 0; --i) {
        int source_index = chain[i];
        l->stack[l->stack_depth].source = l->baa_sources[source_index].source;
        l->stack[l->stack_depth].cur_char = (char*)lex_baa_source_start(l, source_index);
        if (!l->stack[l->stack_depth].cur_char) {
            l->stack[l->stack_depth].cur_char = l->baa_sources[source_index].source;
        }
        l->stack[l->stack_depth].filename = l->baa_sources[source_index].filename;
        l->stack[l->stack_depth].line = 1;
        l->stack[l->stack_depth].col = 1;
        l->stack_depth++;
    }
}

static bool lex_baa_source_chain_contains(Lexer* l, int current_index, const char* resolved_path)
{
    int index = current_index;
    while (l && index >= 0 && (size_t)index < l->baa_source_count) {
        const char* filename = l->baa_sources[index].filename;
        if (filename && resolved_path && lex_paths_equivalent(l, filename, resolved_path)) {
            return true;
        }
        index = l->baa_sources[index].parent;
    }
    return false;
}

static char* lex_baa_dup_span(Lexer* l, const unsigned char* start, int64_t length)
{
    if (!l || !start || length < 0) return NULL;
    char* out = (char*)malloc((size_t)length + 1u);
    if (!out) {
        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ مدى من المحلل المكتوب بباء.");
    }
    if (length > 0) {
        memcpy(out, start, (size_t)length);
    }
    out[length] = '\0';
    return out;
}

static char* lex_baa_copy_value(Lexer* l,
                                const unsigned char* value,
                                int64_t length,
                                int64_t mode)
{
    char* out = lex_baa_dup_span(l, value, length);
    if (mode == BAA_BRIDGE_VALUE_MODE_NUMBER && out) {
        size_t write = 0u;
        for (int64_t read = 0; read < length;) {
            unsigned char byte = (unsigned char)value[read];
            if (byte == 0xD9u &&
                read + 1 < length &&
                value[read + 1] >= 0xA0u &&
                value[read + 1] <= 0xA9u) {
                out[write++] = (char)('0' + (value[read + 1] - 0xA0u));
                read += 2;
            } else {
                out[write++] = (char)byte;
                read++;
            }
        }
        out[write] = '\0';
    }
    return out;
}

static void lex_baa_append_decoded_byte(Lexer* l, char** out, size_t* len, size_t* cap, unsigned char byte)
{
    if (*len + 1u >= *cap) {
        size_t new_cap = (*cap == 0u) ? 32u : (*cap * 2u);
        char* new_out = (char*)realloc(*out, new_cap);
        if (!new_out) {
            free(*out);
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء قيمة وحدة من المحلل المكتوب بباء.");
        }
        *out = new_out;
        *cap = new_cap;
    }
    (*out)[(*len)++] = (char)byte;
}

static bool lex_baa_decode_escape(const unsigned char** cursor, unsigned char* out_byte)
{
    const unsigned char* p = *cursor;
    if (!p || !out_byte) return false;

    if (*p == '\\') {
        *out_byte = (unsigned char)'\\';
        *cursor = p + 1;
        return true;
    }
    if (*p == '"') {
        *out_byte = (unsigned char)'"';
        *cursor = p + 1;
        return true;
    }
    if (*p == '\'') {
        *out_byte = (unsigned char)'\'';
        *cursor = p + 1;
        return true;
    }
    if (p[0] == 0xD8u && p[1] == 0xB3u) {
        *out_byte = (unsigned char)'\n';
        *cursor = p + 2;
        return true;
    }
    if (p[0] == 0xD8u && p[1] == 0xAAu) {
        *out_byte = (unsigned char)'\t';
        *cursor = p + 2;
        return true;
    }
    if (p[0] == 0xD8u && p[1] == 0xB1u) {
        *out_byte = (unsigned char)'\r';
        *cursor = p + 2;
        return true;
    }
    if (p[0] == 0xD9u && p[1] == 0xA0u) {
        *out_byte = 0;
        *cursor = p + 2;
        return true;
    }

    return false;
}

static char* lex_baa_decode_string_literal(Lexer* l, const unsigned char* token_start, BaaTokenType type)
{
    const unsigned char* p = token_start;
    if (type == TOKEN_RAW_STRING) {
        p += strlen("خام");
    }
    if (!p || *p != '"') {
        return lex_baa_dup_span(l, (const unsigned char*)"", 0);
    }
    p++;

    size_t cap = 32u;
    size_t len = 0u;
    char* out = (char*)malloc(cap);
    if (!out) {
        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء قيمة نص من المحلل المكتوب بباء.");
    }

    while (*p && *p != '"') {
        if (*p == '\\') {
            unsigned char decoded = 0;
            p++;
            if (!lex_baa_decode_escape(&p, &decoded)) {
                free(out);
                lex_fatal(l, "خطأ لفظي: تسلسل هروب غير مدعوم داخل النص.");
            }
            lex_baa_append_decoded_byte(l, &out, &len, &cap, decoded);
            continue;
        }
        lex_baa_append_decoded_byte(l, &out, &len, &cap, *p);
        p++;
    }

    lex_baa_append_decoded_byte(l, &out, &len, &cap, 0);
    return out;
}

static char* lex_baa_decode_char_literal(Lexer* l, const unsigned char* token_start)
{
    const unsigned char* p = token_start ? token_start + 1 : NULL;
    unsigned char bytes[4];
    int length = 0;

    if (!p) {
        return lex_baa_dup_span(l, (const unsigned char*)"", 0);
    }

    if (*p == '\\') {
        p++;
        if (!lex_baa_decode_escape(&p, &bytes[0])) {
            lex_fatal(l, "خطأ لفظي: تسلسل هروب غير معروف داخل الحرف الحرفي.");
        }
        length = 1;
    } else {
        if (!lex_utf8_validate_at((const char*)p, &length)) {
            lex_fatal(l, "خطأ لفظي: UTF-8 غير صالح داخل الحرف الحرفي.");
        }
        for (int i = 0; i < length; ++i) {
            bytes[i] = p[i];
        }
    }

    char* out = (char*)malloc((size_t)length + 1u);
    if (!out) {
        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء إنشاء الحرف الحرفي.");
    }
    if (length > 0) {
        memcpy(out, bytes, (size_t)length);
    }
    out[length] = '\0';
    return out;
}

static const char* lex_baa_diag_message(int64_t code)
{
    switch (code) {
        case BAA_BRIDGE_DIAG_UNCLOSED_CONDITION:
            return "خطأ قبلي: نهاية الملف قبل إغلاق شرط قبلي (مفقود #نهاية).";
        case BAA_BRIDGE_DIAG_ELSE_WITHOUT_CONDITION:
            return "خطأ قبلي: #وإلا بدون شرط قبلي مطابق.";
        case BAA_BRIDGE_DIAG_DUPLICATE_ELSE:
            return "خطأ قبلي: تكرار #وإلا داخل نفس الكتلة.";
        case BAA_BRIDGE_DIAG_END_WITHOUT_CONDITION:
            return "خطأ قبلي: #نهاية بدون شرط قبلي مطابق.";
        case BAA_BRIDGE_DIAG_UNKNOWN_DIRECTIVE:
            return "خطأ قبلي: توجيه غير معروف.";
        case BAA_BRIDGE_DIAG_INCLUDE_MISSING:
            return "خطأ قبلي: تعذر فتح ملف التضمين.";
        case BAA_BRIDGE_DIAG_BAD_STRING_ESCAPE:
            return "خطأ لفظي: تسلسل هروب غير مدعوم داخل النص.";
        case BAA_BRIDGE_DIAG_UNCLOSED_STRING:
            return "خطأ لفظي: النص غير مُغلق.";
        case BAA_BRIDGE_DIAG_BAD_CHAR_ESCAPE:
            return "خطأ لفظي: تسلسل هروب غير معروف داخل الحرف الحرفي.";
        case BAA_BRIDGE_DIAG_UNCLOSED_CHAR:
            return "خطأ لفظي: متوقع إغلاق الحرف الحرفي بعلامة ' .";
        case BAA_BRIDGE_DIAG_INVALID_STRING_UTF8:
            return "خطأ لفظي: تسلسل UTF-8 غير صالح داخل النص.";
        case BAA_BRIDGE_DIAG_INVALID_CHAR_UTF8:
            return "خطأ لفظي: UTF-8 غير صالح داخل الحرف الحرفي.";
        case BAA_BRIDGE_DIAG_UNKNOWN_BYTE:
            return "خطأ لفظي: بايت غير معروف.";
        default:
            return "خطأ لفظي: وحدة غير صالحة من المحلل المكتوب بباء.";
    }
}

static void lex_baa_raise_diagnostic(Lexer* l, int64_t start)
{
    int64_t code = 0;
    int64_t line = 1;
    int64_t col = 1;
    int64_t length = 1;
    (void)length;
    (void)محللباءتشخيص(&code, &line, &col, &length);

    int source_index = lex_baa_find_source(l, محللباءمصدر_حالي());
    lex_baa_set_c_location(l, source_index, (int)line, (int)col);
    if (code == BAA_BRIDGE_DIAG_UNKNOWN_BYTE && start >= 0) {
        const unsigned char* source = محللباءمصدر_حالي();
        if (source && source_index >= 0 && (size_t)source_index < l->baa_source_count) {
            uintptr_t begin = (uintptr_t)l->baa_sources[source_index].source;
            uintptr_t end = begin + l->baa_sources[source_index].length;
            uintptr_t byte_addr = (uintptr_t)source + (uintptr_t)start;
            if (byte_addr >= begin && byte_addr < end) {
                lex_fatal(l, "خطأ لفظي: بايت غير معروف 0x%02X.",
                          *(const unsigned char*)byte_addr);
            }
        }
    }
    lex_fatal(l, "%s", lex_baa_diag_message(code));
}

void lex_baa_bridge_init(Lexer* l, char* src, const char* filename)
{
    if (!l || !src) return;
    l->baa_source_count = 0;
    char* root_name = lex_strdup_heap(l, filename ? filename : "<unknown>");
    (void)lex_baa_register_source(l, src, root_name, -1, false, true);
    lex_baa_active_lexer = l;
    محللباءهيئ((unsigned char*)lex_baa_skip_utf8_bom_ptr(src));
}

void lex_baa_bridge_cleanup(Lexer* l)
{
    if (!l) return;
    محللباءنظف();
    for (size_t i = 0; i < l->baa_source_count; ++i) {
        if (l->baa_sources[i].owns_source) {
            free(l->baa_sources[i].source);
        }
        if (l->baa_sources[i].owns_filename) {
            free(l->baa_sources[i].filename);
        }
        l->baa_sources[i].source = NULL;
        l->baa_sources[i].filename = NULL;
        l->baa_sources[i].length = 0u;
        l->baa_sources[i].parent = -1;
        l->baa_sources[i].owns_source = false;
        l->baa_sources[i].owns_filename = false;
    }
    l->baa_source_count = 0u;
    if (lex_baa_active_lexer == l) {
        lex_baa_active_lexer = NULL;
    }
}

unsigned char* محللباءمضيف_تضمين(unsigned char* start, int64_t length)
{
    Lexer* l = lex_baa_active_lexer;
    if (!l || !start || length < 0) return NULL;

    int current_index = lex_baa_find_source(l, start);
    int line = 1;
    int col = 1;
    lex_baa_span_location(l, current_index, start, &line, &col);
    int include_error_col = col;
    if (length >= 0 && length < (int64_t)INT_MAX - col - 1) {
        include_error_col = col + (int)length + 1;
    }
    lex_baa_set_c_location(l, current_index, line, include_error_col);
    lex_baa_prepare_include_stack(l, current_index);

    char* requested = lex_baa_dup_span(l, start, length);
    char* resolved_path = NULL;
    char* included = lex_resolve_and_read_include(l, requested, &resolved_path);
    if (!included || !resolved_path) {
        free(requested);
        free(resolved_path);
        free(included);
        lex_fatal(l, "خطأ قبلي: تعذر فتح ملف التضمين '%.*s'.", (int)length, start);
    }

    if (lex_baa_source_chain_contains(l, current_index, resolved_path)) {
        lex_fatal_include_cycle(l, requested, resolved_path);
    }

    int source_index = lex_baa_register_source(l, included, resolved_path, current_index, true, true);
    if (source_index < 0) {
        free(requested);
        free(included);
        free(resolved_path);
        lex_fatal(l, "خطأ قبلي: تم تجاوز عمق التضمين الأقصى (%u).", (unsigned)BAA_LEXER_BRIDGE_MAX_SOURCES);
    }

    lex_record_dependency(l, resolved_path);
    error_register_source(resolved_path, included);
    free(requested);
    return (unsigned char*)lex_baa_skip_utf8_bom_ptr(included);
}

Token lexer_next_token(Lexer* l)
{
    Token token;
    memset(&token, 0, sizeof(token));
    token.type = TOKEN_INVALID;
    token.filename = (l && l->state.filename) ? l->state.filename : "unknown";
    token.line = 1;
    token.col = 1;
    token.length = 1;

    if (!l) return token;

    int64_t type = TOKEN_INVALID;
    int64_t start = 0;
    int64_t length = 1;
    int64_t line = 1;
    int64_t col = 1;
    int64_t value_ptr = 0;
    int64_t value_length = 0;
    int64_t value_mode = 0;

    lex_baa_active_lexer = l;
    int64_t returned = محللباءالتالي_مؤشر(&type,
                                          &start,
                                          &length,
                                          &line,
                                          &col,
                                          &value_ptr,
                                          &value_length,
                                          &value_mode);
    if (returned != type || type == TOKEN_INVALID) {
        lex_baa_raise_diagnostic(l, start);
    }

    unsigned char* current_source = محللباءمصدر_حالي();
    int source_index = lex_baa_find_source(l, current_source);
    if (source_index >= 0) {
        token.filename = l->baa_sources[source_index].filename;
        token.value = NULL;
    }
    token.type = (BaaTokenType)type;
    token.line = (int)line;
    token.col = (int)col;
    token.length = (int)((length > 0) ? length : 1);

    const unsigned char* token_start = current_source ? current_source + start : NULL;
    const unsigned char* value = (const unsigned char*)(intptr_t)value_ptr;
    if (token.type == TOKEN_STRING || token.type == TOKEN_RAW_STRING || token.type == TOKEN_CHAR) {
        if (token_start &&
            (*token_start == '"' ||
             *token_start == '\'' ||
             (token.type == TOKEN_RAW_STRING && strncmp((const char*)token_start, "خام", strlen("خام")) == 0))) {
            if (token.type == TOKEN_CHAR) {
                token.value = lex_baa_decode_char_literal(l, token_start);
            } else {
                token.value = lex_baa_decode_string_literal(l, token_start, token.type);
            }
        } else if (value && value_length >= 0) {
            token.value = lex_baa_copy_value(l, value, value_length, value_mode);
        }
    } else if (value && value_length > 0 &&
               (token.type == TOKEN_IDENTIFIER || token.type == TOKEN_INT || token.type == TOKEN_FLOAT)) {
        token.value = lex_baa_copy_value(l, value, value_length, value_mode);
    }

    lex_baa_set_c_location(l, source_index, token.line, token.col + token.length);
    return lex_finish_token(l, token);
}
