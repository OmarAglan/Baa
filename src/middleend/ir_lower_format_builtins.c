static IRValue* ir_lower_builtin_print_format_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* fmt_node = call_expr->data.call.args;
    Node* args_after = fmt_node ? fmt_node->next : NULL;
    if (!fmt_node || fmt_node->type != NODE_STRING || !fmt_node->data.string_lit.value) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: نص التنسيق يجب أن يكون ثابتاً (literal).");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
    int spec_count = 0;
    char* cfmt = NULL;
    if (!baa_fmt_translate_ar(ctx, fmt_node, fmt_node->data.string_lit.value, false, &cfmt, specs, &spec_count)) {
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* fmt_val = ir_builder_const_string(b, cfmt);
    free(cfmt);
    if (!fmt_val) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: فشل إنشاء ثابت نص التنسيق.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRValue* call_args[1 + BAA_FMT_MAX_SPECS];
    IRValue* to_free[BAA_FMT_MAX_SPECS];
    int free_count = 0;

    call_args[0] = fmt_val;

    int idx = 0;
    for (Node* an = args_after; an && idx < spec_count; an = an->next, idx++) {
        IRValue* v = lower_expr(ctx, an);
        switch (specs[idx].kind) {
            case BAA_FMT_SPEC_STR: {
                IRValue* s_alloc = ir_lower_baa_string_to_cstr_alloc(ctx, call_expr, v);
                if (free_count < BAA_FMT_MAX_SPECS) {
                    to_free[free_count++] = s_alloc;
                }
                v = ir_lower_cstr_or_literal_on_null(ctx, call_expr, s_alloc, "عدم");
                break;
            }
            case BAA_FMT_SPEC_CHAR:
                v = ir_lower_char_to_cstr_tmp(ctx, call_expr, v);
                break;
            case BAA_FMT_SPEC_F64:
            case BAA_FMT_SPEC_F64_SCI:
                v = cast_to(ctx, v, IR_TYPE_F64_T);
                break;
            case BAA_FMT_SPEC_U64:
            case BAA_FMT_SPEC_HEX:
                v = cast_to(ctx, v, IR_TYPE_U64_T);
                break;
            case BAA_FMT_SPEC_PTR:
                v = cast_to(ctx, v, i8_ptr_t);
                break;
            case BAA_FMT_SPEC_I64:
            default:
                v = ensure_i64(ctx, v);
                break;
        }
        call_args[1 + idx] = v ? v : ir_value_const_int(0, IR_TYPE_I64_T);
    }

    ir_lower_set_loc(b, call_expr);
    ir_builder_emit_call_void(b, "printf", call_args, 1 + spec_count);

    for (int i = 0; i < free_count; i++) {
        IRValue* fargs[1] = { to_free[i] };
        ir_builder_emit_call_void(b, "free", fargs, 1);
    }

    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_format_string_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_value_const_int(0, get_char_ptr_type(NULL));

    Node* fmt_node = call_expr->data.call.args;
    Node* args_after = fmt_node ? fmt_node->next : NULL;
    if (!fmt_node || fmt_node->type != NODE_STRING || !fmt_node->data.string_lit.value) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: نص التنسيق يجب أن يكون ثابتاً (literal).");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL));
    }

    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
    int spec_count = 0;
    char* cfmt = NULL;
    if (!baa_fmt_translate_ar(ctx, fmt_node, fmt_node->data.string_lit.value, false, &cfmt, specs, &spec_count)) {
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* fmt_val = ir_builder_const_string(b, cfmt);
    free(cfmt);
    if (!fmt_val) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: فشل إنشاء ثابت نص التنسيق.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, char_ptr_t);
    }

    IRValue* tmp_args[3 + BAA_FMT_MAX_SPECS];
    IRValue* to_free[BAA_FMT_MAX_SPECS];
    int free_count = 0;

    tmp_args[0] = ir_value_const_int(0, i8_ptr_t);
    tmp_args[1] = ir_value_const_int(0, IR_TYPE_I64_T);
    tmp_args[2] = fmt_val;

    int idx = 0;
    for (Node* an = args_after; an && idx < spec_count; an = an->next, idx++) {
        IRValue* v = lower_expr(ctx, an);
        switch (specs[idx].kind) {
            case BAA_FMT_SPEC_STR: {
                IRValue* s_alloc = ir_lower_baa_string_to_cstr_alloc(ctx, call_expr, v);
                if (free_count < BAA_FMT_MAX_SPECS) {
                    to_free[free_count++] = s_alloc;
                }
                v = ir_lower_cstr_or_literal_on_null(ctx, call_expr, s_alloc, "عدم");
                break;
            }
            case BAA_FMT_SPEC_CHAR:
                v = ir_lower_char_to_cstr_tmp(ctx, call_expr, v);
                break;
            case BAA_FMT_SPEC_F64:
            case BAA_FMT_SPEC_F64_SCI:
                v = cast_to(ctx, v, IR_TYPE_F64_T);
                break;
            case BAA_FMT_SPEC_U64:
            case BAA_FMT_SPEC_HEX:
                v = cast_to(ctx, v, IR_TYPE_U64_T);
                break;
            case BAA_FMT_SPEC_PTR:
                v = cast_to(ctx, v, i8_ptr_t);
                break;
            case BAA_FMT_SPEC_I64:
            default:
                v = ensure_i64(ctx, v);
                break;
        }
        tmp_args[3 + idx] = v ? v : ir_value_const_int(0, IR_TYPE_I64_T);
    }

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);

    IRBlock* b_bad = cf_create_block(ctx, "نسق_فشل");
    IRBlock* b_ok = cf_create_block(ctx, "نسق_عمل");
    IRBlock* b_ret = cf_create_block(ctx, "نسق_إرجاع");
    if (!b_bad || !b_ok || !b_ret) {
        for (int i = 0; i < free_count; i++) {
            IRValue* fargs[1] = { to_free[i] };
            ir_builder_emit_call_void(b, "free", fargs, 1);
        }
        return ir_value_const_int(0, char_ptr_t);
    }

    ir_lower_set_loc(b, call_expr);
    int n_r = ir_builder_emit_call(b, "snprintf", IR_TYPE_I32_T, tmp_args, 3 + spec_count);
    IRValue* n_i32 = ir_value_reg(n_r, IR_TYPE_I32_T);
    IRValue* n_i64 = cast_to(ctx, n_i32, IR_TYPE_I64_T);
    int neg_r = ir_builder_emit_cmp_lt(b, n_i64, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* neg = ir_value_reg(neg_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, neg, b_bad, b_ok);

    ir_builder_set_insert_point(b, b_bad);
    ir_lower_set_loc(b, call_expr);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ok);
    ir_lower_set_loc(b, call_expr);
    IRValue* size1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, n_i64, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* margs[1] = { size1 };
    int buf_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    IRValue* buf = ir_value_reg(buf_r, i8_ptr_t);

    tmp_args[0] = buf;
    tmp_args[1] = size1;
    tmp_args[2] = fmt_val;
    (void)ir_builder_emit_call(b, "snprintf", IR_TYPE_I32_T, tmp_args, 3 + spec_count);

    IRValue* baa = ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, buf);
    IRValue* fbuf[1] = { buf };
    ir_builder_emit_call_void(b, "free", fbuf, 1);
    ir_builder_emit_store(b, baa, res_ptr);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ret);
    ir_lower_set_loc(b, call_expr);
    int rr = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    IRValue* outv = ir_value_reg(rr, char_ptr_t);

    for (int i = 0; i < free_count; i++) {
        IRValue* fargs[1] = { to_free[i] };
        ir_builder_emit_call_void(b, "free", fargs, 1);
    }

    return outv;
}

static IRValue* ir_lower_builtin_read_num_stdin_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (call_expr->data.call.args) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'اقرأ_رقم' لا يقبل معاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    int tmp_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* tmp_ptr = ir_value_reg(tmp_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), tmp_ptr);

    IRValue* fmt_val = ir_builder_const_string(b, "%lld");
    IRValue* args2[2] = { fmt_val, tmp_ptr };
    ir_lower_set_loc(b, call_expr);
    (void)ir_builder_emit_call(b, "scanf", IR_TYPE_I32_T, args2, 2);

    int lr = ir_builder_emit_load(b, IR_TYPE_I64_T, tmp_ptr);
    return ir_value_reg(lr, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_read_line_stdin_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_value_const_int(0, get_char_ptr_type(NULL));

    if (call_expr->data.call.args) {
        // دع مسار اقرأ_سطر(ملف) يمر إلى builtins الملفات (v0.3.12).
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* i32_ptr_t = ir_type_ptr(IR_TYPE_I32_T);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    // res: نص باء أو NULL (عند EOF قبل أي بايت)
    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);

    int buf_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), buf_ptr);

    int len_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int cap_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int last_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I32_T);
    IRValue* len_ptr = ir_value_reg(len_ptr_reg, i64_ptr_t);
    IRValue* cap_ptr = ir_value_reg(cap_ptr_reg, i64_ptr_t);
    IRValue* last_ptr = ir_value_reg(last_ptr_reg, i32_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len_ptr);
    ir_builder_emit_store(b, ir_value_const_int(256, IR_TYPE_I64_T), cap_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I32_T), last_ptr);

    IRValue* cap0 = ir_value_const_int(256, IR_TYPE_I64_T);
    IRValue* margs0[1] = { cap0 };
    ir_lower_set_loc(b, call_expr);
    int buf0_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs0, 1);
    IRValue* buf0 = ir_value_reg(buf0_r, i8_ptr_t);
    ir_builder_emit_store(b, buf0, buf_ptr);

    IRBlock* head = cf_create_block(ctx, "اقرأ_سطر_تحقق");
    IRBlock* check = cf_create_block(ctx, "اقرأ_سطر_سعة");
    IRBlock* grow = cf_create_block(ctx, "اقرأ_سطر_توسيع");
    IRBlock* storeb = cf_create_block(ctx, "اقرأ_سطر_خزن");
    IRBlock* done = cf_create_block(ctx, "اقرأ_سطر_نهاية");
    IRBlock* nullb = cf_create_block(ctx, "اقرأ_سطر_NULL");
    IRBlock* workb = cf_create_block(ctx, "اقرأ_سطر_عمل");
    IRBlock* retb = cf_create_block(ctx, "اقرأ_سطر_إرجاع");
    if (!head || !check || !grow || !storeb || !done || !nullb || !workb || !retb) {
        return ir_value_const_int(0, char_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    // head: ch = getchar(); إذا EOF أو '\n' -> done، وإلا -> check
    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, call_expr);
    int ch_r = ir_builder_emit_call(b, "getchar", IR_TYPE_I32_T, NULL, 0);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_I32_T);
    ir_builder_emit_store(b, ch, last_ptr);
    IRValue* ch_i64 = cast_to(ctx, ch, IR_TYPE_I64_T);
    int is_eof_r = ir_builder_emit_cmp_eq(b, ch_i64, ir_value_const_int(-1, IR_TYPE_I64_T));
    IRValue* is_eof = ir_value_reg(is_eof_r, IR_TYPE_I1_T);
    int is_nl_r = ir_builder_emit_cmp_eq(b, ch_i64, ir_value_const_int(10, IR_TYPE_I64_T));
    IRValue* is_nl = ir_value_reg(is_nl_r, IR_TYPE_I1_T);
    int stop_r = ir_builder_emit_or(b, IR_TYPE_I1_T, is_eof, is_nl);
    IRValue* stop = ir_value_reg(stop_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, stop, done, check);

    // check: إن احتجنا توسعة -> grow else -> store
    ir_builder_set_insert_point(b, check);
    ir_lower_set_loc(b, call_expr);
    IRValue* lenv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* capv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, cap_ptr), IR_TYPE_I64_T);
    IRValue* need = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, lenv, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int need_grow_r = ir_builder_emit_cmp_ge(b, need, capv);
    IRValue* need_grow = ir_value_reg(need_grow_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, need_grow, grow, storeb);

    // grow: cap*=2; buf=realloc(buf,cap); ثم -> store
    ir_builder_set_insert_point(b, grow);
    ir_lower_set_loc(b, call_expr);
    IRValue* cap2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, capv, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bufv0 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* rargs[2] = { bufv0, cap2 };
    int nb_r = ir_builder_emit_call(b, "realloc", i8_ptr_t, rargs, 2);
    IRValue* nb = ir_value_reg(nb_r, i8_ptr_t);
    ir_builder_emit_store(b, nb, buf_ptr);
    ir_builder_emit_store(b, cap2, cap_ptr);
    ir_builder_emit_br(b, storeb);

    // store: buf[len]=byte; len++; -> head
    ir_builder_set_insert_point(b, storeb);
    ir_lower_set_loc(b, call_expr);
    IRValue* bufv1 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* len_store = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* pos = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, bufv1, len_store), i8_ptr_t);
    IRValue* last = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I32_T, last_ptr), IR_TYPE_I32_T);
    ir_builder_emit_store(b, cast_to(ctx, last, IR_TYPE_I8_T), pos);
    IRValue* len2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len_store, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, len2, len_ptr);
    ir_builder_emit_br(b, head);

    // done: إذا EOF و len==0 -> NULL، وإلا -> work
    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, call_expr);
    IRValue* lenf = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* last2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I32_T, last_ptr), IR_TYPE_I32_T);
    IRValue* last2_i64 = cast_to(ctx, last2, IR_TYPE_I64_T);
    int eof2_r = ir_builder_emit_cmp_eq(b, last2_i64, ir_value_const_int(-1, IR_TYPE_I64_T));
    IRValue* eof2 = ir_value_reg(eof2_r, IR_TYPE_I1_T);
    int len0_r = ir_builder_emit_cmp_eq(b, lenf, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* len0 = ir_value_reg(len0_r, IR_TYPE_I1_T);
    int ret_null_r = ir_builder_emit_and(b, IR_TYPE_I1_T, eof2, len0);
    IRValue* ret_null = ir_value_reg(ret_null_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, ret_null, nullb, workb);

    ir_builder_set_insert_point(b, nullb);
    ir_lower_set_loc(b, call_expr);
    IRValue* bufv2 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* f0[1] = { bufv2 };
    ir_builder_emit_call_void(b, "free", f0, 1);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), res_ptr);
    ir_builder_emit_br(b, retb);

    ir_builder_set_insert_point(b, workb);
    ir_lower_set_loc(b, call_expr);
    IRValue* bufv3 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* termp = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, bufv3, lenf), i8_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), termp);
    IRValue* baa = ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, bufv3);
    IRValue* f1[1] = { bufv3 };
    ir_builder_emit_call_void(b, "free", f1, 1);
    ir_builder_emit_store(b, baa, res_ptr);
    ir_builder_emit_br(b, retb);

    ir_builder_set_insert_point(b, retb);
    ir_lower_set_loc(b, call_expr);
    int rr = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(rr, char_ptr_t);
}

static IRValue* ir_lower_builtin_scan_format_ar(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* fmt_node = call_expr->data.call.args;
    Node* args_after = fmt_node ? fmt_node->next : NULL;
    if (!fmt_node || fmt_node->type != NODE_STRING || !fmt_node->data.string_lit.value) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: نص التنسيق في 'اقرأ_منسق' يجب أن يكون ثابتاً (literal).");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    BaaFmtSpec specs[BAA_FMT_MAX_SPECS];
    int spec_count = 0;
    char* cfmt = NULL;
    if (!baa_fmt_translate_ar(ctx, fmt_node, fmt_node->data.string_lit.value, true, &cfmt, specs, &spec_count)) {
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* fmt_val = ir_builder_const_string(b, cfmt);
    free(cfmt);
    if (!fmt_val) {
        ir_lower_report_error(ctx, call_expr, "تنسيق عربي: فشل إنشاء ثابت نص التنسيق.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRValue* call_args[1 + BAA_FMT_MAX_SPECS];
    call_args[0] = fmt_val;

    IRValue* str_bufs[BAA_FMT_MAX_SPECS];
    IRValue* str_out_ptrs[BAA_FMT_MAX_SPECS];
    int str_spec_index[BAA_FMT_MAX_SPECS];
    int str_count = 0;

    int idx = 0;
    for (Node* an = args_after; an && idx < spec_count; an = an->next, idx++) {
        IRValue* v = lower_expr(ctx, an);
        switch (specs[idx].kind) {
            case BAA_FMT_SPEC_STR: {
                int w = specs[idx].has_width ? specs[idx].width : 64;
                if (w <= 0) w = 64;
                IRValue* outp = cast_to(ctx, v, char_ptr_ptr_t);
                IRValue* sz = ir_value_const_int((int64_t)w + 1, IR_TYPE_I64_T);
                IRValue* margs[1] = { sz };
                int buf_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
                IRValue* buf = ir_value_reg(buf_r, i8_ptr_t);
                call_args[1 + idx] = buf;
                if (str_count < BAA_FMT_MAX_SPECS) {
                    str_bufs[str_count] = buf;
                    str_out_ptrs[str_count] = outp;
                    str_spec_index[str_count] = idx;
                    str_count++;
                }
                break;
            }
            case BAA_FMT_SPEC_I64:
                call_args[1 + idx] = cast_to(ctx, v, ir_type_ptr(IR_TYPE_I64_T));
                break;
            case BAA_FMT_SPEC_U64:
            case BAA_FMT_SPEC_HEX:
                call_args[1 + idx] = cast_to(ctx, v, ir_type_ptr(IR_TYPE_U64_T));
                break;
            case BAA_FMT_SPEC_F64:
            case BAA_FMT_SPEC_F64_SCI:
                call_args[1 + idx] = cast_to(ctx, v, ir_type_ptr(IR_TYPE_F64_T));
                break;
            default:
                ir_lower_report_error(ctx, call_expr, "تنسيق عربي: مواصفة غير مدعومة في 'اقرأ_منسق'.");
                call_args[1 + idx] = cast_to(ctx, v, i8_ptr_t);
                break;
        }
    }

    ir_lower_set_loc(b, call_expr);
    int rc_r = ir_builder_emit_call(b, "scanf", IR_TYPE_I32_T, call_args, 1 + spec_count);
    IRValue* rc_i32 = ir_value_reg(rc_r, IR_TYPE_I32_T);
    IRValue* rc_i64 = cast_to(ctx, rc_i32, IR_TYPE_I64_T);

    // لكل %ن: إذا rc > index -> حوّل وخزّن، وإلا لا تغيّر قيمة نص المستخدم.
    for (int si = 0; si < str_count; si++) {
        int spec_i = str_spec_index[si];
        int ok_r = ir_builder_emit_cmp_gt(b, rc_i64, ir_value_const_int((int64_t)spec_i, IR_TYPE_I64_T));
        IRValue* ok = ir_value_reg(ok_r, IR_TYPE_I1_T);

        IRBlock* yesb = cf_create_block(ctx, "اقرأ_منسق_نص_نعم");
        IRBlock* nob = cf_create_block(ctx, "اقرأ_منسق_نص_لا");
        IRBlock* nextb = cf_create_block(ctx, "اقرأ_منسق_نص_تابع");
        if (!yesb || !nob || !nextb) continue;

        ir_builder_emit_br_cond(b, ok, yesb, nob);

        ir_builder_set_insert_point(b, yesb);
        ir_lower_set_loc(b, call_expr);
        IRValue* baa = ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, str_bufs[si]);
        ir_builder_emit_store(b, baa, str_out_ptrs[si]);
        IRValue* f0[1] = { str_bufs[si] };
        ir_builder_emit_call_void(b, "free", f0, 1);
        ir_builder_emit_br(b, nextb);

        ir_builder_set_insert_point(b, nob);
        ir_lower_set_loc(b, call_expr);
        IRValue* f1[1] = { str_bufs[si] };
        ir_builder_emit_call_void(b, "free", f1, 1);
        ir_builder_emit_br(b, nextb);

        ir_builder_set_insert_point(b, nextb);
        ir_lower_set_loc(b, call_expr);
    }

    return rc_i64;
}

static int64_t ir_lower_pointer_elem_size_meta(DataType base, int depth)
{
    if (depth > 1) return 8;
    if (depth <= 0) return 1;

    switch (base) {
        case TYPE_BOOL:
        case TYPE_I8:
        case TYPE_U8:
            return 1;
        case TYPE_I16:
        case TYPE_U16:
            return 2;
        case TYPE_I32:
        case TYPE_U32:
            return 4;
        case TYPE_INT:
        case TYPE_U64:
        case TYPE_CHAR:
        case TYPE_FLOAT:
        case TYPE_ENUM:
        case TYPE_STRING:
        case TYPE_POINTER:
            return 8;
        default:
            return 1;
    }
}

static IRValue* ir_lower_pointer_index_chain(IRLowerCtx* ctx,
                                             Node* site,
                                             IRValue* base_ptr_val,
                                             DataType base_type,
                                             const char* base_type_name,
                                             int depth,
                                             Node* indices,
                                             int index_count,
                                             bool return_address,
                                             IRType** out_pointee_type)
{
    (void)base_type_name;

    if (out_pointee_type) *out_pointee_type = IR_TYPE_I64_T;
    if (!ctx || !ctx->builder || !base_ptr_val || !indices) {
        return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
    }

    int effective_count = index_count > 0 ? index_count : ir_lower_index_count(indices);
    if (effective_count <= 0) {
        ir_lower_report_error(ctx, site, "فهرسة مؤشر بدون فهارس غير صالحة.");
        return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
    }

    IRModule* m = ctx->builder->module;
    IRType* void_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    DataType cur_base = base_type;
    int cur_depth = depth;

    IRValue* cur_ptr = cast_to(ctx, base_ptr_val, void_ptr_t);
    Node* idx_node = indices;

    for (int i = 0; i < effective_count && idx_node; i++, idx_node = idx_node->next) {
        if (cur_depth <= 0) {
            ir_lower_report_error(ctx, site, "عمق المؤشر غير صالح أثناء فهرسة المؤشر.");
            return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
        }

        IRValue* idx0 = ensure_i64(ctx, lower_expr(ctx, idx_node));
        int64_t elem_size = ir_lower_pointer_elem_size_meta(cur_base, cur_depth);
        IRValue* idx_bytes = idx0;
        if (elem_size > 1) {
            int mr = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, idx0,
                                         ir_value_const_int(elem_size, IR_TYPE_I64_T));
            idx_bytes = ir_value_reg(mr, IR_TYPE_I64_T);
        }

        // نستخدم i8* حتى تكون الإزاحة "بالبايت" (delta bytes).
        int ep = ir_builder_emit_ptr_offset(ctx->builder, void_ptr_t, cur_ptr, idx_bytes);
        IRValue* elem_addr_i8 = ir_value_reg(ep, void_ptr_t);

        bool is_last = (i == effective_count - 1);

        if (is_last) {
            IRType* elem_t = (cur_depth > 1) ? void_ptr_t : ir_type_from_datatype(m, cur_base);
            IRType* ptr_to_elem_t = ir_type_ptr(elem_t);
            int cp = ir_builder_emit_cast(ctx->builder, elem_addr_i8, ptr_to_elem_t);
            IRValue* typed_addr = ir_value_reg(cp, ptr_to_elem_t);
            if (out_pointee_type) *out_pointee_type = elem_t;
            if (return_address) {
                return typed_addr;
            }
            int loaded = ir_builder_emit_load(ctx->builder, elem_t, typed_addr);
            return ir_value_reg(loaded, elem_t);
        }

        // ليست آخر فهرسة: نحتاج قيمة قابلة للفهرسة لاحقاً
        if (cur_depth > 1) {
            // العنصر هو مؤشر (نحمّله كـ void* ثم نكمل على عمق أقل)
            IRType* elem_t = void_ptr_t;
            IRType* ptr_to_elem_t = ir_type_ptr(elem_t);
            int cp = ir_builder_emit_cast(ctx->builder, elem_addr_i8, ptr_to_elem_t);
            IRValue* typed_addr = ir_value_reg(cp, ptr_to_elem_t);
            int loaded = ir_builder_emit_load(ctx->builder, elem_t, typed_addr);
            cur_ptr = ir_value_reg(loaded, elem_t);
            cur_ptr = cast_to(ctx, cur_ptr, void_ptr_t);
            cur_depth--;
            continue;
        }

        // cur_depth == 1: العنصر هو نوع الأساس مباشرة
        if (cur_base == TYPE_STRING) {
            // فهرسة ثانية على نص: str[j] → حرف
            IRType* str_t = get_char_ptr_type(m);
            IRType* ptr_to_str_t = ir_type_ptr(str_t);
            int cp0 = ir_builder_emit_cast(ctx->builder, elem_addr_i8, ptr_to_str_t);
            IRValue* str_slot = ir_value_reg(cp0, ptr_to_str_t);
            int ld0 = ir_builder_emit_load(ctx->builder, str_t, str_slot);
            IRValue* str_ptr = ir_value_reg(ld0, str_t);

            int remaining = effective_count - i - 1;
            if (remaining != 1 || !idx_node->next) {
                ir_lower_report_error(ctx, site, "فهرسة متعددة غير مدعومة بعد الوصول إلى 'نص'.");
                return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
            }

            Node* inner_node = idx_node->next;
            IRValue* inner_idx = ensure_i64(ctx, lower_expr(ctx, inner_node));
            int ep2 = ir_builder_emit_ptr_offset(ctx->builder, str_t, str_ptr, inner_idx);
            IRValue* ch_addr = ir_value_reg(ep2, str_t);
            if (out_pointee_type) *out_pointee_type = IR_TYPE_CHAR_T;
            if (return_address) {
                return ch_addr;
            }
            int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, ch_addr);
            return ir_value_reg(ch, IR_TYPE_CHAR_T);
        }

        ir_lower_report_error(ctx, site, "فهرسة متعددة تتطلب مؤشراً/نصاً كعنصر وسيط.");
        return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
    }

    ir_lower_report_error(ctx, site, "عدد الفهارس غير صالح أثناء فهرسة المؤشر.");
    return return_address ? ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T)) : ir_builder_const_i64(0);
}

static IRValue* lower_lvalue_address(IRLowerCtx* ctx, Node* expr, IRType** out_pointee_type)
{
    if (out_pointee_type) *out_pointee_type = IR_TYPE_I64_T;
    if (!ctx || !ctx->builder || !expr) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));

    if (expr->type == NODE_VAR_REF) {
        const char* name = expr->data.var_ref.name;
        IRLowerBinding* b = find_local(ctx, name);
        if (b) {
            IRType* vt = b->value_type ? b->value_type : IR_TYPE_I64_T;
            if (out_pointee_type) *out_pointee_type = vt;
            if (b->is_static_storage) {
                return ir_value_global(ir_lower_binding_storage_name(b), vt);
            }
            return ir_value_reg(b->ptr_reg, ir_type_ptr(vt));
        }

        IRGlobal* g = NULL;
        if (ctx->builder && ctx->builder->module && name) {
            g = ir_module_find_global(ctx->builder->module, name);
        }
        if (g && g->type) {
            if (out_pointee_type) *out_pointee_type = g->type;
            return ir_value_global(name, g->type);
        }

        ir_lower_report_error(ctx, expr, "لا يمكن أخذ عنوان متغير غير معروف '%s'.", name ? name : "???");
        return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
    }

    if (expr->type == NODE_UNARY_OP && expr->data.unary_op.op == UOP_DEREF) {
        IRValue* p = lower_expr(ctx, expr->data.unary_op.operand);
        if (out_pointee_type) {
            IRType* t = ir_type_from_datatype(ctx->builder->module, expr->inferred_type);
            if (!t || t->kind == IR_TYPE_VOID) t = IR_TYPE_I64_T;
            *out_pointee_type = t;
        }
        return p;
    }

    if (expr->type == NODE_MEMBER_ACCESS && expr->data.member_access.is_struct_member) {
        IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
        IRValue* base_ptr = NULL;
        if (expr->data.member_access.root_is_global) {
            base_ptr = ir_value_global(expr->data.member_access.root_var, IR_TYPE_I8_T);
        } else {
            IRLowerBinding* b = find_local(ctx, expr->data.member_access.root_var);
            if (!b || !b->value_type) {
                ir_lower_report_error(ctx, expr, "هيكل محلي غير معروف '%s'.", expr->data.member_access.root_var);
                return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
            }
            if (b->is_static_storage) {
                base_ptr = ir_value_global(ir_lower_binding_storage_name(b), IR_TYPE_I8_T);
            } else {
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
                base_ptr = ir_value_reg(c, ptr_i8_t);
            }
        }
        IRValue* idx = ir_value_const_int((int64_t)expr->data.member_access.member_offset, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
        IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);
        IRType* ft = ir_type_from_datatype(ctx->builder->module, expr->data.member_access.member_type);
        if (!ft || ft->kind == IR_TYPE_VOID) ft = IR_TYPE_I64_T;
        if (out_pointee_type) *out_pointee_type = ft;
        int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, ir_type_ptr(ft));
        return ir_value_reg(fp, ir_type_ptr(ft));
    }

    if (expr->type == NODE_ARRAY_ACCESS) {
        const char* name = expr->data.array_op.name;
        int index_count = expr->data.array_op.index_count;
        if (index_count <= 0) index_count = ir_lower_index_count(expr->data.array_op.indices);

        IRLowerBinding* b = find_local(ctx, name);
        if (b) {
            const char* storage_name = ir_lower_binding_storage_name(b);

            // فهرسة مؤشرات محلية: p[i] و p[i][j]...
            if (b->is_pointer) {
                IRType* ptr_t = b->value_type;
                IRValue* ptr_val = NULL;
                if (b->is_static_storage) {
                    IRValue* gptr = ir_value_global(storage_name, ptr_t);
                    int loaded = ir_builder_emit_load(ctx->builder, ptr_t, gptr);
                    ptr_val = ir_value_reg(loaded, ptr_t);
                } else {
                    IRType* slot_t = ir_type_ptr(ptr_t);
                    IRValue* slot = ir_value_reg(b->ptr_reg, slot_t);
                    int loaded = ir_builder_emit_load(ctx->builder, ptr_t, slot);
                    ptr_val = ir_value_reg(loaded, ptr_t);
                }

                return ir_lower_pointer_index_chain(ctx, expr, ptr_val,
                                                    b->ptr_base_type, b->ptr_base_type_name, b->ptr_depth,
                                                    expr->data.array_op.indices, index_count,
                                                    true, out_pointee_type);
            }

            if (!b->is_array || !b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
                ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
                for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                    (void)lower_expr(ctx, idx);
                }
                return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
            }

            bool elem_is_agg = (b->array_elem_type == TYPE_STRUCT || b->array_elem_type == TYPE_UNION);
            IRType* elem_t = elem_is_agg
                ? IR_TYPE_I8_T
                : (b->value_type->data.array.element ? b->value_type->data.array.element : IR_TYPE_I64_T);
            IRType* elem_ptr_t = ir_type_ptr(elem_t);

            IRValue* base_ptr = NULL;
            if (b->is_static_storage) {
                base_ptr = ir_value_global(storage_name, elem_t);
            } else {
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
                base_ptr = ir_value_reg(base_reg, elem_ptr_t);
            }

            IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                          expr->data.array_op.indices,
                                                          index_count,
                                                          b->array_dims,
                                                          b->array_rank);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
            if (out_pointee_type) *out_pointee_type = elem_t;
            return ir_value_reg(ep, elem_ptr_t);
        }

        IRGlobal* g = NULL;
        if (ctx->builder && ctx->builder->module && name) {
            g = ir_module_find_global(ctx->builder->module, name);
        }
        Node* vdecl = ir_lower_find_global_var_decl(ctx, name);
        if (vdecl && vdecl->type == NODE_VAR_DECL &&
            vdecl->data.var_decl.type == TYPE_POINTER &&
            g && g->type) {
            // فهرسة مؤشرات عامة: نحمّل قيمة المؤشر أولاً من @name ثم نفهرسه بالميتا الدلالية.
            IRValue* gptr = ir_value_global(name, g->type);
            int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
            IRValue* p = ir_value_reg(loaded, g->type);
            return ir_lower_pointer_index_chain(ctx, expr, p,
                                                vdecl->data.var_decl.ptr_base_type,
                                                vdecl->data.var_decl.ptr_base_type_name,
                                                vdecl->data.var_decl.ptr_depth,
                                                expr->data.array_op.indices, index_count,
                                                true, out_pointee_type);
        }
        if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة.", name ? name : "???");
            for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                (void)lower_expr(ctx, idx);
            }
            return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
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

        bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);
        IRType* elem_t = elem_is_agg
            ? IR_TYPE_I8_T
            : (g->type->data.array.element ? g->type->data.array.element : IR_TYPE_I64_T);
        IRType* elem_ptr_t = ir_type_ptr(elem_t);
        IRValue* base_ptr = ir_value_global(name, elem_t);

        IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                      expr->data.array_op.indices,
                                                      index_count,
                                                      dims,
                                                      rank);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
        if (out_pointee_type) *out_pointee_type = elem_t;
        return ir_value_reg(ep, elem_ptr_t);
    }

    ir_lower_report_error(ctx, expr, "أخذ العنوان '&' لهذا التعبير غير مدعوم.");
    return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I64_T));
}

