    if (allow_system_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;

        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
        IRType* char_ptr_t = get_char_ptr_type(m);

        if (strcmp(n, "متغير_بيئة") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'متغير_بيئة' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, char_ptr_t);
            }

            IRValue* key_i8 = NULL;
            bool free_key = false;
            if (a0->type == NODE_STRING && a0->data.string_lit.value) {
                key_i8 = ir_builder_const_string(ctx->builder, a0->data.string_lit.value);
            } else {
                IRValue* key_baa = lower_expr(ctx, a0);
                key_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, key_baa);
                free_key = true;
            }

            int out_ptr_reg = ir_builder_emit_alloca(ctx->builder, i8_ptr_t);
            IRValue* out_ptr = ir_value_reg(out_ptr_reg, ir_type_ptr(i8_ptr_t));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), out_ptr);

            int key_null_r = ir_builder_emit_cmp_eq(ctx->builder, key_i8, ir_value_const_int(0, i8_ptr_t));
            IRValue* key_null = ir_value_reg(key_null_r, IR_TYPE_I1_T);

            IRBlock* nullb = cf_create_block(ctx, "متغير_بيئة_مفتاح_فارغ");
            IRBlock* callb = cf_create_block(ctx, "متغير_بيئة_نداء");
            IRBlock* done = cf_create_block(ctx, "متغير_بيئة_نهاية");
            if (!nullb || !callb || !done) {
                if (free_key) {
                    IRValue* fargs[1] = { key_i8 };
                    ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
                }
                return ir_value_const_int(0, char_ptr_t);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, key_null, nullb, callb);
            }

            ir_builder_set_insert_point(ctx->builder, nullb);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), out_ptr);
            if (free_key) {
                IRValue* fargs[1] = { key_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, callb);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* args[1] = { key_i8 };
            int r = ir_builder_emit_call(ctx->builder, "getenv", i8_ptr_t, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء getenv.");
                ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), out_ptr);
            } else {
                ir_builder_emit_store(ctx->builder, ir_value_reg(r, i8_ptr_t), out_ptr);
            }
            if (free_key) {
                IRValue* fargs[1] = { key_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            int raw_r = ir_builder_emit_load(ctx->builder, i8_ptr_t, out_ptr);
            IRValue* raw = ir_value_reg(raw_r, i8_ptr_t);
            return ir_lower_cstr_to_baa_string_alloc(ctx, expr, raw);
        }

        if (strcmp(n, "نفذ_أمر") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نفذ_أمر' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* cmd_i8 = NULL;
            bool free_cmd = false;
            if (a0->type == NODE_STRING && a0->data.string_lit.value) {
                cmd_i8 = ir_builder_const_string(ctx->builder, a0->data.string_lit.value);
            } else {
                IRValue* cmd_baa = lower_expr(ctx, a0);
                cmd_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, cmd_baa);
                free_cmd = true;
            }

            int ret_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ret_ptr = ir_value_reg(ret_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);

            int cmd_null_r = ir_builder_emit_cmp_eq(ctx->builder, cmd_i8, ir_value_const_int(0, i8_ptr_t));
            IRValue* cmd_null = ir_value_reg(cmd_null_r, IR_TYPE_I1_T);

            IRBlock* nullb = cf_create_block(ctx, "نفذ_أمر_فارغ");
            IRBlock* callb = cf_create_block(ctx, "نفذ_أمر_نداء");
            IRBlock* done = cf_create_block(ctx, "نفذ_أمر_نهاية");
            if (!nullb || !callb || !done) {
                if (free_cmd) {
                    IRValue* fargs[1] = { cmd_i8 };
                    ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
                }
                return ir_builder_const_i64(-1);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, cmd_null, nullb, callb);
            }

            ir_builder_set_insert_point(ctx->builder, nullb);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);
            if (free_cmd) {
                IRValue* fargs[1] = { cmd_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, callb);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* args[1] = { cmd_i8 };
            int r = ir_builder_emit_call(ctx->builder, "system", IR_TYPE_I32_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء system.");
                ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);
            } else {
                ir_builder_emit_store(ctx->builder,
                                      cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T),
                                      ret_ptr);
            }
            if (free_cmd) {
                IRValue* fargs[1] = { cmd_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            int ret_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, ret_ptr);
            return ir_value_reg(ret_r, IR_TYPE_I64_T);
        }
    }

    bool allow_time_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) {
            allow_time_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_time_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) {
                allow_time_builtins = false;
            }
        }
    }

