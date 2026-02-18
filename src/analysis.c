/**
 * @file analysis.c
 * @brief وحدة التحليل الدلالي (Semantic Analysis).
 * @details تقوم هذه الوحدة بالتحقق من صحة البرنامج قبل توليد الكود، بما في ذلك:
 *          - التحقق من الأنواع (Type Checking)
 *          - التحقق من النطاقات (Scope Analysis)
 *          - التحقق من الثوابت (Const Checking)
 *          - اكتشاف المتغيرات غير المستخدمة (Unused Variables)
 *          - اكتشاف الكود الميت (Dead Code)
 * @version 0.3.2.9.4
 */

#include "baa.h"

#include <limits.h>

// ============================================================================
// جداول الرموز (Symbol Tables)
// ============================================================================

// نستخدم نفس الهيكلية البسيطة المستخدمة سابقاً لضمان التوافق.
// في المستقبل، يمكن استبدالها بجداول تجزئة لدعم نطاقات أعمق بشكل أفضل.

#define ANALYSIS_MAX_SYMBOLS 100
#define ANALYSIS_MAX_SCOPES  64
#define ANALYSIS_MAX_FUNCS   128
#define ANALYSIS_MAX_FUNC_PARAMS 32

static Symbol global_symbols[ANALYSIS_MAX_SYMBOLS];
static int global_count = 0;

static Symbol local_symbols[ANALYSIS_MAX_SYMBOLS];
static int local_count = 0;

typedef struct {
    char* name;         // مملوك (strdup) ويتم تحريره في reset_analysis()
    DataType return_type;
    DataType* param_types; // مملوك (malloc) ويتم تحريره في reset_analysis()
    int param_count;
    bool is_defined;
    const char* decl_file;
    int decl_line;
    int decl_col;
} FuncSymbol;

static FuncSymbol func_symbols[ANALYSIS_MAX_FUNCS];
static int func_count = 0;

// مكدس النطاقات المحلية: نخزن قيمة local_count عند دخول نطاق جديد.
static int scope_stack[ANALYSIS_MAX_SCOPES];
static int scope_depth = 0;

static bool has_error = false;
static bool inside_loop = false;
static bool inside_switch = false;

// معلومات الملف الحالي (للتحذيرات)
static const char* current_filename = NULL;

static bool in_function = false;
static DataType current_func_return_type = TYPE_INT;

// ============================================================================
// دوال مساعدة (Helper Functions)
// ============================================================================

static Token semantic_make_token(const char* filename, int line, int col)
{
    Token t;
    memset(&t, 0, sizeof(t));
    t.type = TOKEN_INVALID;
    t.value = NULL;
    t.filename = filename ? filename : (current_filename ? current_filename : "unknown");
    t.line = (line > 0) ? line : 1;
    t.col = (col > 0) ? col : 1;
    return t;
}

/**
 * @brief الإبلاغ عن خطأ دلالي بمعلومات موقع.
 */
static void semantic_error(Node* node, const char* message, ...)
{
    has_error = true;

    Token tok = semantic_make_token(
        node ? node->filename : NULL,
        node ? node->line : 1,
        node ? node->col : 1
    );

    char buf[1024];
    va_list args;
    va_start(args, message);
    (void)vsnprintf(buf, sizeof(buf), message, args);
    va_end(args);

    error_report(tok, "خطأ دلالي: %s", buf);
}

static void semantic_error_loc(const char* filename, int line, int col, const char* message, ...)
{
    has_error = true;

    Token tok = semantic_make_token(filename, line, col);

    char buf[1024];
    va_list args;
    va_start(args, message);
    (void)vsnprintf(buf, sizeof(buf), message, args);
    va_end(args);

    error_report(tok, "خطأ دلالي: %s", buf);
}

/**
 * @brief إعادة تعيين جداول الرموز.
 */
static void reset_analysis() {
    global_count = 0;
    local_count = 0;
    scope_depth = 0;
    for (int i = 0; i < func_count; i++) {
        free(func_symbols[i].name);
        func_symbols[i].name = NULL;
        free(func_symbols[i].param_types);
        func_symbols[i].param_types = NULL;
        func_symbols[i].param_count = 0;
        func_symbols[i].is_defined = false;
    }
    func_count = 0;
    has_error = false;
    inside_loop = false;
    inside_switch = false;
    in_function = false;
    current_func_return_type = TYPE_INT;
}

static FuncSymbol* func_lookup(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < func_count; i++) {
        if (func_symbols[i].name && strcmp(func_symbols[i].name, name) == 0) {
            return &func_symbols[i];
        }
    }
    return NULL;
}

static int func_signature_matches(const FuncSymbol* a, DataType ret_type,
                                 const DataType* params, int param_count)
{
    if (!a) return 0;
    if (a->return_type != ret_type) return 0;
    if (a->param_count != param_count) return 0;
    for (int i = 0; i < param_count; i++) {
        if (!a->param_types || a->param_types[i] != params[i]) return 0;
    }
    return 1;
}

static void func_register(Node* node)
{
    if (!node || node->type != NODE_FUNC_DEF) return;
    const char* name = node->data.func_def.name;
    if (!name) {
        semantic_error(node, "اسم الدالة فارغ.");
        return;
    }

    DataType params_local[ANALYSIS_MAX_FUNC_PARAMS];
    int param_count = 0;
    for (Node* p = node->data.func_def.params; p; p = p->next) {
        if (p->type != NODE_VAR_DECL) continue;
        if (param_count >= ANALYSIS_MAX_FUNC_PARAMS) {
            semantic_error(p, "عدد معاملات الدالة كبير جداً (الحد %d).", ANALYSIS_MAX_FUNC_PARAMS);
            return;
        }
        params_local[param_count++] = p->data.var_decl.type;
    }

    FuncSymbol* existing = func_lookup(name);
    if (existing) {
        if (!func_signature_matches(existing, node->data.func_def.return_type, params_local, param_count)) {
            semantic_error(node, "تعارض في توقيع الدالة '%s'.", name);
            return;
        }
        if (!node->data.func_def.is_prototype) {
            if (existing->is_defined) {
                semantic_error(node, "إعادة تعريف الدالة '%s'.", name);
                return;
            }
            existing->is_defined = true;
        }
        return;
    }

    if (func_count >= ANALYSIS_MAX_FUNCS) {
        semantic_error(node, "عدد الدوال كبير جداً.");
        return;
    }

    FuncSymbol* fs = &func_symbols[func_count++];
    memset(fs, 0, sizeof(*fs));
    fs->name = strdup(name);
    if (!fs->name) {
        semantic_error(node, "نفدت الذاكرة أثناء تسجيل اسم الدالة.");
        return;
    }
    fs->return_type = node->data.func_def.return_type;
    fs->param_count = param_count;
    fs->decl_file = node->filename ? node->filename : current_filename;
    fs->decl_line = node->line;
    fs->decl_col = node->col;
    fs->is_defined = node->data.func_def.is_prototype ? false : true;

    if (param_count > 0) {
        fs->param_types = (DataType*)malloc((size_t)param_count * sizeof(DataType));
        if (!fs->param_types) {
            semantic_error(node, "نفدت الذاكرة أثناء تسجيل توقيع الدالة.");
            return;
        }
        for (int i = 0; i < param_count; i++) {
            fs->param_types[i] = params_local[i];
        }
    }
}

// ============================================================================
// إدارة النطاقات المحلية (Local Scope Management)
// ============================================================================

static void check_unused_local_variables_range(int start, int end);

static int current_scope_start(void) {
    if (scope_depth <= 0) return 0;
    return scope_stack[scope_depth - 1];
}

static void scope_push(void) {
    if (scope_depth >= (int)(sizeof(scope_stack) / sizeof(scope_stack[0]))) {
        semantic_error(NULL, "Too many nested scopes.");
        return;
    }
    scope_stack[scope_depth++] = local_count;
}

static void scope_pop(void) {
    if (scope_depth <= 0) return;
    int start = scope_stack[--scope_depth];

    // تحذيرات المتغيرات غير المستخدمة لهذا النطاق فقط
    check_unused_local_variables_range(start, local_count);

    // إخراج رموز النطاق من الجدول (منطقيًا)
    local_count = start;
}

/**
 * @brief إضافة رمز إلى النطاق الحالي.
 */
static void add_symbol(const char* name,
                       ScopeType scope,
                       DataType type,
                       bool is_const,
                       bool is_array,
                       int array_size,
                       int decl_line,
                       int decl_col,
                       const char* decl_file) {
    if (!name) {
        semantic_error_loc(decl_file, decl_line, decl_col, "اسم الرمز فارغ.");
        return;
    }

    size_t name_len = strlen(name);
    size_t name_cap = sizeof(global_symbols[0].name);
    if (name_len >= name_cap) {
        semantic_error_loc(decl_file, decl_line, decl_col,
                           "اسم الرمز طويل جداً: '%s' (الحد الأقصى %zu حرفاً).",
                           name, name_cap - 1);
        return;
    }

    if (scope == SCOPE_GLOBAL) {
        // التحقق من التكرار
        for (int i = 0; i < global_count; i++) {
            if (strcmp(global_symbols[i].name, name) == 0) {
                semantic_error_loc(decl_file, decl_line, decl_col,
                                   "Redefinition of global variable '%s'.", name);
                return;
            }
        }
        if (global_count >= ANALYSIS_MAX_SYMBOLS) {
            semantic_error_loc(decl_file, decl_line, decl_col, "Too many global variables.");
            return;
        }
        
        memcpy(global_symbols[global_count].name, name, name_len + 1);
        global_symbols[global_count].scope = SCOPE_GLOBAL;
        global_symbols[global_count].type = type;
        global_symbols[global_count].is_array = is_array;
        global_symbols[global_count].array_size = is_array ? array_size : 0;
        global_symbols[global_count].is_const = is_const;
        global_symbols[global_count].is_used = false;
        global_symbols[global_count].decl_line = decl_line;
        global_symbols[global_count].decl_col = decl_col;
        global_symbols[global_count].decl_file = decl_file;
        global_count++;
    } else {
        // التحقق من التكرار داخل النطاق الحالي فقط
        int start = current_scope_start();
        for (int i = start; i < local_count; i++) {
            if (strcmp(local_symbols[i].name, name) == 0) {
                semantic_error_loc(decl_file, decl_line, decl_col,
                                   "Redefinition of local variable '%s'.", name);
                return;
            }
        }
        if (local_count >= ANALYSIS_MAX_SYMBOLS) {
            semantic_error_loc(decl_file, decl_line, decl_col, "Too many local variables.");
            return;
        }

        // تحذير إذا كان المتغير المحلي يحجب متغيراً عاماً
        for (int i = 0; i < global_count; i++) {
            if (strcmp(global_symbols[i].name, name) == 0) {
                warning_report(WARN_SHADOW_VARIABLE, decl_file, decl_line, decl_col,
                    "Local variable '%s' shadows global variable.", name);
                break;
            }
        }

        // تحذير إذا كان المتغير المحلي يحجب متغيراً محلياً خارجياً
        for (int i = 0; i < start; i++) {
            if (strcmp(local_symbols[i].name, name) == 0) {
                warning_report(WARN_SHADOW_VARIABLE, decl_file, decl_line, decl_col,
                    "Local variable '%s' shadows outer local variable.", name);
                break;
            }
        }

        memcpy(local_symbols[local_count].name, name, name_len + 1);
        local_symbols[local_count].scope = SCOPE_LOCAL;
        local_symbols[local_count].type = type;
        local_symbols[local_count].is_array = is_array;
        local_symbols[local_count].array_size = is_array ? array_size : 0;
        local_symbols[local_count].is_const = is_const;
        local_symbols[local_count].is_used = false;
        local_symbols[local_count].decl_line = decl_line;
        local_symbols[local_count].decl_col = decl_col;
        local_symbols[local_count].decl_file = decl_file;
        local_count++;
    }
}

/**
 * @brief البحث عن رمز بالاسم.
 * @param mark_used إذا كان true، يتم تعليم المتغير كمستخدم.
 */
static Symbol* lookup(const char* name, bool mark_used) {
    // البحث في المحلي أولاً (من الأحدث للأقدم لدعم النطاقات المتداخلة)
    for (int i = local_count - 1; i >= 0; i--) {
        if (strcmp(local_symbols[i].name, name) == 0) {
            if (mark_used) local_symbols[i].is_used = true;
            return &local_symbols[i];
        }
    }
    // البحث في العام
    for (int i = 0; i < global_count; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) {
            if (mark_used) global_symbols[i].is_used = true;
            return &global_symbols[i];
        }
    }
    return NULL;
}

/**
 * @brief التحقق من المتغيرات غير المستخدمة في النطاق المحلي.
 */
static void check_unused_local_variables_range(int start, int end) {
    if (start < 0) start = 0;
    if (end < start) end = start;
    if (end > local_count) end = local_count;

    for (int i = start; i < end; i++) {
        if (!local_symbols[i].is_used) {
            warning_report(WARN_UNUSED_VARIABLE,
                local_symbols[i].decl_file,
                local_symbols[i].decl_line,
                local_symbols[i].decl_col,
                "Variable '%s' is declared but never used.",
                local_symbols[i].name);
        }
    }
}

// ملاحظة: تحذيرات غير المستخدم تُطلق عبر scope_pop() لكل نطاق.

/**
 * @brief التحقق من المتغيرات العامة غير المستخدمة.
 */
static void check_unused_global_variables(void) {
    for (int i = 0; i < global_count; i++) {
        if (!global_symbols[i].is_used) {
            warning_report(WARN_UNUSED_VARIABLE,
                global_symbols[i].decl_file,
                global_symbols[i].decl_line,
                global_symbols[i].decl_col,
                "Global variable '%s' is declared but never used.",
                global_symbols[i].name);
        }
    }
}

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
        case TYPE_INT: return "INTEGER";
        case TYPE_STRING: return "STRING";
        case TYPE_BOOL: return "BOOLEAN";
        default: return "UNKNOWN";
    }
}

static bool types_compatible(DataType got, DataType expected)
{
    if (got == expected) return true;
    if (got == TYPE_BOOL && expected == TYPE_INT) return true;
    if (got == TYPE_INT && expected == TYPE_BOOL) return true;
    return false;
}

// ============================================================================
// تقييم تعبير ثابت للأعداد (لتهيئة العوام)
// ============================================================================

static uint64_t u64_add_wrap(uint64_t a, uint64_t b) { return a + b; }
static uint64_t u64_sub_wrap(uint64_t a, uint64_t b) { return a - b; }
static uint64_t u64_mul_wrap(uint64_t a, uint64_t b) { return a * b; }

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

                default:
                    return false;
            }
        }

        default:
            return false;
    }
}

/**
 * @brief استنتاج نوع التعبير.
 * @return DataType نوع التعبير الناتج.
 */
static DataType infer_type(Node* node) {
    if (!node) return TYPE_INT; // افتراضي لتجنب الانهيار

    switch (node->type) {
        case NODE_INT:
        case NODE_CHAR:
            return TYPE_INT;
        
        case NODE_STRING:
            return TYPE_STRING;
        
        case NODE_BOOL:
            return TYPE_BOOL;
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup(node->data.var_ref.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error(node, "Undefined variable '%s'.", node->data.var_ref.name);
                return TYPE_INT; // استرداد الخطأ
            }
            if (sym->is_array) {
                semantic_error(node, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", node->data.var_ref.name);
                return sym->type;
            }
            return sym->type;
        }

        case NODE_ARRAY_ACCESS: {
            Symbol* sym = lookup(node->data.array_op.name, true);
            if (!sym) {
                semantic_error(node, "مصفوفة غير معرّفة '%s'.", node->data.array_op.name ? node->data.array_op.name : "???");
                (void)infer_type(node->data.array_op.index);
                return TYPE_INT;
            }
            if (!sym->is_array) {
                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                (void)infer_type(node->data.array_op.index);
                return sym->type;
            }

            DataType it = infer_type(node->data.array_op.index);
            if (it != TYPE_INT) {
                semantic_error(node->data.array_op.index, "فهرس المصفوفة يجب أن يكون صحيحاً.");
            }

            // تحقق ثابت على الحدود إذا كان الفهرس ثابتاً
            if (node->data.array_op.index && node->data.array_op.index->type == NODE_INT) {
                int64_t idx = (int64_t)node->data.array_op.index->data.integer.value;
                if (idx < 0 || (sym->array_size > 0 && idx >= sym->array_size)) {
                    semantic_error(node, "فهرس خارج الحدود للمصفوفة '%s' (الحجم %d).", sym->name, sym->array_size);
                }
            }

            return sym->type; // نوع العنصر
        }

        case NODE_CALL_EXPR:
        {
            const char* fname = node->data.call.name;
            FuncSymbol* fs = func_lookup(fname);
            if (!fs) {
                semantic_error(node, "استدعاء دالة غير معرّفة '%s'.", fname ? fname : "???");
                // ما زلنا نستنتج أنواع الوسائط لاكتشاف أخطاء أخرى.
                Node* arg = node->data.call.args;
                while (arg) { (void)infer_type(arg); arg = arg->next; }
                return TYPE_INT;
            }

            // تحقق عدد وأنواع المعاملات
            int i = 0;
            Node* arg = node->data.call.args;
            while (arg) {
                DataType at = infer_type(arg);
                if (i < fs->param_count) {
                    if (!types_compatible(at, fs->param_types[i])) {
                        semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                    }
                }
                i++;
                arg = arg->next;
            }
            if (i != fs->param_count) {
                semantic_error(node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", fs->name, fs->param_count);
            }
            return fs->return_type;
        }

        case NODE_BIN_OP: {
            DataType left = infer_type(node->data.bin_op.left);
            DataType right = infer_type(node->data.bin_op.right);
            
            // العمليات الحسابية تتطلب أعداداً صحيحة
            OpType op = node->data.bin_op.op;
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                if (left != TYPE_INT || right != TYPE_INT) {
                    semantic_error(node, "Arithmetic operations require INTEGER operands.");
                }
                return TYPE_INT;
            }
            
            // عمليات المقارنة تتطلب أنواعاً متوافقة وتعيد منطقي
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                if (left != right) {
                    semantic_error(node, "Comparison operations require matching types.");
                }
                return TYPE_BOOL;
            }
            
            // العمليات المنطقية (AND/OR) تتطلب منطقي أو صحيح وتعيد منطقي
            if (op == OP_AND || op == OP_OR) {
                // نقبل INT أو BOOL لأن C تعامل القيم غير الصفرية كـ true
                if ((left != TYPE_INT && left != TYPE_BOOL) || (right != TYPE_INT && right != TYPE_BOOL)) {
                    semantic_error(node, "Logical operations require INTEGER or BOOLEAN operands.");
                }
                return TYPE_BOOL;
            }
            
            return TYPE_INT;
        }

        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP: {
            if (infer_type(node->data.unary_op.operand) != TYPE_INT) {
                semantic_error(node, "Unary operations require INTEGER operand.");
            }
            return TYPE_INT;
        }

        default:
            return TYPE_INT;
    }
}

/**
 * @brief التحقق مما إذا كانت الجملة توقف التدفق (return/break/continue).
 */
static bool is_terminating_statement(Node* node) {
    if (!node) return false;
    return (node->type == NODE_RETURN ||
            node->type == NODE_BREAK ||
            node->type == NODE_CONTINUE);
}

/**
 * @brief تحليل كتلة من الجمل مع اكتشاف الكود الميت.
 * @param statements قائمة الجمل
 * @param context سياق الكتلة (للرسائل)
 */
static void analyze_statements_with_dead_code_check(Node* statements, const char* context) {
    bool found_terminator = false;
    int terminator_line = 0;
    
    Node* stmt = statements;
    while (stmt) {
        if (found_terminator) {
            // كود ميت بعد return/break/continue
            warning_report(WARN_DEAD_CODE, current_filename, terminator_line, 1,
                "Unreachable code after '%s' statement.", context);
            // نستمر في التحليل لاكتشاف أخطاء أخرى
            found_terminator = false; // تجنب تحذيرات متعددة
        }
        
        analyze_node(stmt);
        
        if (is_terminating_statement(stmt)) {
            found_terminator = true;
            terminator_line = (stmt->line > 0) ? stmt->line : 1;
        }
        
        stmt = stmt->next;
    }
}

static void analyze_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            // تسجيل تواقيع الدوال أولاً لدعم الاستدعاء قبل التعريف
            Node* decl0 = node->data.program.declarations;
            while (decl0) {
                if (decl0->type == NODE_FUNC_DEF) {
                    func_register(decl0);
                }
                decl0 = decl0->next;
            }

            Node* decl = node->data.program.declarations;
            while (decl) {
                analyze_node(decl);
                decl = decl->next;
            }
            // في نهاية البرنامج، التحقق من المتغيرات العامة غير المستخدمة
            check_unused_global_variables();
            break;
        }

        case NODE_VAR_DECL: {
            // 1. التحقق من نوع التعبير المخصص (إن وجد)
            if (node->data.var_decl.expression) {
                DataType exprType = infer_type(node->data.var_decl.expression);
                DataType declType = node->data.var_decl.type;
                
                // السماح بتعيين BOOL إلى INT والعكس (التوافق الضمني)
                bool compatible = (exprType == declType) ||
                                  (exprType == TYPE_BOOL && declType == TYPE_INT) ||
                                  (exprType == TYPE_INT && declType == TYPE_BOOL);
                
                if (!compatible) {
                    semantic_error(node, "Type mismatch in declaration of '%s'. Expected %s but got %s.",
                        node->data.var_decl.name,
                        datatype_to_str(declType),
                        datatype_to_str(exprType));
                }
            }
            // 2. التحقق من أن الثوابت لها قيم ابتدائية
            if (node->data.var_decl.is_const && !node->data.var_decl.expression) {
                semantic_error(node, "Constant '%s' must be initialized.", node->data.var_decl.name);
            }
            // 3. إضافة الرمز (مع معلومات الموقع للتحذيرات)
             add_symbol(node->data.var_decl.name,
                        node->data.var_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                        node->data.var_decl.type,
                        node->data.var_decl.is_const,
                        false,
                        0,
                        node->line, node->col, node->filename ? node->filename : current_filename);
             break;
         }

        case NODE_FUNC_DEF: {
            // الدخول في نطاق دالة جديدة (تصفير المحلي + إنشاء نطاق)
            local_count = 0;
            scope_depth = 0;
            scope_push();

            bool prev_in_func = in_function;
            DataType prev_ret = current_func_return_type;
            in_function = true;
            current_func_return_type = node->data.func_def.return_type;

            // إضافة المعاملات كمتغيرات محلية (المعاملات ليست ثوابت افتراضياً)
            Node* param = node->data.func_def.params;
            while (param) {
                 if (param->type == NODE_VAR_DECL) {
                     // المعاملات تُعتبر "مستخدمة" ضمنياً (لتجنب تحذيرات خاطئة)
                    add_symbol(param->data.var_decl.name, SCOPE_LOCAL, param->data.var_decl.type, false,
                               false, 0,
                               param->line, param->col, param->filename ? param->filename : current_filename);
                     // تعليم المعامل كمستخدم مباشرة
                     local_symbols[local_count - 1].is_used = true;
                 }
                param = param->next;
            }

            // تحليل جسم الدالة فقط إذا لم تكن نموذجاً أولياً
            if (!node->data.func_def.is_prototype) {
                analyze_node(node->data.func_def.body);
            }

            // خروج نطاق الدالة (يشمل تحذيرات غير المستخدم)
            scope_pop();

            in_function = prev_in_func;
            current_func_return_type = prev_ret;
            break;
        }

        case NODE_BLOCK: {
            // دخول/خروج نطاق كتلة
            scope_push();
            analyze_statements_with_dead_code_check(node->data.block.statements, "return/break");
            scope_pop();
            break;
        }

        case NODE_ASSIGN: {
            Symbol* sym = lookup(node->data.assign_stmt.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error(node, "Assignment to undefined variable '%s'.", node->data.assign_stmt.name);
            } else {
                if (sym->is_array) {
                    semantic_error(node, "لا يمكن تعيين قيمة للمصفوفة '%s' مباشرة (استخدم الفهرسة).", sym->name);
                }
                // التحقق من الثوابت: لا يمكن إعادة تعيين قيمة ثابت
                if (sym->is_const) {
                    semantic_error(node, "Cannot reassign constant '%s'.", node->data.assign_stmt.name);
                }
                DataType exprType = infer_type(node->data.assign_stmt.expression);
                if (!types_compatible(exprType, sym->type)) {
                    semantic_error(node, "Type mismatch in assignment to '%s'.", node->data.assign_stmt.name);
                }
            }
            break;
        }

        case NODE_IF: {
            DataType condType = infer_type(node->data.if_stmt.condition);
            if (condType != TYPE_INT && condType != TYPE_BOOL) {
                semantic_error(node->data.if_stmt.condition, "'if' condition must be an integer/boolean.");
            }

            // كل فرع يعتبر نطاقاً مستقلاً حتى بدون أقواس
            scope_push();
            analyze_node(node->data.if_stmt.then_branch);
            scope_pop();

            if (node->data.if_stmt.else_branch) {
                scope_push();
                analyze_node(node->data.if_stmt.else_branch);
                scope_pop();
            }
            break;
        }

        case NODE_WHILE: {
            DataType condType = infer_type(node->data.while_stmt.condition);
            if (condType != TYPE_INT && condType != TYPE_BOOL) {
                semantic_error(node->data.while_stmt.condition, "'while' condition must be an integer/boolean.");
            }
            bool prev_loop = inside_loop;
            inside_loop = true;

            scope_push();
            analyze_node(node->data.while_stmt.body);
            scope_pop();

            inside_loop = prev_loop;
            break;
        }

        case NODE_FOR: {
            // نطاق حلقة for يشمل init/cond/incr/body لمنع تسرب متغيرات init خارج الحلقة.
            scope_push();

            if (node->data.for_stmt.init) analyze_node(node->data.for_stmt.init);
            if (node->data.for_stmt.condition) {
                DataType condType = infer_type(node->data.for_stmt.condition);
                if (condType != TYPE_INT && condType != TYPE_BOOL) {
                    semantic_error(node->data.for_stmt.condition, "'for' condition must be an integer/boolean.");
                }
            }
            if (node->data.for_stmt.increment) analyze_node(node->data.for_stmt.increment);

            bool prev_loop = inside_loop;
            inside_loop = true;
            analyze_node(node->data.for_stmt.body);
            inside_loop = prev_loop;

            scope_pop();
            break;
        }

        case NODE_SWITCH: {
            if (infer_type(node->data.switch_stmt.expression) != TYPE_INT) {
                semantic_error(node->data.switch_stmt.expression, "'switch' expression must be an integer.");
            }
            bool prev_switch = inside_switch;
            inside_switch = true;

            // نطاق switch
            scope_push();
            
            Node* current_case = node->data.switch_stmt.cases;
            while (current_case) {
                if (!current_case->data.case_stmt.is_default) {
                    if (infer_type(current_case->data.case_stmt.value) != TYPE_INT) {
                        semantic_error(current_case->data.case_stmt.value, "'case' value must be an integer constant.");
                    }
                }
                // جسم الحالة عبارة عن قائمة جمل
                scope_push();
                analyze_statements_with_dead_code_check(current_case->data.case_stmt.body, "return/break");
                scope_pop();
                current_case = current_case->next;
            }

            scope_pop();
            
            inside_switch = prev_switch;
            break;
        }

        case NODE_BREAK:
            if (!inside_loop && !inside_switch) {
                semantic_error(node, "'break' used outside of loop or switch.");
            }
            break;

        case NODE_CONTINUE:
            if (!inside_loop) {
                semantic_error(node, "'continue' used outside of loop.");
            }
            break;

        case NODE_RETURN:
            if (node->data.return_stmt.expression) {
                DataType rt = infer_type(node->data.return_stmt.expression);
                if (in_function && !types_compatible(rt, current_func_return_type)) {
                    semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة.");
                }
            }
            break;

        case NODE_PRINT:
            infer_type(node->data.print_stmt.expression);
            break;
        
        case NODE_READ: {
            // التحقق من أن المتغير معرف وقابل للتعديل
            Symbol* sym = lookup(node->data.read_stmt.var_name, true);
            if (!sym) {
                semantic_error(node, "Reading into undefined variable '%s'.", node->data.read_stmt.var_name);
            } else {
                if (sym->is_const) {
                    semantic_error(node, "Cannot read into constant variable '%s'.", node->data.read_stmt.var_name);
                }
                // حالياً نقبل القراءة فقط في المتغيرات الصحيحة
                if (sym->type != TYPE_INT) {
                    semantic_error(node, "'اقرأ' currently only supports INTEGER variables.");
                }
            }
            break;
        }
            
        case NODE_CALL_STMT:
        {
            // تحقق استدعاء دالة كجملة
            const char* fname = node->data.call.name;
            FuncSymbol* fs = func_lookup(fname);
            if (!fs) {
                semantic_error(node, "استدعاء دالة غير معرّفة '%s'.", fname ? fname : "???");
                Node* arg = node->data.call.args;
                while (arg) { (void)infer_type(arg); arg = arg->next; }
                break;
            }

            int i = 0;
            Node* arg = node->data.call.args;
            while (arg) {
                DataType at = infer_type(arg);
                if (i < fs->param_count) {
                    if (!types_compatible(at, fs->param_types[i])) {
                        semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                    }
                }
                i++;
                arg = arg->next;
            }
            if (i != fs->param_count) {
                semantic_error(node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", fs->name, fs->param_count);
            }
            break;
        }

        case NODE_ARRAY_DECL: {
            if (node->data.array_decl.size <= 0) {
                semantic_error(node, "حجم المصفوفة غير صالح.");
                break;
            }

            // الثابت يجب أن يُهيّأ صراحةً (حتى `{}` مقبول ويعني أصفار).
            if (node->data.array_decl.is_const && !node->data.array_decl.has_init) {
                semantic_error(node, "Constant array '%s' must be initialized.",
                               node->data.array_decl.name ? node->data.array_decl.name : "???");
            }

            // حالياً المصفوفات مدعومة فقط لنوع صحيح (حسب الصياغة: صحيح س[5].)
            add_symbol(node->data.array_decl.name,
                       node->data.array_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       TYPE_INT,
                       node->data.array_decl.is_const,
                       true,
                       node->data.array_decl.size,
                       node->line,
                       node->col,
                       node->filename ? node->filename : current_filename);

            // التحقق من قائمة التهيئة: تسمح بالتهيئة الجزئية كما في C.
            if (node->data.array_decl.has_init) {
                int n = node->data.array_decl.init_count;
                int cap = node->data.array_decl.size;
                if (n < 0) n = 0;
                if (n > cap) {
                    semantic_error(node, "عدد عناصر تهيئة المصفوفة '%s' (%d) أكبر من الحجم (%d).",
                                   node->data.array_decl.name ? node->data.array_decl.name : "???",
                                   n, cap);
                }

                int idx0 = 0;
                for (Node* v = node->data.array_decl.init_values; v; v = v->next) {
                    DataType vt = infer_type(v);
                    if (!types_compatible(vt, TYPE_INT)) {
                        semantic_error(v, "نوع عنصر التهيئة %d للمصفوفة '%s' غير متوافق.",
                                       idx0 + 1,
                                       node->data.array_decl.name ? node->data.array_decl.name : "???");
                    }

                    if (node->data.array_decl.is_global) {
                        int64_t c = 0;
                        if (!eval_const_int_expr(v, &c)) {
                            semantic_error(v,
                                           "تهيئة المصفوفة العامة '%s' يجب أن تكون تعبيراً ثابتاً.",
                                           node->data.array_decl.name ? node->data.array_decl.name : "???");
                        }
                    }

                    idx0++;
                }
            }
            break;
        }

        case NODE_ARRAY_ASSIGN: {
            Symbol* sym = lookup(node->data.array_op.name, true);
            if (!sym) {
                semantic_error(node, "تعيين إلى مصفوفة غير معرّفة '%s'.", node->data.array_op.name ? node->data.array_op.name : "???");
                (void)infer_type(node->data.array_op.index);
                (void)infer_type(node->data.array_op.value);
                break;
            }
            if (!sym->is_array) {
                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                (void)infer_type(node->data.array_op.index);
                (void)infer_type(node->data.array_op.value);
                break;
            }
            if (sym->is_const) {
                semantic_error(node, "لا يمكن تعديل مصفوفة ثابتة '%s'.", sym->name);
            }

            DataType it = infer_type(node->data.array_op.index);
            if (it != TYPE_INT) {
                semantic_error(node->data.array_op.index, "فهرس المصفوفة يجب أن يكون صحيحاً.");
            }

            DataType vt = infer_type(node->data.array_op.value);
            if (!types_compatible(vt, sym->type)) {
                semantic_error(node, "نوع القيمة في تعيين '%s' غير متوافق.", sym->name);
            }

            if (node->data.array_op.index && node->data.array_op.index->type == NODE_INT) {
                int64_t idx = (int64_t)node->data.array_op.index->data.integer.value;
                if (idx < 0 || (sym->array_size > 0 && idx >= sym->array_size)) {
                    semantic_error(node, "فهرس خارج الحدود للمصفوفة '%s' (الحجم %d).", sym->name, sym->array_size);
                }
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief نقطة الدخول الرئيسية للتحليل.
 * @param program عقدة البرنامج
 * @return true إذا لم تحدث أخطاء
 */
bool analyze(Node* program) {
    reset_analysis();
    
    // تعيين اسم الملف الحالي (للتحذيرات)
    if (program && program->filename) {
        current_filename = program->filename;
    } else {
        current_filename = "source";
    }
    
    analyze_node(program);
    return !has_error;
}
