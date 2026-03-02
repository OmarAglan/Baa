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

#include "baa.h"

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

static void symbol_hash_reset_heads(int* heads, int count)
{
    if (!heads || count <= 0) return;
    for (int i = 0; i < count; i++) heads[i] = -1;
}

static void symbol_hash_reset_next(int* next_arr, int count)
{
    if (!next_arr || count <= 0) return;
    for (int i = 0; i < count; i++) next_arr[i] = -1;
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
        semantic_error(NULL, "عدد النطاقات المتداخلة كبير جداً.");
        return;
    }
    scope_stack[scope_depth++] = local_count;
}

static void scope_pop(void) {
    if (scope_depth <= 0) return;
    int start = scope_stack[--scope_depth];

    // تحذيرات المتغيرات غير المستخدمة لهذا النطاق فقط
    check_unused_local_variables_range(start, local_count);

    for (int i = start; i < local_count; i++) {
        symbol_release_array_dims(&local_symbols[i]);
    }

    // إخراج رموز النطاق من الجدول (منطقيًا)
    local_count = start;
    local_symbol_hash_rebuild();
}

/**
 * @brief إضافة رمز إلى النطاق الحالي.
 */
static void add_symbol(const char* name,
                       ScopeType scope,
                       DataType type,
                       const char* type_name,
                       DataType ptr_base_type,
                       const char* ptr_base_type_name,
                       int ptr_depth,
                       const FuncPtrSig* func_sig,
                       bool is_const,
                       bool is_static,
                       bool is_array,
                       int array_rank,
                       const int* array_dims,
                       int64_t array_total_elems,
                       int decl_line,
                       int decl_col,
                       const char* decl_file) {
    if (!name) {
        semantic_error_loc(decl_file, decl_line, decl_col, "اسم الرمز فارغ.");
        return;
    }

    if (type_alias_lookup_def(name)) {
        semantic_error_loc(decl_file, decl_line, decl_col,
                           "اسم الرمز '%s' متصادم مع اسم نوع بديل.", name);
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
        if (global_lookup_by_name(name)) {
            semantic_error_loc(decl_file, decl_line, decl_col,
                               "إعادة تعريف المتغير العام '%s'.", name);
            return;
        }
        if (global_count >= ANALYSIS_MAX_SYMBOLS) {
            semantic_error_loc(decl_file, decl_line, decl_col, "عدد المتغيرات العامة كبير جداً.");
            return;
        }
        
        memcpy(global_symbols[global_count].name, name, name_len + 1);
        global_symbols[global_count].scope = SCOPE_GLOBAL;
        global_symbols[global_count].type = type;
        global_symbols[global_count].type_name[0] = '\0';
        global_symbols[global_count].ptr_base_type = ptr_base_type;
        global_symbols[global_count].ptr_base_type_name[0] = '\0';
        global_symbols[global_count].ptr_depth = ptr_depth;
        global_symbols[global_count].func_sig = NULL;
        if (type_name && type_name[0]) {
            size_t tn_len = strlen(type_name);
            size_t tn_cap = sizeof(global_symbols[global_count].type_name);
            if (tn_len < tn_cap) {
                memcpy(global_symbols[global_count].type_name, type_name, tn_len + 1);
            }
        }
        if (ptr_base_type_name && ptr_base_type_name[0]) {
            size_t pn_len = strlen(ptr_base_type_name);
            size_t pn_cap = sizeof(global_symbols[global_count].ptr_base_type_name);
            if (pn_len < pn_cap) {
                memcpy(global_symbols[global_count].ptr_base_type_name, ptr_base_type_name, pn_len + 1);
            }
        }
        if (type == TYPE_FUNC_PTR) {
            global_symbols[global_count].func_sig = funcsig_clone(func_sig);
            if (!global_symbols[global_count].func_sig) {
                semantic_error_loc(decl_file, decl_line, decl_col,
                                   "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة للرمز '%s'.", name);
                return;
            }
        }
        global_symbols[global_count].is_array = is_array;
        global_symbols[global_count].array_rank = 0;
        global_symbols[global_count].array_total_elems = 0;
        global_symbols[global_count].array_dims = NULL;
        if (is_array && array_rank > 0 && array_dims) {
            int* dims_copy = (int*)malloc((size_t)array_rank * sizeof(int));
            if (!dims_copy) {
                semantic_error_loc(decl_file, decl_line, decl_col,
                                   "نفدت الذاكرة أثناء نسخ أبعاد المصفوفة.");
                return;
            }
            for (int i = 0; i < array_rank; i++) dims_copy[i] = array_dims[i];
            global_symbols[global_count].array_dims = dims_copy;
            global_symbols[global_count].array_rank = array_rank;
            global_symbols[global_count].array_total_elems = array_total_elems;
        }
        global_symbols[global_count].is_const = is_const;
        global_symbols[global_count].is_static = is_static;
        global_symbols[global_count].is_used = false;
        global_symbols[global_count].decl_line = decl_line;
        global_symbols[global_count].decl_col = decl_col;
        global_symbols[global_count].decl_file = decl_file;
        symbol_hash_insert(global_symbols, global_symbol_hash_head, global_symbol_hash_next, global_count);
        global_count++;
    } else {
        // التحقق من التكرار داخل النطاق الحالي فقط
        int start = current_scope_start();
        if (local_lookup_in_scope(name, start, local_count) >= 0) {
            semantic_error_loc(decl_file, decl_line, decl_col,
                               "إعادة تعريف المتغير المحلي '%s'.", name);
            return;
        }
        if (local_count >= ANALYSIS_MAX_SYMBOLS) {
            semantic_error_loc(decl_file, decl_line, decl_col, "عدد المتغيرات المحلية كبير جداً.");
            return;
        }

        // تحذير إذا كان المتغير المحلي يحجب متغيراً عاماً
        if (global_lookup_by_name(name)) {
            warning_report(WARN_SHADOW_VARIABLE, decl_file, decl_line, decl_col,
                "المتغير المحلي '%s' يحجب متغيراً عاماً.", name);
        }

        // تحذير إذا كان المتغير المحلي يحجب متغيراً محلياً خارجياً
        if (local_lookup_before_scope(name, start) >= 0) {
            warning_report(WARN_SHADOW_VARIABLE, decl_file, decl_line, decl_col,
                "المتغير المحلي '%s' يحجب متغيراً محلياً من نطاق خارجي.", name);
        }

        memcpy(local_symbols[local_count].name, name, name_len + 1);
        local_symbols[local_count].scope = SCOPE_LOCAL;
        local_symbols[local_count].type = type;
        local_symbols[local_count].type_name[0] = '\0';
        local_symbols[local_count].ptr_base_type = ptr_base_type;
        local_symbols[local_count].ptr_base_type_name[0] = '\0';
        local_symbols[local_count].ptr_depth = ptr_depth;
        local_symbols[local_count].func_sig = NULL;
        if (type_name && type_name[0]) {
            size_t tn_len = strlen(type_name);
            size_t tn_cap = sizeof(local_symbols[local_count].type_name);
            if (tn_len < tn_cap) {
                memcpy(local_symbols[local_count].type_name, type_name, tn_len + 1);
            }
        }
        if (ptr_base_type_name && ptr_base_type_name[0]) {
            size_t pn_len = strlen(ptr_base_type_name);
            size_t pn_cap = sizeof(local_symbols[local_count].ptr_base_type_name);
            if (pn_len < pn_cap) {
                memcpy(local_symbols[local_count].ptr_base_type_name, ptr_base_type_name, pn_len + 1);
            }
        }
        if (type == TYPE_FUNC_PTR) {
            local_symbols[local_count].func_sig = funcsig_clone(func_sig);
            if (!local_symbols[local_count].func_sig) {
                semantic_error_loc(decl_file, decl_line, decl_col,
                                   "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة للرمز '%s'.", name);
                return;
            }
        }
        local_symbols[local_count].is_array = is_array;
        local_symbols[local_count].array_rank = 0;
        local_symbols[local_count].array_total_elems = 0;
        local_symbols[local_count].array_dims = NULL;
        if (is_array && array_rank > 0 && array_dims) {
            int* dims_copy = (int*)malloc((size_t)array_rank * sizeof(int));
            if (!dims_copy) {
                semantic_error_loc(decl_file, decl_line, decl_col,
                                   "نفدت الذاكرة أثناء نسخ أبعاد المصفوفة.");
                return;
            }
            for (int i = 0; i < array_rank; i++) dims_copy[i] = array_dims[i];
            local_symbols[local_count].array_dims = dims_copy;
            local_symbols[local_count].array_rank = array_rank;
            local_symbols[local_count].array_total_elems = array_total_elems;
        }
        local_symbols[local_count].is_const = is_const;
        local_symbols[local_count].is_static = is_static;
        local_symbols[local_count].is_used = false;
        local_symbols[local_count].decl_line = decl_line;
        local_symbols[local_count].decl_col = decl_col;
        local_symbols[local_count].decl_file = decl_file;
        symbol_hash_insert(local_symbols, local_symbol_hash_head, local_symbol_hash_next, local_count);
        local_count++;
    }
}

/**
 * @brief البحث عن رمز بالاسم.
 * @param mark_used إذا كان true، يتم تعليم المتغير كمستخدم.
 */
static Symbol* lookup(const char* name, bool mark_used) {
    Symbol* s = local_lookup_latest_by_name(name);
    if (!s) s = global_lookup_by_name(name);
    if (s && mark_used) s->is_used = true;
    return s;
}

static DataType infer_type(Node* node);
static bool types_compatible(DataType got, DataType expected);

static void node_set_inferred_ptr(Node* node, DataType base_type, const char* base_type_name, int depth)
{
    if (!node) return;
    // لا يمكن أن يكون التعبير "مؤشر" و"مؤشر دالة" في نفس الوقت.
    if (node->inferred_func_sig) {
        // ملاحظة: inferred_func_sig مملوك للعقدة (نسخة) ويجب تحريره عند استبداله.
        funcsig_free(node->inferred_func_sig);
        node->inferred_func_sig = NULL;
    }
    node->inferred_ptr_base_type = base_type;
    if (node->inferred_ptr_base_type_name) {
        free(node->inferred_ptr_base_type_name);
        node->inferred_ptr_base_type_name = NULL;
    }
    if (base_type_name && base_type_name[0]) {
        node->inferred_ptr_base_type_name = strdup(base_type_name);
    }
    node->inferred_ptr_depth = depth;
}

static void node_clear_inferred_ptr(Node* node)
{
    if (!node) return;
    node_set_inferred_ptr(node, TYPE_INT, NULL, 0);
}

static void node_set_inferred_funcptr(Node* node, FuncPtrSig* sig)
{
    if (!node) return;
    // تفريغ معلومات المؤشر العادي حتى لا تختلط الدلالات.
    node_set_inferred_ptr(node, TYPE_INT, NULL, 0);
    // مهم: لا نخزن مؤشراً مستعاراً هنا لأن تواقيع رموز النطاقات المحلية تُحرَّر عند scope_pop().
    // لذلك نحتفظ بنسخة مملوكة للعقدة حتى تبقى صالحة أثناء خفض الـ IR.
    node->inferred_func_sig = funcsig_clone(sig);
    if (sig && !node->inferred_func_sig) {
        semantic_error(node, "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة داخل AST.");
    }
}

static void funcsig_free(FuncPtrSig* s)
{
    if (!s) return;
    if (s->param_ptr_base_type_names) {
        for (int i = 0; i < s->param_count; i++) {
            free(s->param_ptr_base_type_names[i]);
        }
    }
    free(s->return_ptr_base_type_name);
    free(s->param_types);
    free(s->param_ptr_base_types);
    free(s->param_ptr_base_type_names);
    free(s->param_ptr_depths);
    free(s);
}

static FuncPtrSig* funcsig_clone(const FuncPtrSig* s)
{
    if (!s) return NULL;

    FuncPtrSig* out = (FuncPtrSig*)calloc(1, sizeof(FuncPtrSig));
    if (!out) return NULL;

    out->return_type = s->return_type;
    out->return_ptr_base_type = s->return_ptr_base_type;
    out->return_ptr_depth = s->return_ptr_depth;
    out->is_variadic = s->is_variadic;
    if (s->return_ptr_base_type_name) {
        out->return_ptr_base_type_name = strdup(s->return_ptr_base_type_name);
        if (!out->return_ptr_base_type_name) {
            free(out);
            return NULL;
        }
    }

    out->param_count = s->param_count;
    if (out->param_count > 0) {
        size_t n = (size_t)out->param_count;
        out->param_types = (DataType*)malloc(n * sizeof(DataType));
        out->param_ptr_base_types = (DataType*)malloc(n * sizeof(DataType));
        out->param_ptr_base_type_names = (char**)calloc(n, sizeof(char*));
        out->param_ptr_depths = (int*)malloc(n * sizeof(int));
        if (!out->param_types || !out->param_ptr_base_types || !out->param_ptr_base_type_names || !out->param_ptr_depths) {
            funcsig_free(out);
            return NULL;
        }

        for (int i = 0; i < out->param_count; i++) {
            out->param_types[i] = s->param_types ? s->param_types[i] : TYPE_INT;
            out->param_ptr_base_types[i] = s->param_ptr_base_types ? s->param_ptr_base_types[i] : TYPE_INT;
            out->param_ptr_depths[i] = s->param_ptr_depths ? s->param_ptr_depths[i] : 0;
            if (s->param_ptr_base_type_names && s->param_ptr_base_type_names[i]) {
                out->param_ptr_base_type_names[i] = strdup(s->param_ptr_base_type_names[i]);
                if (!out->param_ptr_base_type_names[i]) {
                    funcsig_free(out);
                    return NULL;
                }
            }
        }
    }

    return out;
}

static bool ptr_base_named_match(DataType a_type, const char* a_name,
                                 DataType b_type, const char* b_name);

static bool funcsig_equal(const FuncPtrSig* a, const FuncPtrSig* b)
{
    if (a == b) return true;
    if (!a || !b) return false;

    if (a->return_type != b->return_type) return false;
    if (a->return_type == TYPE_POINTER) {
        if (a->return_ptr_depth != b->return_ptr_depth) return false;
        if (!ptr_base_named_match(a->return_ptr_base_type, a->return_ptr_base_type_name,
                                  b->return_ptr_base_type, b->return_ptr_base_type_name)) {
            return false;
        }
    }

    if (a->is_variadic != b->is_variadic) return false;
    if (a->param_count != b->param_count) return false;
    for (int i = 0; i < a->param_count; i++) {
        DataType at = a->param_types ? a->param_types[i] : TYPE_INT;
        DataType bt = b->param_types ? b->param_types[i] : TYPE_INT;
        if (at != bt) return false;
        if (at == TYPE_POINTER) {
            int ad = a->param_ptr_depths ? a->param_ptr_depths[i] : 0;
            int bd = b->param_ptr_depths ? b->param_ptr_depths[i] : 0;
            if (ad != bd) return false;
            DataType ab = a->param_ptr_base_types ? a->param_ptr_base_types[i] : TYPE_INT;
            DataType bb = b->param_ptr_base_types ? b->param_ptr_base_types[i] : TYPE_INT;
            const char* an = (a->param_ptr_base_type_names && a->param_ptr_base_type_names[i]) ? a->param_ptr_base_type_names[i] : NULL;
            const char* bn = (b->param_ptr_base_type_names && b->param_ptr_base_type_names[i]) ? b->param_ptr_base_type_names[i] : NULL;
            if (!ptr_base_named_match(ab, an, bb, bn)) return false;
        }
        // ملاحظة: لا ندعم (حالياً) أنواع دالة داخل توقيع مؤشر دالة (Higher-order).
        if (at == TYPE_FUNC_PTR) return false;
    }
    return true;
}

static bool ptr_base_named_match(DataType a_type, const char* a_name,
                                 DataType b_type, const char* b_name)
{
    if (a_type != b_type) return false;
    if (a_type == TYPE_STRUCT || a_type == TYPE_UNION || a_type == TYPE_ENUM) {
        if (!a_name || !b_name) return false;
        return strcmp(a_name, b_name) == 0;
    }
    return true;
}

static bool ptr_type_compatible(DataType got_base, const char* got_name, int got_depth,
                                DataType expected_base, const char* expected_name, int expected_depth,
                                bool allow_null)
{
    if (allow_null && got_depth == 0) return true;

    // قواعد void*: السماح بتحويل ضمني بين عدم* وأي مؤشر كائن (عمق >= 1).
    // ملاحظة: هذا لا يجعل عدم** عاماً؛ العمق ما زال حساساً (لا يُسمح إلا لعدم* تحديداً).
    if (got_depth > 0 && expected_depth > 0) {
        if (got_base == TYPE_VOID && got_depth == 1) return true;
        if (expected_base == TYPE_VOID && expected_depth == 1) return true;
    }

    if (got_depth != expected_depth) return false;
    if (got_depth <= 0) return false;
    return ptr_base_named_match(got_base, got_name, expected_base, expected_name);
}

static bool ptr_arith_allowed(DataType base_type, int depth)
{
    if (depth <= 0) return false;
    if (depth > 1) return true;
    if (base_type == TYPE_VOID || base_type == TYPE_STRUCT || base_type == TYPE_UNION) {
        return false;
    }
    return true;
}

static int node_list_count(Node* head)
{
    int n = 0;
    for (Node* p = head; p; p = p->next) n++;
    return n;
}

static int analyze_indices_type_and_bounds(Symbol* sym, Node* access_node, Node* indices, int expected_rank)
{
    if (!sym || !access_node) return 0;

    int seen = 0;
    for (Node* idx_node = indices; idx_node; idx_node = idx_node->next, seen++) {
        DataType it = infer_type(idx_node);
        if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
            semantic_error(idx_node, "فهرس المصفوفة يجب أن يكون صحيحاً.");
        }

        if (idx_node->type == NODE_INT && sym->array_dims && seen < sym->array_rank) {
            int64_t idx = (int64_t)idx_node->data.integer.value;
            int dim = sym->array_dims[seen];
            if (idx < 0 || (dim > 0 && idx >= dim)) {
                semantic_error(access_node,
                               "فهرس خارج الحدود للمصفوفة '%s' في البعد %d (الحجم %d).",
                               sym->name, seen + 1, dim);
            }
        }
    }

    if (seen != expected_rank) {
        semantic_error(access_node,
                       "عدد الفهارس للمصفوفة '%s' غير صحيح (المتوقع %d، المعطى %d).",
                       sym->name, expected_rank, seen);
    }

    return seen;
}

static bool symbol_const_linear_index(Symbol* sym, Node* indices, int expected_rank, int64_t* out_linear)
{
    if (!sym || !indices || !out_linear) return false;
    if (!sym->array_dims || sym->array_rank <= 0 || expected_rank <= 0) return false;

    int64_t linear = 0;
    int seen = 0;
    for (Node* idx_node = indices; idx_node && seen < expected_rank; idx_node = idx_node->next, seen++) {
        if (idx_node->type != NODE_INT) return false;
        int64_t idx = (int64_t)idx_node->data.integer.value;
        int dim = sym->array_dims[seen];
        if (idx < 0 || dim <= 0 || idx >= dim) return false;
        linear = linear * (int64_t)dim + idx;
    }

    if (seen != expected_rank) return false;
    *out_linear = linear;
    return true;
}

// ============================================================================
// تعداد/هيكل (v0.3.4): جداول تعريف الأنواع + تخطيط الذاكرة
// ============================================================================

static EnumDef* enum_lookup_def(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < enum_count; i++) {
        if (enum_defs[i].name && strcmp(enum_defs[i].name, name) == 0) return &enum_defs[i];
    }
    return NULL;
}

static StructDef* struct_lookup_def(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < struct_count; i++) {
        if (struct_defs[i].name && strcmp(struct_defs[i].name, name) == 0) return &struct_defs[i];
    }
    return NULL;
}

static UnionDef* union_lookup_def(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < union_count; i++) {
        if (union_defs[i].name && strcmp(union_defs[i].name, name) == 0) return &union_defs[i];
    }
    return NULL;
}

static int align_up_i32(int v, int a)
{
    if (a <= 0) return v;
    int r = v % a;
    if (r == 0) return v;
    return v + (a - r);
}

static int type_size_align(DataType t, const char* type_name, int* out_align);

static StructFieldDef* struct_field_lookup(StructDef* sd, const char* field_name)
{
    if (!sd || !field_name) return NULL;
    for (int i = 0; i < sd->field_count; i++) {
        if (sd->fields[i].name && strcmp(sd->fields[i].name, field_name) == 0) {
            return &sd->fields[i];
        }
    }
    return NULL;
}

static StructFieldDef* union_field_lookup(UnionDef* ud, const char* field_name)
{
    if (!ud || !field_name) return NULL;
    for (int i = 0; i < ud->field_count; i++) {
        if (ud->fields[i].name && strcmp(ud->fields[i].name, field_name) == 0) {
            return &ud->fields[i];
        }
    }
    return NULL;
}

static int struct_compute_layout(StructDef* sd)
{
    if (!sd) return 0;
    if (sd->layout_done) return 1;
    if (sd->layout_in_progress) {
        semantic_error_loc(current_filename, 1, 1,
                           "تعريف هيكل تراجعي/دائري غير مدعوم حالياً (cycle) في '%s'.",
                           sd->name ? sd->name : "???");
        return 0;
    }

    sd->layout_in_progress = true;

    int off = 0;
    int max_align = 1;

    for (int i = 0; i < sd->field_count; i++) {
        StructFieldDef* f = &sd->fields[i];
        int falign = 1;
        int fsize = type_size_align(f->type, f->type_name, &falign);
        if (fsize < 0) fsize = 0;
        if (falign < 1) falign = 1;

        off = align_up_i32(off, falign);
        f->offset = off;
        f->size = fsize;
        f->align = falign;
        off += fsize;

        if (falign > max_align) max_align = falign;
    }

    sd->align = max_align;
    sd->size = align_up_i32(off, max_align);

    sd->layout_in_progress = false;
    sd->layout_done = true;
    return 1;
}

static int union_compute_layout(UnionDef* ud)
{
    if (!ud) return 0;
    if (ud->layout_done) return 1;
    if (ud->layout_in_progress) {
        semantic_error_loc(current_filename, 1, 1,
                           "تعريف اتحاد تراجعي/دائري غير مدعوم حالياً (cycle) في '%s'.",
                           ud->name ? ud->name : "???");
        return 0;
    }

    ud->layout_in_progress = true;

    int max_align = 1;
    int max_size = 0;

    for (int i = 0; i < ud->field_count; i++) {
        StructFieldDef* f = &ud->fields[i];
        int falign = 1;
        int fsize = type_size_align(f->type, f->type_name, &falign);
        if (fsize < 0) fsize = 0;
        if (falign < 1) falign = 1;

        // كل حقول الاتحاد تبدأ من 0
        f->offset = 0;
        f->size = fsize;
        f->align = falign;

        if (falign > max_align) max_align = falign;
        if (fsize > max_size) max_size = fsize;
    }

    ud->align = max_align;
    ud->size = align_up_i32(max_size, max_align);

    ud->layout_in_progress = false;
    ud->layout_done = true;
    return 1;
}

static int type_size_align(DataType t, const char* type_name, int* out_align)
{
    if (out_align) *out_align = 1;
    switch (t) {
        case TYPE_BOOL:
            if (out_align) *out_align = 1;
            return 1;
        case TYPE_INT:
        case TYPE_ENUM:
            if (out_align) *out_align = 8;
            return 8;
        case TYPE_STRING:
        case TYPE_POINTER:
            if (out_align) *out_align = 8;
            return 8;
        case TYPE_STRUCT: {
            StructDef* sd = struct_lookup_def(type_name);
            if (!sd) {
                semantic_error_loc(current_filename, 1, 1,
                                   "هيكل غير معرّف '%s'.", type_name ? type_name : "???");
                return 0;
            }
            (void)struct_compute_layout(sd);
            if (out_align) *out_align = (sd->align > 0) ? sd->align : 1;
            return (sd->size > 0) ? sd->size : 0;
        }
        case TYPE_UNION: {
            UnionDef* ud = union_lookup_def(type_name);
            if (!ud) {
                semantic_error_loc(current_filename, 1, 1,
                                   "اتحاد غير معرّف '%s'.", type_name ? type_name : "???");
                return 0;
            }
            (void)union_compute_layout(ud);
            if (out_align) *out_align = (ud->align > 0) ? ud->align : 1;
            return (ud->size > 0) ? ud->size : 0;
        }
        default:
            return 0;
    }
}

static const char* expr_enum_name(Node* expr)
{
    if (!expr) return NULL;
    if (expr->type == NODE_VAR_REF) {
        Symbol* sym = lookup(expr->data.var_ref.name, false);
        if (sym && sym->type == TYPE_ENUM && sym->type_name[0]) return sym->type_name;
    }
    if (expr->type == NODE_MEMBER_ACCESS) {
        if (expr->data.member_access.is_enum_value) {
            return expr->data.member_access.enum_name;
        }
        if (expr->data.member_access.is_struct_member && expr->data.member_access.member_type == TYPE_ENUM) {
            return expr->data.member_access.member_type_name;
        }
    }
    return NULL;
}

static void resolve_member_access(Node* node)
{
    if (!node || node->type != NODE_MEMBER_ACCESS) return;
    if (node->data.member_access.is_enum_value || node->data.member_access.is_struct_member) return;

    Node* base = node->data.member_access.base;
    const char* member = node->data.member_access.member;
    if (!base || !member) return;

    // 1) محاولة تفسيرها كقيمة تعداد: <EnumName>:<Member>
    if (base->type == NODE_VAR_REF) {
        const char* base_name = base->data.var_ref.name;
        Symbol* sym = lookup(base_name, false);
        EnumDef* ed = enum_lookup_def(base_name);
        if (!sym && ed) {
            for (int i = 0; i < ed->member_count; i++) {
                if (ed->members[i].name && strcmp(ed->members[i].name, member) == 0) {
                    node->data.member_access.is_enum_value = true;
                    node->data.member_access.enum_name = strdup(base_name);
                    node->data.member_access.enum_value = ed->members[i].value;
                    return;
                }
            }
            semantic_error(node, "عضو تعداد غير معرّف '%s:%s'.", base_name ? base_name : "???", member);
            return;
        }
    }

    // 2) وصول عضو هيكل/اتحاد
    const char* root_var = NULL;
    bool root_is_global = false;
    const char* cur_struct = NULL;
    DataType cur_kind = TYPE_STRUCT;
    int base_off = 0;
    bool base_const = false;

    if (base->type == NODE_VAR_REF) {
        Symbol* sym = lookup(base->data.var_ref.name, true);
        if (!sym) {
            semantic_error(node, "متغير غير معرّف '%s'.", base->data.var_ref.name ? base->data.var_ref.name : "???");
            return;
        }
        if (sym->is_array) {
            semantic_error(node, "الوصول إلى عضو مصفوفة يتطلب فهرسة أولاً.");
            return;
        }
        if (sym->type != TYPE_STRUCT && sym->type != TYPE_UNION) {
            semantic_error(node, "'%s' ليس هيكلاً/اتحاداً.", sym->name);
            return;
        }
        root_var = sym->name;
        root_is_global = (sym->scope == SCOPE_GLOBAL);
        cur_struct = sym->type_name[0] ? sym->type_name : NULL;
        cur_kind = sym->type;
        base_off = 0;
        base_const = sym->is_const;
    } else if (base->type == NODE_ARRAY_ACCESS) {
        Symbol* sym = lookup(base->data.array_op.name, true);
        if (!sym) {
            semantic_error(node, "مصفوفة غير معرّفة '%s'.",
                           base->data.array_op.name ? base->data.array_op.name : "???");
            return;
        }
        if (!sym->is_array) {
            semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
            return;
        }
        if (sym->type != TYPE_STRUCT && sym->type != TYPE_UNION) {
            semantic_error(node, "الوصول إلى عضو يتطلب أن يكون عنصر المصفوفة هيكلاً/اتحاداً.");
            return;
        }

        int expected = (sym->array_rank > 0) ? sym->array_rank : 1;
        int supplied = base->data.array_op.index_count;
        if (supplied <= 0) supplied = node_list_count(base->data.array_op.indices);
        (void)analyze_indices_type_and_bounds(sym, base, base->data.array_op.indices, expected);
        if (supplied != expected) {
            return;
        }

        int64_t linear = 0;
        if (!symbol_const_linear_index(sym, base->data.array_op.indices, expected, &linear)) {
            semantic_error(base, "الوصول إلى عضو عنصر مصفوفة مركبة يتطلب فهارس ثابتة حالياً.");
            return;
        }

        int elem_size = type_size_align(sym->type, sym->type_name[0] ? sym->type_name : NULL, NULL);
        if (elem_size <= 0) {
            semantic_error(base, "حجم عنصر المصفوفة غير صالح.");
            return;
        }

        int64_t off64 = linear * (int64_t)elem_size;
        if (off64 < 0 || off64 > INT_MAX) {
            semantic_error(base, "إزاحة عضو المصفوفة كبيرة جداً.");
            return;
        }

        root_var = sym->name;
        root_is_global = (sym->scope == SCOPE_GLOBAL);
        cur_struct = sym->type_name[0] ? sym->type_name : NULL;
        cur_kind = sym->type;
        base_off = (int)off64;
        base_const = sym->is_const;
    } else if (base->type == NODE_MEMBER_ACCESS) {
        resolve_member_access(base);
        if (!base->data.member_access.is_struct_member ||
            (base->data.member_access.member_type != TYPE_STRUCT && base->data.member_access.member_type != TYPE_UNION)) {
            semantic_error(node, "لا يمكن تطبيق ':' على تعبير غير هيكلي.");
            return;
        }
        root_var = base->data.member_access.root_var;
        root_is_global = base->data.member_access.root_is_global;
        cur_struct = base->data.member_access.member_type_name;
        cur_kind = base->data.member_access.member_type;
        base_off = base->data.member_access.member_offset;
        base_const = base->data.member_access.member_is_const;
    } else {
        semantic_error(node, "الوصول للأعضاء يتطلب متغيراً أو وصولاً سابقاً لعضو.");
        return;
    }

    if (!cur_struct) {
        semantic_error(node, "نوع الهيكل غير معروف أثناء تحليل الوصول للأعضاء.");
        return;
    }

    StructFieldDef* f = NULL;
    if (cur_kind == TYPE_STRUCT) {
        StructDef* sd = struct_lookup_def(cur_struct);
        if (!sd) {
            semantic_error(node, "هيكل غير معرّف '%s'.", cur_struct);
            return;
        }
        (void)struct_compute_layout(sd);
        f = struct_field_lookup(sd, member);
    } else {
        UnionDef* ud = union_lookup_def(cur_struct);
        if (!ud) {
            semantic_error(node, "اتحاد غير معرّف '%s'.", cur_struct);
            return;
        }
        (void)union_compute_layout(ud);
        f = union_field_lookup(ud, member);
    }
    if (!f) {
        semantic_error(node, "عضو غير معرّف '%s:%s'.", cur_struct, member);
        return;
    }

    node->data.member_access.is_struct_member = true;
    node->data.member_access.root_var = root_var ? strdup(root_var) : NULL;
    node->data.member_access.root_is_global = root_is_global;
    node->data.member_access.root_struct = strdup(cur_struct);
    node->data.member_access.member_offset = base_off + f->offset;
    node->data.member_access.member_type = f->type;
    node->data.member_access.member_type_name = f->type_name ? strdup(f->type_name) : NULL;
    node->data.member_access.member_ptr_base_type = f->ptr_base_type;
    node->data.member_access.member_ptr_base_type_name = f->ptr_base_type_name ? strdup(f->ptr_base_type_name) : NULL;
    node->data.member_access.member_ptr_depth = f->ptr_depth;
    node->data.member_access.member_is_const = base_const || f->is_const;
}

static void enum_register_decl(Node* node)
{
    if (!node || node->type != NODE_ENUM_DECL) return;
    const char* name = node->data.enum_decl.name;
    if (!name) {
        semantic_error(node, "اسم التعداد فارغ.");
        return;
    }

    if (enum_lookup_def(name) || struct_lookup_def(name) || union_lookup_def(name) ||
        type_alias_lookup_def(name)) {
        semantic_error(node, "تعريف نوع مكرر '%s'.", name);
        return;
    }

    if (enum_count >= ANALYSIS_MAX_ENUMS) {
        semantic_error(node, "عدد التعدادات كبير جداً (الحد %d).", ANALYSIS_MAX_ENUMS);
        return;
    }

    EnumDef* ed = &enum_defs[enum_count++];
    memset(ed, 0, sizeof(*ed));
    ed->name = strdup(name);

    int64_t next = 0;
    for (Node* m = node->data.enum_decl.members; m; m = m->next) {
        if (m->type != NODE_ENUM_MEMBER) continue;
        if (!m->data.enum_member.name) continue;

        // تحقق من التكرار داخل التعداد
        for (int i = 0; i < ed->member_count; i++) {
            if (ed->members[i].name && strcmp(ed->members[i].name, m->data.enum_member.name) == 0) {
                semantic_error(m, "عضو تعداد مكرر '%s:%s'.", name, m->data.enum_member.name);
                return;
            }
        }

        if (ed->member_count >= ANALYSIS_MAX_ENUM_MEMBERS) {
            semantic_error(m, "عدد عناصر التعداد كبير جداً (الحد %d).", ANALYSIS_MAX_ENUM_MEMBERS);
            return;
        }

        ed->members[ed->member_count].name = strdup(m->data.enum_member.name);
        ed->members[ed->member_count].value = next;
        ed->member_count++;

        m->data.enum_member.value = next;
        m->data.enum_member.has_value = true;
        next++;
    }

    if (ed->member_count <= 0) {
        semantic_error(node, "التعداد '%s' يجب أن يحتوي على عنصر واحد على الأقل.", name);
    }
}

static void struct_register_decl(Node* node)
{
    if (!node || node->type != NODE_STRUCT_DECL) return;
    const char* name = node->data.struct_decl.name;
    if (!name) {
        semantic_error(node, "اسم الهيكل فارغ.");
        return;
    }

    if (enum_lookup_def(name) || struct_lookup_def(name) || union_lookup_def(name) ||
        type_alias_lookup_def(name)) {
        semantic_error(node, "تعريف نوع مكرر '%s'.", name);
        return;
    }

    if (struct_count >= ANALYSIS_MAX_STRUCTS) {
        semantic_error(node, "عدد الهياكل كبير جداً (الحد %d).", ANALYSIS_MAX_STRUCTS);
        return;
    }

    StructDef* sd = &struct_defs[struct_count++];
    memset(sd, 0, sizeof(*sd));
    sd->name = strdup(name);
    sd->layout_done = false;
    sd->layout_in_progress = false;

    for (Node* f = node->data.struct_decl.fields; f; f = f->next) {
        if (f->type != NODE_VAR_DECL) continue;
        if (!f->data.var_decl.name) continue;

        if (sd->field_count >= ANALYSIS_MAX_STRUCT_FIELDS) {
            semantic_error(f, "عدد حقول الهيكل '%s' كبير جداً (الحد %d).", name, ANALYSIS_MAX_STRUCT_FIELDS);
            return;
        }

        // تحقق من تكرار أسماء الحقول
        for (int i = 0; i < sd->field_count; i++) {
            if (sd->fields[i].name && strcmp(sd->fields[i].name, f->data.var_decl.name) == 0) {
                semantic_error(f, "حقل مكرر داخل الهيكل '%s': '%s'.", name, f->data.var_decl.name);
                return;
            }
        }

        StructFieldDef* out = &sd->fields[sd->field_count++];
        memset(out, 0, sizeof(*out));
        out->name = strdup(f->data.var_decl.name);
        out->type = f->data.var_decl.type;
        out->type_name = f->data.var_decl.type_name ? strdup(f->data.var_decl.type_name) : NULL;
        out->ptr_base_type = f->data.var_decl.ptr_base_type;
        out->ptr_base_type_name = f->data.var_decl.ptr_base_type_name ? strdup(f->data.var_decl.ptr_base_type_name) : NULL;
        out->ptr_depth = f->data.var_decl.ptr_depth;
        out->is_const = f->data.var_decl.is_const;
    }

    if (sd->field_count <= 0) {
        semantic_error(node, "الهيكل '%s' يجب أن يحتوي على حقل واحد على الأقل.", name);
    }
}

static void union_register_decl(Node* node)
{
    if (!node || node->type != NODE_UNION_DECL) return;
    const char* name = node->data.union_decl.name;
    if (!name) {
        semantic_error(node, "اسم الاتحاد فارغ.");
        return;
    }

    if (enum_lookup_def(name) || struct_lookup_def(name) || union_lookup_def(name) ||
        type_alias_lookup_def(name)) {
        semantic_error(node, "تعريف نوع مكرر '%s'.", name);
        return;
    }

    if (union_count >= ANALYSIS_MAX_STRUCTS) {
        semantic_error(node, "عدد الاتحادات كبير جداً (الحد %d).", ANALYSIS_MAX_STRUCTS);
        return;
    }

    UnionDef* ud = &union_defs[union_count++];
    memset(ud, 0, sizeof(*ud));
    ud->name = strdup(name);
    ud->layout_done = false;
    ud->layout_in_progress = false;

    for (Node* f = node->data.union_decl.fields; f; f = f->next) {
        if (f->type != NODE_VAR_DECL) continue;
        if (!f->data.var_decl.name) continue;

        if (ud->field_count >= ANALYSIS_MAX_STRUCT_FIELDS) {
            semantic_error(f, "عدد حقول الاتحاد '%s' كبير جداً (الحد %d).", name, ANALYSIS_MAX_STRUCT_FIELDS);
            return;
        }

        // تحقق من تكرار أسماء الحقول
        for (int i = 0; i < ud->field_count; i++) {
            if (ud->fields[i].name && strcmp(ud->fields[i].name, f->data.var_decl.name) == 0) {
                semantic_error(f, "حقل مكرر داخل الاتحاد '%s': '%s'.", name, f->data.var_decl.name);
                return;
            }
        }

        StructFieldDef* out = &ud->fields[ud->field_count++];
        memset(out, 0, sizeof(*out));
        out->name = strdup(f->data.var_decl.name);
        out->type = f->data.var_decl.type;
        out->type_name = f->data.var_decl.type_name ? strdup(f->data.var_decl.type_name) : NULL;
        out->ptr_base_type = f->data.var_decl.ptr_base_type;
        out->ptr_base_type_name = f->data.var_decl.ptr_base_type_name ? strdup(f->data.var_decl.ptr_base_type_name) : NULL;
        out->ptr_depth = f->data.var_decl.ptr_depth;
        out->is_const = f->data.var_decl.is_const;
        out->offset = 0;
    }

    if (ud->field_count <= 0) {
        semantic_error(node, "الاتحاد '%s' يجب أن يحتوي على حقل واحد على الأقل.", name);
    }
}

static void type_alias_register_decl(Node* node)
{
    if (!node || node->type != NODE_TYPE_ALIAS) return;

    const char* name = node->data.type_alias.name;
    if (!name) {
        semantic_error(node, "اسم النوع البديل فارغ.");
        return;
    }

    if (type_alias_lookup_def(name) || enum_lookup_def(name) ||
        struct_lookup_def(name) || union_lookup_def(name)) {
        semantic_error(node, "تعريف اسم نوع بديل مكرر '%s'.", name);
        return;
    }

    DataType target_type = node->data.type_alias.target_type;
    const char* target_type_name = node->data.type_alias.target_type_name;
    DataType target_ptr_base_type = node->data.type_alias.target_ptr_base_type;
    const char* target_ptr_base_type_name = node->data.type_alias.target_ptr_base_type_name;
    int target_ptr_depth = node->data.type_alias.target_ptr_depth;
    FuncPtrSig* target_func_sig = node->data.type_alias.target_func_sig;

    if (target_type == TYPE_ENUM) {
        if (!target_type_name || !enum_lookup_def(target_type_name)) {
            semantic_error(node, "نوع هدف غير معروف في الاسم البديل '%s' (تعداد '%s').",
                           name, target_type_name ? target_type_name : "???");
            return;
        }
    } else if (target_type == TYPE_STRUCT) {
        if (!target_type_name || !struct_lookup_def(target_type_name)) {
            semantic_error(node, "نوع هدف غير معروف في الاسم البديل '%s' (هيكل '%s').",
                           name, target_type_name ? target_type_name : "???");
            return;
        }
    } else if (target_type == TYPE_UNION) {
        if (!target_type_name || !union_lookup_def(target_type_name)) {
            semantic_error(node, "نوع هدف غير معروف في الاسم البديل '%s' (اتحاد '%s').",
                           name, target_type_name ? target_type_name : "???");
            return;
        }
    } else if (target_type == TYPE_POINTER) {
        if (target_ptr_depth <= 0) {
            semantic_error(node, "نوع المؤشر في الاسم البديل '%s' غير صالح.", name);
            return;
        }
        if ((target_ptr_base_type == TYPE_STRUCT && (!target_ptr_base_type_name || !struct_lookup_def(target_ptr_base_type_name))) ||
            (target_ptr_base_type == TYPE_UNION && (!target_ptr_base_type_name || !union_lookup_def(target_ptr_base_type_name))) ||
            (target_ptr_base_type == TYPE_ENUM && (!target_ptr_base_type_name || !enum_lookup_def(target_ptr_base_type_name)))) {
            semantic_error(node, "أساس المؤشر في الاسم البديل '%s' غير معرّف.", name);
            return;
        }
    } else if (target_type == TYPE_FUNC_PTR) {
        if (!target_func_sig) {
            semantic_error(node, "توقيع مؤشر الدالة في الاسم البديل '%s' مفقود.", name);
            return;
        }
        // لا ندعم حالياً مؤشرات دوال أعلى رتبة داخل التوقيع.
        if (target_func_sig->return_type == TYPE_FUNC_PTR) {
            semantic_error(node, "توقيع مؤشر الدالة في الاسم البديل '%s' يحتوي على إرجاع مؤشر دالة (غير مدعوم حالياً).",
                           name);
            return;
        }
        for (int i = 0; i < target_func_sig->param_count; i++) {
            DataType pt = target_func_sig->param_types ? target_func_sig->param_types[i] : TYPE_INT;
            if (pt == TYPE_FUNC_PTR) {
                semantic_error(node, "توقيع مؤشر الدالة في الاسم البديل '%s' يحتوي على معامل مؤشر دالة (غير مدعوم حالياً).",
                               name);
                return;
            }
        }
    }

    if (type_alias_count >= ANALYSIS_MAX_TYPE_ALIASES) {
        semantic_error(node, "عدد أسماء الأنواع البديلة كبير جداً (الحد %d).",
                       ANALYSIS_MAX_TYPE_ALIASES);
        return;
    }

    TypeAliasDef* out = &type_alias_defs[type_alias_count++];
    memset(out, 0, sizeof(*out));
    out->name = strdup(name);
    if (!out->name) {
        type_alias_count--;
        semantic_error(node, "نفدت الذاكرة أثناء تسجيل الاسم البديل '%s'.", name);
        return;
    }
    out->target_type = target_type;
    out->target_ptr_base_type = target_ptr_base_type;
    out->target_ptr_depth = target_ptr_depth;
    out->target_func_sig = NULL;
    if (target_type_name) {
        out->target_type_name = strdup(target_type_name);
        if (!out->target_type_name) {
            free(out->name);
            out->name = NULL;
            type_alias_count--;
            semantic_error(node, "نفدت الذاكرة أثناء تسجيل هدف الاسم البديل '%s'.", name);
            return;
        }
    }
    if (target_ptr_base_type_name) {
        out->target_ptr_base_type_name = strdup(target_ptr_base_type_name);
        if (!out->target_ptr_base_type_name) {
            free(out->target_type_name);
            out->target_type_name = NULL;
            free(out->name);
            out->name = NULL;
            type_alias_count--;
            semantic_error(node, "نفدت الذاكرة أثناء تسجيل أساس مؤشر الاسم البديل '%s'.", name);
            return;
        }
    }
    if (target_type == TYPE_FUNC_PTR) {
        out->target_func_sig = funcsig_clone(target_func_sig);
        if (!out->target_func_sig) {
            free(out->target_ptr_base_type_name);
            out->target_ptr_base_type_name = NULL;
            free(out->target_type_name);
            out->target_type_name = NULL;
            free(out->name);
            out->name = NULL;
            type_alias_count--;
            semantic_error(node, "نفدت الذاكرة أثناء نسخ توقيع مؤشر الدالة في الاسم البديل '%s'.", name);
            return;
        }
    }
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

static DataType infer_type(Node* node);

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

static const BuiltinFuncSig* builtin_lookup_string_func(const char* name)
{
    if (!name) return NULL;
    const int n = (int)(sizeof(builtin_string_funcs) / sizeof(builtin_string_funcs[0]));
    for (int i = 0; i < n; i++) {
        if (strcmp(name, builtin_string_funcs[i].name) == 0) {
            return &builtin_string_funcs[i];
        }
    }
    return NULL;
}

/**
 * @brief التحقق من صحة استدعاء دوال السلاسل المدمجة في v0.3.9.
 * @return true إذا كان الاسم دالة مدمجة (سواء مع أخطاء أو بدونها)، false إذا لم يكن مدمجاً.
 */
static bool builtin_check_string_call(Node* call_node, const char* fname, Node* args, DataType* out_return_type)
{
    const BuiltinFuncSig* sig = builtin_lookup_string_func(fname);
    if (!sig) return false;

    int i = 0;
    for (Node* arg = args; arg; arg = arg->next, i++) {
        DataType got = infer_type(arg);
        if (i < sig->param_count) {
            DataType expected = sig->param_types[i];
            if (!types_compatible(got, expected)) {
                semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, sig->name);
            } else {
                maybe_warn_implicit_narrowing(got, expected, arg);
            }
        }
    }

    if (i != sig->param_count) {
        semantic_error(call_node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", sig->name, sig->param_count);
    }

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

static const BuiltinMemFuncSig* builtin_lookup_mem_func(const char* name)
{
    if (!name) return NULL;
    const int n = (int)(sizeof(builtin_mem_funcs) / sizeof(builtin_mem_funcs[0]));
    for (int i = 0; i < n; i++) {
        if (strcmp(name, builtin_mem_funcs[i].name) == 0) {
            return &builtin_mem_funcs[i];
        }
    }
    return NULL;
}

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
                if (got != TYPE_POINTER ||
                    !ptr_type_compatible(arg->inferred_ptr_base_type,
                                         arg->inferred_ptr_base_type_name,
                                         arg->inferred_ptr_depth,
                                         TYPE_VOID, NULL, 1,
                                         true)) {
                    semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, sig->name);
                }
            } else if (!types_compatible(got, expected)) {
                semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, sig->name);
            } else {
                maybe_warn_implicit_narrowing(got, expected, arg);
            }
        }
    }

    if (i != sig->param_count) {
        semantic_error(call_node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", sig->name, sig->param_count);
    }

    if (out_return_type) *out_return_type = sig->return_type;

    if (sig->return_type == TYPE_POINTER) {
        node_set_inferred_ptr(call_node, TYPE_VOID, NULL, 1);
    } else {
        node_clear_inferred_ptr(call_node);
    }

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

static const BuiltinFileFuncSig* builtin_lookup_file_func(const char* name)
{
    if (!name) return NULL;
    const int n = (int)(sizeof(builtin_file_funcs) / sizeof(builtin_file_funcs[0]));
    for (int i = 0; i < n; i++) {
        if (strcmp(name, builtin_file_funcs[i].name) == 0) {
            return &builtin_file_funcs[i];
        }
    }
    return NULL;
}

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
                if (got != TYPE_POINTER ||
                    !ptr_type_compatible(arg->inferred_ptr_base_type,
                                         arg->inferred_ptr_base_type_name,
                                         arg->inferred_ptr_depth,
                                         TYPE_VOID, NULL, 1,
                                         true)) {
                    semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, sig->name);
                }
            } else if (!types_compatible(got, expected)) {
                semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, sig->name);
            } else {
                maybe_warn_implicit_narrowing(got, expected, arg);
            }
        }
    }

    if (i != sig->param_count) {
        semantic_error(call_node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", sig->name, sig->param_count);
    }

    if (out_return_type) *out_return_type = sig->return_type;

    if (sig->return_type == TYPE_POINTER) {
        node_set_inferred_ptr(call_node, TYPE_VOID, NULL, 1);
    } else {
        node_clear_inferred_ptr(call_node);
    }

    return true;
}

static bool variadic_extra_arg_type_supported(DataType t)
{
    if (t == TYPE_VOID || t == TYPE_STRUCT || t == TYPE_UNION || t == TYPE_FUNC_PTR) return false;
    return true;
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

        // ص ط س ن ح ع م
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
            } else if (k == BAA_FMT_SPEC_F64) {
                if (!ptr_type_compatible(a->inferred_ptr_base_type, a->inferred_ptr_base_type_name, a->inferred_ptr_depth,
                                         TYPE_FLOAT, NULL, 1, true)) {
                    semantic_error(a, "نداء 'اقرأ_منسق': المعامل %d يجب أن يكون 'عشري*' مع %%ع.", idx + 2);
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
        } else if (k == BAA_FMT_SPEC_F64) {
            DataType got = infer_type(a);
            if (got != TYPE_FLOAT) {
                semantic_error(a, "نوع المعامل %d في '%s' يجب أن يكون 'عشري' مع %%ع.", idx + 2, fname);
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
 * @brief استنتاج نوع التعبير.
 * @return DataType نوع التعبير الناتج.
 */
static DataType infer_type_internal(Node* node);

static DataType infer_type_internal(Node* node) {
    if (!node) return TYPE_INT; // افتراضي لتجنب الانهيار

    switch (node->type) {
        case NODE_INT:
        {
            // قاعدة نمط C للأعداد العشرية بدون لاحقة:
            // إذا كانت ضمن مدى 32-بت موقّع → اعتبرها ص٣٢، وإلا اعتبرها صحيح/ص٦٤.
            int64_t v = (int64_t)node->data.integer.value;
            if (v >= INT32_MIN && v <= INT32_MAX)
                return TYPE_I32;
            return TYPE_INT;
        }

        case NODE_FLOAT:
            return TYPE_FLOAT;

        case NODE_CHAR:
            return TYPE_CHAR;
        
        case NODE_STRING:
            return TYPE_STRING;
        
        case NODE_BOOL:
            return TYPE_BOOL;

        case NODE_NULL:
            node_set_inferred_ptr(node, TYPE_INT, NULL, 0);
            return TYPE_POINTER;

        case NODE_SIZEOF: {
            int64_t size = 0;
            bool ok = false;

            if (node->data.sizeof_expr.has_type_form) {
                ok = datatype_size_bytes(node->data.sizeof_expr.target_type,
                                         node->data.sizeof_expr.target_type_name,
                                         &size);
            } else {
                if (!node->data.sizeof_expr.expression) {
                    semantic_error(node, "تعبير 'حجم' يتطلب نوعاً أو تعبيراً صالحاً.");
                    ok = false;
                } else {
                    ok = sizeof_expr_bytes(node->data.sizeof_expr.expression, &size);
                }
            }

            if (!ok) {
                semantic_error(node, "لا يمكن حساب 'حجم' لهذا التعبير/النوع.");
                node->data.sizeof_expr.size_known = false;
                node->data.sizeof_expr.size_bytes = 0;
            } else {
                node->data.sizeof_expr.size_known = true;
                node->data.sizeof_expr.size_bytes = size;
            }

            return TYPE_INT;
        }
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup(node->data.var_ref.name, true); // تعليم كمستخدم
            if (!sym) {
                // قد يكون الاسم اسم دالة مستخدمة كقيمة (مؤشر دالة).
                FuncSymbol* fs = func_lookup(node->data.var_ref.name);
                if (fs && fs->ref_funcptr_sig) {
                    node_set_inferred_funcptr(node, fs->ref_funcptr_sig);
                    return TYPE_FUNC_PTR;
                }
                if (fs && !fs->ref_funcptr_sig) {
                    semantic_error(node,
                                   "لا يمكن أخذ مرجع الدالة '%s' كمؤشر دالة حالياً (Higher-order غير مدعوم).",
                                   node->data.var_ref.name ? node->data.var_ref.name : "???");
                    return TYPE_FUNC_PTR;
                }
                semantic_error(node, "متغير غير معرّف '%s'.", node->data.var_ref.name);
                return TYPE_INT; // استرداد الخطأ
            }
            if (sym->is_array) {
                semantic_error(node, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", node->data.var_ref.name);
                return sym->type;
            }
            if (sym->type == TYPE_STRUCT || sym->type == TYPE_UNION) {
                semantic_error(node, "لا يمكن استخدام النوع المركب '%s' كقيمة مباشرة (استخدم ':' للوصول لعضو).", sym->name);
                return sym->type;
            }
            if (sym->type == TYPE_POINTER) {
                node_set_inferred_ptr(node,
                                      sym->ptr_base_type,
                                      sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                      sym->ptr_depth);
            } else if (sym->type == TYPE_FUNC_PTR) {
                node_set_inferred_funcptr(node, sym->func_sig);
            } else {
                node_clear_inferred_ptr(node);
            }
            return sym->type;
        }

        case NODE_MEMBER_ACCESS: {
            resolve_member_access(node);
            if (node->data.member_access.is_enum_value) {
                return TYPE_ENUM;
            }
            if (node->data.member_access.is_struct_member) {
                if (node->data.member_access.member_type == TYPE_STRUCT ||
                    node->data.member_access.member_type == TYPE_UNION) {
                    semantic_error(node, "لا يمكن استخدام عضو مركب كقيمة (استخدم ':' للتعشيش).");
                }
                if (node->data.member_access.member_type == TYPE_POINTER) {
                    node_set_inferred_ptr(node,
                                          node->data.member_access.member_ptr_base_type,
                                          node->data.member_access.member_ptr_base_type_name,
                                          node->data.member_access.member_ptr_depth);
                } else {
                    node_clear_inferred_ptr(node);
                }
                return node->data.member_access.member_type;
            }
            semantic_error(node, "وصول عضو غير صالح.");
            return TYPE_INT;
        }

        case NODE_ARRAY_ACCESS: {
            Symbol* sym = lookup(node->data.array_op.name, true);
            if (!sym) {
                semantic_error(node, "مصفوفة غير معرّفة '%s'.", node->data.array_op.name ? node->data.array_op.name : "???");
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                return TYPE_INT;
            }

            int supplied = node->data.array_op.index_count;
            if (supplied <= 0) supplied = node_list_count(node->data.array_op.indices);

            if (!sym->is_array) {
                // السماح بفهرسة النص على أنه حرف[] (v0.3.5)
                if (sym->type == TYPE_STRING) {
                    if (supplied != 1) {
                        semantic_error(node, "فهرسة النص '%s' تتطلب فهرساً واحداً.", sym->name);
                    }
                    for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                        DataType it = infer_type(idx);
                        if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                            semantic_error(idx, "فهرس النص يجب أن يكون صحيحاً.");
                        }
                    }
                    node_clear_inferred_ptr(node);
                    return TYPE_CHAR;
                }

                // فهرسة المؤشر: p[i] ⇢ *(p + i)
                if (sym->type == TYPE_POINTER) {
                    DataType cur_t = TYPE_POINTER;
                    DataType cur_base = sym->ptr_base_type;
                    const char* cur_base_name = (sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL);
                    int cur_depth = sym->ptr_depth;

                    if (cur_depth <= 0) {
                        semantic_error(node, "عمق المؤشر غير صالح في '%s'.", sym->name);
                        node_clear_inferred_ptr(node);
                        return TYPE_INT;
                    }

                    Node* idx_node = node->data.array_op.indices;
                    for (int i = 0; i < supplied && idx_node; i++, idx_node = idx_node->next) {
                        DataType it = infer_type(idx_node);
                        if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                            semantic_error(idx_node, "فهرس المؤشر يجب أن يكون صحيحاً.");
                        }

                        if (cur_t == TYPE_POINTER) {
                            if (!ptr_arith_allowed(cur_base, cur_depth)) {
                                semantic_error(node, "فهرسة هذا المؤشر غير مسموحة (نوع الأساس غير قابل للحساب).");
                                cur_t = TYPE_INT;
                                break;
                            }
                            if (cur_depth <= 0) {
                                semantic_error(node, "عمق المؤشر غير صالح أثناء فهرسة '%s'.", sym->name);
                                cur_t = TYPE_INT;
                                break;
                            }
                            if (cur_depth == 1) {
                                cur_t = cur_base;
                            } else {
                                cur_depth--;
                                cur_t = TYPE_POINTER;
                            }
                        } else if (cur_t == TYPE_STRING) {
                            cur_t = TYPE_CHAR;
                        } else {
                            semantic_error(node, "لا يمكن فهرسة الناتج من '%s' بهذه الطريقة.", sym->name);
                            cur_t = TYPE_INT;
                            break;
                        }

                        if (cur_t == TYPE_CHAR && i != supplied - 1) {
                            semantic_error(node, "فهرسة متعددة بعد محرف غير مدعومة.");
                            break;
                        }
                    }

                    if (cur_t == TYPE_POINTER) {
                        node_set_inferred_ptr(node, cur_base, cur_base_name, cur_depth);
                    } else {
                        node_clear_inferred_ptr(node);
                    }

                    if (cur_t == TYPE_STRUCT || cur_t == TYPE_UNION) {
                        semantic_error(node, "استخدام نوع مركب كناتج فهرسة '%s' كقيمة غير مدعوم.", sym->name);
                    }
                    return cur_t;
                }

                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                return sym->type;
            }

            int expected = (sym->array_rank > 0) ? sym->array_rank : 1;
            if (sym->type == TYPE_STRING && supplied == expected + 1) {
                Node* idx = node->data.array_op.indices;
                for (int i = 0; i < expected && idx; i++, idx = idx->next) {
                    DataType it = infer_type(idx);
                    if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                        semantic_error(idx, "فهرس المصفوفة يجب أن يكون صحيحاً.");
                    }
                    if (idx->type == NODE_INT && sym->array_dims && i < sym->array_rank) {
                        int64_t iv = (int64_t)idx->data.integer.value;
                        int dim = sym->array_dims[i];
                        if (iv < 0 || (dim > 0 && iv >= dim)) {
                            semantic_error(node,
                                           "فهرس خارج الحدود للمصفوفة '%s' في البعد %d (الحجم %d).",
                                           sym->name, i + 1, dim);
                        }
                    }
                }
                if (!idx) {
                    semantic_error(node, "فهرس النص داخل مصفوفة النصوص مفقود.");
                    node_clear_inferred_ptr(node);
                    return TYPE_CHAR;
                }
                DataType sit = infer_type(idx);
                if (!types_compatible(sit, TYPE_INT) || sit == TYPE_FLOAT) {
                    semantic_error(idx, "فهرس النص يجب أن يكون صحيحاً.");
                }
                node_clear_inferred_ptr(node);
                return TYPE_CHAR;
            }
            (void)analyze_indices_type_and_bounds(sym, node, node->data.array_op.indices, expected);
            if (sym->type == TYPE_POINTER) {
                node_set_inferred_ptr(node,
                                      sym->ptr_base_type,
                                      sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                      sym->ptr_depth);
            } else {
                node_clear_inferred_ptr(node);
            }
            return sym->type; // نوع العنصر
        }

        case NODE_CALL_EXPR:
        {
            const char* fname = node->data.call.name;
            // 1) أعطِ أولوية للرموز في النطاق (متغيرات/معاملات) حتى تعمل الـ shadowing بشكل صحيح.
            Symbol* callee_sym = lookup(fname, true);
            if (callee_sym) {
                if (callee_sym->is_array) {
                    semantic_error(node, "لا يمكن نداء المصفوفة '%s'.", callee_sym->name);
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    return TYPE_INT;
                }

                if (callee_sym->type != TYPE_FUNC_PTR) {
                    semantic_error(node, "المعرف '%s' ليس دالة ولا مؤشر دالة قابل للنداء.", fname ? fname : "???");
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    return TYPE_INT;
                }

                FuncPtrSig* sig = callee_sym->func_sig;
                if (!sig) {
                    semantic_error(node, "توقيع مؤشر الدالة للرمز '%s' مفقود.", fname ? fname : "???");
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }
                if (sig->is_variadic) {
                    semantic_error(node, "نداء مؤشر دالة متغير المعاملات غير مدعوم حالياً.");
                }

                int i = 0;
                Node* arg = node->data.call.args;
                while (arg) {
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
                                semantic_error(arg, "نوع المعامل %d في نداء '%s' غير متوافق.", i + 1, fname ? fname : "???");
                            }
                        } else if (exp == TYPE_FUNC_PTR) {
                            semantic_error(arg, "Higher-order غير مدعوم: معامل %d في توقيع مؤشر الدالة هو مؤشر دالة.", i + 1);
                        } else if (!types_compatible(at, exp)) {
                            semantic_error(arg, "نوع المعامل %d في نداء '%s' غير متوافق.", i + 1, fname ? fname : "???");
                        } else {
                            maybe_warn_implicit_narrowing(at, exp, arg);
                        }
                    } else {
                        DataType at = infer_type(arg);
                        if (sig->is_variadic) {
                            if (!variadic_extra_arg_type_supported(at)) {
                                semantic_error(arg, "نوع المعامل المتغير %d في نداء '%s' غير مدعوم.",
                                               i + 1, fname ? fname : "???");
                            }
                        }
                    }
                    i++;
                    arg = arg->next;
                }

                if (sig->is_variadic) {
                    if (i < sig->param_count) {
                        semantic_error(node, "عدد معاملات نداء '%s' غير صحيح (المتوقع على الأقل %d).",
                                       fname ? fname : "???", sig->param_count);
                    }
                } else if (i != sig->param_count) {
                    semantic_error(node, "عدد معاملات نداء '%s' غير صحيح (المتوقع %d).",
                                   fname ? fname : "???", sig->param_count);
                }

                if (sig->return_type == TYPE_POINTER) {
                    node_set_inferred_ptr(node,
                                          sig->return_ptr_base_type,
                                          sig->return_ptr_base_type_name,
                                          sig->return_ptr_depth);
                } else if (sig->return_type == TYPE_FUNC_PTR) {
                    semantic_error(node, "Higher-order غير مدعوم: نوع الإرجاع في توقيع مؤشر الدالة هو مؤشر دالة.");
                    node_clear_inferred_ptr(node);
                } else {
                    node_clear_inferred_ptr(node);
                }

                return sig->return_type;
            }

            // 2) ثم نحل الدوال المعرفة (أو builtins) إذا لم يوجد رمز بنفس الاسم.
            FuncSymbol* fs = func_lookup(fname);
            if (!fs || !fs->is_defined) {
                DataType built_ret = TYPE_INT;
                if (builtin_check_format_call(node, fname, node->data.call.args, &built_ret)) {
                    return built_ret;
                }
                if (builtin_check_variadic_runtime_call(node, fname, node->data.call.args, &built_ret)) {
                    return built_ret;
                }
                if (builtin_check_string_call(node, fname, node->data.call.args, &built_ret)) {
                    node_clear_inferred_ptr(node);
                    return built_ret;
                }
                if (builtin_check_mem_call(node, fname, node->data.call.args, &built_ret)) {
                    return built_ret;
                }
                if (builtin_check_file_call(node, fname, node->data.call.args, &built_ret)) {
                    return built_ret;
                }

                if (!fs) {
                    semantic_error(node, "استدعاء دالة غير معرّفة '%s'.", fname ? fname : "???");
                    // ما زلنا نستنتج أنواع الوسائط لاكتشاف أخطاء أخرى.
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    return TYPE_INT;
                }
            }
            // تحقق عدد وأنواع المعاملات
            int i = 0;
            Node* arg = node->data.call.args;
            while (arg) {
                if (i < fs->param_count) {
                    DataType expected0 = fs->param_types[i];
                    DataType at = infer_type_allow_null_string(arg, expected0);
                    if (fs->param_types[i] == TYPE_POINTER) {
                        if (at != TYPE_POINTER ||
                            !ptr_type_compatible(arg->inferred_ptr_base_type,
                                                 arg->inferred_ptr_base_type_name,
                                                 arg->inferred_ptr_depth,
                                                 fs->param_ptr_base_types ? fs->param_ptr_base_types[i] : TYPE_INT,
                                                 (fs->param_ptr_base_type_names && fs->param_ptr_base_type_names[i]) ? fs->param_ptr_base_type_names[i] : NULL,
                                                 fs->param_ptr_depths ? fs->param_ptr_depths[i] : 0,
                                                 true)) {
                            semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                        }
                    } else if (fs->param_types[i] == TYPE_FUNC_PTR) {
                        FuncPtrSig* expected_sig = (fs->param_func_sigs) ? fs->param_func_sigs[i] : NULL;
                        if (!expected_sig) {
                            semantic_error(arg, "توقيع مؤشر الدالة لمعامل %d في '%s' مفقود.", i + 1, fs->name);
                        } else if (arg->type == NODE_NULL) {
                            // سماح null لمؤشر الدالة: نُحدد نوعه كي يُخفض بشكل صحيح.
                            arg->inferred_type = TYPE_FUNC_PTR;
                            node_set_inferred_funcptr(arg, expected_sig);
                        } else if (at != TYPE_FUNC_PTR || !funcsig_equal(arg->inferred_func_sig, expected_sig)) {
                            semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق (توقيع مؤشر دالة مختلف).", i + 1, fs->name);
                        }
                    } else if (!types_compatible(at, fs->param_types[i])) {
                        semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                    } else {
                        maybe_warn_implicit_narrowing(at, fs->param_types[i], arg);
                    }
                } else {
                    DataType at = infer_type(arg);
                    if (fs->is_variadic) {
                        if (!variadic_extra_arg_type_supported(at)) {
                            semantic_error(arg, "نوع المعامل المتغير %d في '%s' غير مدعوم.", i + 1, fs->name);
                        }
                    }
                }
                i++;
                arg = arg->next;
            }
            if (fs->is_variadic) {
                if (i < fs->param_count) {
                    semantic_error(node, "عدد معاملات '%s' غير صحيح (المتوقع على الأقل %d).",
                                   fs->name, fs->param_count);
                }
            } else if (i != fs->param_count) {
                semantic_error(node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", fs->name, fs->param_count);
            }
            if (fs->return_type == TYPE_POINTER) {
                node_set_inferred_ptr(node,
                                      fs->return_ptr_base_type,
                                      fs->return_ptr_base_type_name,
                                      fs->return_ptr_depth);
            } else if (fs->return_type == TYPE_FUNC_PTR) {
                node_set_inferred_funcptr(node, fs->return_func_sig);
            } else {
                node_clear_inferred_ptr(node);
            }
            return fs->return_type;
        }

        case NODE_CAST: {
            Node* src = node->data.cast_expr.expression;
            DataType src_t = infer_type(src);
            DataType dst_t = node->data.cast_expr.target_type;

            if (src_t == TYPE_FUNC_PTR || dst_t == TYPE_FUNC_PTR) {
                semantic_error(node, "التحويل الصريح من/إلى مؤشر دالة غير مدعوم حالياً.");
                node_clear_inferred_ptr(node);
                return dst_t;
            }

            if (dst_t == TYPE_VOID) {
                semantic_error(node, "التحويل الصريح إلى 'عدم' غير مدعوم حالياً.");
                node_clear_inferred_ptr(node);
                return TYPE_VOID;
            }

            if (dst_t == TYPE_STRUCT || dst_t == TYPE_UNION) {
                semantic_error(node, "التحويل الصريح إلى نوع مركب غير مدعوم حالياً.");
                node_clear_inferred_ptr(node);
                return dst_t;
            }

            if (dst_t == TYPE_POINTER) {
                if (node->data.cast_expr.target_ptr_depth <= 0) {
                    semantic_error(node, "نوع المؤشر الهدف في التحويل غير صالح.");
                }

                if ((node->data.cast_expr.target_ptr_base_type == TYPE_STRUCT &&
                     (!node->data.cast_expr.target_ptr_base_type_name || !struct_lookup_def(node->data.cast_expr.target_ptr_base_type_name))) ||
                    (node->data.cast_expr.target_ptr_base_type == TYPE_UNION &&
                     (!node->data.cast_expr.target_ptr_base_type_name || !union_lookup_def(node->data.cast_expr.target_ptr_base_type_name))) ||
                    (node->data.cast_expr.target_ptr_base_type == TYPE_ENUM &&
                     (!node->data.cast_expr.target_ptr_base_type_name || !enum_lookup_def(node->data.cast_expr.target_ptr_base_type_name)))) {
                    semantic_error(node, "أساس المؤشر الهدف في التحويل غير معرّف.");
                }

                if (!(src_t == TYPE_POINTER || datatype_is_intlike(src_t))) {
                    semantic_error(node, "التحويل إلى مؤشر يتطلب مؤشراً أو عدداً صحيحاً.");
                }

                node_set_inferred_ptr(node,
                                      node->data.cast_expr.target_ptr_base_type,
                                      node->data.cast_expr.target_ptr_base_type_name,
                                      node->data.cast_expr.target_ptr_depth);
                return TYPE_POINTER;
            }

            if (src_t == TYPE_POINTER) {
                if (!datatype_is_intlike(dst_t)) {
                    semantic_error(node, "التحويل من مؤشر يدعم الأنواع الصحيحة فقط حالياً.");
                }
                node_clear_inferred_ptr(node);
                return dst_t;
            }

            if (!(datatype_is_numeric_scalar(src_t) && datatype_is_numeric_scalar(dst_t))) {
                semantic_error(node, "التحويل الصريح يدعم التحويلات العددية أو المؤشرية فقط.");
            }

            node_clear_inferred_ptr(node);
            return dst_t;
        }

        case NODE_BIN_OP: {
            DataType left = infer_type(node->data.bin_op.left);
            DataType right = infer_type(node->data.bin_op.right);
            
            // العمليات الحسابية تتطلب أعداداً صحيحة
            OpType op = node->data.bin_op.op;

            // مقارنة مؤشرات الدوال (==/!=) فقط، مع السماح بالمقارنة مع عدم.
            if (left == TYPE_FUNC_PTR || right == TYPE_FUNC_PTR) {
                if (!(op == OP_EQ || op == OP_NEQ)) {
                    semantic_error(node, "مقارنة مؤشرات الدوال تدعم فقط (==) أو (!=).");
                }

                Node* l = node->data.bin_op.left;
                Node* r = node->data.bin_op.right;
                bool l_null = (l && l->type == NODE_NULL);
                bool r_null = (r && r->type == NODE_NULL);

                if (l_null && right == TYPE_FUNC_PTR) {
                    l->inferred_type = TYPE_FUNC_PTR;
                    node_set_inferred_funcptr(l, r ? r->inferred_func_sig : NULL);
                }
                if (r_null && left == TYPE_FUNC_PTR) {
                    r->inferred_type = TYPE_FUNC_PTR;
                    node_set_inferred_funcptr(r, l ? l->inferred_func_sig : NULL);
                }

                if (!l_null && !r_null) {
                    if (left != TYPE_FUNC_PTR || right != TYPE_FUNC_PTR) {
                        semantic_error(node, "مقارنة مؤشر دالة تتطلب مؤشراً مقابلاً أو قيمة عدم.");
                    } else if (!funcsig_equal(l ? l->inferred_func_sig : NULL, r ? r->inferred_func_sig : NULL)) {
                        semantic_error(node, "مقارنة مؤشري دالة تتطلب تطابق التوقيع.");
                    }
                }

                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }

            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                if (left == TYPE_POINTER || right == TYPE_POINTER) {
                    if ((op == OP_ADD || op == OP_SUB) &&
                        ((left == TYPE_POINTER && datatype_is_intlike(right)) ||
                         (op == OP_ADD && right == TYPE_POINTER && datatype_is_intlike(left)))) {
                        Node* pnode = (left == TYPE_POINTER) ? node->data.bin_op.left : node->data.bin_op.right;
                        if (!pnode || !ptr_arith_allowed(pnode->inferred_ptr_base_type, pnode->inferred_ptr_depth)) {
                            semantic_error(node, "الحساب على هذا النوع من المؤشرات غير مدعوم حالياً.");
                        }
                        node_set_inferred_ptr(node,
                                              pnode ? pnode->inferred_ptr_base_type : TYPE_INT,
                                              pnode ? pnode->inferred_ptr_base_type_name : NULL,
                                              pnode ? pnode->inferred_ptr_depth : 1);
                        return TYPE_POINTER;
                    }
                    if (op == OP_SUB && left == TYPE_POINTER && right == TYPE_POINTER) {
                        Node* lp = node->data.bin_op.left;
                        Node* rp = node->data.bin_op.right;
                        if (!lp || !rp || lp->inferred_ptr_depth <= 0 || rp->inferred_ptr_depth <= 0) {
                            semantic_error(node, "طرح المؤشر الفارغ غير مدعوم.");
                        } else if (!ptr_type_compatible(lp->inferred_ptr_base_type,
                                                        lp->inferred_ptr_base_type_name,
                                                        lp->inferred_ptr_depth,
                                                        rp->inferred_ptr_base_type,
                                                        rp->inferred_ptr_base_type_name,
                                                        rp->inferred_ptr_depth,
                                                        false)) {
                            semantic_error(node, "طرح مؤشرين يتطلب توافق نوع المؤشر.");
                        }
                        if (!ptr_arith_allowed(lp ? lp->inferred_ptr_base_type : TYPE_INT,
                                               lp ? lp->inferred_ptr_depth : 0)) {
                            semantic_error(node, "الحساب على هذا النوع من المؤشرات غير مدعوم حالياً.");
                        }
                        node_clear_inferred_ptr(node);
                        return TYPE_INT;
                    }
                    semantic_error(node, "عمليات المؤشرات المدعومة: pointer +/- int أو pointer - pointer.");
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }

                if (op == OP_MOD && (left == TYPE_FLOAT || right == TYPE_FLOAT)) {
                    semantic_error(node, "عملية باقي غير مدعومة على 'عشري'.");
                }

                // إذا كان أحد الطرفين عشرياً → النتيجة عشري (بعد تحويل الطرف الآخر).
                if (left == TYPE_FLOAT || right == TYPE_FLOAT) {
                    if (!((datatype_is_intlike(left) || left == TYPE_FLOAT) && (datatype_is_intlike(right) || right == TYPE_FLOAT))) {
                        semantic_error(node, "عمليات العشري تتطلب معاملات عددية.");
                    }
                    return TYPE_FLOAT;
                }

                if (!datatype_is_intlike(left) || !datatype_is_intlike(right)) {
                    semantic_error(node, "العمليات الحسابية تتطلب معاملات صحيحة.");
                }
                node_clear_inferred_ptr(node);
                return datatype_usual_arith(left, right);
            }

            // العمليات البتية والإزاحة
            if (op == OP_BIT_AND || op == OP_BIT_OR || op == OP_BIT_XOR ||
                op == OP_SHL || op == OP_SHR) {
                if (!datatype_is_intlike(left) || !datatype_is_intlike(right)) {
                    semantic_error(node, "العمليات البتية تتطلب معاملات عددية صحيحة.");
                    return TYPE_INT;
                }

                if ((op == OP_SHL || op == OP_SHR) &&
                    node->data.bin_op.right &&
                    node->data.bin_op.right->type == NODE_INT) {
                    int64_t sh = node->data.bin_op.right->data.integer.value;
                    if (sh < 0 || sh >= 64) {
                        semantic_error(node->data.bin_op.right, "قيمة الإزاحة يجب أن تكون بين ٠ و ٦٣.");
                    }
                }

                if (op == OP_SHL || op == OP_SHR) {
                    node_clear_inferred_ptr(node);
                    return datatype_int_promote(left);
                }
                node_clear_inferred_ptr(node);
                return datatype_usual_arith(left, right);
            }
            
            // عمليات المقارنة تتطلب أنواعاً متوافقة وتعيد منطقي
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                // سماح v0.3.12: مقارنة نص مع نص (==/!=) وكذلك نص مع عدم (==/!=) لتسهيل نمط EOF في اقرأ_سطر.
                if (left == TYPE_STRING || right == TYPE_STRING) {
                    if (!(op == OP_EQ || op == OP_NEQ)) {
                        semantic_error(node, "مقارنة النص تدعم فقط (==) أو (!=).");
                    }

                    Node* l = node->data.bin_op.left;
                    Node* r = node->data.bin_op.right;
                    bool l_null = (l && l->type == NODE_NULL);
                    bool r_null = (r && r->type == NODE_NULL);

                    // الحالات المدعومة:
                    // - نص == نص
                    // - نص == عدم (أو العكس)
                    if (left == TYPE_STRING && !(right == TYPE_STRING || r_null)) {
                        semantic_error(node, "مقارنة النص تتطلب نصاً أو 'عدم' على الطرف الآخر.");
                    }
                    if (right == TYPE_STRING && !(left == TYPE_STRING || l_null)) {
                        semantic_error(node, "مقارنة النص تتطلب نصاً أو 'عدم' على الطرف الآخر.");
                    }

                    // اجعل NODE_NULL يُعامل كنص فارغ (NULL) في سياق مقارنة النص.
                    if (l_null) {
                        l->inferred_type = TYPE_STRING;
                        node_clear_inferred_ptr(l);
                    }
                    if (r_null) {
                        r->inferred_type = TYPE_STRING;
                        node_clear_inferred_ptr(r);
                    }

                    node_clear_inferred_ptr(node);
                    return TYPE_BOOL;
                }

                if (left == TYPE_POINTER || right == TYPE_POINTER) {
                    if (left == TYPE_POINTER && right == TYPE_POINTER) {
                        int ldepth = node->data.bin_op.left->inferred_ptr_depth;
                        int rdepth = node->data.bin_op.right->inferred_ptr_depth;
                        if (!(ldepth == 0 || rdepth == 0) &&
                            !ptr_type_compatible(node->data.bin_op.left->inferred_ptr_base_type,
                                                 node->data.bin_op.left->inferred_ptr_base_type_name,
                                                 ldepth,
                                                 node->data.bin_op.right->inferred_ptr_base_type,
                                                 node->data.bin_op.right->inferred_ptr_base_type_name,
                                                 rdepth,
                                                 true)) {
                            semantic_error(node, "مقارنة المؤشرات تتطلب توافق نوع المؤشر.");
                        }
                        node_clear_inferred_ptr(node);
                        return TYPE_BOOL;
                    }
                    semantic_error(node, "مقارنة المؤشر تتطلب مؤشراً مقابلاً أو قيمة عدم.");
                    node_clear_inferred_ptr(node);
                    return TYPE_BOOL;
                }

                // مقارنة العشري: نسمح بمقارنة عشري مع عدد (يحوّل العدد إلى عشري).
                if (left == TYPE_FLOAT || right == TYPE_FLOAT) {
                    if (!((datatype_is_intlike(left) || left == TYPE_FLOAT) && (datatype_is_intlike(right) || right == TYPE_FLOAT))) {
                        semantic_error(node, "مقارنات العشري تتطلب معاملات عددية.");
                    }
                    node_clear_inferred_ptr(node);
                    return TYPE_BOOL;
                }
                maybe_warn_signed_unsigned_compare(left, right, node);
                if (left != right) {
                    // سماح C-like: مقارنة حرف مع صحيح.
                    if (!((datatype_is_intlike(left) && datatype_is_intlike(right)) ||
                          (left == TYPE_ENUM && right == TYPE_ENUM))) {
                        semantic_error(node, "عمليات المقارنة تتطلب تطابق نوعي المعاملات.");
                    }
                } else if (left == TYPE_ENUM) {
                    const char* ln = expr_enum_name(node->data.bin_op.left);
                    const char* rn = expr_enum_name(node->data.bin_op.right);
                    if (!ln || !rn || strcmp(ln, rn) != 0) {
                        semantic_error(node, "لا يمكن مقارنة قيم تعداد من أنواع مختلفة.");
                    }
                } else if (left == TYPE_STRUCT) {
                    semantic_error(node, "لا يمكن مقارنة الهياكل.");
                }
                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }
            
            // العمليات المنطقية (AND/OR) تتطلب منطقي أو صحيح وتعيد منطقي
            if (op == OP_AND || op == OP_OR) {
                // نقبل INT أو BOOL لأن C تعامل القيم غير الصفرية كـ true
                if ((!datatype_is_intlike(left) && left != TYPE_BOOL && left != TYPE_FLOAT) ||
                    (!datatype_is_intlike(right) && right != TYPE_BOOL && right != TYPE_FLOAT)) {
                    semantic_error(node, "العمليات المنطقية تتطلب معاملات صحيحة أو منطقية.");
                }
                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }
            
            return TYPE_INT;
        }

        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP: {
            DataType ot = infer_type(node->data.unary_op.operand);

            if (node->data.unary_op.op == UOP_ADDR) {
                Node* target = node->data.unary_op.operand;
                if (!target ||
                    (target->type != NODE_VAR_REF &&
                     target->type != NODE_MEMBER_ACCESS &&
                     target->type != NODE_ARRAY_ACCESS &&
                     target->type != NODE_UNARY_OP)) {
                    semantic_error(node, "أخذ العنوان '&' يتطلب قيمة قابلة للإسناد.");
                }

                if (ot == TYPE_STRUCT || ot == TYPE_UNION || ot == TYPE_VOID) {
                    semantic_error(node, "لا يمكن أخذ عنوان هذا النوع.");
                    node_set_inferred_ptr(node, TYPE_INT, NULL, 1);
                    return TYPE_POINTER;
                }

                if (ot == TYPE_POINTER) {
                    node_set_inferred_ptr(node,
                                          node->data.unary_op.operand->inferred_ptr_base_type,
                                          node->data.unary_op.operand->inferred_ptr_base_type_name,
                                          node->data.unary_op.operand->inferred_ptr_depth + 1);
                } else {
                    const char* base_name = NULL;
                    if (target->type == NODE_MEMBER_ACCESS &&
                        target->data.member_access.member_type == TYPE_ENUM) {
                        base_name = target->data.member_access.member_type_name;
                    }
                    node_set_inferred_ptr(node, ot, base_name, 1);
                }
                return TYPE_POINTER;
            }

            if (node->data.unary_op.op == UOP_DEREF) {
                if (ot != TYPE_POINTER) {
                    semantic_error(node, "فك الإشارة '*' يتطلب مؤشراً.");
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }

                int depth = node->data.unary_op.operand->inferred_ptr_depth;
                if (depth <= 0) {
                    semantic_error(node, "لا يمكن فك مؤشر فارغ مباشرة.");
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }

                if (depth == 1) {
                    node_clear_inferred_ptr(node);
                    return node->data.unary_op.operand->inferred_ptr_base_type;
                }

                node_set_inferred_ptr(node,
                                      node->data.unary_op.operand->inferred_ptr_base_type,
                                      node->data.unary_op.operand->inferred_ptr_base_type_name,
                                      depth - 1);
                return TYPE_POINTER;
            }

            if (ot == TYPE_FLOAT) {
                if (node->data.unary_op.op == UOP_NEG) {
                    // السماح بسالب عشري كحالة دنيا (بدون عمليات عشري أخرى).
                    node_clear_inferred_ptr(node);
                    return TYPE_FLOAT;
                }
                semantic_error(node, "عمليات العشري غير مدعومة حالياً.");
                node_clear_inferred_ptr(node);
                return TYPE_FLOAT;
            }

            if (!datatype_is_intlike(ot)) {
                semantic_error(node, "Unary operations require INTEGER operand.");
            }
            // -x و ~x يتبعان ترقيات الأعداد الصحيحة (قد يصبحان أوسع).
            if (node->data.unary_op.op == UOP_NEG || node->data.unary_op.op == UOP_BIT_NOT) {
                node_clear_inferred_ptr(node);
                return datatype_int_promote(ot);
            }
            if (node->data.unary_op.op == UOP_NOT) {
                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }
            node_clear_inferred_ptr(node);
            return ot;
        }

        default:
            return TYPE_INT;
    }
}

static DataType infer_type(Node* node)
{
    if (!node) return TYPE_INT;
    DataType t = infer_type_internal(node);
    node->inferred_type = t;
    if (t != TYPE_POINTER && t != TYPE_FUNC_PTR) {
        node_clear_inferred_ptr(node);
    }
    return t;
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
            // 0) تسجيل تعريفات الأنواع (تعداد/هيكل) أولاً
            Node* td = node->data.program.declarations;
            while (td) {
                if (td->type == NODE_ENUM_DECL) {
                    enum_register_decl(td);
                } else if (td->type == NODE_STRUCT_DECL) {
                    struct_register_decl(td);
                } else if (td->type == NODE_UNION_DECL) {
                    union_register_decl(td);
                }
                td = td->next;
            }

            // حساب تخطيط الذاكرة للهياكل/الاتحادات مبكراً لالتقاط الأخطاء بسرعة
            for (int i = 0; i < struct_count; i++) {
                (void)struct_compute_layout(&struct_defs[i]);
            }

            for (int i = 0; i < union_count; i++) {
                (void)union_compute_layout(&union_defs[i]);
            }

            // 0.5) تسجيل أسماء الأنواع البديلة (نوع) بعد تعريفات الأنواع المركبة
            Node* ta = node->data.program.declarations;
            while (ta) {
                if (ta->type == NODE_TYPE_ALIAS) {
                    type_alias_register_decl(ta);
                }
                ta = ta->next;
            }

            // 1) تسجيل تواقيع الدوال أولاً لدعم الاستدعاء قبل التعريف
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
            DataType declType = node->data.var_decl.type;
            const char* declTypeName = node->data.var_decl.type_name;
            bool has_static_storage = node->data.var_decl.is_global || node->data.var_decl.is_static;

            if (declType == TYPE_VOID) {
                semantic_error(node,
                               node->data.var_decl.is_global
                                   ? "لا يمكن تعريف متغير عام من نوع 'عدم'."
                                   : "لا يمكن تعريف متغير من نوع 'عدم'.");
                if (node->data.var_decl.expression) {
                    (void)infer_type(node->data.var_decl.expression);
                }
                break;
            }

            if (declType == TYPE_POINTER) {
                if (node->data.var_decl.ptr_depth <= 0) {
                    semantic_error(node, "عمق المؤشر غير صالح في تعريف '%s'.",
                                   node->data.var_decl.name ? node->data.var_decl.name : "???");
                }
                if ((node->data.var_decl.ptr_base_type == TYPE_STRUCT &&
                     (!node->data.var_decl.ptr_base_type_name || !struct_lookup_def(node->data.var_decl.ptr_base_type_name))) ||
                    (node->data.var_decl.ptr_base_type == TYPE_UNION &&
                     (!node->data.var_decl.ptr_base_type_name || !union_lookup_def(node->data.var_decl.ptr_base_type_name))) ||
                    (node->data.var_decl.ptr_base_type == TYPE_ENUM &&
                     (!node->data.var_decl.ptr_base_type_name || !enum_lookup_def(node->data.var_decl.ptr_base_type_name)))) {
                    semantic_error(node, "أساس المؤشر غير معرّف في '%s'.",
                                   node->data.var_decl.name ? node->data.var_decl.name : "???");
                }
            }

            // 1) تحقق الأنواع المركبة
            if (declType == TYPE_ENUM) {
                if (!declTypeName || !enum_lookup_def(declTypeName)) {
                    semantic_error(node, "تعداد غير معرّف '%s'.", declTypeName ? declTypeName : "???");
                }

                if (node->data.var_decl.expression) {
                    DataType exprType = infer_type(node->data.var_decl.expression);
                    const char* en = expr_enum_name(node->data.var_decl.expression);
                    if (!types_compatible_named(exprType, en, TYPE_ENUM, declTypeName)) {
                        semantic_error(node,
                                       "عدم تطابق نوع التعداد في تهيئة '%s'.", node->data.var_decl.name);
                    }
                    if (has_static_storage &&
                        !static_storage_initializer_is_const(node->data.var_decl.expression, declType)) {
                        semantic_error(node, "تهيئة المتغير الساكن '%s' يجب أن تكون ثابتة وقت الترجمة.",
                                       node->data.var_decl.name ? node->data.var_decl.name : "???");
                    }
                } else {
                    if (!has_static_storage) {
                        semantic_error(node, "متغير التعداد '%s' يجب أن يُهيّأ.", node->data.var_decl.name);
                    }
                }
            }
            else if (declType == TYPE_STRUCT || declType == TYPE_UNION) {
                if (!declTypeName) {
                    semantic_error(node, "اسم نوع مركب مفقود.");
                }
                if (declType == TYPE_STRUCT) {
                    StructDef* sd = struct_lookup_def(declTypeName);
                    if (!sd) {
                        semantic_error(node, "هيكل غير معرّف '%s'.", declTypeName ? declTypeName : "???");
                    } else {
                        (void)struct_compute_layout(sd);
                        node->data.var_decl.struct_size = sd->size;
                        node->data.var_decl.struct_align = sd->align;
                    }
                } else {
                    UnionDef* ud = union_lookup_def(declTypeName);
                    if (!ud) {
                        semantic_error(node, "اتحاد غير معرّف '%s'.", declTypeName ? declTypeName : "???");
                    } else {
                        (void)union_compute_layout(ud);
                        node->data.var_decl.struct_size = ud->size;
                        node->data.var_decl.struct_align = ud->align;
                    }
                }

                if (node->data.var_decl.expression) {
                    semantic_error(node, "تهيئة النوع المركب بهذه الصيغة غير مدعومة حالياً.");
                }
                if (node->data.var_decl.is_const) {
                    semantic_error(node, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
                }
            }
            else {
                // 2) الأنواع البدائية
                if (node->data.var_decl.expression) {
                    DataType exprType = infer_type_allow_null_string(node->data.var_decl.expression, declType);
                    if (declType == TYPE_POINTER) {
                        if (!ptr_type_compatible(node->data.var_decl.expression->inferred_ptr_base_type,
                                                 node->data.var_decl.expression->inferred_ptr_base_type_name,
                                                 node->data.var_decl.expression->inferred_ptr_depth,
                                                 node->data.var_decl.ptr_base_type,
                                                 node->data.var_decl.ptr_base_type_name,
                                                 node->data.var_decl.ptr_depth,
                                                 true)) {
                            semantic_error(node, "عدم تطابق نوع المؤشر في تعريف '%s'.",
                                           node->data.var_decl.name ? node->data.var_decl.name : "???");
                        }
                    } else if (declType == TYPE_FUNC_PTR) {
                        FuncPtrSig* expected_sig = node->data.var_decl.func_sig;
                        if (!expected_sig) {
                            semantic_error(node, "توقيع مؤشر الدالة في تعريف '%s' مفقود.",
                                           node->data.var_decl.name ? node->data.var_decl.name : "???");
                        } else if (node->data.var_decl.expression->type == NODE_NULL) {
                            // تثبيت نوع null كي يُخفض إلى IR_TYPE_FUNC بشكل صحيح.
                            node->data.var_decl.expression->inferred_type = TYPE_FUNC_PTR;
                            node_set_inferred_funcptr(node->data.var_decl.expression, expected_sig);
                        } else if (exprType != TYPE_FUNC_PTR ||
                                   !funcsig_equal(node->data.var_decl.expression->inferred_func_sig, expected_sig)) {
                            semantic_error(node,
                                           "عدم تطابق توقيع مؤشر الدالة في تعريف '%s'.",
                                           node->data.var_decl.name ? node->data.var_decl.name : "???");
                        }
                    } else if (!types_compatible(exprType, declType)) {
                        semantic_error(node,
                                      "عدم تطابق النوع في تعريف '%s' (المتوقع %s لكن وُجد %s).",
                                      node->data.var_decl.name,
                                      datatype_to_str(declType),
                                      datatype_to_str(exprType));
                    } else {
                        maybe_warn_implicit_narrowing(exprType, declType, node->data.var_decl.expression);
                    }
                    if (has_static_storage &&
                        !static_storage_initializer_is_const(node->data.var_decl.expression, declType)) {
                        semantic_error(node, "تهيئة المتغير الساكن '%s' يجب أن تكون ثابتة وقت الترجمة.",
                                       node->data.var_decl.name ? node->data.var_decl.name : "???");
                    }
                }
                if (node->data.var_decl.is_const &&
                    !has_static_storage &&
                    !node->data.var_decl.expression) {
                    semantic_error(node, "الثابت '%s' يجب تهيئته.", node->data.var_decl.name);
                }
            }

            // 3) إضافة الرمز (مع معلومات الموقع للتحذيرات)
            add_symbol(node->data.var_decl.name,
                       node->data.var_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       declType,
                       declTypeName,
                       node->data.var_decl.ptr_base_type,
                       node->data.var_decl.ptr_base_type_name,
                       node->data.var_decl.ptr_depth,
                       node->data.var_decl.func_sig,
                       node->data.var_decl.is_const,
                       node->data.var_decl.is_static,
                       false,
                       0,
                       NULL,
                       0,
                       node->line, node->col, node->filename ? node->filename : current_filename);
            break;
         }

        case NODE_TYPE_ALIAS:
            // تم تسجيل أسماء الأنواع البديلة في مرحلة NODE_PROGRAM.
            break;

        case NODE_FUNC_DEF: {
            // الدخول في نطاق دالة جديدة (تصفير المحلي + إنشاء نطاق)
            local_count = 0;
            symbol_hash_reset_heads(local_symbol_hash_head, ANALYSIS_SYMBOL_HASH_BUCKETS);
            symbol_hash_reset_next(local_symbol_hash_next, ANALYSIS_MAX_SYMBOLS);
            scope_depth = 0;
            scope_push();

            bool prev_in_func = in_function;
            DataType prev_ret = current_func_return_type;
            DataType prev_ret_ptr_base = current_func_return_ptr_base_type;
            const char* prev_ret_ptr_base_name = current_func_return_ptr_base_type_name;
            int prev_ret_ptr_depth = current_func_return_ptr_depth;
            FuncPtrSig* prev_ret_func_sig = current_func_return_func_sig;
            bool prev_is_variadic = current_func_is_variadic;
            const char* prev_variadic_anchor = current_func_variadic_anchor_name;
            in_function = true;
            current_func_return_type = node->data.func_def.return_type;
            current_func_return_ptr_base_type = node->data.func_def.return_ptr_base_type;
            current_func_return_ptr_base_type_name = node->data.func_def.return_ptr_base_type_name;
            current_func_return_ptr_depth = node->data.func_def.return_ptr_depth;
            current_func_return_func_sig = node->data.func_def.return_func_sig;
            current_func_is_variadic = node->data.func_def.is_variadic;
            current_func_variadic_anchor_name = NULL;

            // إضافة المعاملات كمتغيرات محلية (المعاملات ليست ثوابت افتراضياً)
            Node* param = node->data.func_def.params;
            while (param) {
                 if (param->type == NODE_VAR_DECL) {
                    if (param->data.var_decl.type == TYPE_VOID) {
                        semantic_error(param, "لا يمكن استخدام 'عدم' كنوع معامل.");
                        param = param->next;
                        continue;
                    }
                    if (param->data.var_decl.type == TYPE_POINTER && param->data.var_decl.ptr_depth <= 0) {
                        semantic_error(param, "عمق مؤشر المعامل غير صالح.");
                    }
                      // المعاملات تُعتبر "مستخدمة" ضمنياً (لتجنب تحذيرات خاطئة)
                     add_symbol(param->data.var_decl.name, SCOPE_LOCAL,
                                param->data.var_decl.type, param->data.var_decl.type_name,
                                param->data.var_decl.ptr_base_type,
                                param->data.var_decl.ptr_base_type_name,
                                param->data.var_decl.ptr_depth,
                                param->data.var_decl.func_sig,
                                false,
                                false,
                                false, 0, NULL, 0,
                                param->line, param->col, param->filename ? param->filename : current_filename);
                     // تعليم المعامل كمستخدم مباشرة
                     if (local_count > 0) {
                         local_symbols[local_count - 1].is_used = true;
                     }
                     current_func_variadic_anchor_name = param->data.var_decl.name;
                 }
                param = param->next;
            }

            if (current_func_is_variadic && !current_func_variadic_anchor_name) {
                semantic_error(node, "الدالة المتغيرة المعاملات تتطلب معاملاً ثابتاً واحداً على الأقل قبل '...'.");
            }

            // تحليل جسم الدالة فقط إذا لم تكن نموذجاً أولياً
            if (!node->data.func_def.is_prototype) {
                analyze_node(node->data.func_def.body);
            }

            // خروج نطاق الدالة (يشمل تحذيرات غير المستخدم)
            scope_pop();

            in_function = prev_in_func;
            current_func_return_type = prev_ret;
            current_func_return_ptr_base_type = prev_ret_ptr_base;
            current_func_return_ptr_base_type_name = prev_ret_ptr_base_name;
            current_func_return_ptr_depth = prev_ret_ptr_depth;
            current_func_return_func_sig = prev_ret_func_sig;
            current_func_is_variadic = prev_is_variadic;
            current_func_variadic_anchor_name = prev_variadic_anchor;
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
                semantic_error(node, "إسناد إلى متغير غير معرّف '%s'.", node->data.assign_stmt.name);
            } else {
                if (sym->is_array) {
                    semantic_error(node, "لا يمكن تعيين قيمة للمصفوفة '%s' مباشرة (استخدم الفهرسة).", sym->name);
                }
                if (sym->type == TYPE_STRUCT) {
                    semantic_error(node, "لا يمكن إسناد قيمة إلى هيكل '%s' مباشرة.", sym->name);
                }
                // التحقق من الثوابت: لا يمكن إعادة تعيين قيمة ثابت
                if (sym->is_const) {
                    semantic_error(node, "لا يمكن إعادة إسناد الثابت '%s'.", node->data.assign_stmt.name);
                }
                DataType exprType = infer_type_allow_null_string(node->data.assign_stmt.expression, sym->type);
                if (sym->type == TYPE_ENUM) {
                    const char* en = expr_enum_name(node->data.assign_stmt.expression);
                    if (!types_compatible_named(exprType, en, TYPE_ENUM, sym->type_name)) {
                        semantic_error(node, "عدم تطابق نوع التعداد في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (sym->type == TYPE_POINTER) {
                    if (!ptr_type_compatible(node->data.assign_stmt.expression->inferred_ptr_base_type,
                                             node->data.assign_stmt.expression->inferred_ptr_base_type_name,
                                             node->data.assign_stmt.expression->inferred_ptr_depth,
                                             sym->ptr_base_type,
                                             sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                             sym->ptr_depth,
                                             true)) {
                        semantic_error(node, "عدم تطابق نوع المؤشر في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (sym->type == TYPE_FUNC_PTR) {
                    FuncPtrSig* expected_sig = sym->func_sig;
                    if (!expected_sig) {
                        semantic_error(node, "توقيع مؤشر الدالة للرمز '%s' مفقود.", node->data.assign_stmt.name);
                    } else if (node->data.assign_stmt.expression &&
                               node->data.assign_stmt.expression->type == NODE_NULL) {
                        node->data.assign_stmt.expression->inferred_type = TYPE_FUNC_PTR;
                        node_set_inferred_funcptr(node->data.assign_stmt.expression, expected_sig);
                    } else if (exprType != TYPE_FUNC_PTR ||
                               !funcsig_equal(node->data.assign_stmt.expression->inferred_func_sig, expected_sig)) {
                        semantic_error(node, "عدم تطابق توقيع مؤشر الدالة في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (!types_compatible(exprType, sym->type)) {
                    semantic_error(node, "عدم تطابق النوع في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                } else {
                    maybe_warn_implicit_narrowing(exprType, sym->type, node->data.assign_stmt.expression);
                }
            }
            break;
        }

        case NODE_MEMBER_ASSIGN: {
            Node* target = node->data.member_assign.target;
            Node* value = node->data.member_assign.value;

            if (!target || target->type != NODE_MEMBER_ACCESS) {
                semantic_error(node, "الهدف في إسناد العضو غير صالح.");
                (void)infer_type(value);
                break;
            }

            resolve_member_access(target);

            if (target->data.member_access.is_enum_value) {
                semantic_error(node, "لا يمكن الإسناد إلى قيمة تعداد.");
                (void)infer_type(value);
                break;
            }
            if (!target->data.member_access.is_struct_member) {
                semantic_error(node, "إسناد عضو يتطلب هيكلاً.");
                (void)infer_type(value);
                break;
            }
            if (target->data.member_access.member_type == TYPE_STRUCT) {
                semantic_error(node, "إسناد عضو من نوع هيكل غير مدعوم حالياً.");
                (void)infer_type(value);
                break;
            }

            if (target->data.member_access.member_is_const) {
                semantic_error(node, "لا يمكن تعديل عضو ثابت داخل الهيكل.");
            }

            // تحقق ثابتية الهيكل الجذري
            Symbol* root = NULL;
            if (target->data.member_access.root_var) {
                root = lookup(target->data.member_access.root_var, true);
            }
            if (root && root->is_const) {
                semantic_error(node, "لا يمكن تعديل هيكل ثابت '%s'.", root->name);
            }

            DataType got = infer_type(value);
            DataType expected = target->data.member_access.member_type;
            if (expected == TYPE_ENUM) {
                const char* en = expr_enum_name(value);
                const char* expn = target->data.member_access.member_type_name;
                if (!types_compatible_named(got, en, TYPE_ENUM, expn)) {
                    semantic_error(node, "عدم تطابق نوع التعداد في إسناد العضو.");
                }
            } else if (expected == TYPE_POINTER) {
                if (!ptr_type_compatible(value->inferred_ptr_base_type,
                                         value->inferred_ptr_base_type_name,
                                         value->inferred_ptr_depth,
                                         target->data.member_access.member_ptr_base_type,
                                         target->data.member_access.member_ptr_base_type_name,
                                         target->data.member_access.member_ptr_depth,
                                         true)) {
                    semantic_error(node, "عدم تطابق نوع المؤشر في إسناد العضو.");
                }
            } else if (!types_compatible(got, expected)) {
                semantic_error(node, "عدم تطابق النوع في إسناد العضو.");
            } else {
                maybe_warn_implicit_narrowing(got, expected, value);
            }
            break;
        }

        case NODE_DEREF_ASSIGN: {
            Node* target = node->data.deref_assign.target;
            Node* value = node->data.deref_assign.value;
            if (!target || target->type != NODE_UNARY_OP || target->data.unary_op.op != UOP_DEREF) {
                semantic_error(node, "هدف الإسناد عبر المؤشر غير صالح.");
                if (value) (void)infer_type(value);
                break;
            }

            DataType tt = infer_type(target);
            if (tt == TYPE_STRUCT || tt == TYPE_UNION || tt == TYPE_VOID) {
                semantic_error(node, "الإسناد عبر المؤشر لهذا النوع غير مدعوم.");
                if (value) (void)infer_type(value);
                break;
            }

            DataType vt = infer_type(value);
            if (!types_compatible(vt, tt)) {
                semantic_error(node, "نوع القيمة في الإسناد عبر المؤشر غير متوافق.");
            }
            break;
        }

        case NODE_IF: {
            DataType condType = infer_type(node->data.if_stmt.condition);
            if (!datatype_is_intlike(condType)) {
                semantic_error(node->data.if_stmt.condition, "شرط 'إذا' يجب أن يكون عدداً صحيحاً أو منطقياً.");
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
            if (!datatype_is_intlike(condType)) {
                semantic_error(node->data.while_stmt.condition, "شرط 'طالما' يجب أن يكون عدداً صحيحاً أو منطقياً.");
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
                if (!datatype_is_intlike(condType)) {
                    semantic_error(node->data.for_stmt.condition, "شرط 'لكل' يجب أن يكون عدداً صحيحاً أو منطقياً.");
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
            DataType st = infer_type(node->data.switch_stmt.expression);
            if (!datatype_is_intlike(st)) {
                semantic_error(node->data.switch_stmt.expression, "تعبير 'اختر' يجب أن يكون عدداً صحيحاً.");
            }
            bool prev_switch = inside_switch;
            inside_switch = true;

            // نطاق switch
            scope_push();
            
            Node* current_case = node->data.switch_stmt.cases;
            while (current_case) {
                if (!current_case->data.case_stmt.is_default) {
                    DataType ct = infer_type(current_case->data.case_stmt.value);
                    if (!datatype_is_intlike(ct)) {
                        semantic_error(current_case->data.case_stmt.value, "قيمة 'حالة' يجب أن تكون ثابتاً صحيحاً.");
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
                semantic_error(node, "تم استخدام 'توقف' خارج حلقة أو 'اختر'.");
            }
            break;

        case NODE_CONTINUE:
            if (!inside_loop) {
                semantic_error(node, "تم استخدام 'استمر' خارج حلقة.");
            }
            break;

        case NODE_RETURN:
            if (!in_function) {
                semantic_error(node, "جملة 'إرجع' خارج دالة.");
                if (node->data.return_stmt.expression) {
                    (void)infer_type(node->data.return_stmt.expression);
                }
                break;
            }

            if (current_func_return_type == TYPE_VOID) {
                if (node->data.return_stmt.expression) {
                    (void)infer_type(node->data.return_stmt.expression);
                    semantic_error(node, "الدالة من نوع 'عدم' لا تعيد قيمة.");
                }
                break;
            }

            if (!node->data.return_stmt.expression) {
                semantic_error(node, "الدالة غير 'عدم' يجب أن تعيد قيمة.");
                break;
            }

            {
                DataType rt = infer_type_allow_null_string(node->data.return_stmt.expression, current_func_return_type);
                if (current_func_return_type == TYPE_POINTER) {
                    if (!ptr_type_compatible(node->data.return_stmt.expression->inferred_ptr_base_type,
                                             node->data.return_stmt.expression->inferred_ptr_base_type_name,
                                             node->data.return_stmt.expression->inferred_ptr_depth,
                                             current_func_return_ptr_base_type,
                                             current_func_return_ptr_base_type_name,
                                             current_func_return_ptr_depth,
                                             true)) {
                        semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة.");
                    }
                } else if (current_func_return_type == TYPE_FUNC_PTR) {
                    FuncPtrSig* expected_sig = current_func_return_func_sig;
                    if (!expected_sig) {
                        semantic_error(node, "توقيع مؤشر الدالة كنوع إرجاع للدالة مفقود.");
                    } else if (node->data.return_stmt.expression->type == NODE_NULL) {
                        node->data.return_stmt.expression->inferred_type = TYPE_FUNC_PTR;
                        node_set_inferred_funcptr(node->data.return_stmt.expression, expected_sig);
                    } else if (rt != TYPE_FUNC_PTR ||
                               !funcsig_equal(node->data.return_stmt.expression->inferred_func_sig, expected_sig)) {
                        semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة (توقيع مؤشر دالة مختلف).");
                    }
                } else if (!types_compatible(rt, current_func_return_type)) {
                    semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة.");
                } else {
                    maybe_warn_implicit_narrowing(rt, current_func_return_type, node->data.return_stmt.expression);
                }
            }
            break;

        case NODE_PRINT:
        {
            DataType pt = infer_type(node->data.print_stmt.expression);
            (void)pt;
            break;
        }
        
        case NODE_READ: {
            // التحقق من أن المتغير معرف وقابل للتعديل
            Symbol* sym = lookup(node->data.read_stmt.var_name, true);
            if (!sym) {
                semantic_error(node, "القراءة إلى متغير غير معرّف '%s'.", node->data.read_stmt.var_name);
            } else {
                if (sym->is_const) {
                    semantic_error(node, "لا يمكن القراءة إلى متغير ثابت '%s'.", node->data.read_stmt.var_name);
                }
                // نقبل القراءة في الأنواع الصحيحة (بما فيها الأحجام المختلفة).
                if (!datatype_is_int(sym->type)) {
                    semantic_error(node, "'اقرأ' يدعم حالياً المتغيرات الصحيحة فقط.");
                }
            }
            break;
        }
            
        case NODE_CALL_STMT:
        {
            // تحقق استدعاء دالة كجملة
            const char* fname = node->data.call.name;
            // 1) أعطِ أولوية للرموز في النطاق (متغيرات/معاملات) حتى تعمل الـ shadowing بشكل صحيح.
            Symbol* callee_sym = lookup(fname, true);
            if (callee_sym) {
                if (callee_sym->is_array) {
                    semantic_error(node, "لا يمكن نداء المصفوفة '%s'.", callee_sym->name);
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    break;
                }

                if (callee_sym->type != TYPE_FUNC_PTR) {
                    semantic_error(node, "المعرف '%s' ليس دالة ولا مؤشر دالة قابل للنداء.", fname ? fname : "???");
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    break;
                }

                FuncPtrSig* sig = callee_sym->func_sig;
                if (!sig) {
                    semantic_error(node, "توقيع مؤشر الدالة للرمز '%s' مفقود.", fname ? fname : "???");
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    break;
                }
                if (sig->is_variadic) {
                    semantic_error(node, "نداء مؤشر دالة متغير المعاملات غير مدعوم حالياً.");
                }

                int i = 0;
                Node* arg = node->data.call.args;
                while (arg) {
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
                                semantic_error(arg, "نوع المعامل %d في نداء '%s' غير متوافق.", i + 1, fname ? fname : "???");
                            }
                        } else if (exp == TYPE_FUNC_PTR) {
                            semantic_error(arg, "Higher-order غير مدعوم: معامل %d في توقيع مؤشر الدالة هو مؤشر دالة.", i + 1);
                        } else if (!types_compatible(at, exp)) {
                            semantic_error(arg, "نوع المعامل %d في نداء '%s' غير متوافق.", i + 1, fname ? fname : "???");
                        } else {
                            maybe_warn_implicit_narrowing(at, exp, arg);
                        }
                    } else {
                        DataType at = infer_type(arg);
                        if (sig->is_variadic) {
                            if (!variadic_extra_arg_type_supported(at)) {
                                semantic_error(arg, "نوع المعامل المتغير %d في نداء '%s' غير مدعوم.",
                                               i + 1, fname ? fname : "???");
                            }
                        }
                    }
                    i++;
                    arg = arg->next;
                }

                if (sig->is_variadic) {
                    if (i < sig->param_count) {
                        semantic_error(node, "عدد معاملات نداء '%s' غير صحيح (المتوقع على الأقل %d).",
                                       fname ? fname : "???", sig->param_count);
                    }
                } else if (i != sig->param_count) {
                    semantic_error(node, "عدد معاملات نداء '%s' غير صحيح (المتوقع %d).",
                                   fname ? fname : "???", sig->param_count);
                }
                break;
            }

            // 2) ثم نحل الدوال المعرفة (أو builtins) إذا لم يوجد رمز بنفس الاسم.
            FuncSymbol* fs = func_lookup(fname);
            if (!fs || !fs->is_defined) {
                DataType built_ret = TYPE_VOID;
                if (builtin_check_format_call(node, fname, node->data.call.args, &built_ret)) {
                    break;
                }
                if (builtin_check_variadic_runtime_call(node, fname, node->data.call.args, &built_ret)) {
                    break;
                }
                if (builtin_check_string_call(node, fname, node->data.call.args, &built_ret)) {
                    break;
                }
                if (builtin_check_mem_call(node, fname, node->data.call.args, &built_ret)) {
                    break;
                }
                if (builtin_check_file_call(node, fname, node->data.call.args, &built_ret)) {
                    break;
                }
                if (!fs) {
                    semantic_error(node, "استدعاء دالة غير معرّفة '%s'.", fname ? fname : "???");
                    Node* arg = node->data.call.args;
                    while (arg) { (void)infer_type(arg); arg = arg->next; }
                    break;
                }
            }

            int i = 0;
            Node* arg = node->data.call.args;
            while (arg) {
                if (i < fs->param_count) {
                    DataType expected0 = fs->param_types[i];
                    DataType at = infer_type_allow_null_string(arg, expected0);
                    if (fs->param_types[i] == TYPE_POINTER) {
                        if (at != TYPE_POINTER ||
                            !ptr_type_compatible(arg->inferred_ptr_base_type,
                                                 arg->inferred_ptr_base_type_name,
                                                 arg->inferred_ptr_depth,
                                                 fs->param_ptr_base_types ? fs->param_ptr_base_types[i] : TYPE_INT,
                                                 (fs->param_ptr_base_type_names && fs->param_ptr_base_type_names[i]) ? fs->param_ptr_base_type_names[i] : NULL,
                                                 fs->param_ptr_depths ? fs->param_ptr_depths[i] : 0,
                                                 true)) {
                            semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                        }
                    } else if (fs->param_types[i] == TYPE_FUNC_PTR) {
                        FuncPtrSig* expected_sig = (fs->param_func_sigs) ? fs->param_func_sigs[i] : NULL;
                        if (!expected_sig) {
                            semantic_error(arg, "توقيع مؤشر الدالة لمعامل %d في '%s' مفقود.", i + 1, fs->name);
                        } else if (arg->type == NODE_NULL) {
                            arg->inferred_type = TYPE_FUNC_PTR;
                            node_set_inferred_funcptr(arg, expected_sig);
                        } else if (at != TYPE_FUNC_PTR || !funcsig_equal(arg->inferred_func_sig, expected_sig)) {
                            semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق (توقيع مؤشر دالة مختلف).", i + 1, fs->name);
                        }
                    } else if (!types_compatible(at, fs->param_types[i])) {
                        semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                    } else {
                        maybe_warn_implicit_narrowing(at, fs->param_types[i], arg);
                    }
                } else {
                    DataType at = infer_type(arg);
                    if (fs->is_variadic) {
                        if (!variadic_extra_arg_type_supported(at)) {
                            semantic_error(arg, "نوع المعامل المتغير %d في '%s' غير مدعوم.", i + 1, fs->name);
                        }
                    }
                }
                i++;
                arg = arg->next;
            }
            if (fs->is_variadic) {
                if (i < fs->param_count) {
                    semantic_error(node, "عدد معاملات '%s' غير صحيح (المتوقع على الأقل %d).",
                                   fs->name, fs->param_count);
                }
            } else if (i != fs->param_count) {
                semantic_error(node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", fs->name, fs->param_count);
            }
            break;
        }

        case NODE_ARRAY_DECL: {
            if (node->data.array_decl.dim_count <= 0 ||
                !node->data.array_decl.dims ||
                node->data.array_decl.total_elems <= 0) {
                semantic_error(node, "حجم المصفوفة غير صالح.");
                break;
            }
            for (int i = 0; i < node->data.array_decl.dim_count; i++) {
                if (node->data.array_decl.dims[i] <= 0) {
                    semantic_error(node, "البعد %d للمصفوفة غير صالح.", i + 1);
                }
            }

            bool has_static_storage = node->data.array_decl.is_global || node->data.array_decl.is_static;
            DataType elem_type = node->data.array_decl.element_type;
            const char* elem_type_name = node->data.array_decl.element_type_name;

            if (elem_type == TYPE_VOID) {
                semantic_error(node, "لا يمكن تعريف مصفوفة من نوع 'عدم'.");
            }
            if (elem_type == TYPE_POINTER && node->data.array_decl.element_ptr_depth <= 0) {
                semantic_error(node, "عمق مؤشر عنصر المصفوفة غير صالح.");
            }
            if (elem_type == TYPE_ENUM) {
                if (!elem_type_name || !enum_lookup_def(elem_type_name)) {
                    semantic_error(node, "تعداد عنصر المصفوفة غير معرّف '%s'.",
                                   elem_type_name ? elem_type_name : "???");
                }
            } else if (elem_type == TYPE_STRUCT) {
                StructDef* sd = elem_type_name ? struct_lookup_def(elem_type_name) : NULL;
                if (!sd) {
                    semantic_error(node, "هيكل عنصر المصفوفة غير معرّف '%s'.",
                                   elem_type_name ? elem_type_name : "???");
                } else {
                    (void)struct_compute_layout(sd);
                    node->data.array_decl.element_struct_size = sd->size;
                    node->data.array_decl.element_struct_align = sd->align;
                }
            } else if (elem_type == TYPE_UNION) {
                UnionDef* ud = elem_type_name ? union_lookup_def(elem_type_name) : NULL;
                if (!ud) {
                    semantic_error(node, "اتحاد عنصر المصفوفة غير معرّف '%s'.",
                                   elem_type_name ? elem_type_name : "???");
                } else {
                    (void)union_compute_layout(ud);
                    node->data.array_decl.element_struct_size = ud->size;
                    node->data.array_decl.element_struct_align = ud->align;
                }
            } else if (elem_type == TYPE_POINTER) {
                if ((node->data.array_decl.element_ptr_base_type == TYPE_STRUCT &&
                     (!node->data.array_decl.element_ptr_base_type_name || !struct_lookup_def(node->data.array_decl.element_ptr_base_type_name))) ||
                    (node->data.array_decl.element_ptr_base_type == TYPE_UNION &&
                     (!node->data.array_decl.element_ptr_base_type_name || !union_lookup_def(node->data.array_decl.element_ptr_base_type_name))) ||
                    (node->data.array_decl.element_ptr_base_type == TYPE_ENUM &&
                     (!node->data.array_decl.element_ptr_base_type_name || !enum_lookup_def(node->data.array_decl.element_ptr_base_type_name)))) {
                    semantic_error(node, "أساس مؤشر عنصر المصفوفة غير معرّف.");
                }
            }

            // الثابت يجب أن يُهيّأ صراحةً (حتى `{}` مقبول ويعني أصفار).
            if (node->data.array_decl.is_const && !has_static_storage && !node->data.array_decl.has_init) {
                semantic_error(node, "المصفوفة الثابتة '%s' يجب تهيئتها.",
                               node->data.array_decl.name ? node->data.array_decl.name : "???");
            }

            add_symbol(node->data.array_decl.name,
                       node->data.array_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       elem_type,
                       elem_type_name,
                       node->data.array_decl.element_ptr_base_type,
                       node->data.array_decl.element_ptr_base_type_name,
                       node->data.array_decl.element_ptr_depth,
                       NULL,
                       node->data.array_decl.is_const,
                       node->data.array_decl.is_static,
                       true,
                       node->data.array_decl.dim_count,
                       node->data.array_decl.dims,
                       node->data.array_decl.total_elems,
                       node->line,
                       node->col,
                       node->filename ? node->filename : current_filename);

            // التحقق من قائمة التهيئة: تسمح بالتهيئة الجزئية كما في C.
            if (node->data.array_decl.has_init) {
                int n = node->data.array_decl.init_count;
                int64_t cap64 = node->data.array_decl.total_elems;
                int cap = (cap64 > (int64_t)INT_MAX) ? INT_MAX : (int)cap64;
                if (n < 0) n = 0;
                if (n > cap) {
                    semantic_error(node, "عدد عناصر تهيئة المصفوفة '%s' (%d) أكبر من الحجم (%d).",
                                   node->data.array_decl.name ? node->data.array_decl.name : "???",
                                   n, cap);
                }

                int idx0 = 0;
                for (Node* v = node->data.array_decl.init_values; v; v = v->next) {
                    DataType vt = infer_type(v);
                    if (elem_type == TYPE_POINTER) {
                        if (!ptr_type_compatible(v->inferred_ptr_base_type,
                                                 v->inferred_ptr_base_type_name,
                                                 v->inferred_ptr_depth,
                                                 node->data.array_decl.element_ptr_base_type,
                                                 node->data.array_decl.element_ptr_base_type_name,
                                                 node->data.array_decl.element_ptr_depth,
                                                 true)) {
                            semantic_error(v, "نوع عنصر التهيئة %d للمصفوفة '%s' غير متوافق.",
                                           idx0 + 1,
                                           node->data.array_decl.name ? node->data.array_decl.name : "???");
                        }
                    } else if (!types_compatible(vt, elem_type)) {
                        semantic_error(v, "نوع عنصر التهيئة %d للمصفوفة '%s' غير متوافق.",
                                       idx0 + 1,
                                       node->data.array_decl.name ? node->data.array_decl.name : "???");
                    } else {
                        maybe_warn_implicit_narrowing(vt, elem_type, v);
                    }

                    if (has_static_storage) {
                        if (!static_storage_initializer_is_const(v, elem_type)) {
                            semantic_error(v,
                                           "تهيئة المصفوفة الساكنة '%s' يجب أن تكون تعبيراً ثابتاً.",
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
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                (void)infer_type(node->data.array_op.value);
                break;
            }

            int supplied = node->data.array_op.index_count;
            if (supplied <= 0) supplied = node_list_count(node->data.array_op.indices);

            if (!sym->is_array) {
                if (sym->type == TYPE_STRING) {
                    semantic_error(node, "لا يمكن تعديل عناصر النص '%s' حالياً.", sym->name);
                    for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                        (void)infer_type(idx);
                    }
                    (void)infer_type(node->data.array_op.value);
                    break;
                }
                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                (void)infer_type(node->data.array_op.value);
                break;
            }
            if (sym->is_const) {
                semantic_error(node, "لا يمكن تعديل مصفوفة ثابتة '%s'.", sym->name);
            }

            int expected = (sym->array_rank > 0) ? sym->array_rank : 1;
            (void)analyze_indices_type_and_bounds(sym, node, node->data.array_op.indices, expected);

            DataType vt = infer_type(node->data.array_op.value);
            if (sym->type == TYPE_POINTER) {
                if (!ptr_type_compatible(node->data.array_op.value->inferred_ptr_base_type,
                                         node->data.array_op.value->inferred_ptr_base_type_name,
                                         node->data.array_op.value->inferred_ptr_depth,
                                         sym->ptr_base_type,
                                         sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                         sym->ptr_depth,
                                         true)) {
                    semantic_error(node, "نوع القيمة في تعيين '%s' غير متوافق.", sym->name);
                }
            } else if (!types_compatible(vt, sym->type)) {
                semantic_error(node, "نوع القيمة في تعيين '%s' غير متوافق.", sym->name);
            } else {
                maybe_warn_implicit_narrowing(vt, sym->type, node->data.array_op.value);
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
        current_filename = "المصدر";
    }
    
    analyze_node(program);
    return !has_error;
}
