    if (allow_string_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;
        Node* a2 = a1 ? a1->next : NULL;

        if (strcmp(n, "طول_نص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'طول_نص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_strlen_from_value(ctx, expr, sv);
        }

        if (strcmp(n, "قارن_نص") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'قارن_نص' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* av = lower_expr(ctx, a0);
            IRValue* bv = lower_expr(ctx, a1);
            return ir_lower_builtin_strcmp_from_values(ctx, expr, av, bv);
        }

        if (strcmp(n, "طول_خام") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'طول_خام' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_raw_len_from_value(ctx, expr, sv);
        }

        if (strcmp(n, "قارن_خام") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'قارن_خام' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* av = lower_expr(ctx, a0);
            IRValue* bv = lower_expr(ctx, a1);
            return ir_lower_builtin_raw_cmp_from_values(ctx, expr, av, bv);
        }

        if (strcmp(n, "قارن_خام_بطول") == 0) {
            Node* a3 = a2 ? a2->next : NULL;
            if (!a0 || !a1 || !a2 || a3) {
                ir_lower_report_error(ctx, expr, "استدعاء 'قارن_خام_بطول' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_I1_T);
            }
            IRValue* av = lower_expr(ctx, a0);
            IRValue* lv = lower_expr(ctx, a1);
            IRValue* bv = lower_expr(ctx, a2);
            return ir_lower_builtin_raw_eq_len_from_values(ctx, expr, av, lv, bv);
        }

        if (strcmp(n, "نسخ_نص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نسخ_نص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, get_char_ptr_type(m));
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_copy_from_value(ctx, expr, sv);
        }

        if (strcmp(n, "دمج_نص") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'دمج_نص' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, get_char_ptr_type(m));
            }
            IRValue* av = lower_expr(ctx, a0);
            IRValue* bv = lower_expr(ctx, a1);
            return ir_lower_builtin_concat_from_values(ctx, expr, av, bv);
        }

        if (strcmp(n, "حرر_نص") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'حرر_نص' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* sv = lower_expr(ctx, a0);
            return ir_lower_builtin_free_value(ctx, expr, sv);
        }
    }
