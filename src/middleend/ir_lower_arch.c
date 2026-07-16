/**
 * @file ir_lower_arch.c
 * @brief خفض العمليات المعمارية العربية المهيكلة إلى IR.
 */

static bool ir_lower_arch_builtin_shadowed(IRLowerCtx* ctx,
                                           IRModule* module,
                                           const char* name)
{
    if (!ctx || !name) return true;
    if (find_local(ctx, name)) return true;
    if (module && ir_module_find_global(module, name)) return true;
    if (module) {
        IRFunc* func = ir_module_find_func(module, name);
        if (func && !func->is_prototype) return true;
    }
    return false;
}

static bool ir_lower_try_arch_builtin(IRLowerCtx* ctx,
                                      Node* expr,
                                      IRValue** out_value)
{
    if (out_value) *out_value = NULL;
    if (!ctx || !ctx->builder || !expr || expr->type != NODE_CALL_EXPR ||
        !expr->data.call.name) {
        return false;
    }

    const char* name = expr->data.call.name;
    if (strcmp(name, "لا_تفعل") != 0 &&
        strcmp(name, "اقرأ_عداد_الزمن") != 0) {
        return false;
    }

    IRModule* module = ctx->builder->module;
    if (ir_lower_arch_builtin_shadowed(ctx, module, name)) return false;

    if (expr->data.call.args) {
        ir_lower_report_error(ctx, expr, "استدعاء '%s' لا يقبل معاملات.", name);
        ir_lower_eval_call_args(ctx, expr->data.call.args);
        if (out_value) *out_value = ir_builder_const_i64(0);
        return true;
    }

    ir_lower_set_loc(ctx->builder, expr);
    if (strcmp(name, "لا_تفعل") == 0) {
        ir_builder_emit_cpu_nop(ctx->builder);
        if (out_value) *out_value = ir_builder_const_i64(0);
        return true;
    }

    int result = ir_builder_emit_read_tsc(ctx->builder);
    if (result < 0) {
        ir_lower_report_error(ctx, expr, "فشل خفض قراءة عداد الزمن.");
        if (out_value) *out_value = ir_builder_const_i64(0);
        return true;
    }
    if (out_value) *out_value = ir_value_reg(result, IR_TYPE_I64_T);
    return true;
}
