/**
 * @file ir_lower_process.c
 * @brief خفض واجهات العمليات ونظام الملفات إلى مكتبة وقت تشغيل باء.
 */

static bool ir_lower_try_process_builtin(IRLowerCtx* ctx, Node* expr, IRValue** out_value)
{
    if (out_value) *out_value = NULL;
    if (!ctx || !ctx->builder || !expr || expr->type != NODE_CALL_EXPR ||
        !expr->data.call.name) return false;

    const char* name = expr->data.call.name;
    const char* symbol = NULL;
    int expected = 1;
    IRType* result_type = IR_TYPE_I64_T;
    bool returns_void = false;

    if (strcmp(name, "ابدأ_عملية") == 0) {
        symbol = "baa_runtime_process_start";
        expected = 7;
        result_type = ir_type_ptr(IR_TYPE_I8_T);
    } else if (strcmp(name, "حالة_عملية") == 0) {
        symbol = "baa_runtime_process_poll";
    } else if (strcmp(name, "انتظر_عملية") == 0) {
        symbol = "baa_runtime_process_wait";
    } else if (strcmp(name, "الغ_عملية") == 0) {
        symbol = "baa_runtime_process_cancel";
    } else if (strcmp(name, "كود_خروج_عملية") == 0) {
        symbol = "baa_runtime_process_exit_code";
    } else if (strcmp(name, "حرر_عملية") == 0) {
        symbol = "baa_runtime_process_free";
        result_type = IR_TYPE_VOID_T;
        returns_void = true;
    } else if (strcmp(name, "انشئ_مجلدات") == 0) {
        symbol = "baa_runtime_make_dirs";
    } else if (strcmp(name, "احذف_شجرة") == 0) {
        symbol = "baa_runtime_remove_tree";
    } else if (strcmp(name, "تجزئة_ملف_شا٢٥٦") == 0) {
        symbol = "baa_runtime_sha256_file";
        result_type = get_char_ptr_type(ctx->builder->module);
    } else {
        return false;
    }

    IRValue* args[7] = {0};
    int count = 0;
    for (Node* arg = expr->data.call.args; arg && count < 7; arg = arg->next) {
        args[count++] = lower_expr(ctx, arg);
    }
    if (count != expected) {
        ir_lower_report_error(ctx, expr, "عدد معاملات '%s' غير صحيح أثناء الخفض.", name);
        if (out_value) *out_value = returns_void
            ? ir_value_const_int(0, IR_TYPE_I64_T)
            : ir_value_const_int(-1, result_type);
        return true;
    }

    if (strcmp(name, "ابدأ_عملية") == 0) {
        args[1] = ensure_i64(ctx, args[1]);
        args[4] = ensure_i64(ctx, args[4]);
    }

    ir_lower_set_loc(ctx->builder, expr);
    if (returns_void) {
        ir_builder_emit_call_void(ctx->builder, symbol, args, count);
        if (out_value) *out_value = ir_value_const_int(0, IR_TYPE_I64_T);
        return true;
    }

    int result = ir_builder_emit_call(ctx->builder, symbol, result_type, args, count);
    if (result < 0) {
        ir_lower_report_error(ctx, expr, "فشل خفض نداء '%s'.", name);
        if (out_value) *out_value = ir_value_const_int(-1, result_type);
        return true;
    }
    if (out_value) *out_value = ir_value_reg(result, result_type);
    return true;
}
