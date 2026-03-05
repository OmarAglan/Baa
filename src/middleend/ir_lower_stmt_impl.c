// ============================================================================
// Statement lowering (v0.3.0.4)
// ============================================================================

static void lower_array_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // التصريحات العامة تُخفض على مستوى الوحدة داخل ir_lower_program().
    if (stmt->data.array_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global array decl '%s' ignored in statement lowering\n",
                stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
        return;
    }
    if (stmt->data.array_decl.total_elems <= 0 || stmt->data.array_decl.total_elems > INT_MAX) {
        ir_lower_report_error(ctx, stmt, "حجم المصفوفة غير صالح.");
        return;
    }

    const int total_count = (int)stmt->data.array_decl.total_elems;
    DataType elem_dt = stmt->data.array_decl.element_type;
    const char* elem_type_name = stmt->data.array_decl.element_type_name;
    bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* elem_t = NULL;
    IRType* arr_t = NULL;
    int elem_size_bytes = 0;

    if (elem_is_agg) {
        elem_size_bytes = stmt->data.array_decl.element_struct_size;
        if (elem_size_bytes <= 0) {
            ir_lower_report_error(ctx, stmt, "حجم عنصر المصفوفة المركبة غير صالح.");
            return;
        }
        int64_t total_bytes64 = stmt->data.array_decl.total_elems * (int64_t)elem_size_bytes;
        if (total_bytes64 <= 0 || total_bytes64 > INT_MAX) {
            ir_lower_report_error(ctx, stmt, "حجم تخزين المصفوفة المركبة كبير جداً.");
            return;
        }
        elem_t = IR_TYPE_I8_T;
        arr_t = ir_type_array(IR_TYPE_I8_T, (int)total_bytes64);
    } else {
        elem_t = ir_type_from_datatype(m, elem_dt);
        if (!elem_t || elem_t->kind == IR_TYPE_VOID) elem_t = IR_TYPE_I64_T;
        arr_t = ir_type_array(elem_t, total_count);
    }

    if (!arr_t) {
        ir_lower_report_error(ctx, stmt, "فشل إنشاء نوع مصفوفة أثناء خفض IR.");
        return;
    }

    if (stmt->data.array_decl.is_static) {
        if (!m) {
            ir_lower_report_error(ctx, stmt, "وحدة IR غير متاحة لتعريف مصفوفة ساكنة.");
            return;
        }
        ir_module_set_current(m);

        char* gname = ir_lower_make_static_label(ctx, stmt->data.array_decl.name);
        if (!gname) return;

        IRGlobal* g = ir_builder_create_global(ctx->builder, gname, arr_t,
                                               stmt->data.array_decl.is_const ? 1 : 0);
        if (!g) {
            ir_lower_report_error(ctx, stmt, "فشل إنشاء مصفوفة ساكنة '%s'.",
                                  stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
            return;
        }
        g->is_internal = true;
        g->has_init_list = stmt->data.array_decl.has_init ? true : false;

        if (stmt->data.array_decl.has_init && !elem_is_agg) {
            int n = stmt->data.array_decl.init_count;
            if (n < 0) n = 0;
            if (n > total_count) {
                n = total_count;
            }

            IRValue** elems = NULL;
            if (n > 0) {
                elems = (IRValue**)ir_arena_calloc(&m->arena, (size_t)n, sizeof(IRValue*), _Alignof(IRValue*));
                if (!elems) {
                    ir_lower_report_error(ctx, stmt, "فشل تخصيص مهيئات مصفوفة ساكنة '%s'.",
                                          stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
                    return;
                }
            }

            int i = 0;
            for (Node* v = stmt->data.array_decl.init_values; v && i < n; v = v->next, i++) {
                elems[i] = ir_lower_global_init_value(ctx->builder, v, elem_t);
            }

            g->init_elems = elems;
            g->init_elem_count = n;
        } else if (stmt->data.array_decl.has_init && elem_is_agg && stmt->data.array_decl.init_count > 0) {
            ir_lower_report_error(ctx, stmt, "تهيئة مصفوفات الأنواع المركبة غير مدعومة حالياً في خفض IR.");
        }

        ir_lower_bind_local_static(ctx, stmt->data.array_decl.name, gname, arr_t, TYPE_INT, NULL, 0);
        ir_lower_set_local_array_meta(ctx,
                                      stmt->data.array_decl.name,
                                      stmt->data.array_decl.dim_count,
                                      stmt->data.array_decl.dims,
                                      elem_dt,
                                      elem_type_name);
        return;
    }

    int ptr_reg = ir_builder_emit_alloca(ctx->builder, arr_t);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.array_decl.name);

    ir_lower_bind_local(ctx, stmt->data.array_decl.name, ptr_reg, arr_t, TYPE_INT, NULL, 0);
    ir_lower_set_local_array_meta(ctx,
                                  stmt->data.array_decl.name,
                                  stmt->data.array_decl.dim_count,
                                  stmt->data.array_decl.dims,
                                  elem_dt,
                                  elem_type_name);

    // تهيئة المصفوفة كما في C:
    // - إذا وُجدت قائمة تهيئة (حتى `{}`)، نُهيّئ العناصر المحددة ثم نملأ الباقي بأصفار.
    if (!stmt->data.array_decl.has_init) {
        return;
    }
    if (elem_is_agg) {
        if (stmt->data.array_decl.init_count > 0) {
            ir_lower_report_error(ctx, stmt, "تهيئة مصفوفات الأنواع المركبة غير مدعومة حالياً في خفض IR.");
        }
        return;
    }

    IRType* arr_ptr_t = ir_type_ptr(arr_t);
    IRType* elem_ptr_t = ir_type_ptr(elem_t);
    IRValue* arr_ptr = ir_value_reg(ptr_reg, arr_ptr_t);

    // base: cast ptr(array) -> ptr(elem)
    int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, elem_ptr_t);

    int i = 0;
    Node* v = stmt->data.array_decl.init_values;
    while (v && i < total_count) {
        // اربط موقع التوليد بموقع عنصر التهيئة.
        ir_lower_set_loc(ctx->builder, v);

        IRValue* idx = ir_value_const_int((int64_t)i, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

        IRValue* val = lower_expr(ctx, v);
        if (val && val->type && !ir_types_equal(val->type, elem_t)) {
            int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
            val = ir_value_reg(cv, elem_t);
        }

        ir_builder_emit_store(ctx->builder, val, elem_ptr);
        i++;
        v = v->next;
    }

    // تعبئة الباقي بأصفار
    for (; i < total_count; i++) {
        ir_lower_set_loc(ctx->builder, stmt);
        IRValue* idx = ir_value_const_int((int64_t)i, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);
        IRValue* z = ir_value_const_int(0, elem_t);
        ir_builder_emit_store(ctx->builder, z, elem_ptr);
    }
}

static void lower_var_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // Ignore global declarations here (globals are lowered at module scope later).
    if (stmt->data.var_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global var decl '%s' ignored in statement lowering\n",
                stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
        return;
    }

    if (stmt->data.var_decl.is_static) {
        IRModule* m = ctx->builder->module;
        if (!m) {
            ir_lower_report_error(ctx, stmt, "وحدة IR غير متاحة لتعريف متغير ساكن.");
            return;
        }
        ir_module_set_current(m);

        char* gname = ir_lower_make_static_label(ctx, stmt->data.var_decl.name);
        if (!gname) return;

        if (stmt->data.var_decl.type == TYPE_STRUCT || stmt->data.var_decl.type == TYPE_UNION) {
            int n = stmt->data.var_decl.struct_size;
            if (n <= 0) {
                ir_lower_report_error(ctx, stmt, "حجم الهيكل غير صالح أثناء خفض IR.");
                return;
            }

            IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
            IRGlobal* g = ir_builder_create_global(ctx->builder, gname, bytes_t,
                                                   stmt->data.var_decl.is_const ? 1 : 0);
            if (!g) {
                ir_lower_report_error(ctx, stmt, "فشل إنشاء متغير ساكن '%s'.",
                                      stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
                return;
            }
            g->is_internal = true;
        ir_lower_bind_local_static(ctx, stmt->data.var_decl.name, gname, bytes_t, TYPE_INT, NULL, 0);
        return;
    }

        IRType* value_type = ir_type_from_datatype_ex(m, stmt->data.var_decl.type, stmt->data.var_decl.func_sig);
        IRValue* init = NULL;
        if (stmt->data.var_decl.expression) {
            init = ir_lower_global_init_value(ctx->builder, stmt->data.var_decl.expression, value_type);
        } else {
            init = ir_value_const_int(0, value_type);
        }

        IRGlobal* g = ir_builder_create_global_init(ctx->builder, gname, value_type, init,
                                                    stmt->data.var_decl.is_const ? 1 : 0);
        if (!g) {
            ir_lower_report_error(ctx, stmt, "فشل إنشاء متغير ساكن '%s'.",
                                  stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
            return;
        }
        g->is_internal = true;
        ir_lower_bind_local_static(ctx, stmt->data.var_decl.name, gname, value_type,
                                   stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type : TYPE_INT,
                                   stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type_name : NULL,
                                   stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_depth : 0);
        return;
    }

    // نوع مركب محلي (هيكل/اتحاد): نخزّنه ككتلة بايتات (array<i8, N>) على المكدس.
    if (stmt->data.var_decl.type == TYPE_STRUCT || stmt->data.var_decl.type == TYPE_UNION) {
        int n = stmt->data.var_decl.struct_size;
        if (n <= 0) {
            ir_lower_report_error(ctx, stmt, "حجم الهيكل غير صالح أثناء خفض IR.");
            return;
        }

        IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
        int ptr_reg = ir_builder_emit_alloca(ctx->builder, bytes_t);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);
        ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, bytes_t, TYPE_INT, NULL, 0);
        return;
    }

    IRType* value_type = ir_type_from_datatype_ex(ctx && ctx->builder ? ctx->builder->module : NULL,
                                                  stmt->data.var_decl.type,
                                                  stmt->data.var_decl.func_sig);

    // %ptr = حجز <value_type>
    int ptr_reg = ir_builder_emit_alloca(ctx->builder, value_type);

    // ربط اسم المتغير بتعليمة الحجز للتتبع.
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);

    // Bind name -> ptr reg so NODE_VAR_REF can emit حمل.
    ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, value_type,
                        stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type : TYPE_INT,
                        stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_base_type_name : NULL,
                        stmt->data.var_decl.type == TYPE_POINTER ? stmt->data.var_decl.ptr_depth : 0);

    // Initializer is required for locals in the current language rules.
    IRValue* init = NULL;
    if (stmt->data.var_decl.expression) {
        init = lower_expr(ctx, stmt->data.var_decl.expression);
    } else {
        // Fallback (should not happen for locals; parser enforces initializer).
        init = ir_value_const_int(0, value_type);
    }

    if (init && init->type && value_type && !ir_types_equal(init->type, value_type)) {
        init = cast_to(ctx, init, value_type);
    }

    // خزن <init>, %ptr
    IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
    IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);

    // نعيد الموقع إلى تصريح المتغير كي تُنسب عملية الخزن لسطر التصريح.
    ir_lower_set_loc(ctx->builder, stmt);
    ir_builder_emit_store(ctx->builder, init, ptr);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, stmt->data.var_decl.name);
}

static void lower_array_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.array_op.name;
    int index_count = stmt->data.array_op.index_count;
    if (index_count <= 0) index_count = ir_lower_index_count(stmt->data.array_op.indices);

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        const char* storage_name = ir_lower_binding_storage_name(b);
        if (!b->is_array || !b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, stmt, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
            for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }
        if (b->array_elem_type == TYPE_STRUCT || b->array_elem_type == TYPE_UNION) {
            ir_lower_report_error(ctx, stmt, "تعيين عنصر مصفوفة مركبة غير مدعوم حالياً.");
            for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }

        IRType* elem_t = b->value_type->data.array.element ? b->value_type->data.array.element : IR_TYPE_I64_T;
        IRType* elem_ptr_t = ir_type_ptr(elem_t);

        IRValue* base_ptr = NULL;
        if (b->is_static_storage) {
            base_ptr = ir_value_global(storage_name, elem_t);
        } else {
            IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
            int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
            base_ptr = ir_value_reg(base_reg, elem_ptr_t);
        }

        if (!base_ptr) {
            ir_lower_report_error(ctx, stmt, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
            for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }

        IRValue* linear = ir_lower_build_linear_index(ctx, stmt,
                                                      stmt->data.array_op.indices,
                                                      index_count,
                                                      b->array_dims,
                                                      b->array_rank);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

        IRValue* val = lower_expr(ctx, stmt->data.array_op.value);
        if (val && val->type && !ir_types_equal(val->type, elem_t)) {
            int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
            val = ir_value_reg(cv, elem_t);
        }

        ir_builder_emit_store(ctx->builder, val, elem_ptr);
        return;
    }

    // مصفوفة عامة
    IRGlobal* g = NULL;
    if (ctx->builder && ctx->builder->module && name) {
        g = ir_module_find_global(ctx->builder->module, name);
    }
    if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
        ir_lower_report_error(ctx, stmt, "تعيين إلى مصفوفة غير معرّفة '%s'.", name ? name : "???");
        for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
            (void)lower_expr(ctx, idx);
        }
        (void)lower_expr(ctx, stmt->data.array_op.value);
        return;
    }

    Node* gdecl = ir_lower_find_global_array_decl(ctx, name);
    int rank = 1;
    const int* dims = NULL;
    DataType elem_dt = TYPE_INT;
    if (gdecl && gdecl->type == NODE_ARRAY_DECL) {
        rank = gdecl->data.array_decl.dim_count > 0 ? gdecl->data.array_decl.dim_count : 1;
        dims = gdecl->data.array_decl.dims;
        elem_dt = gdecl->data.array_decl.element_type;
    }
    if (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION) {
        ir_lower_report_error(ctx, stmt, "تعيين عنصر مصفوفة مركبة غير مدعوم حالياً.");
        for (Node* idx = stmt->data.array_op.indices; idx; idx = idx->next) {
            (void)lower_expr(ctx, idx);
        }
        (void)lower_expr(ctx, stmt->data.array_op.value);
        return;
    }

    IRType* elem_t = g->type->data.array.element ? g->type->data.array.element : IR_TYPE_I64_T;
    IRType* elem_ptr_t = ir_type_ptr(elem_t);
    IRValue* base_ptr = ir_value_global(name, elem_t);

    IRValue* linear = ir_lower_build_linear_index(ctx, stmt,
                                                  stmt->data.array_op.indices,
                                                  index_count,
                                                  dims,
                                                  rank);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
    IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

    IRValue* val = lower_expr(ctx, stmt->data.array_op.value);
    if (val && val->type && !ir_types_equal(val->type, elem_t)) {
        int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
        val = ir_value_reg(cv, elem_t);
    }

    ir_builder_emit_store(ctx->builder, val, elem_ptr);
}

static void lower_member_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    Node* target = stmt->data.member_assign.target;
    Node* value = stmt->data.member_assign.value;
    if (!target || target->type != NODE_MEMBER_ACCESS) {
        ir_lower_report_error(ctx, stmt, "هدف إسناد العضو غير صالح.");
        (void)lower_expr(ctx, value);
        return;
    }

    if (!target->data.member_access.is_struct_member || !target->data.member_access.root_var) {
        ir_lower_report_error(ctx, stmt, "إسناد عضو يتطلب عضواً هيكلياً محلولاً دلالياً.");
        (void)lower_expr(ctx, value);
        return;
    }

    if (target->data.member_access.member_type == TYPE_STRUCT ||
        target->data.member_access.member_type == TYPE_UNION) {
        ir_lower_report_error(ctx, stmt, "إسناد عضو من نوع هيكل غير مدعوم حالياً.");
        (void)lower_expr(ctx, value);
        return;
    }

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* base_ptr = NULL;

    if (target->data.member_access.root_is_global) {
        base_ptr = ir_value_global(target->data.member_access.root_var, IR_TYPE_I8_T);
    } else {
        IRLowerBinding* b = find_local(ctx, target->data.member_access.root_var);
        if (!b || !b->value_type) {
            ir_lower_report_error(ctx, stmt, "هيكل محلي غير معروف '%s'.", target->data.member_access.root_var);
            (void)lower_expr(ctx, value);
            return;
        }
        if (b->is_static_storage) {
            base_ptr = ir_value_global(ir_lower_binding_storage_name(b), IR_TYPE_I8_T);
        } else {
            IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
            int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
            base_ptr = ir_value_reg(c, ptr_i8_t);
        }
    }

    IRValue* idx = ir_value_const_int((int64_t)target->data.member_access.member_offset, IR_TYPE_I64_T);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
    IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

    IRType* field_val_t = ir_type_from_datatype(m, target->data.member_access.member_type);
    if (!field_val_t || field_val_t->kind == IR_TYPE_VOID) field_val_t = IR_TYPE_I64_T;
    IRType* field_ptr_t = ir_type_ptr(field_val_t);
    int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, field_ptr_t);
    IRValue* field_ptr = ir_value_reg(fp, field_ptr_t);

    IRValue* rhs = lower_expr(ctx, value);
    if (rhs && rhs->type && !ir_types_equal(rhs->type, field_val_t)) {
        int cv = ir_builder_emit_cast(ctx->builder, rhs, field_val_t);
        rhs = ir_value_reg(cv, field_val_t);
    }

    ir_builder_emit_store(ctx->builder, rhs, field_ptr);
}

static void lower_deref_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;
    Node* target = stmt->data.deref_assign.target;
    Node* value = stmt->data.deref_assign.value;
    if (!target || target->type != NODE_UNARY_OP || target->data.unary_op.op != UOP_DEREF) {
        ir_lower_report_error(ctx, stmt, "هدف الإسناد عبر المؤشر غير صالح.");
        if (value) (void)lower_expr(ctx, value);
        return;
    }

    IRValue* p0 = lower_expr(ctx, target->data.unary_op.operand);
    IRType* value_t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, target->inferred_type);
    if (!value_t || value_t->kind == IR_TYPE_VOID) value_t = IR_TYPE_I64_T;
    IRType* ptr_t = ir_type_ptr(value_t);
    IRValue* ptr = cast_to(ctx, p0, ptr_t);

    IRValue* rhs = lower_expr(ctx, value);
    if (rhs && rhs->type && !ir_types_equal(rhs->type, value_t)) {
        rhs = cast_to(ctx, rhs, value_t);
    }

    ir_builder_emit_store(ctx->builder, rhs, ptr);
}

static void lower_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.assign_stmt.name;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        const char* storage_name = ir_lower_binding_storage_name(b);
        IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && value_type && !ir_types_equal(rhs->type, value_type)) {
            rhs = cast_to(ctx, rhs, value_type);
        }

        // خزن <rhs>, %ptr
        (void)value_type; // reserved for future casts/type checks
        IRValue* ptr = NULL;
        if (b->is_static_storage) {
            ptr = ir_value_global(storage_name, value_type);
        } else {
            IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
            ptr = ir_value_reg(b->ptr_reg, ptr_type);
        }
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, ptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, storage_name);
        return;
    }

    // Assignment to a global (module scope)
    IRGlobal* g = NULL;
    if (ctx->builder && ctx->builder->module && name) {
        g = ir_module_find_global(ctx->builder->module, name);
    }
    if (g) {
        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && g->type && !ir_types_equal(rhs->type, g->type)) {
            rhs = cast_to(ctx, rhs, g->type);
        }

        IRValue* gptr = ir_value_global(name, g->type);
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, gptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, name);
        return;
    }

    ir_lower_report_error(ctx, stmt, "تعيين إلى متغير غير معروف '%s'.", name ? name : "???");
}

static void lower_return(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = NULL;
    if (stmt->data.return_stmt.expression) {
        value = lower_expr(ctx, stmt->data.return_stmt.expression);
    }

    IRFunc* f = ir_builder_get_func(ctx->builder);
    if (f && f->ret_type && value && value->type && !ir_types_equal(f->ret_type, value->type)) {
        value = cast_to(ctx, value, f->ret_type);
    }
    ir_builder_emit_ret(ctx->builder, value);
}

static void lower_printf_cstr(IRLowerCtx* ctx, Node* stmt, const char* fmt, IRValue* arg)
{
    if (!ctx || !ctx->builder || !fmt) return;
    if (stmt) ir_lower_set_loc(ctx->builder, stmt);

    IRValue* fmt_val = ir_builder_const_string(ctx->builder, fmt);

    if (!arg) {
        IRValue* args1[1] = { fmt_val };
        ir_builder_emit_call_void(ctx->builder, "اطبع", args1, 1);
        return;
    }

    IRValue* args2[2] = { fmt_val, arg };
    ir_builder_emit_call_void(ctx->builder, "اطبع", args2, 2);
}

static void lower_print_char_packed(IRLowerCtx* ctx, Node* stmt, IRValue* ch, const char* fmt)
{
    if (!ctx || !ctx->builder || !ch || !fmt) return;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, 5);
    int buf_ptr_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, ir_type_ptr(buf_arr_t));

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    int base_reg = ir_builder_emit_cast(b, buf_ptr, i8_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, i8_ptr_t);

    // i64 معبأ = bytes + len*2^32
    int pr = ir_builder_emit_cast(b, ch, IR_TYPE_I64_T);
    IRValue* packed = ir_value_reg(pr, IR_TYPE_I64_T);

    IRValue* c_2p32 = ir_value_const_int(4294967296LL, IR_TYPE_I64_T);
    int len_r = ir_builder_emit_div(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    int bytes_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* c256 = ir_value_const_int(256, IR_TYPE_I64_T);

    int b0_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* b0 = ir_value_reg(b0_r, IR_TYPE_I64_T);
    int t1_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* t1 = ir_value_reg(t1_r, IR_TYPE_I64_T);
    int b1_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, t1, c256);
    IRValue* b1 = ir_value_reg(b1_r, IR_TYPE_I64_T);
    int t2_r = ir_builder_emit_div(b, IR_TYPE_I64_T, t1, c256);
    IRValue* t2 = ir_value_reg(t2_r, IR_TYPE_I64_T);
    int b2_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, t2, c256);
    IRValue* b2 = ir_value_reg(b2_r, IR_TYPE_I64_T);
    int t3_r = ir_builder_emit_div(b, IR_TYPE_I64_T, t2, c256);
    IRValue* t3 = ir_value_reg(t3_r, IR_TYPE_I64_T);
    int b3_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, t3, c256);
    IRValue* b3 = ir_value_reg(b3_r, IR_TYPE_I64_T);

    IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
    IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);
    IRValue* idx4 = ir_value_const_int(4, IR_TYPE_I64_T);

    int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
    int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
    int p2r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2);
    int p3r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3);
    int p4r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx4);

    ir_builder_emit_store(b, cast_to(ctx, b0, IR_TYPE_I8_T), ir_value_reg(p0r, i8_ptr_t));
    ir_builder_emit_store(b, cast_to(ctx, b1, IR_TYPE_I8_T), ir_value_reg(p1r, i8_ptr_t));
    ir_builder_emit_store(b, cast_to(ctx, b2, IR_TYPE_I8_T), ir_value_reg(p2r, i8_ptr_t));
    ir_builder_emit_store(b, cast_to(ctx, b3, IR_TYPE_I8_T), ir_value_reg(p3r, i8_ptr_t));
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), ir_value_reg(p4r, i8_ptr_t));

    // كتابة NUL في buf[len]
    int pend = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, len);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), ir_value_reg(pend, i8_ptr_t));

    lower_printf_cstr(ctx, stmt, fmt, base_ptr);
}

static void lower_print_baa_string(IRLowerCtx* ctx, Node* stmt, IRValue* str_ptr)
{
    if (!ctx || !ctx->builder || !str_ptr) return;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = ir_type_ptr(IR_TYPE_CHAR_T);
    IRValue* base = str_ptr;
    if (!base->type || base->type->kind != IR_TYPE_PTR) {
        int cr = ir_builder_emit_cast(b, base, char_ptr_t);
        base = ir_value_reg(cr, char_ptr_t);
    }

    // idx_ptr = alloca i64
    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    IRBlock* head = cf_create_block(ctx, "اطبع_نص_حلقة");
    IRBlock* body = cf_create_block(ctx, "اطبع_نص_جسم");
    IRBlock* done = cf_create_block(ctx, "اطبع_نص_نهاية");

    if (!ir_builder_is_block_terminated(b))
        ir_builder_emit_br(b, head);

    // head
    ir_builder_set_insert_point(b, head);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);

    int ep = ir_builder_emit_ptr_offset(b, char_ptr_t, base, idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    int ch_r = ir_builder_emit_load(b, IR_TYPE_CHAR_T, elem_ptr);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_CHAR_T);

    int chi_r = ir_builder_emit_cast(b, ch, IR_TYPE_I64_T);
    IRValue* chi = ir_value_reg(chi_r, IR_TYPE_I64_T);
    int is_end_r = ir_builder_emit_cmp_eq(b, chi, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(is_end_r, IR_TYPE_I1_T);

    if (!ir_builder_is_block_terminated(b))
        ir_builder_emit_br_cond(b, is_end, done, body);

    // body
    ir_builder_set_insert_point(b, body);
    lower_print_char_packed(ctx, stmt, ch, "%s");

    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    if (!ir_builder_is_block_terminated(b))
        ir_builder_emit_br(b, head);

    // done
    ir_builder_set_insert_point(b, done);
    lower_printf_cstr(ctx, stmt, "\n", NULL);
}

static void lower_print(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = lower_expr(ctx, stmt->data.print_stmt.expression);

    if (value && value->type && value->type->kind == IR_TYPE_CHAR) {
        lower_print_char_packed(ctx, stmt, value, "%s\n");
        return;
    }

    if (value && value->type && value->type->kind == IR_TYPE_PTR &&
        value->type->data.pointee && value->type->data.pointee->kind == IR_TYPE_CHAR) {
        lower_print_baa_string(ctx, stmt, value);
        return;
    }

    // عشري
    if (stmt->data.print_stmt.expression && stmt->data.print_stmt.expression->inferred_type == TYPE_FLOAT)
    {
        IRValue* vf = value ? cast_to(ctx, value, IR_TYPE_F64_T) : ir_value_const_int(0, IR_TYPE_F64_T);
        lower_printf_cstr(ctx, stmt, "%g\n", vf);
        return;
    }

    // افتراضي: طباعة عدد صحيح.
    // الأنواع غير الموقّعة تُطبع كـ unsigned عبر تحويلها إلى ط٦٤.
    DataType dt = (stmt->data.print_stmt.expression) ? stmt->data.print_stmt.expression->inferred_type : TYPE_INT;
    if (dt == TYPE_U8 || dt == TYPE_U16 || dt == TYPE_U32 || dt == TYPE_U64)
    {
        IRValue* vu = value ? cast_to(ctx, value, IR_TYPE_U64_T) : ir_value_const_int(0, IR_TYPE_U64_T);
        lower_printf_cstr(ctx, stmt, "%llu\n", vu);
        return;
    }

    IRValue* v64 = value ? ensure_i64(ctx, value) : ir_value_const_int(0, IR_TYPE_I64_T);
    lower_printf_cstr(ctx, stmt, "%lld\n", v64);
}

static void lower_read(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRLowerBinding* b = find_local(ctx, stmt->data.read_stmt.var_name);
    if (!b) {
        ir_lower_report_error(ctx, stmt, "قراءة إلى متغير غير معروف '%s'.",
                              stmt->data.read_stmt.var_name ? stmt->data.read_stmt.var_name : "???");
        return;
    }

    IRType* vt = (b->value_type) ? b->value_type : IR_TYPE_I64_T;
    IRType* ptr_type = ir_type_ptr(vt);
    IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);

    // i64/u64 يمكن القراءة مباشرة إلى المتغير. للأحجام الأصغر نقرأ إلى مؤقت i64 ثم نقتطع.
    if (vt && (vt->kind == IR_TYPE_I64 || vt->kind == IR_TYPE_U64))
    {
        const char* fmt = (vt->kind == IR_TYPE_U64) ? "%llu" : "%lld";
        IRValue* fmt_val = ir_builder_const_string(ctx->builder, fmt);
        IRValue* args[2] = { fmt_val, ptr };
        ir_builder_emit_call_void(ctx->builder, "اقرأ", args, 2);
        return;
    }

    // tmp_ptr = alloca i64
    int tmp_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
    IRType* tmp_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* tmp_ptr = ir_value_reg(tmp_ptr_reg, tmp_ptr_t);
    ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), tmp_ptr);

    IRValue* fmt_val = ir_builder_const_string(ctx->builder, "%lld");
    IRValue* args[2] = { fmt_val, tmp_ptr };
    ir_builder_emit_call_void(ctx->builder, "اقرأ", args, 2);

    int tmp_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, tmp_ptr);
    IRValue* tmp_v = ir_value_reg(tmp_r, IR_TYPE_I64_T);
    IRValue* cv = cast_to(ctx, tmp_v, vt);
    ir_builder_emit_store(ctx->builder, cv, ptr);
}

static bool ir_lower_inline_asm_push_arg(IRLowerCtx* ctx, const Node* site,
                                         IRValue*** args, int* count, int* cap,
                                         IRValue* value)
{
    if (!args || !count || !cap) return false;
    if (*count >= *cap) {
        int new_cap = (*cap <= 0) ? 8 : (*cap * 2);
        IRValue** grown = (IRValue**)realloc(*args, (size_t)new_cap * sizeof(IRValue*));
        if (!grown) {
            ir_lower_report_error(ctx, site, "نفدت الذاكرة أثناء خفض معاملات 'مجمع'.");
            return false;
        }
        *args = grown;
        *cap = new_cap;
    }
    (*args)[(*count)++] = value;
    return true;
}

static int ir_lower_inline_asm_count_nodes(Node* list)
{
    int n = 0;
    for (Node* it = list; it; it = it->next) n++;
    return n;
}

static void lower_inline_asm_stmt(IRLowerCtx* ctx, Node* stmt)
{
    if (!ctx || !ctx->builder || !stmt) return;

    int template_count = ir_lower_inline_asm_count_nodes(stmt->data.inline_asm.templates);
    int output_count = ir_lower_inline_asm_count_nodes(stmt->data.inline_asm.outputs);
    int input_count = ir_lower_inline_asm_count_nodes(stmt->data.inline_asm.inputs);

    IRValue** args = NULL;
    int arg_count = 0;
    int arg_cap = 0;

    if (!ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap,
                                      ir_value_const_int((int64_t)template_count, IR_TYPE_I64_T)) ||
        !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap,
                                      ir_value_const_int((int64_t)output_count, IR_TYPE_I64_T)) ||
        !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap,
                                      ir_value_const_int((int64_t)input_count, IR_TYPE_I64_T))) {
        free(args);
        return;
    }

    for (Node* t = stmt->data.inline_asm.templates; t; t = t->next) {
        const char* s = NULL;
        if (t->type == NODE_STRING && t->data.string_lit.value) {
            s = t->data.string_lit.value;
        } else {
            ir_lower_report_error(ctx, t ? t : stmt, "سطر 'مجمع' يجب أن يكون نصاً ثابتاً.");
            s = "";
        }
        IRValue* sv = ir_builder_const_string(ctx->builder, s);
        if (!sv || !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, sv)) {
            free(args);
            return;
        }
    }

    for (Node* op = stmt->data.inline_asm.outputs; op; op = op->next) {
        const char* cstr = (op->type == NODE_ASM_OPERAND && op->data.asm_operand.constraint)
                               ? op->data.asm_operand.constraint
                               : "";
        IRValue* cv = ir_builder_const_string(ctx->builder, cstr);
        if (!cv || !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, cv)) {
            free(args);
            return;
        }

        IRType* pointee = IR_TYPE_I64_T;
        IRValue* addr = NULL;
        if (op->type == NODE_ASM_OPERAND && op->data.asm_operand.expression) {
            addr = lower_lvalue_address(ctx, op->data.asm_operand.expression, &pointee);
        } else {
            ir_lower_report_error(ctx, op ? op : stmt, "خرج 'مجمع' غير صالح.");
            addr = ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
            pointee = IR_TYPE_I64_T;
        }
        if (!addr || !addr->type || addr->type->kind != IR_TYPE_PTR) {
            addr = cast_to(ctx, addr, ir_type_ptr(pointee ? pointee : IR_TYPE_I64_T));
        }
        if (!ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, addr)) {
            free(args);
            return;
        }
    }

    for (Node* op = stmt->data.inline_asm.inputs; op; op = op->next) {
        const char* cstr = (op->type == NODE_ASM_OPERAND && op->data.asm_operand.constraint)
                               ? op->data.asm_operand.constraint
                               : "";
        IRValue* cv = ir_builder_const_string(ctx->builder, cstr);
        if (!cv || !ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, cv)) {
            free(args);
            return;
        }

        IRValue* iv = NULL;
        if (op->type == NODE_ASM_OPERAND && op->data.asm_operand.expression) {
            iv = lower_expr(ctx, op->data.asm_operand.expression);
        } else {
            ir_lower_report_error(ctx, op ? op : stmt, "إدخال 'مجمع' غير صالح.");
            iv = ir_builder_const_i64(0);
        }
        if (!ir_lower_inline_asm_push_arg(ctx, stmt, &args, &arg_count, &arg_cap, iv)) {
            free(args);
            return;
        }
    }

    ir_lower_set_loc(ctx->builder, stmt);
    ir_builder_emit_call_void(ctx->builder, BAA_INLINE_ASM_PSEUDO_CALL, args, arg_count);
    free(args);
}

