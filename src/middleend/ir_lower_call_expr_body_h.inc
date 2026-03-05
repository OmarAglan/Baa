    // حاول العثور على توقيع الدالة داخل وحدة IR (مباشر)، أو تفسير الاسم كـ مؤشر دالة (غير مباشر).
    IRFunc* callee = NULL;
    IRValue* callee_val = NULL;
    IRType* callee_sig = NULL; // IR_TYPE_FUNC عند النداء غير المباشر

    if (expr->data.call.name) {
        const char* n = expr->data.call.name;

        // 1) متغير محلي من نوع دالة(...)->...
        IRLowerBinding* b = find_local(ctx, n);
        if (b && b->value_type && b->value_type->kind == IR_TYPE_FUNC) {
            const char* storage_name = ir_lower_binding_storage_name(b);
            IRType* ft = b->value_type;
            IRValue* ptr = NULL;
            if (b->is_static_storage) {
                ptr = ir_value_global(storage_name, ft);
            } else {
                IRType* ptr_type = ir_type_ptr(ft ? ft : IR_TYPE_I64_T);
                ptr = ir_value_reg(b->ptr_reg, ptr_type);
            }
            int loaded = ir_builder_emit_load(ctx->builder, ft, ptr);
            callee_val = ir_value_reg(loaded, ft);
            callee_sig = ft;
        }

        // 2) متغير عام من نوع دالة(...)->...
        if (!callee_sig && ctx->builder && ctx->builder->module) {
            IRGlobal* g = ir_module_find_global(ctx->builder->module, n);
            if (g && g->type && g->type->kind == IR_TYPE_FUNC) {
                IRValue* gptr = ir_value_global(n, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                callee_val = ir_value_reg(loaded, g->type);
                callee_sig = g->type;
            }
        }

        // 3) إذا لم يكن هناك مؤشر دالة في النطاق/العوام، فاعتبره نداء مباشراً للدالة.
        if (!callee_sig && m) {
            callee = ir_module_find_func(m, n);
        }
    }

    IRType* ret_type = NULL;
    if (callee && callee->ret_type) {
        ret_type = callee->ret_type;
    } else if (callee_sig && callee_sig->kind == IR_TYPE_FUNC) {
        ret_type = callee_sig->data.func.ret;
    } else {
        ret_type = ir_type_from_datatype_ex(m, expr->inferred_type, expr->inferred_func_sig);
    }
    if (!ret_type) ret_type = IR_TYPE_I64_T;
    bool callee_is_variadic = (callee && callee->is_variadic);
    int callee_fixed_params = 0;
    if (callee) {
        callee_fixed_params = callee->param_count;
        if (callee_is_variadic && callee_fixed_params > 0) {
            callee_fixed_params -= 1; // معامل خفي: __baa_va_base
        }
    }

    // Count args
    int arg_count = 0;
    for (Node* a = expr->data.call.args; a; a = a->next) arg_count++;
    IRValue** args = NULL;
    if (arg_count > 0) {
        args = (IRValue**)malloc(sizeof(IRValue*) * (size_t)arg_count);
        if (!args) {
            ir_lower_report_error(ctx, expr, "فشل تخصيص معاملات النداء.");
            return ir_builder_const_i64(0);
        }
    }

    int i = 0;
    for (Node* a = expr->data.call.args; a; a = a->next) {
        IRValue* av = lower_expr(ctx, a);
        IRType* exp_t = NULL;
        if (callee && i < callee_fixed_params && callee->params) {
            exp_t = callee->params[i].type;
        } else if (callee_sig && callee_sig->kind == IR_TYPE_FUNC &&
                   i < callee_sig->data.func.param_count && callee_sig->data.func.params) {
            exp_t = callee_sig->data.func.params[i];
        }
        if (exp_t) {
            av = cast_to(ctx, av, exp_t);
        }
        args[i++] = av;
    }

    IRValue** call_args = args;
    int call_arg_count = arg_count;
    if (callee_is_variadic) {
        IRValue* va_base = ir_lower_pack_variadic_args(ctx, expr, args, arg_count);
        int fixed = callee_fixed_params;
        if (fixed < 0) fixed = 0;
        if (fixed > arg_count) fixed = arg_count;

        call_arg_count = fixed + 1;
        call_args = (IRValue**)malloc(sizeof(IRValue*) * (size_t)call_arg_count);
        if (!call_args) {
            if (args) free(args);
            ir_lower_report_error(ctx, expr, "فشل تخصيص معاملات النداء المتغير.");
            return ir_builder_const_i64(0);
        }
        for (int k = 0; k < fixed; k++) {
            call_args[k] = args[k];
        }
        call_args[fixed] = va_base;
    }

    int dest = -1;
    if (callee) {
        dest = ir_builder_emit_call(ctx->builder, expr->data.call.name, ret_type, call_args, call_arg_count);
    } else if (callee_val) {
        dest = ir_builder_emit_call_indirect(ctx->builder, callee_val, ret_type, call_args, call_arg_count);
    } else {
        ir_lower_report_error(ctx, expr, "استدعاء غير صالح: '%s' ليس دالة ولا مؤشر دالة.",
                              expr->data.call.name ? expr->data.call.name : "???");
        dest = -1;
    }

    if (call_args && call_args != args) free(call_args);
    if (args) free(args);

    if (dest < 0) {
        // Void call used as expression: return 0 (placeholder).
        return ir_builder_const_i64(0);
    }

    return ir_value_reg(dest, ret_type);
