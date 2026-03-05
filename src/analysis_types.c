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

