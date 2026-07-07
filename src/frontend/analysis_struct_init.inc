/**
 * @brief تحليل عناصر تهيئة حقول الهيكل وربطها بإزاحات التخطيط المحسوبة.
 */
static void analyze_struct_field_initializers(Node* decl, StructDef* sd, bool has_static_storage)
{
    if (!decl || decl->type != NODE_VAR_DECL || !decl->data.var_decl.struct_init_values) return;

    if (!sd) {
        for (Node* item = decl->data.var_decl.struct_init_values; item; item = item->next) {
            if (item->type == NODE_STRUCT_FIELD_INIT && item->data.struct_field_init.value) {
                (void)infer_type(item->data.struct_field_init.value);
            }
        }
        return;
    }

    if (has_static_storage) {
        semantic_error(decl, "تهيئة حقول الهيكل ذات التخزين الساكن غير مدعومة حالياً.");
    }

    bool seen[ANALYSIS_MAX_STRUCT_FIELDS] = {0};
    for (Node* item = decl->data.var_decl.struct_init_values; item; item = item->next) {
        if (!item || item->type != NODE_STRUCT_FIELD_INIT) continue;

        const char* field_name = item->data.struct_field_init.field_name;
        StructFieldDef* f = struct_field_lookup(sd, field_name);
        if (!f) {
            semantic_error(item, "حقل غير معرّف في تهيئة الهيكل '%s:%s'.",
                           sd->name ? sd->name : "???",
                           field_name ? field_name : "???");
            if (item->data.struct_field_init.value) {
                (void)infer_type(item->data.struct_field_init.value);
            }
            continue;
        }

        int field_index = (int)(f - sd->fields);
        if (field_index >= 0 && field_index < ANALYSIS_MAX_STRUCT_FIELDS) {
            if (seen[field_index]) {
                semantic_error(item, "تكرار تهيئة الحقل '%s:%s'.",
                               sd->name ? sd->name : "???",
                               field_name ? field_name : "???");
            }
            seen[field_index] = true;
        }

        item->data.struct_field_init.field_offset = f->offset;
        item->data.struct_field_init.field_type = f->type;
        item->data.struct_field_init.field_type_name = f->type_name ? strdup(f->type_name) : NULL;
        item->data.struct_field_init.field_ptr_base_type = f->ptr_base_type;
        item->data.struct_field_init.field_ptr_base_type_name =
            f->ptr_base_type_name ? strdup(f->ptr_base_type_name) : NULL;
        item->data.struct_field_init.field_ptr_depth = f->ptr_depth;

        if (f->type == TYPE_STRUCT || f->type == TYPE_UNION) {
            semantic_error(item, "تهيئة حقل مركب داخل الهيكل غير مدعومة حالياً.");
            if (item->data.struct_field_init.value) {
                (void)infer_type(item->data.struct_field_init.value);
            }
            continue;
        }

        if (f->type == TYPE_FUNC_PTR) {
            semantic_error(item, "تهيئة حقل مؤشر دالة داخل الهيكل غير مدعومة حالياً.");
            if (item->data.struct_field_init.value) {
                (void)infer_type(item->data.struct_field_init.value);
            }
            continue;
        }

        DataType expr_type = infer_type_allow_null_string(item->data.struct_field_init.value, f->type);
        if (f->type == TYPE_ENUM) {
            const char* enum_name = expr_enum_name(item->data.struct_field_init.value);
            if (!types_compatible_named(expr_type, enum_name, TYPE_ENUM, f->type_name)) {
                semantic_error(item, "عدم تطابق نوع التعداد في تهيئة الحقل '%s'.",
                               field_name ? field_name : "???");
            }
        } else if (f->type == TYPE_POINTER) {
            Node* value = item->data.struct_field_init.value;
            if (!value ||
                !ptr_type_compatible(value->inferred_ptr_base_type,
                                     value->inferred_ptr_base_type_name,
                                     value->inferred_ptr_depth,
                                     f->ptr_base_type,
                                     f->ptr_base_type_name,
                                     f->ptr_depth,
                                     true)) {
                semantic_error(item, "عدم تطابق نوع المؤشر في تهيئة الحقل '%s'.",
                               field_name ? field_name : "???");
            }
        } else if (!types_compatible(expr_type, f->type)) {
            semantic_error(item,
                           "عدم تطابق النوع في تهيئة الحقل '%s' (المتوقع %s لكن وُجد %s).",
                           field_name ? field_name : "???",
                           datatype_to_str(f->type),
                           datatype_to_str(expr_type));
        } else {
            maybe_warn_implicit_narrowing(expr_type, f->type, item->data.struct_field_init.value);
        }
    }
}
