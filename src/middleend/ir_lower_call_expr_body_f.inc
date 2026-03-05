    if (allow_time_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;

        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
        IRType* char_ptr_t = get_char_ptr_type(m);

        if (strcmp(n, "وقت_حالي") == 0) {
            if (a0) {
                ir_lower_report_error(ctx, expr, "استدعاء 'وقت_حالي' لا يقبل معاملات.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* args[1] = { ir_value_const_int(0, i8_ptr_t) };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "time", IR_TYPE_I64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء time.");
                return ir_builder_const_i64(-1);
            }
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "وقت_كنص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'وقت_كنص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, char_ptr_t);
            }

            IRValue* ts = ensure_i64(ctx, lower_expr(ctx, a0));
            int ts_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ts_ptr = ir_value_reg(ts_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
            ir_builder_emit_store(ctx->builder, ts, ts_ptr);

            IRValue* ts_arg = cast_to(ctx, ts_ptr, i8_ptr_t);
            IRValue* args[1] = { ts_arg };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "ctime", i8_ptr_t, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء ctime.");
                return ir_value_const_int(0, char_ptr_t);
            }

            IRValue* raw = ir_value_reg(r, i8_ptr_t);
            return ir_lower_cstr_to_baa_string_alloc(ctx, expr, raw);
        }
    }

    bool allow_file_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        // نفس قواعد الـ shadowing: متغير/تخزين بنفس الاسم يمنع تحويله إلى builtin.
        if (find_local(ctx, n0)) {
            allow_file_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_file_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            // إذا كانت الدالة prototype فقط (مثلاً من stdlib)، نسمح للـ builtin.
            if (f0 && !f0->is_prototype) {
                allow_file_builtins = false;
            }
        }
    }

