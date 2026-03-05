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

