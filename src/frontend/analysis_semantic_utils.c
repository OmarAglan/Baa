// ============================================================================
// دوال التحليل (Analysis Functions)
// ============================================================================

// تصريح مسبق للدالة العودية
static void analyze_node(Node* node);

/**
 * @brief تحويل نوع البيانات إلى نص.
 */
static const char* datatype_to_str(DataType type) {
    switch (type) {
        case TYPE_INT: return "I64";
        case TYPE_I8: return "I8";
        case TYPE_I16: return "I16";
        case TYPE_I32: return "I32";
        case TYPE_U8: return "U8";
        case TYPE_U16: return "U16";
        case TYPE_U32: return "U32";
        case TYPE_U64: return "U64";
        case TYPE_STRING: return "STRING";
        case TYPE_POINTER: return "POINTER";
        case TYPE_FUNC_PTR: return "FUNC_PTR";
        case TYPE_BOOL: return "BOOLEAN";
        case TYPE_CHAR: return "CHAR";
        case TYPE_FLOAT: return "FLOAT";
        case TYPE_VOID: return "VOID";
        case TYPE_ENUM: return "ENUM";
        case TYPE_STRUCT: return "STRUCT";
        case TYPE_UNION: return "UNION";
        default: return "UNKNOWN";
    }
}

static bool datatype_is_int(DataType t)
{
    switch (t) {
        case TYPE_INT:
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
            return true;
        default:
            return false;
    }
}

static bool datatype_is_intlike(DataType t)
{
    return datatype_is_int(t) || t == TYPE_BOOL || t == TYPE_CHAR || t == TYPE_ENUM;
}

static bool datatype_is_unsigned_int(DataType t)
{
    return t == TYPE_U8 || t == TYPE_U16 || t == TYPE_U32 || t == TYPE_U64;
}

static int datatype_int_rank(DataType t)
{
    // ترتيب بسيط على نمط C: 32-بت ثم 64-بت.
    // نعتبر "صحيح" (TYPE_INT) هو ص٦٤.
    switch (t)
    {
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_ENUM:
            return 32;

        case TYPE_INT:
        case TYPE_U64:
            return 64;

        default:
            return 0;
    }
}

// ترقية الأعداد الصحيحة (Integer promotions) على نمط C.
// نُرقّي الأنواع الأصغر من 32-بت (ومنطقي/حرف/تعداد) إلى ص٣٢.
static DataType datatype_int_promote(DataType t)
{
    switch (t)
    {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_ENUM:
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
            return TYPE_I32;

        case TYPE_I32:
        case TYPE_U32:
        case TYPE_INT:
        case TYPE_U64:
            return t;

        default:
            return t;
    }
}

static DataType datatype_unsigned_for_rank(int rank)
{
    return (rank >= 64) ? TYPE_U64 : TYPE_U32;
}

// التحويلات الحسابية المعتادة (Usual arithmetic conversions) على نمط C.
static DataType datatype_usual_arith(DataType a, DataType b)
{
    DataType pa = datatype_int_promote(a);
    DataType pb = datatype_int_promote(b);

    if (pa == pb)
        return pa;

    // في هذه المرحلة نتعامل فقط مع أنواع عددية صحيحة (بما فيها u/s).
    int ra = datatype_int_rank(pa);
    int rb = datatype_int_rank(pb);
    if (ra == 0 || rb == 0)
        return TYPE_INT;

    bool ua = datatype_is_unsigned_int(pa);
    bool ub = datatype_is_unsigned_int(pb);

    // نفس الإشارة: اختر الأكبر رتبة.
    if (ua == ub)
        return (ra >= rb) ? pa : pb;

    // مختلفا الإشارة: طبق قواعد C.
    DataType u = ua ? pa : pb;
    DataType s = ua ? pb : pa;
    int ru = ua ? ra : rb;
    int rs = ua ? rb : ra;

    if (ru > rs)
        return u;

    if (ru < rs)
    {
        // إذا كانت رتبة الموقّع أكبر وكان عرضه أكبر، يمكنه تمثيل كل قيم غير الموقّع.
        // (نستخدم معياراً بسيطاً: bits(s) > bits(u) => قابل للتمثيل).
        if (rs > ru)
            return s;
    }

    // وإلا: حوّل إلى غير موقّع من رتبة الموقّع.
    return datatype_unsigned_for_rank(rs);
}

static bool types_compatible(DataType got, DataType expected)
{
    if (got == expected) return true;
    if (expected == TYPE_POINTER && got == TYPE_POINTER) return true;
    if (expected == TYPE_FUNC_PTR || got == TYPE_FUNC_PTR) {
        // مؤشرات الدوال لا تدعم تحويلات ضمنية رقمية؛ التوافق الفعلي (التوقيع) يُفحص في مواقع محددة.
        return got == expected;
    }
    if (got == TYPE_VOID || expected == TYPE_VOID) return false;

    // تحويلات C-like بين الأنواع العددية.
    if (datatype_is_int(expected) && (datatype_is_int(got) || got == TYPE_BOOL || got == TYPE_CHAR)) return true;
    if (expected == TYPE_BOOL && (datatype_is_int(got) || got == TYPE_BOOL || got == TYPE_CHAR)) return true;
    if (expected == TYPE_CHAR && (datatype_is_int(got) || got == TYPE_BOOL)) return true;
    if (datatype_is_int(expected) && got == TYPE_FLOAT) return true;
    if (expected == TYPE_FLOAT && (datatype_is_int(got) || got == TYPE_BOOL || got == TYPE_CHAR)) return true;
    return false;
}

static bool types_compatible_named(DataType got, const char* got_name,
                                  DataType expected, const char* expected_name)
{
    if (expected == TYPE_ENUM) {
        if (got != TYPE_ENUM) return false;
        if (!got_name || !expected_name) return false;
        return strcmp(got_name, expected_name) == 0;
    }
    if (expected == TYPE_STRUCT) {
        if (got != TYPE_STRUCT) return false;
        if (!got_name || !expected_name) return false;
        return strcmp(got_name, expected_name) == 0;
    }
    if (expected == TYPE_UNION) {
        if (got != TYPE_UNION) return false;
        if (!got_name || !expected_name) return false;
        return strcmp(got_name, expected_name) == 0;
    }
    return types_compatible(got, expected);
}

// ============================================================================
// تقييم تعبير ثابت للأعداد (لتهيئة العوام)
// ============================================================================

static uint64_t u64_add_wrap(uint64_t a, uint64_t b) { return a + b; }
static uint64_t u64_sub_wrap(uint64_t a, uint64_t b) { return a - b; }
static uint64_t u64_mul_wrap(uint64_t a, uint64_t b) { return a * b; }

static bool eval_const_float_expr(Node* expr, double* out_value);

/**
 * @brief محاولة تقييم تعبير كـ ثابت عدد صحيح (int64) بأسلوب C (مع التفاف 2's complement).
 *
 * هذا يُستخدم لفرض أن تهيئة المصفوفات/المتغيرات العامة قابلة للإصدار في قسم .data.
 */
static bool eval_const_int_expr(Node* expr, int64_t* out_value)
{
    if (!out_value) return false;
    *out_value = 0;
    if (!expr) return true;

    switch (expr->type)
    {
        case NODE_INT:
            *out_value = (int64_t)expr->data.integer.value;
            return true;

        case NODE_BOOL:
            *out_value = expr->data.bool_lit.value ? 1 : 0;
            return true;

        case NODE_CHAR:
            *out_value = (int64_t)expr->data.char_lit.value;
            return true;

        case NODE_MEMBER_ACCESS:
            if (expr->data.member_access.is_enum_value) {
                *out_value = expr->data.member_access.enum_value;
                return true;
            }
            return false;

        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP:
        {
            // postfix ++/-- ليست تعبيراً ثابتاً صالحاً في التهيئة العامة.
            if (expr->type == NODE_POSTFIX_OP) return false;

            int64_t v = 0;
            if (!eval_const_int_expr(expr->data.unary_op.operand, &v)) return false;

            if (expr->data.unary_op.op == UOP_NEG)
            {
                uint64_t uv = (uint64_t)v;
                *out_value = (int64_t)u64_sub_wrap(0u, uv);
                return true;
            }
            if (expr->data.unary_op.op == UOP_NOT)
            {
                *out_value = (v == 0) ? 1 : 0;
                return true;
            }
            if (expr->data.unary_op.op == UOP_BIT_NOT)
            {
                *out_value = (int64_t)(~(uint64_t)v);
                return true;
            }

            return false;
        }

        case NODE_BIN_OP:
        {
            int64_t a = 0;
            int64_t b = 0;
            if (!eval_const_int_expr(expr->data.bin_op.left, &a)) return false;
            if (!eval_const_int_expr(expr->data.bin_op.right, &b)) return false;

            uint64_t ua = (uint64_t)a;
            uint64_t ub = (uint64_t)b;

            switch (expr->data.bin_op.op)
            {
                case OP_ADD: *out_value = (int64_t)u64_add_wrap(ua, ub); return true;
                case OP_SUB: *out_value = (int64_t)u64_sub_wrap(ua, ub); return true;
                case OP_MUL: *out_value = (int64_t)u64_mul_wrap(ua, ub); return true;

                case OP_DIV:
                    if (b == 0) return false;
                    if (a == INT64_MIN && b == -1) { *out_value = INT64_MIN; return true; }
                    *out_value = a / b;
                    return true;

                case OP_MOD:
                    if (b == 0) return false;
                    if (a == INT64_MIN && b == -1) { *out_value = 0; return true; }
                    *out_value = a % b;
                    return true;

                case OP_EQ:  *out_value = (a == b) ? 1 : 0; return true;
                case OP_NEQ: *out_value = (a != b) ? 1 : 0; return true;
                case OP_LT:  *out_value = (a <  b) ? 1 : 0; return true;
                case OP_GT:  *out_value = (a >  b) ? 1 : 0; return true;
                case OP_LTE: *out_value = (a <= b) ? 1 : 0; return true;
                case OP_GTE: *out_value = (a >= b) ? 1 : 0; return true;

                case OP_AND: *out_value = ((a != 0) && (b != 0)) ? 1 : 0; return true;
                case OP_OR:  *out_value = ((a != 0) || (b != 0)) ? 1 : 0; return true;
                case OP_BIT_AND: *out_value = (int64_t)(ua & ub); return true;
                case OP_BIT_OR:  *out_value = (int64_t)(ua | ub); return true;
                case OP_BIT_XOR: *out_value = (int64_t)(ua ^ ub); return true;
                case OP_SHL:
                    if (b < 0 || b >= 64) return false;
                    *out_value = (int64_t)(ua << (unsigned)b);
                    return true;
                case OP_SHR:
                    if (b < 0 || b >= 64) return false;
                    *out_value = (a >> (unsigned)b);
                    return true;

                default:
                    return false;
            }
        }

        case NODE_CAST:
        {
            if (!expr->data.cast_expr.expression) return false;

            DataType target = expr->data.cast_expr.target_type;
            int64_t iv = 0;
            if (eval_const_int_expr(expr->data.cast_expr.expression, &iv)) {
                *out_value = iv;
                return target != TYPE_FLOAT && target != TYPE_STRUCT && target != TYPE_UNION && target != TYPE_VOID;
            }

            double fv = 0.0;
            if (eval_const_float_expr(expr->data.cast_expr.expression, &fv)) {
                *out_value = (int64_t)fv;
                return target != TYPE_STRUCT && target != TYPE_UNION && target != TYPE_VOID;
            }

            return false;
        }

        default:
            return false;
    }
}

static bool datatype_is_numeric_scalar(DataType t)
{
    return datatype_is_intlike(t) || t == TYPE_FLOAT;
}

static bool datatype_signed_unsigned_compare_after_promotion(DataType left, DataType right)
{
    DataType pl = datatype_int_promote(left);
    DataType pr = datatype_int_promote(right);
    if (!datatype_is_int(pl) || !datatype_is_int(pr)) {
        return false;
    }
    return datatype_is_unsigned_int(pl) != datatype_is_unsigned_int(pr);
}

static bool datatype_int_info(DataType t, int* out_bits, bool* out_unsigned)
{
    if (!out_bits || !out_unsigned) return false;

    switch (t) {
        case TYPE_BOOL:
            *out_bits = 1;
            *out_unsigned = true;
            return true;
        case TYPE_I8:
            *out_bits = 8;
            *out_unsigned = false;
            return true;
        case TYPE_U8:
            *out_bits = 8;
            *out_unsigned = true;
            return true;
        case TYPE_I16:
            *out_bits = 16;
            *out_unsigned = false;
            return true;
        case TYPE_U16:
            *out_bits = 16;
            *out_unsigned = true;
            return true;
        case TYPE_I32:
            *out_bits = 32;
            *out_unsigned = false;
            return true;
        case TYPE_U32:
            *out_bits = 32;
            *out_unsigned = true;
            return true;
        case TYPE_INT:
            *out_bits = 64;
            *out_unsigned = false;
            return true;
        case TYPE_U64:
            *out_bits = 64;
            *out_unsigned = true;
            return true;
        case TYPE_CHAR:
            *out_bits = 64;
            *out_unsigned = true;
            return true;
        case TYPE_ENUM:
            *out_bits = 64;
            *out_unsigned = false;
            return true;
        default:
            return false;
    }
}

static bool int64_fits_in_datatype(int64_t value, DataType dst_type)
{
    int bits = 0;
    bool is_unsigned = false;
    if (!datatype_int_info(dst_type, &bits, &is_unsigned)) {
        return false;
    }

    if (is_unsigned) {
        if (value < 0) {
            return false;
        }
        if (bits >= 64) {
            return true;
        }
        uint64_t max_value = (1ULL << (unsigned)bits) - 1ULL;
        return (uint64_t)value <= max_value;
    }

    if (bits >= 64) {
        return true;
    }

    int64_t min_value = -(1LL << (unsigned)(bits - 1));
    int64_t max_value = (1LL << (unsigned)(bits - 1)) - 1LL;
    return value >= min_value && value <= max_value;
}

static bool eval_const_float_expr(Node* expr, double* out_value)
{
    if (!expr || !out_value) return false;

    switch (expr->type)
    {
        case NODE_FLOAT:
            *out_value = expr->data.float_lit.value;
            return true;

        case NODE_UNARY_OP:
            if (expr->data.unary_op.op == UOP_NEG) {
                double v = 0.0;
                if (!eval_const_float_expr(expr->data.unary_op.operand, &v)) return false;
                *out_value = -v;
                return true;
            }
            return false;

        case NODE_CAST:
            if (!expr->data.cast_expr.expression) return false;
            if (expr->data.cast_expr.target_type == TYPE_FLOAT) {
                double fv = 0.0;
                if (eval_const_float_expr(expr->data.cast_expr.expression, &fv)) {
                    *out_value = fv;
                    return true;
                }
                int64_t iv = 0;
                if (eval_const_int_expr(expr->data.cast_expr.expression, &iv)) {
                    *out_value = (double)iv;
                    return true;
                }
            }
            return false;

        default:
            return false;
    }
}

/**
 * @brief التحقق من صلاحية مُهيّئ تخزين ساكن كتعريف ثابت وقت الترجمة.
 */
static bool static_storage_initializer_is_const(Node* expr, DataType decl_type)
{
    if (!expr) return true;

    if (decl_type == TYPE_FUNC_PTR) {
        // نسمح بشكل محافظ بتهيئة ثابتة لمؤشر الدالة فقط من:
        // - عدم (NULL)
        // - اسم دالة (مرجع دالة) مثل: دالة(...) -> صحيح ف = جمع.
        if (expr->type == NODE_NULL) return true;
        if (expr->type == NODE_VAR_REF && expr->data.var_ref.name) {
            return func_lookup(expr->data.var_ref.name) != NULL;
        }
        return false;
    }

    if (decl_type == TYPE_STRING) {
        return expr->type == NODE_STRING;
    }

    if (decl_type == TYPE_FLOAT) {
        double fv = 0.0;
        if (eval_const_float_expr(expr, &fv)) return true;

        int64_t iv = 0;
        return eval_const_int_expr(expr, &iv);
    }

    if (decl_type == TYPE_STRUCT || decl_type == TYPE_UNION || decl_type == TYPE_VOID) {
        return false;
    }

    int64_t iv = 0;
    return eval_const_int_expr(expr, &iv);
}

static bool float_const_is_exact_int64(double value, int64_t* out_value)
{
    if (!out_value) return false;
    if (value < (double)INT64_MIN || value > (double)INT64_MAX) {
        return false;
    }

    int64_t iv = (int64_t)value;
    if ((double)iv != value) {
        return false;
    }

    *out_value = iv;
    return true;
}

static bool implicit_conversion_may_lose_data(DataType src_type, DataType dst_type, Node* expr)
{
    if (src_type == dst_type) return false;
    if (!datatype_is_numeric_scalar(src_type) || !datatype_is_numeric_scalar(dst_type)) return false;

    if (src_type == TYPE_FLOAT && dst_type == TYPE_FLOAT) return false;

    if (src_type == TYPE_FLOAT) {
        if (!datatype_is_intlike(dst_type)) {
            return false;
        }

        double fv = 0.0;
        int64_t iv = 0;
        if (eval_const_float_expr(expr, &fv) &&
            float_const_is_exact_int64(fv, &iv) &&
            int64_fits_in_datatype(iv, dst_type)) {
            return false;
        }

        return true;
    }

    if (dst_type == TYPE_FLOAT) {
        int src_bits = 0;
        bool src_unsigned = false;
        if (!datatype_int_info(src_type, &src_bits, &src_unsigned)) {
            return false;
        }

        if (src_bits <= 53) {
            return false;
        }

        int64_t iv = 0;
        if (eval_const_int_expr(expr, &iv)) {
            const int64_t exact_limit = 9007199254740992LL; // 2^53
            if (iv >= -exact_limit && iv <= exact_limit) {
                return false;
            }
        }

        return true;
    }

    int src_bits = 0;
    int dst_bits = 0;
    bool src_unsigned = false;
    bool dst_unsigned = false;

    if (!datatype_int_info(src_type, &src_bits, &src_unsigned) ||
        !datatype_int_info(dst_type, &dst_bits, &dst_unsigned)) {
        return false;
    }

    bool may_lose = false;
    if (src_unsigned == dst_unsigned) {
        may_lose = dst_bits < src_bits;
    } else if (src_unsigned && !dst_unsigned) {
        may_lose = dst_bits <= src_bits;
    } else {
        may_lose = true;
    }

    if (!may_lose) {
        return false;
    }

    int64_t iv = 0;
    if (eval_const_int_expr(expr, &iv) && int64_fits_in_datatype(iv, dst_type)) {
        return false;
    }

    return true;
}

static void maybe_warn_implicit_narrowing(DataType src_type, DataType dst_type, Node* expr)
{
    if (!expr) return;
    if (!implicit_conversion_may_lose_data(src_type, dst_type, expr)) return;

    warning_report(WARN_IMPLICIT_NARROWING,
                   expr->filename ? expr->filename : current_filename,
                   (expr->line > 0) ? expr->line : 1,
                   (expr->col > 0) ? expr->col : 1,
                   "تحويل ضمني قد يسبب فقدان بيانات من %s إلى %s.",
                   datatype_to_str(src_type),
                   datatype_to_str(dst_type));
}

static void maybe_warn_signed_unsigned_compare(DataType left_type, DataType right_type, Node* expr)
{
    if (!expr) return;
    if (!datatype_signed_unsigned_compare_after_promotion(left_type, right_type)) return;

    warning_report(WARN_SIGNED_UNSIGNED_COMPARE,
                   expr->filename ? expr->filename : current_filename,
                   (expr->line > 0) ? expr->line : 1,
                   (expr->col > 0) ? expr->col : 1,
                   "مقارنة بين نوع موقّع وغير موقّع قد تعطي نتائج غير متوقعة.");
}

