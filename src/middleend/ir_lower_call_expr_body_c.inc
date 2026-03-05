
    // دوال الذاكرة المدمجة في v0.3.11: حجز/تحرير/إعادة_حجز/نسخ_ذاكرة/تعيين_ذاكرة.
    bool allow_mem_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        // نفس قواعد الـ shadowing: متغير/تخزين بنفس الاسم يمنع تحويله إلى builtin.
        if (find_local(ctx, n0)) {
            allow_mem_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_mem_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            // إذا كانت الدالة prototype فقط (مثلاً من stdlib)، نسمح للـ builtin.
            if (f0 && !f0->is_prototype) {
                allow_mem_builtins = false;
            }
        }
    }

    if (allow_mem_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;
        Node* a2 = a1 ? a1->next : NULL;
        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

        if (strcmp(n, "حجز_ذاكرة") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'حجز_ذاكرة' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* size_v = lower_expr(ctx, a0);
            IRValue* size_i64 = ensure_i64(ctx, size_v);
            IRValue* args[1] = { size_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "malloc", i8_ptr_t, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء malloc.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }

        if (strcmp(n, "تحرير_ذاكرة") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'تحرير_ذاكرة' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* pv = lower_expr(ctx, a0);
            IRValue* p_i8 = cast_to(ctx, pv, i8_ptr_t);
            IRValue* args[1] = { p_i8 };
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_call_void(ctx->builder, "free", args, 1);
            return ir_builder_const_i64(0);
        }

        if (strcmp(n, "إعادة_حجز") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'إعادة_حجز' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* pv = lower_expr(ctx, a0);
            IRValue* size_v = lower_expr(ctx, a1);
            IRValue* p_i8 = cast_to(ctx, pv, i8_ptr_t);
            IRValue* size_i64 = ensure_i64(ctx, size_v);
            IRValue* args[2] = { p_i8, size_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "realloc", i8_ptr_t, args, 2);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء realloc.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }

        if (strcmp(n, "نسخ_ذاكرة") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نسخ_ذاكرة' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* dv = lower_expr(ctx, a0);
            IRValue* sv = lower_expr(ctx, a1);
            IRValue* nv = lower_expr(ctx, a2);
            IRValue* dst = cast_to(ctx, dv, i8_ptr_t);
            IRValue* src = cast_to(ctx, sv, i8_ptr_t);
            IRValue* n_i64 = ensure_i64(ctx, nv);
            IRValue* args[3] = { dst, src, n_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "memcpy", i8_ptr_t, args, 3);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء memcpy.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }

        if (strcmp(n, "تعيين_ذاكرة") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'تعيين_ذاكرة' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }
            IRValue* pv = lower_expr(ctx, a0);
            IRValue* vv = lower_expr(ctx, a1);
            IRValue* nv = lower_expr(ctx, a2);
            IRValue* p_i8 = cast_to(ctx, pv, i8_ptr_t);
            IRValue* v_i64 = ensure_i64(ctx, vv);
            IRValue* n_i64 = ensure_i64(ctx, nv);
            IRValue* args[3] = { p_i8, v_i64, n_i64 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "memset", i8_ptr_t, args, 3);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء memset.");
                return ir_value_const_int(0, i8_ptr_t);
            }
            return ir_value_reg(r, i8_ptr_t);
        }
    }

    bool allow_math_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) {
            allow_math_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_math_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) {
                allow_math_builtins = false;
            }
        }
    }

