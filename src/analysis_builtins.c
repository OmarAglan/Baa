static DataType infer_type(Node* node);
static DataType infer_type_allow_null_string(Node* expr, DataType expected);

static void builtin_report_param_type_mismatch(Node* arg, int param_index, const char* builtin_name)
{
    semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", param_index, builtin_name);
}

static void builtin_report_param_count_mismatch(Node* call_node, const char* builtin_name, int expected_count)
{
    semantic_error(call_node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", builtin_name, expected_count);
}

/**
 * @brief التحقق من توافق المؤشر كـ عدم* (void*) في الدوال المدمجة.
 */
static bool builtin_arg_is_void_ptr_compatible(Node* arg, DataType got)
{
    if (!arg) return false;
    if (got != TYPE_POINTER) return false;
    return ptr_type_compatible(arg->inferred_ptr_base_type,
                               arg->inferred_ptr_base_type_name,
                               arg->inferred_ptr_depth,
                               TYPE_VOID, NULL, 1,
                               true);
}

/**
 * @brief مسار تحقق موحد لمعاملات الدوال المدمجة البسيطة.
 */
static void builtin_check_args_scaffold(Node* call_node,
                                        const char* builtin_name,
                                        Node* args,
                                        const DataType* param_types,
                                        int param_count,
                                        bool allow_null_string,
                                        bool warn_narrowing)
{
    int i = 0;
    for (Node* arg = args; arg; arg = arg->next, i++) {
        DataType expected = (i < param_count) ? param_types[i] : TYPE_INT;
        DataType got = allow_null_string ? infer_type_allow_null_string(arg, expected) : infer_type(arg);

        if (i >= param_count) continue;
        if (!types_compatible(got, expected)) {
            builtin_report_param_type_mismatch(arg, i + 1, builtin_name);
        } else if (warn_narrowing) {
            maybe_warn_implicit_narrowing(got, expected, arg);
        }
    }

    if (i != param_count) {
        builtin_report_param_count_mismatch(call_node, builtin_name, param_count);
    }
}

/**
 * @brief إنهاء ضبط نوع إرجاع استدعاء دالة مدمجة وتحديث معلومات المؤشر المستنتج للعقدة.
 */
static void builtin_finalize_call_return(Node* call_node,
                                         DataType return_type,
                                         DataType* out_return_type,
                                         bool pointer_returns_as_void_ptr)
{
    if (out_return_type) *out_return_type = return_type;
    if (!call_node) return;

    if (pointer_returns_as_void_ptr && return_type == TYPE_POINTER) {
        node_set_inferred_ptr(call_node, TYPE_VOID, NULL, 1);
    } else {
        node_clear_inferred_ptr(call_node);
    }
}

#define ANALYSIS_ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

/**
 * @brief توليد دوال بحث خطي متجانسة لجداول الدوال المدمجة.
 */
#define DEFINE_BUILTIN_LOOKUP(fn_name, sig_type, table_name)                          \
    static const sig_type* fn_name(const char* name)                                   \
    {                                                                                   \
        if (!name) return NULL;                                                        \
        const int n = ANALYSIS_ARRAY_LEN(table_name);                                  \
        for (int i = 0; i < n; i++) {                                                  \
            if (strcmp(name, table_name[i].name) == 0) {                               \
                return &table_name[i];                                                  \
            }                                                                           \
        }                                                                               \
        return NULL;                                                                    \
    }

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[2];
} BuiltinFuncSig;

static const BuiltinFuncSig builtin_string_funcs[] = {
    { "طول_نص", TYPE_INT,    1, { TYPE_STRING, TYPE_INT } },
    { "قارن_نص", TYPE_INT,    2, { TYPE_STRING, TYPE_STRING } },
    { "نسخ_نص", TYPE_STRING, 1, { TYPE_STRING, TYPE_INT } },
    { "دمج_نص", TYPE_STRING, 2, { TYPE_STRING, TYPE_STRING } },
    { "حرر_نص", TYPE_VOID,   1, { TYPE_STRING, TYPE_INT } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_string_func, BuiltinFuncSig, builtin_string_funcs)

/**
 * @brief التحقق من صحة استدعاء دوال السلاسل المدمجة في v0.3.9.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_string_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinFuncSig* sig = builtin_lookup_string_func(fname);
    if (!sig) return false;

    builtin_check_args_scaffold(call_node, sig->name, args, sig->param_types, sig->param_count, false, true);

    if (out_return_type) *out_return_type = sig->return_type;
    return true;
}

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[3];
} BuiltinMemFuncSig;

static const BuiltinMemFuncSig builtin_mem_funcs[] = {
    { "حجز_ذاكرة",   TYPE_POINTER, 1, { TYPE_INT,     TYPE_INT, TYPE_INT } },
    { "تحرير_ذاكرة", TYPE_VOID,    1, { TYPE_POINTER, TYPE_INT, TYPE_INT } },
    { "إعادة_حجز",   TYPE_POINTER, 2, { TYPE_POINTER, TYPE_INT, TYPE_INT } },
    { "نسخ_ذاكرة",   TYPE_POINTER, 3, { TYPE_POINTER, TYPE_POINTER, TYPE_INT } },
    { "تعيين_ذاكرة", TYPE_POINTER, 3, { TYPE_POINTER, TYPE_INT, TYPE_INT } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_mem_func, BuiltinMemFuncSig, builtin_mem_funcs)

/**
 * @brief التحقق من صحة استدعاء دوال الذاكرة المدمجة في v0.3.11.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_mem_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinMemFuncSig* sig = builtin_lookup_mem_func(fname);
    if (!sig) return false;

    int i = 0;
    for (Node* arg = args; arg; arg = arg->next, i++) {
        DataType got = infer_type(arg);
        if (i < sig->param_count) {
            DataType expected = sig->param_types[i];
            if (expected == TYPE_POINTER) {
                // جميع معاملات المؤشر هنا هي عدم* (void*)، ويُسمح بالتحويلات الضمنية وفق قواعد void*.
                if (!builtin_arg_is_void_ptr_compatible(arg, got)) {
                    builtin_report_param_type_mismatch(arg, i + 1, sig->name);
                }
            } else if (!types_compatible(got, expected)) {
                builtin_report_param_type_mismatch(arg, i + 1, sig->name);
            } else {
                maybe_warn_implicit_narrowing(got, expected, arg);
            }
        }
    }

    if (i != sig->param_count) {
        builtin_report_param_count_mismatch(call_node, sig->name, sig->param_count);
    }

    builtin_finalize_call_return(call_node, sig->return_type, out_return_type, true);
    return true;
}

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[3];
} BuiltinFileFuncSig;

static const BuiltinFileFuncSig builtin_file_funcs[] = {
    { "فتح_ملف",   TYPE_POINTER, 2, { TYPE_STRING,  TYPE_STRING,  TYPE_INT } },
    { "اغلق_ملف",  TYPE_INT,     1, { TYPE_POINTER, TYPE_INT,     TYPE_INT } },
    { "اقرأ_حرف",  TYPE_CHAR,    1, { TYPE_POINTER, TYPE_INT,     TYPE_INT } },
    { "اكتب_حرف",  TYPE_INT,     2, { TYPE_POINTER, TYPE_CHAR,    TYPE_INT } },
    { "اقرأ_ملف",  TYPE_INT,     3, { TYPE_POINTER, TYPE_POINTER, TYPE_INT } },
    { "اكتب_ملف",  TYPE_INT,     3, { TYPE_POINTER, TYPE_POINTER, TYPE_INT } },
    { "نهاية_ملف", TYPE_BOOL,    1, { TYPE_POINTER, TYPE_INT,     TYPE_INT } },
    { "موقع_ملف",  TYPE_INT,     1, { TYPE_POINTER, TYPE_INT,     TYPE_INT } },
    { "اذهب_لموقع", TYPE_INT,    2, { TYPE_POINTER, TYPE_INT,     TYPE_INT } },
    { "اقرأ_سطر",  TYPE_STRING,  1, { TYPE_POINTER, TYPE_INT,     TYPE_INT } },
    { "اكتب_سطر",  TYPE_INT,     2, { TYPE_POINTER, TYPE_STRING,  TYPE_INT } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_file_func, BuiltinFileFuncSig, builtin_file_funcs)

static DataType infer_type_allow_null_string(Node* expr, DataType expected)
{
    if (!expr) return TYPE_INT;
    if (expected == TYPE_STRING && expr->type == NODE_NULL) {
        // سماح: نص قابل للإلغاء (NULL) — يُستخدم مثلاً لتمثيل EOF في اقرأ_سطر.
        expr->inferred_type = TYPE_STRING;
        node_clear_inferred_ptr(expr);
        return TYPE_STRING;
    }
    return infer_type(expr);
}

/**
 * @brief التحقق من صحة استدعاء دوال الملفات المدمجة في v0.3.12.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_file_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinFileFuncSig* sig = builtin_lookup_file_func(fname);
    if (!sig) return false;

    int i = 0;
    for (Node* arg = args; arg; arg = arg->next, i++) {
        DataType expected = (i < sig->param_count) ? sig->param_types[i] : TYPE_INT;
        DataType got = infer_type_allow_null_string(arg, expected);

        if (i < sig->param_count) {
            if (expected == TYPE_POINTER) {
                // جميع معاملات المؤشر هنا هي عدم* (void*)، ويُسمح بالتحويلات الضمنية وفق قواعد void*.
                if (!builtin_arg_is_void_ptr_compatible(arg, got)) {
                    builtin_report_param_type_mismatch(arg, i + 1, sig->name);
                }
            } else if (!types_compatible(got, expected)) {
                builtin_report_param_type_mismatch(arg, i + 1, sig->name);
            } else {
                maybe_warn_implicit_narrowing(got, expected, arg);
            }
        }
    }

    if (i != sig->param_count) {
        builtin_report_param_count_mismatch(call_node, sig->name, sig->param_count);
    }

    builtin_finalize_call_return(call_node, sig->return_type, out_return_type, true);
    return true;
}

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[2];
} BuiltinMathFuncSig;

static const BuiltinMathFuncSig builtin_math_funcs[] = {
    { "جذر_تربيعي", TYPE_FLOAT, 1, { TYPE_FLOAT, TYPE_INT } },
    { "أس",         TYPE_FLOAT, 2, { TYPE_FLOAT, TYPE_FLOAT } },
    { "جيب",        TYPE_FLOAT, 1, { TYPE_FLOAT, TYPE_INT } },
    { "جيب_تمام",   TYPE_FLOAT, 1, { TYPE_FLOAT, TYPE_INT } },
    { "ظل",         TYPE_FLOAT, 1, { TYPE_FLOAT, TYPE_INT } },
    { "مطلق",       TYPE_INT,   1, { TYPE_INT,   TYPE_INT } },
    { "عشوائي",     TYPE_INT,   0, { TYPE_INT,   TYPE_INT } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_math_func, BuiltinMathFuncSig, builtin_math_funcs)

/**
 * @brief التحقق من استدعاءات دوال الرياضيات المدمجة في v0.4.1.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_math_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinMathFuncSig* sig = builtin_lookup_math_func(fname);
    if (!sig) return false;

    builtin_check_args_scaffold(call_node, sig->name, args, sig->param_types, sig->param_count, false, true);

    builtin_finalize_call_return(call_node, sig->return_type, out_return_type, false);
    return true;
}

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[1];
} BuiltinSystemFuncSig;

static const BuiltinSystemFuncSig builtin_system_funcs[] = {
    { "متغير_بيئة", TYPE_STRING, 1, { TYPE_STRING } },
    { "نفذ_أمر",    TYPE_INT,    1, { TYPE_STRING } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_system_func, BuiltinSystemFuncSig, builtin_system_funcs)

/**
 * @brief التحقق من استدعاءات دوال النظام المدمجة في v0.4.1.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_system_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinSystemFuncSig* sig = builtin_lookup_system_func(fname);
    if (!sig) return false;

    builtin_check_args_scaffold(call_node, sig->name, args, sig->param_types, sig->param_count, true, false);

    builtin_finalize_call_return(call_node, sig->return_type, out_return_type, false);
    return true;
}

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[1];
} BuiltinTimeFuncSig;

static const BuiltinTimeFuncSig builtin_time_funcs[] = {
    { "وقت_حالي", TYPE_INT,    0, { TYPE_INT } },
    { "وقت_كنص",  TYPE_STRING, 1, { TYPE_INT } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_time_func, BuiltinTimeFuncSig, builtin_time_funcs)

/**
 * @brief التحقق من استدعاءات دوال الوقت المدمجة في v0.4.1.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_time_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinTimeFuncSig* sig = builtin_lookup_time_func(fname);
    if (!sig) return false;

    builtin_check_args_scaffold(call_node, sig->name, args, sig->param_types, sig->param_count, false, true);

    builtin_finalize_call_return(call_node, sig->return_type, out_return_type, false);
    return true;
}

typedef struct {
    const char* name;
    DataType return_type;
    int param_count;
    DataType param_types[2];
} BuiltinErrorFuncSig;

static const BuiltinErrorFuncSig builtin_error_funcs[] = {
    { "تأكد",               TYPE_VOID,   2, { TYPE_BOOL,   TYPE_STRING } },
    { "توقف_فوري",          TYPE_VOID,   1, { TYPE_STRING, TYPE_INT } },
    { "كود_خطأ_النظام",     TYPE_INT,    0, { TYPE_INT,    TYPE_INT } },
    { "ضبط_كود_خطأ_النظام", TYPE_VOID,   1, { TYPE_INT,    TYPE_INT } },
    { "نص_كود_خطأ",         TYPE_STRING, 1, { TYPE_INT,    TYPE_INT } },
};

DEFINE_BUILTIN_LOOKUP(builtin_lookup_error_func, BuiltinErrorFuncSig, builtin_error_funcs)

#undef DEFINE_BUILTIN_LOOKUP
#undef ANALYSIS_ARRAY_LEN

/**
 * @brief التحقق من استدعاءات دوال معالجة الأخطاء المدمجة في v0.4.3.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_error_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinErrorFuncSig* sig = builtin_lookup_error_func(fname);
    if (!sig) return false;

    builtin_check_args_scaffold(call_node, sig->name, args, sig->param_types, sig->param_count, true, true);

    builtin_finalize_call_return(call_node, sig->return_type, out_return_type, false);
    return true;
}

static bool variadic_extra_arg_type_supported(DataType t)
{
    if (t == TYPE_VOID || t == TYPE_STRUCT || t == TYPE_UNION || t == TYPE_FUNC_PTR) return false;
    return true;
}

static bool inline_asm_constraint_supported(const char* constraint, bool is_output, char* out_reg_code)
{
    if (out_reg_code) *out_reg_code = '\0';
    if (!constraint || !constraint[0]) return false;

    const char* p = constraint;
    if (is_output) {
        if (p[0] != '=') return false;
        p++;
    } else if (p[0] == '=') {
        return false;
    }

    if (p[0] == '\0' || p[1] != '\0') return false;

    if (p[0] != 'a' && p[0] != 'c' && p[0] != 'd') {
        return false;
    }

    if (out_reg_code) *out_reg_code = p[0];
    return true;
}

static bool inline_asm_expr_is_lvalue(Node* expr)
{
    if (!expr) return false;
    if (expr->type == NODE_VAR_REF) return true;
    if (expr->type == NODE_MEMBER_ACCESS) return true;
    if (expr->type == NODE_ARRAY_ACCESS) return true;
    if (expr->type == NODE_UNARY_OP && expr->data.unary_op.op == UOP_DEREF) return true;
    return false;
}

static void analyze_inline_asm_operands(Node* stmt, Node* list, bool is_output)
{
    for (Node* op = list; op; op = op->next) {
        if (op->type != NODE_ASM_OPERAND) {
            semantic_error(stmt, "تركيب جملة 'مجمع' غير صالح.");
            continue;
        }

        const char* constraint = op->data.asm_operand.constraint;
        if (!inline_asm_constraint_supported(constraint, is_output, NULL)) {
            semantic_error(op,
                           is_output
                               ? "قيد خرج غير مدعوم في 'مجمع' (المتاح حالياً: =a، =c، =d)."
                               : "قيد إدخال غير مدعوم في 'مجمع' (المتاح حالياً: a، c، d).");
        }

        Node* expr = op->data.asm_operand.expression;
        if (!expr) {
            semantic_error(op, "معامل 'مجمع' يتطلب تعبيراً بين القوسين.");
            continue;
        }

        DataType t = infer_type(expr);

        if (is_output && !inline_asm_expr_is_lvalue(expr)) {
            semantic_error(expr, "خرج 'مجمع' يجب أن يكون قيمة قابلة للإسناد.");
        }

        if (!(datatype_is_intlike(t) || t == TYPE_POINTER)) {
            semantic_error(expr, "معاملات 'مجمع' تدعم القيم الصحيحة أو المؤشرات فقط.");
        }
    }
}

static bool builtin_validate_variadic_cursor_arg(Node* arg, const char* fname)
{
    if (!arg) {
        semantic_error(NULL, "نداء '%s': وسيط قائمة المعاملات مفقود.", fname ? fname : "???");
        return false;
    }

    DataType t = infer_type(arg);
    bool ok = true;

    if (arg->type != NODE_VAR_REF || !arg->data.var_ref.name) {
        semantic_error(arg, "نداء '%s': وسيط قائمة المعاملات يجب أن يكون متغيراً.", fname ? fname : "???");
        ok = false;
    }

    if (t != TYPE_POINTER) {
        semantic_error(arg, "نداء '%s': وسيط قائمة المعاملات يجب أن يكون من نوع مؤشر.", fname ? fname : "???");
        ok = false;
    }

    if (arg->type == NODE_VAR_REF && arg->data.var_ref.name) {
        Symbol* s = lookup(arg->data.var_ref.name, false);
        if (s && s->is_const) {
            semantic_error(arg, "نداء '%s': لا يمكن تمرير متغير ثابت كقائمة معاملات.", fname ? fname : "???");
            ok = false;
        }
    }

    return ok;
}

/**
 * @brief التحقق من دوال المعاملات المتغيرة (v0.4.0.5):
 * - بدء_معاملات(قائمة، آخر_ثابت)
 * - معامل_تالي(قائمة، نوع)
 * - نهاية_معاملات(قائمة)
 */
static bool builtin_check_variadic_runtime_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    if (!call_node || !fname) return false;

    bool is_va_start = (strcmp(fname, "بدء_معاملات") == 0);
    bool is_va_next = (strcmp(fname, "معامل_تالي") == 0);
    bool is_va_end = (strcmp(fname, "نهاية_معاملات") == 0);

    if (!is_va_start && !is_va_next && !is_va_end) {
        return false;
    }

    if (!current_func_is_variadic) {
        semantic_error(call_node, "استدعاء '%s' مسموح فقط داخل دالة متغيرة المعاملات.", fname);
    }

    int supplied = 0;
    for (Node* a = args; a; a = a->next) supplied++;

    if (is_va_start) {
        if (supplied != 2) {
            semantic_error(call_node, "نداء 'بدء_معاملات' يتطلب معاملين.");
        }

        Node* a0 = args;
        Node* a1 = a0 ? a0->next : NULL;
        (void)builtin_validate_variadic_cursor_arg(a0, fname);

        if (!a1) {
            semantic_error(call_node, "نداء 'بدء_معاملات': المعامل الثاني (آخر_ثابت) مفقود.");
        } else {
            (void)infer_type(a1);
            if (a1->type != NODE_VAR_REF || !a1->data.var_ref.name) {
                semantic_error(a1, "نداء 'بدء_معاملات': المعامل الثاني يجب أن يكون اسم آخر معامل ثابت.");
            } else if (!current_func_variadic_anchor_name) {
                semantic_error(a1, "نداء 'بدء_معاملات': لا يوجد معامل ثابت في الدالة الحالية.");
            } else if (strcmp(a1->data.var_ref.name, current_func_variadic_anchor_name) != 0) {
                semantic_error(a1,
                               "نداء 'بدء_معاملات': المعامل الثاني يجب أن يكون '%s'.",
                               current_func_variadic_anchor_name);
            }
        }

        if (out_return_type) *out_return_type = TYPE_VOID;
        node_clear_inferred_ptr(call_node);
        return true;
    }

    if (is_va_end) {
        if (supplied != 1) {
            semantic_error(call_node, "نداء 'نهاية_معاملات' يتطلب معاملاً واحداً.");
        }
        Node* a0 = args;
        (void)builtin_validate_variadic_cursor_arg(a0, fname);
        if (out_return_type) *out_return_type = TYPE_VOID;
        node_clear_inferred_ptr(call_node);
        return true;
    }

    // معامل_تالي
    if (supplied != 2) {
        semantic_error(call_node, "نداء 'معامل_تالي' يتطلب معاملين.");
    }

    Node* a0 = args;
    Node* a1 = a0 ? a0->next : NULL;
    (void)builtin_validate_variadic_cursor_arg(a0, fname);

    DataType want = TYPE_INT;
    bool have_type_arg = false;
    if (!a1) {
        semantic_error(call_node, "نداء 'معامل_تالي': معامل النوع مفقود.");
    } else if (a1->type != NODE_SIZEOF || !a1->data.sizeof_expr.has_type_form) {
        // ملاحظة: المعامل الثاني يُحلّل نحوياً كنوع خاص داخل parser.
        semantic_error(a1, "نداء 'معامل_تالي': المعامل الثاني يجب أن يكون نوعاً (مثال: صحيح).");
    } else {
        want = a1->data.sizeof_expr.target_type;
        have_type_arg = true;
        if (!variadic_extra_arg_type_supported(want) || want == TYPE_FUNC_PTR) {
            semantic_error(a1, "نداء 'معامل_تالي': نوع غير مدعوم في هذا الإصدار.");
        }
    }

    if (out_return_type) *out_return_type = want;
    if (have_type_arg && want == TYPE_POINTER) {
        node_set_inferred_ptr(call_node, TYPE_INT, NULL, 1);
    } else {
        node_clear_inferred_ptr(call_node);
    }
    return true;
}

