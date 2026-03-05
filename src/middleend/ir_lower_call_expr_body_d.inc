    if (allow_math_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;

        if (strcmp(n, "جذر_تربيعي") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'جذر_تربيعي' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "sqrt", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء sqrt.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "أس") == 0) {
            if (!a0 || !a1 || a1->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'أس' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* base_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* exp_v = cast_to(ctx, lower_expr(ctx, a1), IR_TYPE_F64_T);
            IRValue* args[2] = { base_v, exp_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "pow", IR_TYPE_F64_T, args, 2);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء pow.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "جيب") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'جيب' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "sin", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء sin.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "جيب_تمام") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'جيب_تمام' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "cos", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء cos.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "ظل") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'ظل' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }

            IRValue* in_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_F64_T);
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "tan", IR_TYPE_F64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء tan.");
                return ir_value_const_int(0, IR_TYPE_F64_T);
            }
            return ir_value_reg(r, IR_TYPE_F64_T);
        }

        if (strcmp(n, "مطلق") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'مطلق' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }

            IRValue* in_v = ensure_i64(ctx, lower_expr(ctx, a0));
            IRValue* args[1] = { in_v };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "llabs", IR_TYPE_I64_T, args, 1);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء llabs.");
                return ir_builder_const_i64(0);
            }
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "عشوائي") == 0) {
            if (a0) {
                ir_lower_report_error(ctx, expr, "استدعاء 'عشوائي' لا يقبل معاملات.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }

            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "rand", IR_TYPE_I32_T, NULL, 0);
            if (r < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء rand.");
                return ir_builder_const_i64(0);
            }
            return cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T);
        }
    }

    bool allow_system_builtins = true;
    if (expr->data.call.name) {
        const char* n0 = expr->data.call.name;
        if (find_local(ctx, n0)) {
            allow_system_builtins = false;
        }
        if (m && ir_module_find_global(m, n0)) {
            allow_system_builtins = false;
        }
        if (m) {
            IRFunc* f0 = ir_module_find_func(m, n0);
            if (f0 && !f0->is_prototype) {
                allow_system_builtins = false;
            }
        }
    }

