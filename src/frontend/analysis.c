/**
 * @file analysis.c
 * @brief وحدة التحليل الدلالي (Semantic Analysis).
 * @details تقوم هذه الوحدة بالتحقق من صحة البرنامج قبل توليد الكود، بما في ذلك:
 *          - التحقق من الأنواع (Type Checking)
 *          - التحقق من النطاقات (Scope Analysis)
 *          - التحقق من الثوابت (Const Checking)
 *          - اكتشاف المتغيرات غير المستخدمة (Unused Variables)
 *          - اكتشاف الكود الميت (Dead Code)
 * @version 0.3.4
 */

#include "frontend_internal.h"

#include <ctype.h>
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
#define ANALYSIS_SYMBOL_HASH_BUCKETS 257

// أنواع مركبة (v0.3.4)
#define ANALYSIS_MAX_ENUMS   128
#define ANALYSIS_MAX_STRUCTS 128
#define ANALYSIS_MAX_ENUM_MEMBERS 128
#define ANALYSIS_MAX_STRUCT_FIELDS 128
#define ANALYSIS_MAX_TYPE_ALIASES 256

typedef struct {
    char* name; // مملوك (strdup)
    int member_count;
    struct {
        char* name;   // مملوك (strdup)
        int64_t value;
    } members[ANALYSIS_MAX_ENUM_MEMBERS];
} EnumDef;

typedef struct {
    char* name;       // مملوك (strdup)
    DataType type;
    char* type_name;  // مملوك (strdup) عند TYPE_ENUM/TYPE_STRUCT
    DataType ptr_base_type;
    char* ptr_base_type_name;
    int ptr_depth;
    bool is_const;
    int offset;
    int size;
    int align;
} StructFieldDef;

typedef struct {
    char* name; // مملوك (strdup)
    int field_count;
    StructFieldDef fields[ANALYSIS_MAX_STRUCT_FIELDS];
    int size;
    int align;
    bool layout_done;
    bool layout_in_progress;
} StructDef;

typedef struct {
    char* name; // مملوك (strdup)
    int field_count;
    StructFieldDef fields[ANALYSIS_MAX_STRUCT_FIELDS];
    int size;
    int align;
    bool layout_done;
    bool layout_in_progress;
} UnionDef;

typedef struct {
    char* name;              // مملوك (strdup)
    DataType target_type;    // النوع الهدف بعد فك الاسم البديل
    char* target_type_name;  // مملوك (strdup) عند TYPE_ENUM/TYPE_STRUCT/TYPE_UNION
    DataType target_ptr_base_type;
    char* target_ptr_base_type_name;
    int target_ptr_depth;
    FuncPtrSig* target_func_sig; // مملوك (clone) عند TYPE_FUNC_PTR
} TypeAliasDef;

static EnumDef enum_defs[ANALYSIS_MAX_ENUMS];
static int enum_count = 0;

static StructDef struct_defs[ANALYSIS_MAX_STRUCTS];
static int struct_count = 0;

static UnionDef union_defs[ANALYSIS_MAX_STRUCTS];
static int union_count = 0;

static TypeAliasDef type_alias_defs[ANALYSIS_MAX_TYPE_ALIASES];
static int type_alias_count = 0;

static Symbol global_symbols[ANALYSIS_MAX_SYMBOLS];
static int global_count = 0;
static int global_symbol_hash_head[ANALYSIS_SYMBOL_HASH_BUCKETS];
static int global_symbol_hash_next[ANALYSIS_MAX_SYMBOLS];

static Symbol local_symbols[ANALYSIS_MAX_SYMBOLS];
static int local_count = 0;
static int local_symbol_hash_head[ANALYSIS_SYMBOL_HASH_BUCKETS];
static int local_symbol_hash_next[ANALYSIS_MAX_SYMBOLS];

typedef struct {
    char* name;         // مملوك (strdup) ويتم تحريره في reset_analysis()
    DataType return_type;
    DataType return_ptr_base_type;
    char* return_ptr_base_type_name; // مملوك (strdup) إن كان أساس المؤشر مسمى
    int return_ptr_depth;
    FuncPtrSig* return_func_sig; // مملوك (clone) عند TYPE_FUNC_PTR
    DataType* param_types; // مملوك (malloc) ويتم تحريره في reset_analysis()
    DataType* param_ptr_base_types;
    char** param_ptr_base_type_names; // مملوك (malloc+strdup) عند الحاجة
    int* param_ptr_depths;
    FuncPtrSig** param_func_sigs; // مملوك (malloc+clone) عند TYPE_FUNC_PTR
    int param_count;
    bool is_variadic; // هل الدالة تقبل معاملات متغيرة ( ... )
    // توقيع "مرجع الدالة" عندما تُستخدم الدالة كقيمة (مؤشر دالة) — مملوك (clone).
    // ملاحظة: لا يتم ملؤه إن كان التوقيع يتضمن معاملات/إرجاع من نوع TYPE_FUNC_PTR (Higher-order غير مدعوم حالياً).
    FuncPtrSig* ref_funcptr_sig;
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
static DataType current_func_return_ptr_base_type = TYPE_INT;
static const char* current_func_return_ptr_base_type_name = NULL;
static int current_func_return_ptr_depth = 0;
static FuncPtrSig* current_func_return_func_sig = NULL;
static bool current_func_is_variadic = false;
static const char* current_func_variadic_anchor_name = NULL;

// ============================================================================
// دوال مساعدة (Helper Functions)
// ============================================================================

static bool ptr_base_named_match(DataType a_type, const char* a_name,
                                 DataType b_type, const char* b_name);
static bool funcsig_equal(const FuncPtrSig* a, const FuncPtrSig* b);
static void funcsig_free(FuncPtrSig* s);
static FuncPtrSig* funcsig_clone(const FuncPtrSig* s);

static Token semantic_make_token(const char* filename, int line, int col)
{
    Token t;
    memset(&t, 0, sizeof(t));
    t.type = TOKEN_INVALID;
    t.value = NULL;
    t.filename = filename ? filename : (current_filename ? current_filename : "غير_معروف");
    t.line = (line > 0) ? line : 1;
    t.col = (col > 0) ? col : 1;
    return t;
}

/**
 * @brief تنفيذ مسار إخراج الخطأ الدلالي المشترك (مع va_list).
 */
static void semantic_error_vloc(const char* filename,
                                int line,
                                int col,
                                const char* message,
                                va_list args)
{
    has_error = true;

    Token tok = semantic_make_token(filename, line, col);
    char buf[1024];
    (void)vsnprintf(buf, sizeof(buf), message, args);
    error_report(tok, "خطأ دلالي: %s", buf);
}

/**
 * @brief الإبلاغ عن خطأ دلالي بمعلومات موقع.
 */
static void semantic_error(Node* node, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    semantic_error_vloc(node ? node->filename : NULL,
                        node ? node->line : 1,
                        node ? node->col : 1,
                        message,
                        args);
    va_end(args);
}

static void semantic_error_loc(const char* filename, int line, int col, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    semantic_error_vloc(filename, line, col, message, args);
    va_end(args);
}

/**
 * @brief دالة تجزئة بسيطة لأسماء الرموز (FNV-1a 32-bit).
 */
static unsigned int symbol_hash_name(const char* name)
{
    if (!name) return 0u;
    unsigned int h = 2166136261u;
    const unsigned char* p = (const unsigned char*)name;
    while (*p) {
        h ^= (unsigned int)(*p++);
        h *= 16777619u;
    }
    return h;
}

static void int_array_fill_minus_one(int* arr, int count)
{
    if (!arr || count <= 0) return;
    for (int i = 0; i < count; i++) arr[i] = -1;
}

static void symbol_hash_reset_heads(int* heads, int count)
{
    int_array_fill_minus_one(heads, count);
}

static void symbol_hash_reset_next(int* next_arr, int count)
{
    int_array_fill_minus_one(next_arr, count);
}

static void symbol_hash_insert(const Symbol* symbols,
                               int* heads,
                               int* next_arr,
                               int idx)
{
    if (!symbols || !heads || !next_arr || idx < 0) return;
    const unsigned int h = symbol_hash_name(symbols[idx].name) % (unsigned int)ANALYSIS_SYMBOL_HASH_BUCKETS;
    next_arr[idx] = heads[h];
    heads[h] = idx;
}

static Symbol* global_lookup_by_name(const char* name)
{
    if (!name) return NULL;
    const unsigned int h = symbol_hash_name(name) % (unsigned int)ANALYSIS_SYMBOL_HASH_BUCKETS;
    for (int i = global_symbol_hash_head[h]; i >= 0; i = global_symbol_hash_next[i]) {
        if (strcmp(global_symbols[i].name, name) == 0) return &global_symbols[i];
    }
    return NULL;
}

static Symbol* local_lookup_latest_by_name(const char* name)
{
    if (!name) return NULL;
    const unsigned int h = symbol_hash_name(name) % (unsigned int)ANALYSIS_SYMBOL_HASH_BUCKETS;
    for (int i = local_symbol_hash_head[h]; i >= 0; i = local_symbol_hash_next[i]) {
        if (strcmp(local_symbols[i].name, name) == 0) return &local_symbols[i];
    }
    return NULL;
}

static int local_lookup_in_scope(const char* name, int start, int end_exclusive)
{
    if (!name) return -1;
    if (start < 0) start = 0;
    if (end_exclusive < start) end_exclusive = start;
    const unsigned int h = symbol_hash_name(name) % (unsigned int)ANALYSIS_SYMBOL_HASH_BUCKETS;
    for (int i = local_symbol_hash_head[h]; i >= 0; i = local_symbol_hash_next[i]) {
        if (i >= start && i < end_exclusive && strcmp(local_symbols[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int local_lookup_before_scope(const char* name, int before_index)
{
    if (!name) return -1;
    if (before_index <= 0) return -1;
    const unsigned int h = symbol_hash_name(name) % (unsigned int)ANALYSIS_SYMBOL_HASH_BUCKETS;
    for (int i = local_symbol_hash_head[h]; i >= 0; i = local_symbol_hash_next[i]) {
        if (i < before_index && strcmp(local_symbols[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void local_symbol_hash_rebuild(void)
{
    symbol_hash_reset_heads(local_symbol_hash_head, ANALYSIS_SYMBOL_HASH_BUCKETS);
    symbol_hash_reset_next(local_symbol_hash_next, ANALYSIS_MAX_SYMBOLS);
    for (int i = 0; i < local_count; i++) {
        symbol_hash_insert(local_symbols, local_symbol_hash_head, local_symbol_hash_next, i);
    }
}

static void symbol_release_array_dims(Symbol* sym)
{
    if (!sym) return;
    // تحرير ملكيات الرمز (غير مملوكة عبر arena).
    if (sym->func_sig) {
        funcsig_free(sym->func_sig);
        sym->func_sig = NULL;
    }
    if (sym->array_dims) {
        free(sym->array_dims);
        sym->array_dims = NULL;
    }
    sym->array_rank = 0;
    sym->array_total_elems = 0;
}

/**
 * @brief إعادة تعيين جداول الرموز.
 */
static void reset_analysis() {
    for (int i = 0; i < global_count; i++) {
        symbol_release_array_dims(&global_symbols[i]);
    }
    for (int i = 0; i < local_count; i++) {
        symbol_release_array_dims(&local_symbols[i]);
    }

    global_count = 0;
    local_count = 0;
    scope_depth = 0;
    symbol_hash_reset_heads(global_symbol_hash_head, ANALYSIS_SYMBOL_HASH_BUCKETS);
    symbol_hash_reset_next(global_symbol_hash_next, ANALYSIS_MAX_SYMBOLS);
    symbol_hash_reset_heads(local_symbol_hash_head, ANALYSIS_SYMBOL_HASH_BUCKETS);
    symbol_hash_reset_next(local_symbol_hash_next, ANALYSIS_MAX_SYMBOLS);
    for (int i = 0; i < func_count; i++) {
        free(func_symbols[i].name);
        func_symbols[i].name = NULL;
        free(func_symbols[i].return_ptr_base_type_name);
        func_symbols[i].return_ptr_base_type_name = NULL;
        funcsig_free(func_symbols[i].return_func_sig);
        func_symbols[i].return_func_sig = NULL;
        funcsig_free(func_symbols[i].ref_funcptr_sig);
        func_symbols[i].ref_funcptr_sig = NULL;
        free(func_symbols[i].param_types);
        func_symbols[i].param_types = NULL;
        free(func_symbols[i].param_ptr_base_types);
        func_symbols[i].param_ptr_base_types = NULL;
        if (func_symbols[i].param_ptr_base_type_names) {
            for (int k = 0; k < func_symbols[i].param_count; k++) {
                free(func_symbols[i].param_ptr_base_type_names[k]);
            }
        }
        free(func_symbols[i].param_ptr_base_type_names);
        func_symbols[i].param_ptr_base_type_names = NULL;
        free(func_symbols[i].param_ptr_depths);
        func_symbols[i].param_ptr_depths = NULL;
        if (func_symbols[i].param_func_sigs) {
            for (int k = 0; k < func_symbols[i].param_count; k++) {
                funcsig_free(func_symbols[i].param_func_sigs[k]);
            }
        }
        free(func_symbols[i].param_func_sigs);
        func_symbols[i].param_func_sigs = NULL;
        func_symbols[i].param_count = 0;
        func_symbols[i].is_variadic = false;
        func_symbols[i].is_defined = false;
    }
    func_count = 0;

    // تحرير تعريفات التعداد/الهياكل
    for (int i = 0; i < enum_count; i++) {
        free(enum_defs[i].name);
        enum_defs[i].name = NULL;
        for (int j = 0; j < enum_defs[i].member_count; j++) {
            free(enum_defs[i].members[j].name);
            enum_defs[i].members[j].name = NULL;
        }
        enum_defs[i].member_count = 0;
    }
    enum_count = 0;

    for (int i = 0; i < struct_count; i++) {
        free(struct_defs[i].name);
        struct_defs[i].name = NULL;
        for (int j = 0; j < struct_defs[i].field_count; j++) {
            free(struct_defs[i].fields[j].name);
            struct_defs[i].fields[j].name = NULL;
            free(struct_defs[i].fields[j].type_name);
            struct_defs[i].fields[j].type_name = NULL;
            free(struct_defs[i].fields[j].ptr_base_type_name);
            struct_defs[i].fields[j].ptr_base_type_name = NULL;
        }
        struct_defs[i].field_count = 0;
        struct_defs[i].size = 0;
        struct_defs[i].align = 0;
        struct_defs[i].layout_done = false;
        struct_defs[i].layout_in_progress = false;
    }
    struct_count = 0;

    for (int i = 0; i < union_count; i++) {
        free(union_defs[i].name);
        union_defs[i].name = NULL;
        for (int j = 0; j < union_defs[i].field_count; j++) {
            free(union_defs[i].fields[j].name);
            union_defs[i].fields[j].name = NULL;
            free(union_defs[i].fields[j].type_name);
            union_defs[i].fields[j].type_name = NULL;
            free(union_defs[i].fields[j].ptr_base_type_name);
            union_defs[i].fields[j].ptr_base_type_name = NULL;
        }
        union_defs[i].field_count = 0;
        union_defs[i].size = 0;
        union_defs[i].align = 0;
        union_defs[i].layout_done = false;
        union_defs[i].layout_in_progress = false;
    }
    union_count = 0;

    for (int i = 0; i < type_alias_count; i++) {
        free(type_alias_defs[i].name);
        type_alias_defs[i].name = NULL;
        free(type_alias_defs[i].target_type_name);
        type_alias_defs[i].target_type_name = NULL;
        free(type_alias_defs[i].target_ptr_base_type_name);
        type_alias_defs[i].target_ptr_base_type_name = NULL;
        funcsig_free(type_alias_defs[i].target_func_sig);
        type_alias_defs[i].target_func_sig = NULL;
        type_alias_defs[i].target_type = TYPE_INT;
        type_alias_defs[i].target_ptr_base_type = TYPE_INT;
        type_alias_defs[i].target_ptr_depth = 0;
    }
    type_alias_count = 0;

    has_error = false;
    inside_loop = false;
    inside_switch = false;
    in_function = false;
    current_func_return_type = TYPE_INT;
    current_func_return_ptr_base_type = TYPE_INT;
    current_func_return_ptr_base_type_name = NULL;
    current_func_return_ptr_depth = 0;
    current_func_return_func_sig = NULL;
    current_func_is_variadic = false;
    current_func_variadic_anchor_name = NULL;
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

static int func_signature_matches_decl(const FuncSymbol* a, const Node* node)
{
    if (!a || !node || node->type != NODE_FUNC_DEF) return 0;
    if (a->is_variadic != node->data.func_def.is_variadic) return 0;

    // نوع الإرجاع
    if (a->return_type != node->data.func_def.return_type) return 0;

    if (a->return_type == TYPE_POINTER) {
        if (a->return_ptr_depth != node->data.func_def.return_ptr_depth) return 0;
        if (!ptr_base_named_match(a->return_ptr_base_type, a->return_ptr_base_type_name,
                                  node->data.func_def.return_ptr_base_type,
                                  node->data.func_def.return_ptr_base_type_name)) {
            return 0;
        }
    } else if (a->return_type == TYPE_FUNC_PTR) {
        if (!funcsig_equal(a->return_func_sig, node->data.func_def.return_func_sig)) return 0;
    }

    // المعاملات
    int param_count = 0;
    for (Node* p = node->data.func_def.params; p; p = p->next) {
        if (p->type != NODE_VAR_DECL) continue;
        param_count++;
    }

    if (a->param_count != param_count) return 0;

    int i = 0;
    for (Node* p = node->data.func_def.params; p; p = p->next) {
        if (p->type != NODE_VAR_DECL) continue;
        if (!a->param_types || a->param_types[i] != p->data.var_decl.type) return 0;

        if (p->data.var_decl.type == TYPE_POINTER) {
            DataType have_base = a->param_ptr_base_types ? a->param_ptr_base_types[i] : TYPE_INT;
            int have_depth = a->param_ptr_depths ? a->param_ptr_depths[i] : 0;
            const char* have_name = (a->param_ptr_base_type_names && a->param_ptr_base_type_names[i])
                                        ? a->param_ptr_base_type_names[i]
                                        : NULL;
            if (have_depth != p->data.var_decl.ptr_depth) return 0;
            if (!ptr_base_named_match(have_base, have_name,
                                      p->data.var_decl.ptr_base_type,
                                      p->data.var_decl.ptr_base_type_name)) {
                return 0;
            }
        } else if (p->data.var_decl.type == TYPE_FUNC_PTR) {
            FuncPtrSig* have = (a->param_func_sigs) ? a->param_func_sigs[i] : NULL;
            if (!funcsig_equal(have, p->data.var_decl.func_sig)) return 0;
        }

        i++;
    }

    return 1;
}

static TypeAliasDef* type_alias_lookup_def(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < type_alias_count; i++) {
        if (type_alias_defs[i].name && strcmp(type_alias_defs[i].name, name) == 0) {
            return &type_alias_defs[i];
        }
    }
    return NULL;
}

static void func_register(Node* node)
{
    if (!node || node->type != NODE_FUNC_DEF) return;
    const char* name = node->data.func_def.name;
    if (!name) {
        semantic_error(node, "اسم الدالة فارغ.");
        return;
    }

    if (type_alias_lookup_def(name)) {
        semantic_error(node, "اسم الدالة '%s' متصادم مع اسم نوع بديل.", name);
        return;
    }

    DataType params_local[ANALYSIS_MAX_FUNC_PARAMS];
    DataType param_ptr_base_types_local[ANALYSIS_MAX_FUNC_PARAMS];
    const char* param_ptr_base_type_names_local[ANALYSIS_MAX_FUNC_PARAMS];
    int param_ptr_depths_local[ANALYSIS_MAX_FUNC_PARAMS];
    FuncPtrSig* param_func_sigs_local[ANALYSIS_MAX_FUNC_PARAMS];
    int param_count = 0;
    Node* param0 = NULL;
    Node* param1 = NULL;
    for (Node* p = node->data.func_def.params; p; p = p->next) {
        if (p->type != NODE_VAR_DECL) continue;
        if (param_count >= ANALYSIS_MAX_FUNC_PARAMS) {
            semantic_error(p, "عدد معاملات الدالة كبير جداً (الحد %d).", ANALYSIS_MAX_FUNC_PARAMS);
            return;
        }
        if (param_count == 0) param0 = p;
        if (param_count == 1) param1 = p;
        params_local[param_count] = p->data.var_decl.type;
        param_ptr_base_types_local[param_count] = p->data.var_decl.ptr_base_type;
        param_ptr_base_type_names_local[param_count] = p->data.var_decl.ptr_base_type_name;
        param_ptr_depths_local[param_count] = p->data.var_decl.ptr_depth;
        param_func_sigs_local[param_count] = p->data.var_decl.func_sig;
        param_count++;
    }

    if (strcmp(name, "الرئيسية") == 0) {
        if (node->data.func_def.is_variadic) {
            semantic_error(node,
                           "توقيع 'الرئيسية' غير صحيح. لا يُسمح بالمعاملات المتغيرة في 'الرئيسية'.");
        }
        if (node->data.func_def.return_type != TYPE_INT) {
            semantic_error(node, "الدالة 'الرئيسية' يجب أن تُرجع 'صحيح'.");
        }
        if (param_count == 0) {
            // صحيح الرئيسية()
        } else if (param_count == 2) {
            bool ok0 = (param0 && param0->type == NODE_VAR_DECL && param0->data.var_decl.type == TYPE_INT);
            bool ok1 = (param1 && param1->type == NODE_VAR_DECL &&
                        param1->data.var_decl.type == TYPE_POINTER &&
                        param1->data.var_decl.ptr_base_type == TYPE_STRING &&
                        param1->data.var_decl.ptr_depth == 1);
            if (!ok0 || !ok1) {
                semantic_error(node,
                               "توقيع 'الرئيسية' غير صحيح. الصيغ المدعومة: "
                               "صحيح الرئيسية() أو صحيح الرئيسية(صحيح عدد، نص[] معاملات).");
            }
        } else {
            semantic_error(node,
                           "توقيع 'الرئيسية' غير صحيح. الصيغ المدعومة: "
                           "صحيح الرئيسية() أو صحيح الرئيسية(صحيح عدد، نص[] معاملات).");
        }
    }

    FuncSymbol* existing = func_lookup(name);
    if (existing) {
        if (!func_signature_matches_decl(existing, node)) {
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
    fs->return_ptr_base_type = node->data.func_def.return_ptr_base_type;
    fs->return_ptr_depth = node->data.func_def.return_ptr_depth;
    fs->return_ptr_base_type_name = NULL;
    fs->return_func_sig = NULL;
    fs->ref_funcptr_sig = NULL;
    fs->param_count = param_count;
    fs->is_variadic = node->data.func_def.is_variadic;
    fs->decl_file = node->filename ? node->filename : current_filename;
    fs->decl_line = node->line;
    fs->decl_col = node->col;
    fs->is_defined = node->data.func_def.is_prototype ? false : true;

    if (fs->return_type == TYPE_POINTER &&
        node->data.func_def.return_ptr_base_type_name &&
        node->data.func_def.return_ptr_base_type_name[0]) {
        fs->return_ptr_base_type_name = strdup(node->data.func_def.return_ptr_base_type_name);
        if (!fs->return_ptr_base_type_name) {
            semantic_error(node, "نفدت الذاكرة أثناء تسجيل اسم أساس مؤشر الإرجاع للدالة '%s'.", name);
            return;
        }
    }

    if (fs->return_type == TYPE_FUNC_PTR) {
        fs->return_func_sig = funcsig_clone(node->data.func_def.return_func_sig);
        if (!fs->return_func_sig) {
            semantic_error(node, "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة كنوع إرجاع للدالة '%s'.", name);
            return;
        }
    }

    if (param_count > 0) {
        fs->param_types = (DataType*)malloc((size_t)param_count * sizeof(DataType));
        fs->param_ptr_base_types = (DataType*)malloc((size_t)param_count * sizeof(DataType));
        fs->param_ptr_base_type_names = (char**)calloc((size_t)param_count, sizeof(char*));
        fs->param_ptr_depths = (int*)malloc((size_t)param_count * sizeof(int));
        fs->param_func_sigs = (FuncPtrSig**)calloc((size_t)param_count, sizeof(FuncPtrSig*));
        if (!fs->param_types) {
            semantic_error(node, "نفدت الذاكرة أثناء تسجيل توقيع الدالة.");
            return;
        }
        if (!fs->param_ptr_base_types || !fs->param_ptr_depths || !fs->param_ptr_base_type_names || !fs->param_func_sigs) {
            semantic_error(node, "نفدت الذاكرة أثناء تسجيل توقيع الدالة.");
            return;
        }
        for (int i = 0; i < param_count; i++) {
            fs->param_types[i] = params_local[i];
            fs->param_ptr_base_types[i] = param_ptr_base_types_local[i];
            fs->param_ptr_depths[i] = param_ptr_depths_local[i];
            if (params_local[i] == TYPE_POINTER &&
                param_ptr_base_type_names_local[i] &&
                param_ptr_base_type_names_local[i][0]) {
                fs->param_ptr_base_type_names[i] = strdup(param_ptr_base_type_names_local[i]);
                if (!fs->param_ptr_base_type_names[i]) {
                    semantic_error(node, "نفدت الذاكرة أثناء نسخ اسم أساس مؤشر معامل الدالة '%s'.", name);
                    return;
                }
            }
            if (params_local[i] == TYPE_FUNC_PTR) {
                fs->param_func_sigs[i] = funcsig_clone(param_func_sigs_local[i]);
                if (!fs->param_func_sigs[i]) {
                    semantic_error(node, "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة لمعامل في '%s'.", name);
                    return;
                }
            }
        }
    }

    // بناء توقيع "مرجع الدالة" بشكل محافظ (بدون Higher-order).
    bool can_ref = true;
    if (fs->return_type == TYPE_FUNC_PTR) can_ref = false;
    for (int i = 0; i < fs->param_count; i++) {
        if (fs->param_types && fs->param_types[i] == TYPE_FUNC_PTR) {
            can_ref = false;
            break;
        }
    }

    if (can_ref) {
        FuncPtrSig tmp;
        memset(&tmp, 0, sizeof(tmp));
        tmp.return_type = fs->return_type;
        tmp.return_ptr_base_type = fs->return_ptr_base_type;
        tmp.return_ptr_depth = fs->return_ptr_depth;
        tmp.return_ptr_base_type_name = fs->return_ptr_base_type_name;
        tmp.param_count = fs->param_count;
        tmp.is_variadic = fs->is_variadic;
        tmp.param_types = fs->param_types;
        tmp.param_ptr_base_types = fs->param_ptr_base_types;
        tmp.param_ptr_base_type_names = fs->param_ptr_base_type_names;
        tmp.param_ptr_depths = fs->param_ptr_depths;
        fs->ref_funcptr_sig = funcsig_clone(&tmp);
        // فشل النسخ هنا غير قاتل، لكنه يمنع أخذ مرجع هذه الدالة كقيمة.
        if (!fs->ref_funcptr_sig) {
            semantic_error(node, "نفدت الذاكرة أثناء بناء توقيع مرجع الدالة '%s'.", name);
        }
    }
}

#include "analysis_scope.c"
#include "analysis_types.c"
#include "analysis_semantic_utils.c"
#include "analysis_builtins.c"
#include "analysis_format.c"
#include "analysis_infer_expr.c"
#include "analysis_visit.c"
