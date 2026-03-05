void ir_lower_ctx_init(IRLowerCtx* ctx, IRBuilder* builder) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->builder = builder;
    ctx->had_error = 0;

    // v0.3.0.5 control-flow lowering state
    ctx->label_counter = 0;
    ctx->cf_depth = 0;
    ctx->scope_depth = 0;
    ctx->current_func_name = NULL;
    ctx->static_local_counter = 0;
    ctx->current_func_is_variadic = false;
    ctx->current_func_fixed_params = 0;
    ctx->current_func_va_base_reg = -1;
    ctx->enable_bounds_checks = false;
    ctx->program_root = NULL;
    ctx->target = NULL;
}

void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type,
                         DataType ptr_base_type, const char* ptr_base_type_name, int ptr_depth) {
    if (!ctx || !name) return;
    bool is_ptr = (ptr_depth > 0);

    // ابحث فقط داخل النطاق الحالي: إن وجد، حدّث الربط.
    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = ptr_reg;
            ctx->locals[i].value_type = value_type;
            ctx->locals[i].global_name = NULL;
            ctx->locals[i].is_static_storage = false;
            ctx->locals[i].is_pointer = is_ptr;
            ctx->locals[i].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
            ctx->locals[i].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
            ctx->locals[i].ptr_depth = is_ptr ? ptr_depth : 0;
            ctx->locals[i].is_array = false;
            ctx->locals[i].array_rank = 0;
            ctx->locals[i].array_dims = NULL;
            ctx->locals[i].array_elem_type = TYPE_INT;
            ctx->locals[i].array_elem_type_name = NULL;
            return;
        }
    }

    if (ctx->local_count >= (int)(sizeof(ctx->locals) / sizeof(ctx->locals[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد جدول المتغيرات المحلية أثناء ربط '%s'.", name);
        return;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].ptr_reg = ptr_reg;
    ctx->locals[ctx->local_count].value_type = value_type;
    ctx->locals[ctx->local_count].global_name = NULL;
    ctx->locals[ctx->local_count].is_static_storage = false;
    ctx->locals[ctx->local_count].is_pointer = is_ptr;
    ctx->locals[ctx->local_count].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
    ctx->locals[ctx->local_count].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
    ctx->locals[ctx->local_count].ptr_depth = is_ptr ? ptr_depth : 0;
    ctx->locals[ctx->local_count].is_array = false;
    ctx->locals[ctx->local_count].array_rank = 0;
    ctx->locals[ctx->local_count].array_dims = NULL;
    ctx->locals[ctx->local_count].array_elem_type = TYPE_INT;
    ctx->locals[ctx->local_count].array_elem_type_name = NULL;
    ctx->local_count++;
}

void ir_lower_bind_local_static(IRLowerCtx* ctx, const char* name,
                                const char* global_name, IRType* value_type,
                                DataType ptr_base_type, const char* ptr_base_type_name, int ptr_depth) {
    if (!ctx || !name || !global_name) return;
    bool is_ptr = (ptr_depth > 0);

    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = -1;
            ctx->locals[i].value_type = value_type;
            ctx->locals[i].global_name = global_name;
            ctx->locals[i].is_static_storage = true;
            ctx->locals[i].is_pointer = is_ptr;
            ctx->locals[i].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
            ctx->locals[i].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
            ctx->locals[i].ptr_depth = is_ptr ? ptr_depth : 0;
            ctx->locals[i].is_array = false;
            ctx->locals[i].array_rank = 0;
            ctx->locals[i].array_dims = NULL;
            ctx->locals[i].array_elem_type = TYPE_INT;
            ctx->locals[i].array_elem_type_name = NULL;
            return;
        }
    }

    if (ctx->local_count >= (int)(sizeof(ctx->locals) / sizeof(ctx->locals[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد جدول المتغيرات المحلية أثناء ربط '%s'.", name);
        return;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].ptr_reg = -1;
    ctx->locals[ctx->local_count].value_type = value_type;
    ctx->locals[ctx->local_count].global_name = global_name;
    ctx->locals[ctx->local_count].is_static_storage = true;
    ctx->locals[ctx->local_count].is_pointer = is_ptr;
    ctx->locals[ctx->local_count].ptr_base_type = is_ptr ? ptr_base_type : TYPE_INT;
    ctx->locals[ctx->local_count].ptr_base_type_name = is_ptr ? ptr_base_type_name : NULL;
    ctx->locals[ctx->local_count].ptr_depth = is_ptr ? ptr_depth : 0;
    ctx->locals[ctx->local_count].is_array = false;
    ctx->locals[ctx->local_count].array_rank = 0;
    ctx->locals[ctx->local_count].array_dims = NULL;
    ctx->locals[ctx->local_count].array_elem_type = TYPE_INT;
    ctx->locals[ctx->local_count].array_elem_type_name = NULL;
    ctx->local_count++;
}

static void ir_lower_set_local_array_meta(IRLowerCtx* ctx,
                                          const char* name,
                                          int array_rank,
                                          const int* array_dims,
                                          DataType elem_type,
                                          const char* elem_type_name)
{
    if (!ctx || !name) return;
    for (int i = ctx->local_count - 1; i >= 0; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].is_array = true;
            ctx->locals[i].array_rank = array_rank;
            ctx->locals[i].array_dims = array_dims;
            ctx->locals[i].array_elem_type = elem_type;
            ctx->locals[i].array_elem_type_name = elem_type_name;
            return;
        }
    }
}

