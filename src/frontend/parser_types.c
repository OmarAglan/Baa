static bool parser_current_starts_type(void)
{
    if (is_type_keyword(parser.current.type)) return true;
    if (parser.current.type == TOKEN_IDENTIFIER &&
        parser.current.value &&
        strcmp(parser.current.value, "دالة") == 0) {
        return true;
    }
    if (parser.current.type == TOKEN_IDENTIFIER &&
        parser.current.value &&
        parser_type_alias_lookup(parser.current.value)) {
        return true;
    }
    return false;
}

static bool parser_current_is_type_alias_keyword(void)
{
    if (parser.current.type == TOKEN_TYPE_ALIAS) return true;
    return parser.current.type == TOKEN_IDENTIFIER &&
           parser.current.value &&
           strcmp(parser.current.value, "نوع") == 0;
}

static bool parser_is_decl_qualifier(BaaTokenType type)
{
    return type == TOKEN_CONST || type == TOKEN_STATIC;
}

typedef struct {
    bool is_const;
    bool is_static;
    Token tok_const;
    Token tok_static;
} ParserDeclQualifiers;

static void parser_parse_decl_qualifiers(ParserDeclQualifiers* out_q)
{
    if (!out_q) return;

    memset(out_q, 0, sizeof(*out_q));

    while (parser_is_decl_qualifier(parser.current.type)) {
        if (parser.current.type == TOKEN_CONST) {
            if (out_q->is_const) {
                error_report(parser.current, "تكرار 'ثابت' غير مسموح.");
            } else {
                out_q->is_const = true;
                out_q->tok_const = parser.current;
            }
            eat(TOKEN_CONST);
            continue;
        }

        if (parser.current.type == TOKEN_STATIC) {
            if (out_q->is_static) {
                error_report(parser.current, "تكرار 'ساكن' غير مسموح.");
            } else {
                out_q->is_static = true;
                out_q->tok_static = parser.current;
            }
            eat(TOKEN_STATIC);
            continue;
        }
    }
}

/**
 * @brief تحويل وحدة النوع إلى قيمة DataType.
 */
DataType token_to_datatype(BaaTokenType type) {
    if (type == TOKEN_KEYWORD_I8) return TYPE_I8;
    if (type == TOKEN_KEYWORD_I16) return TYPE_I16;
    if (type == TOKEN_KEYWORD_I32) return TYPE_I32;
    if (type == TOKEN_KEYWORD_I64) return TYPE_INT;
    if (type == TOKEN_KEYWORD_U8) return TYPE_U8;
    if (type == TOKEN_KEYWORD_U16) return TYPE_U16;
    if (type == TOKEN_KEYWORD_U32) return TYPE_U32;
    if (type == TOKEN_KEYWORD_U64) return TYPE_U64;
    if (type == TOKEN_KEYWORD_STRING) return TYPE_STRING;
    if (type == TOKEN_KEYWORD_BOOL) return TYPE_BOOL;
    if (type == TOKEN_KEYWORD_CHAR) return TYPE_CHAR;
    if (type == TOKEN_KEYWORD_FLOAT) return TYPE_FLOAT;
    if (type == TOKEN_KEYWORD_FLOAT32) return TYPE_FLOAT;
    if (type == TOKEN_KEYWORD_VOID) return TYPE_VOID;
    return TYPE_INT;
}

static bool parse_float_token_checked(Token tok, double* out)
{
    if (out) *out = 0.0;
    if (tok.type != TOKEN_FLOAT || !tok.value) return false;

    char* endp = NULL;
    double v = strtod(tok.value, &endp);
    if (!endp || endp == tok.value || *endp != '\0') return false;
    if (out) *out = v;
    return true;
}

static bool utf8_decode_one(const char* s, uint32_t* out_cp)
{
    if (out_cp) *out_cp = 0;
    if (!s || !*s) return false;

    const unsigned char b0 = (unsigned char)s[0];
    if ((b0 & 0x80u) == 0x00u) {
        if (out_cp) *out_cp = (uint32_t)b0;
        return true;
    }
    if ((b0 & 0xE0u) == 0xC0u) {
        const unsigned char b1 = (unsigned char)s[1];
        if ((b1 & 0xC0u) != 0x80u) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        if (cp < 0x80u) return false; // overlong
        if (out_cp) *out_cp = cp;
        return s[2] == '\0';
    }
    if ((b0 & 0xF0u) == 0xE0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        if (((b1 & 0xC0u) != 0x80u) || ((b2 & 0xC0u) != 0x80u)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) | ((uint32_t)(b1 & 0x3Fu) << 6) | (uint32_t)(b2 & 0x3Fu);
        if (cp < 0x800u) return false; // overlong
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false; // surrogate
        if (out_cp) *out_cp = cp;
        return s[3] == '\0';
    }
    if ((b0 & 0xF8u) == 0xF0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        const unsigned char b3 = (unsigned char)s[3];
        if (((b1 & 0xC0u) != 0x80u) || ((b2 & 0xC0u) != 0x80u) || ((b3 & 0xC0u) != 0x80u)) return false;
        uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) | ((uint32_t)(b1 & 0x3Fu) << 12) |
                      ((uint32_t)(b2 & 0x3Fu) << 6) | (uint32_t)(b3 & 0x3Fu);
        if (cp < 0x10000u) return false; // overlong
        if (cp > 0x10FFFFu) return false;
        if (out_cp) *out_cp = cp;
        return s[4] == '\0';
    }
    return false;
}

// ============================================================================
// تحليل نوع (TypeSpec) بما في ذلك تعداد/هيكل (v0.3.4)
// ============================================================================

/**
 * @brief تحليل مواصفة نوع: نوع بدائي أو تعداد/هيكل.
 *
 * الصيغ المدعومة:
 * - صحيح | نص | منطقي
 * - تعداد <اسم>
 * - هيكل <اسم>
 */
static bool parse_type_spec_ex(DataType* out_type, char** out_type_name,
                               DataType* out_ptr_base_type, char** out_ptr_base_type_name,
                               int* out_ptr_depth,
                               FuncPtrSig** out_func_sig,
                               bool allow_func_ptr)
{
    if (out_type) *out_type = TYPE_INT;
    if (out_type_name) *out_type_name = NULL;
    if (out_ptr_base_type) *out_ptr_base_type = TYPE_INT;
    if (out_ptr_base_type_name) *out_ptr_base_type_name = NULL;
    if (out_ptr_depth) *out_ptr_depth = 0;
    if (out_func_sig) *out_func_sig = NULL;

    DataType dt = TYPE_INT;
    char* tn = NULL;
    DataType ptr_base_type = TYPE_INT;
    char* ptr_base_name = NULL;
    int ptr_depth = 0;
    FuncPtrSig* fsig = NULL;
    bool parsed = false;

    // --------------------------------------------------------------------
    // نوع مؤشر دالة: دالة(…)->…
    // --------------------------------------------------------------------
    if (allow_func_ptr &&
        parser.current.type == TOKEN_IDENTIFIER &&
        parser.current.value &&
        strcmp(parser.current.value, "دالة") == 0)
    {
        Token tok_kw = parser.current;
        eat(TOKEN_IDENTIFIER); // دالة

        eat(TOKEN_LPAREN);

        // جمع معاملات التوقيع
        int cap = 0;
        int cnt = 0;
        bool is_variadic = false;
        DataType* p_types = NULL;
        DataType* p_ptr_base_types = NULL;
        char** p_ptr_base_names = NULL;
        int* p_ptr_depths = NULL;

        if (parser.current.type != TOKEN_RPAREN) {
            while (1) {
                if (parser.current.type == TOKEN_ELLIPSIS) {
                    if (cnt <= 0) {
                        error_report(parser.current, "غير مدعوم: '...' يتطلب وجود معامل ثابت واحد على الأقل.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                        free(p_types);
                        free(p_ptr_base_types);
                        free(p_ptr_base_names);
                        free(p_ptr_depths);
                        return false;
                    }
                    is_variadic = true;
                    eat(TOKEN_ELLIPSIS);
                    break;
                }

                Token tok_pt = parser.current;
                DataType pdt = TYPE_INT;
                char* ptn = NULL;
                DataType pptr_base = TYPE_INT;
                char* pptr_base_name = NULL;
                int pptr_depth = 0;
                FuncPtrSig* pfs = NULL;

                if (!parse_type_spec_ex(&pdt, &ptn, &pptr_base, &pptr_base_name, &pptr_depth, &pfs, false)) {
                    error_report(parser.current, "متوقع نوع لمعامل توقيع 'دالة(...)'.");
                    free(ptn);
                    free(pptr_base_name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                    free(p_types);
                    free(p_ptr_base_types);
                    free(p_ptr_base_names);
                    free(p_ptr_depths);
                    return false;
                }

                if (pfs) {
                    parser_funcsig_free(pfs);
                    pfs = NULL;
                }

                if (pdt == TYPE_VOID) {
                    error_report(tok_pt, "لا يمكن استخدام 'عدم' كنوع معامل في توقيع مؤشر دالة.");
                    free(ptn);
                    free(pptr_base_name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                    free(p_types);
                    free(p_ptr_base_types);
                    free(p_ptr_base_names);
                    free(p_ptr_depths);
                    return false;
                }
                if (pdt == TYPE_ENUM || pdt == TYPE_STRUCT || pdt == TYPE_UNION) {
                    error_report(tok_pt, "أنواع المعاملات المعرفة من المستخدم غير مدعومة بعد في تواقيع مؤشرات الدوال.");
                    free(ptn);
                    free(pptr_base_name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                    free(p_types);
                    free(p_ptr_base_types);
                    free(p_ptr_base_names);
                    free(p_ptr_depths);
                    return false;
                }

                if (cnt >= cap) {
                    int new_cap = (cap == 0) ? 4 : (cap * 2);
                    DataType* nt = (DataType*)realloc(p_types, (size_t)new_cap * sizeof(DataType));
                    if (!nt) {
                        error_report(tok_kw, "نفدت الذاكرة أثناء توسيع توقيع مؤشر الدالة.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                        free(p_types);
                        free(p_ptr_base_types);
                        free(p_ptr_base_names);
                        free(p_ptr_depths);
                        free(ptn);
                        free(pptr_base_name);
                        return false;
                    }
                    p_types = nt;

                    DataType* nb = (DataType*)realloc(p_ptr_base_types, (size_t)new_cap * sizeof(DataType));
                    if (!nb) {
                        error_report(tok_kw, "نفدت الذاكرة أثناء توسيع توقيع مؤشر الدالة.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                        free(p_types);
                        free(p_ptr_base_types);
                        free(p_ptr_base_names);
                        free(p_ptr_depths);
                        free(ptn);
                        free(pptr_base_name);
                        return false;
                    }
                    p_ptr_base_types = nb;

                    char** nn = (char**)realloc(p_ptr_base_names, (size_t)new_cap * sizeof(char*));
                    if (!nn) {
                        error_report(tok_kw, "نفدت الذاكرة أثناء توسيع توقيع مؤشر الدالة.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                        free(p_types);
                        free(p_ptr_base_types);
                        free(p_ptr_base_names);
                        free(p_ptr_depths);
                        free(ptn);
                        free(pptr_base_name);
                        return false;
                    }
                    p_ptr_base_names = nn;

                    int* nd = (int*)realloc(p_ptr_depths, (size_t)new_cap * sizeof(int));
                    if (!nd) {
                        error_report(tok_kw, "نفدت الذاكرة أثناء توسيع توقيع مؤشر الدالة.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
                        free(p_types);
                        free(p_ptr_base_types);
                        free(p_ptr_base_names);
                        free(p_ptr_depths);
                        free(ptn);
                        free(pptr_base_name);
                        return false;
                    }
                    p_ptr_depths = nd;

                    for (int i = cap; i < new_cap; i++) {
                        p_ptr_base_names[i] = NULL;
                    }
                    cap = new_cap;
                }

                p_types[cnt] = pdt;
                p_ptr_base_types[cnt] = pptr_base;
                p_ptr_depths[cnt] = pptr_depth;
                p_ptr_base_names[cnt] = pptr_base_name ? pptr_base_name : NULL;
                free(ptn);
                cnt++;

                if (parser.current.type == TOKEN_COMMA) {
                    eat(TOKEN_COMMA);
                    continue;
                }
                break;
            }
        }

        eat(TOKEN_RPAREN);

        // السهم: ->
        if (parser.current.type != TOKEN_MINUS || parser.next.type != TOKEN_GT) {
            error_report(parser.current, "متوقع '->' بعد معاملات توقيع الدالة.");
            for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
            free(p_types); free(p_ptr_base_types); free(p_ptr_base_names); free(p_ptr_depths);
            return false;
        }
        eat(TOKEN_MINUS);
        eat(TOKEN_GT);

        // نوع الإرجاع
        Token tok_rt = parser.current;
        DataType rdt = TYPE_INT;
        char* rtn = NULL;
        DataType rptr_base = TYPE_INT;
        char* rptr_base_name = NULL;
        int rptr_depth = 0;
        FuncPtrSig* rfs = NULL;
        if (!parse_type_spec_ex(&rdt, &rtn, &rptr_base, &rptr_base_name, &rptr_depth, &rfs, false)) {
            error_report(parser.current, "متوقع نوع إرجاع بعد '->' في توقيع مؤشر الدالة.");
            for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
            free(p_types); free(p_ptr_base_types); free(p_ptr_base_names); free(p_ptr_depths);
            free(rtn);
            free(rptr_base_name);
            synchronize_mode(PARSER_SYNC_DECLARATION);
            return false;
        }
        if (rfs) {
            parser_funcsig_free(rfs);
            rfs = NULL;
        }

        if (rdt == TYPE_ENUM || rdt == TYPE_STRUCT || rdt == TYPE_UNION) {
            error_report(tok_rt, "أنواع الإرجاع المعرفة من المستخدم غير مدعومة بعد في تواقيع مؤشرات الدوال.");
            for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
            free(p_types); free(p_ptr_base_types); free(p_ptr_base_names); free(p_ptr_depths);
            free(rtn);
            free(rptr_base_name);
            synchronize_mode(PARSER_SYNC_DECLARATION);
            return false;
        }

        free(rtn);

        fsig = (FuncPtrSig*)calloc(1, sizeof(FuncPtrSig));
        if (!fsig) {
            error_report(tok_kw, "نفدت الذاكرة أثناء إنشاء توقيع مؤشر الدالة.");
            for (int i = 0; i < cnt; i++) free(p_ptr_base_names ? p_ptr_base_names[i] : NULL);
            free(p_types); free(p_ptr_base_types); free(p_ptr_base_names); free(p_ptr_depths);
            free(rptr_base_name);
            return false;
        }

        fsig->return_type = rdt;
        fsig->return_ptr_base_type = rptr_base;
        fsig->return_ptr_base_type_name = rptr_base_name;
        fsig->return_ptr_depth = rptr_depth;
        fsig->param_count = cnt;
        fsig->is_variadic = is_variadic;
        fsig->param_types = p_types;
        fsig->param_ptr_base_types = p_ptr_base_types;
        fsig->param_ptr_base_type_names = p_ptr_base_names;
        fsig->param_ptr_depths = p_ptr_depths;

        dt = TYPE_FUNC_PTR;
        parsed = true;
    }

    // --------------------------------------------------------------------
    // الأنواع البدائية / المركبة / alias
    // --------------------------------------------------------------------
    else if (parser.current.type == TOKEN_KEYWORD_INT ||
             parser.current.type == TOKEN_KEYWORD_I8 ||
             parser.current.type == TOKEN_KEYWORD_I16 ||
             parser.current.type == TOKEN_KEYWORD_I32 ||
             parser.current.type == TOKEN_KEYWORD_I64 ||
             parser.current.type == TOKEN_KEYWORD_U8 ||
             parser.current.type == TOKEN_KEYWORD_U16 ||
             parser.current.type == TOKEN_KEYWORD_U32 ||
             parser.current.type == TOKEN_KEYWORD_U64 ||
             parser.current.type == TOKEN_KEYWORD_STRING ||
             parser.current.type == TOKEN_KEYWORD_BOOL ||
             parser.current.type == TOKEN_KEYWORD_CHAR ||
             parser.current.type == TOKEN_KEYWORD_VOID ||
             parser.current.type == TOKEN_KEYWORD_FLOAT ||
             parser.current.type == TOKEN_KEYWORD_FLOAT32) {
        dt = token_to_datatype(parser.current.type);
        eat(parser.current.type);
        parsed = true;
    }

    else if (parser.current.type == TOKEN_ENUM || parser.current.type == TOKEN_STRUCT || parser.current.type == TOKEN_UNION) {
        bool is_enum = (parser.current.type == TOKEN_ENUM);
        bool is_union = (parser.current.type == TOKEN_UNION);
        eat(parser.current.type);

        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم معرّف بعد كلمة النوع.");
            return false;
        }

        tn = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        dt = is_enum ? TYPE_ENUM : (is_union ? TYPE_UNION : TYPE_STRUCT);
        parsed = true;
    }

    else if (parser.current.type == TOKEN_IDENTIFIER && parser.current.value) {
        const ParserTypeAlias* alias = parser_type_alias_lookup(parser.current.value);
        if (!alias) return false;

        dt = alias->target_type;
        ptr_base_type = alias->target_ptr_base_type;
        ptr_depth = alias->target_ptr_depth;

        // منع الالتفاف على قيد "لا تواقيع أعلى رتبة" داخل دالة(…)->…:
        // إذا كان هذا الاسم البديل يحل إلى نوع مؤشر دالة، فلا نسمح به عندما allow_func_ptr == false
        // (أي أثناء تحليل معاملات/إرجاع توقيع مؤشر دالة آخر).
        if (!allow_func_ptr && dt == TYPE_FUNC_PTR) {
            error_report(parser.current,
                         "غير مدعوم: استخدام نوع بديل لمؤشر دالة داخل توقيع مؤشر دالة (لا ندعم Higher-order).");
            return false;
        }

        if (alias->target_func_sig) {
            fsig = parser_funcsig_clone(alias->target_func_sig);
            if (!fsig) {
                error_report(parser.current, "نفدت الذاكرة أثناء نسخ توقيع النوع البديل.");
                return false;
            }
        }

        if (alias->target_type_name) {
            tn = strdup(alias->target_type_name);
            if (!tn) {
                parser_funcsig_free(fsig);
                error_report(parser.current, "نفدت الذاكرة أثناء نسخ النوع البديل.");
                return false;
            }
        }

        if (alias->target_ptr_base_type_name) {
            ptr_base_name = strdup(alias->target_ptr_base_type_name);
            if (!ptr_base_name) {
                free(tn);
                parser_funcsig_free(fsig);
                error_report(parser.current, "نفدت الذاكرة أثناء نسخ أساس مؤشر النوع البديل.");
                return false;
            }
        }

        eat(TOKEN_IDENTIFIER);
        parsed = true;
    }

    if (!parsed) return false;

    if (dt == TYPE_FUNC_PTR) {
        if (parser.current.type == TOKEN_STAR) {
            error_report(parser.current, "غير مدعوم: لاحقة '*' بعد نوع مؤشر دالة.");
            parser_funcsig_free(fsig);
            return false;
        }

        // تنظيف الحقول غير المستخدمة
        free(tn);
        tn = NULL;
        free(ptr_base_name);
        ptr_base_name = NULL;
        ptr_base_type = TYPE_INT;
        ptr_depth = 0;
    } else {
        int suffix_ptr_depth = 0;
        while (parser.current.type == TOKEN_STAR) {
            eat(TOKEN_STAR);
            suffix_ptr_depth++;
        }

        if (suffix_ptr_depth > 0) {
            if (dt == TYPE_POINTER) {
                ptr_depth += suffix_ptr_depth;
            } else {
                ptr_base_type = dt;
                ptr_depth = suffix_ptr_depth;

                if ((ptr_base_type == TYPE_ENUM || ptr_base_type == TYPE_STRUCT || ptr_base_type == TYPE_UNION) &&
                    tn && !ptr_base_name) {
                    ptr_base_name = strdup(tn);
                    if (!ptr_base_name) {
                        free(tn);
                        error_report(parser.current, "نفدت الذاكرة أثناء نسخ اسم أساس المؤشر.");
                        return false;
                    }
                }
            }

            dt = TYPE_POINTER;
        }

        if (dt == TYPE_POINTER && ptr_depth <= 0) {
            // حماية احترازية لحالات alias pointer.
            ptr_depth = 1;
        }

        if (dt != TYPE_POINTER) {
            ptr_base_type = TYPE_INT;
            free(ptr_base_name);
            ptr_base_name = NULL;
        }
    }

    if (out_type) {
        *out_type = dt;
    }

    if (out_type_name) {
        *out_type_name = tn;
    } else {
        free(tn);
    }

    if (out_ptr_base_type) {
        *out_ptr_base_type = ptr_base_type;
    }

    if (out_ptr_base_type_name) {
        *out_ptr_base_type_name = ptr_base_name;
    } else {
        free(ptr_base_name);
    }

    if (out_ptr_depth) {
        *out_ptr_depth = ptr_depth;
    }

    if (out_func_sig) {
        *out_func_sig = fsig;
    } else {
        parser_funcsig_free(fsig);
    }

    return true;
}

static bool parse_type_spec(DataType* out_type, char** out_type_name,
                            DataType* out_ptr_base_type, char** out_ptr_base_type_name,
                            int* out_ptr_depth,
                            FuncPtrSig** out_func_sig)
{
    return parse_type_spec_ex(out_type, out_type_name,
                              out_ptr_base_type, out_ptr_base_type_name,
                              out_ptr_depth,
                              out_func_sig,
                              true);
}

static bool parser_current_starts_declaration_anchor(void)
{
    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) return true;
    if (parser_current_starts_type()) return true;
    return parser.current.type == TOKEN_CONST || parser.current.type == TOKEN_STATIC;
}

static bool parser_current_starts_statement_anchor(void)
{
    if (parser_current_starts_type()) return true;
    if (parser_current_is_type_alias_keyword()) return true;
    if (parser.current.type == TOKEN_CONST || parser.current.type == TOKEN_STATIC) return true;
    if (parser.current.type == TOKEN_IDENTIFIER) return true;

    switch (parser.current.type) {
        case TOKEN_LBRACE:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_FOR:
        case TOKEN_PRINT:
        case TOKEN_READ:
        case TOKEN_ASM:
        case TOKEN_RETURN:
        case TOKEN_SWITCH:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
            return true;
        default:
            return false;
    }
}

/**
 * @brief محاولة التعافي من الخطأ حسب سياق التحليل.
 */
static void synchronize_mode(ParserSyncMode mode)
{
    while (parser.current.type != TOKEN_EOF) {
        // إذا وجدنا فاصلة منقوطة، فغالباً انتهت الجملة السابقة
        if (parser.current.type == TOKEN_SEMICOLON || parser.current.type == TOKEN_DOT) {
            advance();
            parser.panic_mode = false;
            return;
        }

        if (mode == PARSER_SYNC_SWITCH) {
            if (parser.current.type == TOKEN_CASE ||
                parser.current.type == TOKEN_DEFAULT ||
                parser.current.type == TOKEN_RBRACE) {
                parser.panic_mode = false;
                return;
            }
        } else if (mode == PARSER_SYNC_DECLARATION) {
            if (parser.current.type == TOKEN_RBRACE) {
                advance();
                parser.panic_mode = false;
                return;
            }
            if (parser_current_starts_declaration_anchor()) {
                parser.panic_mode = false;
                return;
            }
        } else {
            if (parser_current_starts_statement_anchor() || parser.current.type == TOKEN_RBRACE) {
                parser.panic_mode = false;
                return;
            }
        }

        advance();
    }

    parser.panic_mode = false;
}

/**
 * @brief محاولة التعافي من الخطأ بالقفز إلى بداية الجملة التالية.
 */
void synchronize() {
    synchronize_mode(PARSER_SYNC_STATEMENT);
}

// --- تصريحات مسبقة ---
Node* parse_expression();
Node* parse_statement();
Node* parse_block();
Node* parse_primary();
Node* parse_shift();
Node* parse_bitwise_and();
Node* parse_bitwise_xor();
Node* parse_bitwise_or();
static Node* parse_inline_asm_statement(void);

/**
 * @brief إضافة عنصر إلى نهاية قائمة عقد أحادية الربط.
 */
static void parser_list_append(Node** head, Node** tail, Node* item)
{
    if (!item) return;
    item->next = NULL;
    if (!*head) {
        *head = item;
        *tail = item;
    } else {
        (*tail)->next = item;
        *tail = item;
    }
}

/**
 * @brief تحليل سلسلة فهارس بالشكل: [expr][expr]...
 */
static Node* parse_array_indices(int* out_count)
{
    if (out_count) *out_count = 0;

    Node* head = NULL;
    Node* tail = NULL;
    int count = 0;

    while (parser.current.type == TOKEN_LBRACKET) {
        eat(TOKEN_LBRACKET);
        Node* index = parse_expression();
        eat(TOKEN_RBRACKET);

        if (index) {
            parser_list_append(&head, &tail, index);
            count++;
        }
    }

    if (out_count) *out_count = count;
    return head;
}

static bool parser_mul_i64_checked(int64_t a, int64_t b, int64_t* out)
{
    if (!out) return false;
    *out = 0;
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
    if (a > INT64_MAX / b) return false;
    *out = a * b;
    return true;
}

/**
 * @brief تحليل أبعاد مصفوفة ثابتة: [N][M]...
 */
static bool parse_array_dimensions(int** out_dims, int* out_rank, int64_t* out_total)
{
    if (out_dims) *out_dims = NULL;
    if (out_rank) *out_rank = 0;
    if (out_total) *out_total = 0;

    int cap = 4;
    int rank = 0;
    int* dims = NULL;
    int64_t total = 1;

    while (parser.current.type == TOKEN_LBRACKET) {
        eat(TOKEN_LBRACKET);

        int dim = 0;
        if (parser.current.type == TOKEN_INT) {
            int64_t v = 0;
            if (parse_int_token_checked(parser.current, &v)) {
                if (v <= 0) {
                    error_report(parser.current, "حجم البعد يجب أن يكون عدداً موجباً.");
                } else if (v > INT_MAX) {
                    error_report(parser.current, "حجم البعد كبير جداً.");
                } else {
                    dim = (int)v;
                }
            } else {
                error_report(parser.current, "حجم البعد يجب أن يكون عدداً صحيحاً.");
            }
            eat(TOKEN_INT);
        } else {
            error_report(parser.current, "متوقع حجم بعد مصفوفة ثابت.");
        }

        eat(TOKEN_RBRACKET);

        if (rank >= cap) {
            int new_cap = cap * 2;
            int* new_dims = (int*)realloc(dims, (size_t)new_cap * sizeof(int));
            if (!new_dims) {
                free(dims);
                error_report(parser.current, "نفدت الذاكرة أثناء تحليل أبعاد المصفوفة.");
                return false;
            }
            dims = new_dims;
            cap = new_cap;
        }
        if (!dims) {
            dims = (int*)malloc((size_t)cap * sizeof(int));
            if (!dims) {
                error_report(parser.current, "نفدت الذاكرة أثناء تحليل أبعاد المصفوفة.");
                return false;
            }
        }

        dims[rank++] = dim;

        if (dim > 0) {
            int64_t nt = 0;
            if (!parser_mul_i64_checked(total, (int64_t)dim, &nt)) {
                error_report(parser.current, "حجم المصفوفة الكلي كبير جداً.");
                total = 0;
            } else {
                total = nt;
            }
        } else {
            total = 0;
        }
    }

    if (rank == 0) {
        free(dims);
        return false;
    }

    if (out_dims) *out_dims = dims;
    else free(dims);
    if (out_rank) *out_rank = rank;
    if (out_total) *out_total = total;
    return true;
}
