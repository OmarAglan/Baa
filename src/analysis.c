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

// أنواع مركبة (v0.3.4)
#define ANALYSIS_MAX_ENUMS   128
#define ANALYSIS_MAX_STRUCTS 128
#define ANALYSIS_MAX_ENUM_MEMBERS 128
#define ANALYSIS_MAX_STRUCT_FIELDS 128

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

static EnumDef enum_defs[ANALYSIS_MAX_ENUMS];
static int enum_count = 0;

static StructDef struct_defs[ANALYSIS_MAX_STRUCTS];
static int struct_count = 0;

static UnionDef union_defs[ANALYSIS_MAX_STRUCTS];
static int union_count = 0;

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
        }
        union_defs[i].field_count = 0;
        union_defs[i].size = 0;
        union_defs[i].align = 0;
        union_defs[i].layout_done = false;
        union_defs[i].layout_in_progress = false;
    }
    union_count = 0;
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
                       const char* type_name,
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
        global_symbols[global_count].type_name[0] = '\0';
        if (type_name && type_name[0]) {
            size_t tn_len = strlen(type_name);
            size_t tn_cap = sizeof(global_symbols[global_count].type_name);
            if (tn_len < tn_cap) {
                memcpy(global_symbols[global_count].type_name, type_name, tn_len + 1);
            }
        }
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
        local_symbols[local_count].type_name[0] = '\0';
        if (type_name && type_name[0]) {
            size_t tn_len = strlen(type_name);
            size_t tn_cap = sizeof(local_symbols[local_count].type_name);
            if (tn_len < tn_cap) {
                memcpy(local_symbols[local_count].type_name, type_name, tn_len + 1);
            }
        }
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

    if (enum_lookup_def(name) || struct_lookup_def(name)) {
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

    if (enum_lookup_def(name) || struct_lookup_def(name)) {
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

    if (enum_lookup_def(name) || struct_lookup_def(name) || union_lookup_def(name)) {
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
        out->is_const = f->data.var_decl.is_const;
        out->offset = 0;
    }

    if (ud->field_count <= 0) {
        semantic_error(node, "الاتحاد '%s' يجب أن يحتوي على حقل واحد على الأقل.", name);
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
        case TYPE_BOOL: return "BOOLEAN";
        case TYPE_CHAR: return "CHAR";
        case TYPE_FLOAT: return "FLOAT";
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
static DataType infer_type(Node* node);
static DataType infer_type_internal(Node* node);

static DataType infer_type_internal(Node* node) {
    if (!node) return TYPE_INT; // افتراضي لتجنب الانهيار

    switch (node->type) {
        case NODE_INT:
            return TYPE_INT;

        case NODE_FLOAT:
            return TYPE_FLOAT;

        case NODE_CHAR:
            return TYPE_CHAR;
        
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
            if (sym->type == TYPE_STRUCT || sym->type == TYPE_UNION) {
                semantic_error(node, "لا يمكن استخدام النوع المركب '%s' كقيمة مباشرة (استخدم ':' للوصول لعضو).", sym->name);
                return sym->type;
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
                return node->data.member_access.member_type;
            }
            semantic_error(node, "وصول عضو غير صالح.");
            return TYPE_INT;
        }

        case NODE_ARRAY_ACCESS: {
            Symbol* sym = lookup(node->data.array_op.name, true);
            if (!sym) {
                semantic_error(node, "مصفوفة غير معرّفة '%s'.", node->data.array_op.name ? node->data.array_op.name : "???");
                (void)infer_type(node->data.array_op.index);
                return TYPE_INT;
            }
            if (!sym->is_array) {
                // السماح بفهرسة النص على أنه حرف[] (v0.3.5)
                if (sym->type == TYPE_STRING) {
                    DataType it = infer_type(node->data.array_op.index);
                    if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                        semantic_error(node->data.array_op.index, "فهرس النص يجب أن يكون صحيحاً.");
                    }
                    return TYPE_CHAR;
                }

                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                (void)infer_type(node->data.array_op.index);
                return sym->type;
            }

            DataType it = infer_type(node->data.array_op.index);
            if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
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
                    semantic_error(node, "Arithmetic operations require INTEGER operands.");
                }
                return datatype_usual_arith(left, right);
            }
            
            // عمليات المقارنة تتطلب أنواعاً متوافقة وتعيد منطقي
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                // مقارنة العشري: نسمح بمقارنة عشري مع عدد (يحوّل العدد إلى عشري).
                if (left == TYPE_FLOAT || right == TYPE_FLOAT) {
                    if (!((datatype_is_intlike(left) || left == TYPE_FLOAT) && (datatype_is_intlike(right) || right == TYPE_FLOAT))) {
                        semantic_error(node, "مقارنات العشري تتطلب معاملات عددية.");
                    }
                    return TYPE_BOOL;
                }
                if (left != right) {
                    // سماح C-like: مقارنة حرف مع صحيح.
                    if (!((datatype_is_intlike(left) && datatype_is_intlike(right)) ||
                          (left == TYPE_ENUM && right == TYPE_ENUM))) {
                        semantic_error(node, "Comparison operations require matching types.");
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
                return TYPE_BOOL;
            }
            
            // العمليات المنطقية (AND/OR) تتطلب منطقي أو صحيح وتعيد منطقي
            if (op == OP_AND || op == OP_OR) {
                // نقبل INT أو BOOL لأن C تعامل القيم غير الصفرية كـ true
                if ((!datatype_is_intlike(left) && left != TYPE_BOOL && left != TYPE_FLOAT) ||
                    (!datatype_is_intlike(right) && right != TYPE_BOOL && right != TYPE_FLOAT)) {
                    semantic_error(node, "Logical operations require INTEGER or BOOLEAN operands.");
                }
                return TYPE_BOOL;
            }
            
            return TYPE_INT;
        }

        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP: {
            DataType ot = infer_type(node->data.unary_op.operand);
            if (ot == TYPE_FLOAT) {
                if (node->data.unary_op.op == UOP_NEG) {
                    // السماح بسالب عشري كحالة دنيا (بدون عمليات عشري أخرى).
                    return TYPE_FLOAT;
                }
                semantic_error(node, "عمليات العشري غير مدعومة حالياً.");
                return TYPE_FLOAT;
            }

            if (!datatype_is_intlike(ot)) {
                semantic_error(node, "Unary operations require INTEGER operand.");
            }
            // -x يتبع ترقيات الأعداد الصحيحة (قد يصبح ط٦٤).
            if (node->data.unary_op.op == UOP_NEG) {
                return datatype_int_promote(ot);
            }
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
                } else {
                    // الثوابت يجب أن تُهيّأ
                    if (!node->data.var_decl.is_global || node->data.var_decl.is_const) {
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
                    DataType exprType = infer_type(node->data.var_decl.expression);
                    if (!types_compatible(exprType, declType)) {
                        semantic_error(node,
                                      "Type mismatch in declaration of '%s'. Expected %s but got %s.",
                                      node->data.var_decl.name,
                                      datatype_to_str(declType),
                                      datatype_to_str(exprType));
                    }
                }
                if (node->data.var_decl.is_const && !node->data.var_decl.expression) {
                    semantic_error(node, "Constant '%s' must be initialized.", node->data.var_decl.name);
                }
            }

            // 3) إضافة الرمز (مع معلومات الموقع للتحذيرات)
            add_symbol(node->data.var_decl.name,
                       node->data.var_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       declType,
                       declTypeName,
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
                     add_symbol(param->data.var_decl.name, SCOPE_LOCAL, param->data.var_decl.type, NULL, false,
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
                if (sym->type == TYPE_STRUCT) {
                    semantic_error(node, "لا يمكن إسناد قيمة إلى هيكل '%s' مباشرة.", sym->name);
                }
                // التحقق من الثوابت: لا يمكن إعادة تعيين قيمة ثابت
                if (sym->is_const) {
                    semantic_error(node, "Cannot reassign constant '%s'.", node->data.assign_stmt.name);
                }
                DataType exprType = infer_type(node->data.assign_stmt.expression);
                if (sym->type == TYPE_ENUM) {
                    const char* en = expr_enum_name(node->data.assign_stmt.expression);
                    if (!types_compatible_named(exprType, en, TYPE_ENUM, sym->type_name)) {
                        semantic_error(node, "عدم تطابق نوع التعداد في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (!types_compatible(exprType, sym->type)) {
                    semantic_error(node, "Type mismatch in assignment to '%s'.", node->data.assign_stmt.name);
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
            } else if (!types_compatible(got, expected)) {
                semantic_error(node, "عدم تطابق النوع في إسناد العضو.");
            }
            break;
        }

        case NODE_IF: {
            DataType condType = infer_type(node->data.if_stmt.condition);
            if (!datatype_is_intlike(condType)) {
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
            if (!datatype_is_intlike(condType)) {
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
                if (!datatype_is_intlike(condType)) {
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
            DataType st = infer_type(node->data.switch_stmt.expression);
            if (!datatype_is_intlike(st)) {
                semantic_error(node->data.switch_stmt.expression, "'switch' expression must be an integer.");
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
        {
            DataType pt = infer_type(node->data.print_stmt.expression);
            (void)pt;
            break;
        }
        
        case NODE_READ: {
            // التحقق من أن المتغير معرف وقابل للتعديل
            Symbol* sym = lookup(node->data.read_stmt.var_name, true);
            if (!sym) {
                semantic_error(node, "Reading into undefined variable '%s'.", node->data.read_stmt.var_name);
            } else {
                if (sym->is_const) {
                    semantic_error(node, "Cannot read into constant variable '%s'.", node->data.read_stmt.var_name);
                }
                // نقبل القراءة في الأنواع الصحيحة (بما فيها الأحجام المختلفة).
                if (!datatype_is_int(sym->type)) {
                    semantic_error(node, "'اقرأ' currently only supports integer variables.");
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
                       NULL,
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
                if (sym->type == TYPE_STRING) {
                    semantic_error(node, "لا يمكن تعديل عناصر النص '%s' حالياً.", sym->name);
                    (void)infer_type(node->data.array_op.index);
                    (void)infer_type(node->data.array_op.value);
                    break;
                }
                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                (void)infer_type(node->data.array_op.index);
                (void)infer_type(node->data.array_op.value);
                break;
            }
            if (sym->is_const) {
                semantic_error(node, "لا يمكن تعديل مصفوفة ثابتة '%s'.", sym->name);
            }

            DataType it = infer_type(node->data.array_op.index);
            if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
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
