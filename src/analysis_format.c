// ============================================================================
// تنسيق عربي للإخراج/الإدخال (v0.4.0)
// ============================================================================

#define BAA_FMT_MAX_SPECS 128

typedef enum {
    BAA_FMT_SPEC_I64,
    BAA_FMT_SPEC_U64,
    BAA_FMT_SPEC_HEX,
    BAA_FMT_SPEC_STR,
    BAA_FMT_SPEC_CHAR,
    BAA_FMT_SPEC_F64,
    BAA_FMT_SPEC_F64_SCI,
    BAA_FMT_SPEC_PTR,
} BaaFmtSpecKind;

typedef struct {
    BaaFmtSpecKind kind;
    int width; // للأمان في scanf مع %ن
    bool has_width;
} BaaFmtSpec;

typedef struct {
    int spec_count;
    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
} BaaFmtParse;

static bool baa_utf8_decode_one(const char* s, uint32_t* out_cp, int* out_len)
{
    if (!s || !out_cp || !out_len) return false;
    const unsigned char b0 = (unsigned char)s[0];
    if (b0 == 0) return false;

    if ((b0 & 0x80u) == 0x00u) {
        *out_cp = (uint32_t)b0;
        *out_len = 1;
        return true;
    }
    if ((b0 & 0xE0u) == 0xC0u) {
        const unsigned char b1 = (unsigned char)s[1];
        if ((b1 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        *out_len = 2;
        return true;
    }
    if ((b0 & 0xF0u) == 0xE0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        if ((b1 & 0xC0u) != 0x80u) return false;
        if ((b2 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                  ((uint32_t)(b1 & 0x3Fu) << 6) |
                  (uint32_t)(b2 & 0x3Fu);
        *out_len = 3;
        return true;
    }
    if ((b0 & 0xF8u) == 0xF0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        const unsigned char b3 = (unsigned char)s[3];
        if ((b1 & 0xC0u) != 0x80u) return false;
        if ((b2 & 0xC0u) != 0x80u) return false;
        if ((b3 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x07u) << 18) |
                  ((uint32_t)(b1 & 0x3Fu) << 12) |
                  ((uint32_t)(b2 & 0x3Fu) << 6) |
                  (uint32_t)(b3 & 0x3Fu);
        *out_len = 4;
        return true;
    }

    return false;
}

static bool baa_fmt_parse_ar(Node* site, const char* fmt, bool is_input, BaaFmtParse* out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!fmt) return false;

    const char* p = fmt;
    while (*p) {
        if (*p != '%') {
            p++;
            continue;
        }

        // %%
        if (p[1] == '%') {
            p += 2;
            continue;
        }

        p++; // تخطِ '%'

        // flags (ASCII)
        while (*p && (*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#')) {
            p++;
        }

        int width = 0;
        bool has_width = false;
        while (*p && isdigit((unsigned char)*p)) {
            has_width = true;
            width = width * 10 + (int)(*p - '0');
            if (width > 1000000) width = 1000000;
            p++;
        }

        // precision (.NN) — نسمح بالأرقام فقط (لا نسمح بـ *)
        if (*p == '.') {
            p++;
            if (*p == '*') {
                semantic_error(site, "تنسيق عربي: الدقة باستخدام '*' غير مدعومة حالياً.");
                return false;
            }
            while (*p && isdigit((unsigned char)*p)) {
                p++;
            }
        }

        uint32_t cp = 0;
        int ulen = 0;
        if (!baa_utf8_decode_one(p, &cp, &ulen) || ulen <= 0) {
            semantic_error(site, "تنسيق عربي: محرف UTF-8 غير صالح بعد '%%'.");
            return false;
        }

        if (out->spec_count >= BAA_FMT_MAX_SPECS) {
            semantic_error(site, "تنسيق عربي: عدد المواصفات كبير جداً (الحد %d).", BAA_FMT_MAX_SPECS);
            return false;
        }

        BaaFmtSpec spec;
        memset(&spec, 0, sizeof(spec));
        spec.width = width;
        spec.has_width = has_width;

        // ص ط س ن ح ع أ م
        if (cp == 0x0635u) { // ص
            spec.kind = BAA_FMT_SPEC_I64;
        } else if (cp == 0x0637u) { // ط
            spec.kind = BAA_FMT_SPEC_U64;
        } else if (cp == 0x0633u) { // س
            spec.kind = BAA_FMT_SPEC_HEX;
        } else if (cp == 0x0646u) { // ن
            spec.kind = BAA_FMT_SPEC_STR;
            if (is_input) {
                if (!has_width || width <= 0) {
                    semantic_error(site, "تنسيق عربي: %%ن في الإدخال يتطلب عرضاً رقمياً (مثلاً %%64ن).");
                    return false;
                }
            }
        } else if (cp == 0x062Du) { // ح
            spec.kind = BAA_FMT_SPEC_CHAR;
            if (is_input) {
                semantic_error(site, "تنسيق عربي: %%ح غير مدعوم في الإدخال حالياً.");
                return false;
            }
        } else if (cp == 0x0639u) { // ع
            spec.kind = BAA_FMT_SPEC_F64;
        } else if (cp == 0x0623u) { // أ (صيغة علمية)
            spec.kind = BAA_FMT_SPEC_F64_SCI;
        } else if (cp == 0x0645u) { // م
            spec.kind = BAA_FMT_SPEC_PTR;
            if (is_input) {
                semantic_error(site, "تنسيق عربي: %%م غير مدعوم في الإدخال حالياً.");
                return false;
            }
        } else {
            semantic_error(site, "تنسيق عربي: مواصفة غير معروفة بعد '%%'.");
            return false;
        }

        out->specs[out->spec_count++] = spec;
        p += ulen;
    }

    return true;
}

/**
 * @brief التحقق من استدعاءات التنسيق العربية (v0.4.0).
 *
 * الأسماء المدعومة:
 * - اطبع_منسق(نص تنسيق، ...): void
 * - نسق(نص تنسيق، ...): نص
 * - اقرأ_منسق(نص تنسيق، ...): صحيح (عدد العناصر المقروءة)
 * - اقرأ_سطر(): نص
 * - اقرأ_رقم(): صحيح
 */
static bool builtin_check_format_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    if (!call_node || !fname) return false;

    bool is_print = (strcmp(fname, "اطبع_منسق") == 0);
    bool is_format = (strcmp(fname, "نسق") == 0);
    bool is_scan = (strcmp(fname, "اقرأ_منسق") == 0);
    bool is_read_line = (strcmp(fname, "اقرأ_سطر") == 0);
    bool is_read_num = (strcmp(fname, "اقرأ_رقم") == 0);

    if (!is_print && !is_format && !is_scan && !is_read_line && !is_read_num) {
        return false;
    }

    // اقرأ_سطر(): نُميّزها فقط عند صفر معاملات حتى لا تتعارض مع اقرأ_سطر(ملف) في stdlib.
    if (is_read_line) {
        if (args) return false;
        if (out_return_type) *out_return_type = TYPE_STRING;
        node_clear_inferred_ptr(call_node);
        return true;
    }

    if (is_read_num) {
        if (args) {
            semantic_error(call_node, "استدعاء 'اقرأ_رقم' لا يقبل معاملات.");
            Node* a = args;
            while (a) { (void)infer_type(a); a = a->next; }
        }
        if (out_return_type) *out_return_type = TYPE_INT;
        node_clear_inferred_ptr(call_node);
        return true;
    }

    // بقية الدوال: تتطلب نص تنسيق كمعامل أول.
    if (!args) {
        semantic_error(call_node, "استدعاء '%s' يتطلب نص تنسيق كمعامل أول.", fname);
        if (out_return_type) *out_return_type = is_scan ? TYPE_INT : (is_format ? TYPE_STRING : TYPE_VOID);
        node_clear_inferred_ptr(call_node);
        return true;
    }

    Node* fmt_arg = args;
    DataType fmt_t = infer_type(fmt_arg);
    if (fmt_t != TYPE_STRING) {
        semantic_error(fmt_arg, "المعامل الأول في '%s' يجب أن يكون من نوع 'نص'.", fname);
    }
    if (fmt_arg->type != NODE_STRING || !fmt_arg->data.string_lit.value) {
        semantic_error(fmt_arg, "نص التنسيق في '%s' يجب أن يكون ثابتاً (literal) في هذا الإصدار.", fname);
    }

    BaaFmtParse parsed;
    memset(&parsed, 0, sizeof(parsed));
    bool ok = true;
    if (fmt_arg->type == NODE_STRING && fmt_arg->data.string_lit.value) {
        ok = baa_fmt_parse_ar(fmt_arg, fmt_arg->data.string_lit.value, is_scan, &parsed);
    } else {
        ok = false;
    }

    // تحقق عدد المعاملات
    int supplied = 0;
    for (Node* a = args ? args->next : NULL; a; a = a->next) supplied++;
    if (ok && supplied != parsed.spec_count) {
        semantic_error(call_node, "عدد معاملات '%s' لا يطابق نص التنسيق (المتوقع %d).",
                       fname, parsed.spec_count + 1);
    }

    // تحقق الأنواع
    int idx = 0;
    for (Node* a = args ? args->next : NULL; a; a = a->next, idx++) {
        if (!ok || idx >= parsed.spec_count) {
            (void)infer_type(a);
            continue;
        }

        BaaFmtSpecKind k = parsed.specs[idx].kind;

        if (is_scan) {
            DataType got = infer_type(a);
            if (got != TYPE_POINTER) {
                semantic_error(a, "نداء 'اقرأ_منسق': المعامل %d يجب أن يكون مؤشراً.", idx + 2);
                continue;
            }

            // v0.4.0: ندعم الإدخال إلى 64-بت فقط بشكل محافظ.
            if (k == BAA_FMT_SPEC_I64) {
                if (!ptr_type_compatible(a->inferred_ptr_base_type, a->inferred_ptr_base_type_name, a->inferred_ptr_depth,
                                         TYPE_INT, NULL, 1, true)) {
                    semantic_error(a, "نداء 'اقرأ_منسق': المعامل %d يجب أن يكون 'صحيح*' مع %%ص.", idx + 2);
                }
            } else if (k == BAA_FMT_SPEC_U64 || k == BAA_FMT_SPEC_HEX) {
                if (!ptr_type_compatible(a->inferred_ptr_base_type, a->inferred_ptr_base_type_name, a->inferred_ptr_depth,
                                         TYPE_U64, NULL, 1, true)) {
                    semantic_error(a, "نداء 'اقرأ_منسق': المعامل %d يجب أن يكون 'ط٦٤*' مع %%ط/%%س.", idx + 2);
                }
            } else if (k == BAA_FMT_SPEC_F64 || k == BAA_FMT_SPEC_F64_SCI) {
                if (!ptr_type_compatible(a->inferred_ptr_base_type, a->inferred_ptr_base_type_name, a->inferred_ptr_depth,
                                         TYPE_FLOAT, NULL, 1, true)) {
                    semantic_error(a, "نداء 'اقرأ_منسق': المعامل %d يجب أن يكون 'عشري*' مع %%ع/%%أ.", idx + 2);
                }
            } else if (k == BAA_FMT_SPEC_STR) {
                if (!ptr_type_compatible(a->inferred_ptr_base_type, a->inferred_ptr_base_type_name, a->inferred_ptr_depth,
                                         TYPE_STRING, NULL, 1, true)) {
                    semantic_error(a, "نداء 'اقرأ_منسق': المعامل %d يجب أن يكون 'نص*' مع %%ن.", idx + 2);
                }
            } else {
                semantic_error(a, "نداء 'اقرأ_منسق': مواصفة غير مدعومة في الإدخال.");
            }
            continue;
        }

        // إخراج/تنسيق نصي
        if (k == BAA_FMT_SPEC_STR) {
            DataType got = infer_type_allow_null_string(a, TYPE_STRING);
            if (got != TYPE_STRING) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون 'نص' مع %%ن.", idx + 2, fname);
            }
        } else if (k == BAA_FMT_SPEC_CHAR) {
            DataType got = infer_type(a);
            if (got != TYPE_CHAR) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون 'حرف' مع %%ح.", idx + 2, fname);
            }
        } else if (k == BAA_FMT_SPEC_F64 || k == BAA_FMT_SPEC_F64_SCI) {
            DataType got = infer_type(a);
            if (got != TYPE_FLOAT) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون 'عشري' مع %%ع/%%أ.", idx + 2, fname);
            }
        } else if (k == BAA_FMT_SPEC_PTR) {
            DataType got = infer_type_allow_null_string(a, TYPE_POINTER);
            if (!(got == TYPE_POINTER || got == TYPE_STRING || got == TYPE_FUNC_PTR)) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون مؤشراً مع %%م.", idx + 2, fname);
            }
        } else if (k == BAA_FMT_SPEC_U64 || k == BAA_FMT_SPEC_HEX) {
            DataType got = infer_type(a);
            if (!datatype_is_unsigned_int(got)) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون غير موقّع مع %%ط/%%س.", idx + 2, fname);
            }
        } else { // I64
            DataType got = infer_type(a);
            if (!datatype_is_intlike(got) || datatype_is_unsigned_int(got)) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون موقّعاً مع %%ص.", idx + 2, fname);
            }
        }
    }

    if (out_return_type) {
        if (is_print) *out_return_type = TYPE_VOID;
        else if (is_format) *out_return_type = TYPE_STRING;
        else if (is_scan) *out_return_type = TYPE_INT;
        else *out_return_type = TYPE_VOID;
    }

    node_clear_inferred_ptr(call_node);
    return true;
}

static bool datatype_size_bytes(DataType type, const char* type_name, int64_t* out_size)
{
    if (!out_size) return false;
    *out_size = 0;

    switch (type) {
        case TYPE_BOOL:
        case TYPE_I8:
        case TYPE_U8:
            *out_size = 1;
            return true;
        case TYPE_I16:
        case TYPE_U16:
            *out_size = 2;
            return true;
        case TYPE_I32:
        case TYPE_U32:
            *out_size = 4;
            return true;
        case TYPE_INT:
        case TYPE_U64:
        case TYPE_STRING:
        case TYPE_POINTER:
        case TYPE_FUNC_PTR:
        case TYPE_CHAR:
        case TYPE_FLOAT:
        case TYPE_ENUM:
            *out_size = 8;
            return true;

        case TYPE_STRUCT: {
            if (!type_name) return false;
            StructDef* sd = struct_lookup_def(type_name);
            if (!sd) return false;
            (void)struct_compute_layout(sd);
            *out_size = (int64_t)sd->size;
            return true;
        }
        case TYPE_UNION: {
            if (!type_name) return false;
            UnionDef* ud = union_lookup_def(type_name);
            if (!ud) return false;
            (void)union_compute_layout(ud);
            *out_size = (int64_t)ud->size;
            return true;
        }

        case TYPE_VOID:
        default:
            return false;
    }
}

static bool sizeof_expr_bytes(Node* expr, int64_t* out_size)
{
    if (!expr || !out_size) return false;

    if (expr->type == NODE_VAR_REF && expr->data.var_ref.name) {
        Symbol* sym = lookup(expr->data.var_ref.name, false);
        if (!sym) return false;

        if (sym->is_array) {
            int64_t elem_size = 0;
            if (!datatype_size_bytes(sym->type, sym->type_name[0] ? sym->type_name : NULL, &elem_size)) {
                return false;
            }
            *out_size = elem_size * sym->array_total_elems;
            return true;
        }

        return datatype_size_bytes(sym->type, sym->type_name[0] ? sym->type_name : NULL, out_size);
    }

    if (expr->type == NODE_MEMBER_ACCESS) {
        resolve_member_access(expr);
        if (expr->data.member_access.is_struct_member) {
            return datatype_size_bytes(expr->data.member_access.member_type,
                                       expr->data.member_access.member_type_name,
                                       out_size);
        }
        if (expr->data.member_access.is_enum_value) {
            *out_size = 8;
            return true;
        }
    }

    DataType t = infer_type(expr);
    return datatype_size_bytes(t, NULL, out_size);
}

/**
 * @brief استهلاك معاملات نداء أثناء الاسترداد لاكتشاف أخطاء إضافية.
 */
static void analyze_consume_call_args(Node* args)
{
    for (Node* arg = args; arg; arg = arg->next) {
        (void)infer_type(arg);
    }
}

/**
 * @brief التحقق من معاملات نداء مؤشر دالة.
 */
static void analyze_funcptr_call_args(Node* call_node,
                                      const char* fname,
                                      Node* args,
                                      const FuncPtrSig* sig)
{
    if (!call_node || !sig) return;
    const char* call_name = fname ? fname : "???";

    if (sig->is_variadic) {
        semantic_error(call_node, "نداء مؤشر دالة متغير المعاملات غير مدعوم حالياً.");
    }

    int i = 0;
    for (Node* arg = args; arg; arg = arg->next, i++) {
        if (i < sig->param_count) {
            DataType exp = sig->param_types ? sig->param_types[i] : TYPE_INT;
            DataType at = infer_type_allow_null_string(arg, exp);
            if (exp == TYPE_POINTER) {
                DataType eb = sig->param_ptr_base_types ? sig->param_ptr_base_types[i] : TYPE_INT;
                int ed = sig->param_ptr_depths ? sig->param_ptr_depths[i] : 0;
                const char* en = (sig->param_ptr_base_type_names && sig->param_ptr_base_type_names[i])
                                     ? sig->param_ptr_base_type_names[i]
                                     : NULL;
                if (at != TYPE_POINTER ||
                    !ptr_type_compatible(arg->inferred_ptr_base_type,
                                         arg->inferred_ptr_base_type_name,
                                         arg->inferred_ptr_depth,
                                         eb, en, ed, true)) {
                    semantic_error(arg, "نوع المعامل %d في نداء '%s' غير متوافق.", i + 1, call_name);
                }
            } else if (exp == TYPE_FUNC_PTR) {
                semantic_error(arg, "Higher-order غير مدعوم: معامل %d في توقيع مؤشر الدالة هو مؤشر دالة.", i + 1);
            } else if (!types_compatible(at, exp)) {
                semantic_error(arg, "نوع المعامل %d في نداء '%s' غير متوافق.", i + 1, call_name);
            } else {
                maybe_warn_implicit_narrowing(at, exp, arg);
            }
        } else {
            DataType at = infer_type(arg);
            if (sig->is_variadic) {
                if (!variadic_extra_arg_type_supported(at)) {
                    semantic_error(arg, "نوع المعامل المتغير %d في نداء '%s' غير مدعوم.",
                                   i + 1, call_name);
                }
            }
        }
    }

    if (sig->is_variadic) {
        if (i < sig->param_count) {
            semantic_error(call_node, "عدد معاملات نداء '%s' غير صحيح (المتوقع على الأقل %d).",
                           call_name, sig->param_count);
        }
    } else if (i != sig->param_count) {
        semantic_error(call_node, "عدد معاملات نداء '%s' غير صحيح (المتوقع %d).",
                       call_name, sig->param_count);
    }
}

/**
 * @brief تنفيذ سلسلة فحص الدوال المدمجة بالترتيب القياسي.
 * @return true إذا تم التعرف على الدالة كمدمجة وتم التعامل معها.
 */
