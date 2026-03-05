static IRValue* ir_lower_builtin_strlen_from_value(IRLowerCtx* ctx, const Node* site, IRValue* str_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRValue* base = ir_lower_as_char_ptr(ctx, str_in);
    if (!base) base = ir_value_const_int(0, char_ptr_t);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    IRBlock* head = cf_create_block(ctx, "طول_نص_تحقق");
    IRBlock* body = cf_create_block(ctx, "طول_نص_جسم");
    IRBlock* done = cf_create_block(ctx, "طول_نص_نهاية");
    if (!head || !body || !done) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);

    IRValue* ch = ir_lower_load_char_at(ctx, base, idx);
    IRValue* ch_i64 = ensure_i64(ctx, ch);
    int is_end_r = ir_builder_emit_cmp_eq(b, ch_i64, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(is_end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int len_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    return ir_value_reg(len_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_strcmp_from_values(IRLowerCtx* ctx, const Node* site, IRValue* a_in, IRValue* b_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRValue* a = ir_lower_as_char_ptr(ctx, a_in);
    IRValue* c = ir_lower_as_char_ptr(ctx, b_in);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    int res_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), res_ptr);

    IRBlock* head = cf_create_block(ctx, "قارن_نص_تحقق");
    IRBlock* same = cf_create_block(ctx, "قارن_نص_متساوي");
    IRBlock* diff = cf_create_block(ctx, "قارن_نص_مختلف");
    IRBlock* step = cf_create_block(ctx, "قارن_نص_زيادة");
    IRBlock* done = cf_create_block(ctx, "قارن_نص_نهاية");
    if (!head || !same || !diff || !step || !done) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);

    IRValue* ach = ir_lower_load_char_at(ctx, a, idx);
    IRValue* bch = ir_lower_load_char_at(ctx, c, idx);
    IRValue* ai = ensure_i64(ctx, ach);
    IRValue* bi = ensure_i64(ctx, bch);

    int eq_r = ir_builder_emit_cmp_eq(b, ai, bi);
    IRValue* eq = ir_value_reg(eq_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, eq, same, diff);

    ir_builder_set_insert_point(b, same);
    ir_lower_set_loc(b, site);
    int end_r = ir_builder_emit_cmp_eq(b, ai, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, step);

    ir_builder_set_insert_point(b, step);
    ir_lower_set_loc(b, site);
    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, diff);
    ir_lower_set_loc(b, site);
    int delta_r = ir_builder_emit_sub(b, IR_TYPE_I64_T, ai, bi);
    IRValue* delta = ir_value_reg(delta_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, delta, res_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I64_T, res_ptr);
    return ir_value_reg(out_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_copy_from_value(IRLowerCtx* ctx, const Node* site, IRValue* src_in)
{
    if (!ctx || !ctx->builder) {
        return ir_value_const_int(0, get_char_ptr_type(NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* src = ir_lower_as_char_ptr(ctx, src_in);
    IRValue* len = ir_lower_builtin_strlen_from_value(ctx, site, src);

    int plus1_r = ir_builder_emit_add(b, IR_TYPE_I64_T, len, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* total = ir_value_reg(plus1_r, IR_TYPE_I64_T);
    int bytes_r = ir_builder_emit_mul(b, IR_TYPE_I64_T, total, ir_value_const_int(8, IR_TYPE_I64_T));
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* margs[1] = { bytes };
    int raw_reg = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    if (raw_reg < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc.");
        return ir_value_const_int(0, char_ptr_t);
    }
    IRValue* raw_ptr = ir_value_reg(raw_reg, i8_ptr_t);
    int dst_cast_r = ir_builder_emit_cast(b, raw_ptr, char_ptr_t);
    IRValue* dst = ir_value_reg(dst_cast_r, char_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    IRValue* null_char_ptr = ir_value_const_int(0, char_ptr_t);
    ir_builder_emit_store(b, null_char_ptr, res_ptr);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    int is_null_r = ir_builder_emit_cmp_eq(b, dst, null_char_ptr);
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* init = cf_create_block(ctx, "نسخ_نص_تهيئة");
    IRBlock* head = cf_create_block(ctx, "نسخ_نص_تحقق");
    IRBlock* body = cf_create_block(ctx, "نسخ_نص_جسم");
    IRBlock* done = cf_create_block(ctx, "نسخ_نص_نهاية");
    if (!init || !head || !body || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, init);
    }

    ir_builder_set_insert_point(b, init);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, dst, res_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);
    IRValue* ch = ir_lower_load_char_at(ctx, src, idx);
    ir_lower_store_char_at(ctx, dst, idx, ch);
    IRValue* chi = ensure_i64(ctx, ch);
    int is_end_r = ir_builder_emit_cmp_eq(b, chi, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(is_end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    int inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* inc = ir_value_reg(inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(out_r, char_ptr_t);
}

static IRValue* ir_lower_builtin_concat_from_values(IRLowerCtx* ctx, const Node* site, IRValue* a_in, IRValue* b_in)
{
    if (!ctx || !ctx->builder) {
        return ir_value_const_int(0, get_char_ptr_type(NULL));
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);

    IRValue* a = ir_lower_as_char_ptr(ctx, a_in);
    IRValue* c = ir_lower_as_char_ptr(ctx, b_in);

    IRValue* len_a = ir_lower_builtin_strlen_from_value(ctx, site, a);
    IRValue* len_b = ir_lower_builtin_strlen_from_value(ctx, site, c);

    int sum_r = ir_builder_emit_add(b, IR_TYPE_I64_T, len_a, len_b);
    IRValue* sum = ir_value_reg(sum_r, IR_TYPE_I64_T);
    int total_r = ir_builder_emit_add(b, IR_TYPE_I64_T, sum, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* total = ir_value_reg(total_r, IR_TYPE_I64_T);
    int bytes_r = ir_builder_emit_mul(b, IR_TYPE_I64_T, total, ir_value_const_int(8, IR_TYPE_I64_T));
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* margs[1] = { bytes };
    int raw_reg = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    if (raw_reg < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc.");
        return ir_value_const_int(0, char_ptr_t);
    }
    IRValue* raw_ptr = ir_value_reg(raw_reg, i8_ptr_t);
    int dst_cast_r = ir_builder_emit_cast(b, raw_ptr, char_ptr_t);
    IRValue* dst = ir_value_reg(dst_cast_r, char_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    IRValue* null_char_ptr = ir_value_const_int(0, char_ptr_t);
    ir_builder_emit_store(b, null_char_ptr, res_ptr);

    int idx_a_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_a_ptr = ir_value_reg(idx_a_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_a_ptr);

    int idx_b_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_b_ptr = ir_value_reg(idx_b_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_b_ptr);

    int is_null_r = ir_builder_emit_cmp_eq(b, dst, null_char_ptr);
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* init = cf_create_block(ctx, "دمج_نص_تهيئة");
    IRBlock* a_head = cf_create_block(ctx, "دمج_نص_أ_تحقق");
    IRBlock* a_body = cf_create_block(ctx, "دمج_نص_أ_جسم");
    IRBlock* a_done = cf_create_block(ctx, "دمج_نص_أ_نهاية");
    IRBlock* b_head = cf_create_block(ctx, "دمج_نص_ب_تحقق");
    IRBlock* b_body = cf_create_block(ctx, "دمج_نص_ب_جسم");
    IRBlock* done = cf_create_block(ctx, "دمج_نص_نهاية");
    if (!init || !a_head || !a_body || !a_done || !b_head || !b_body || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, init);
    }

    ir_builder_set_insert_point(b, init);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, dst, res_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_a_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_b_ptr);
    ir_builder_emit_br(b, a_head);

    ir_builder_set_insert_point(b, a_head);
    ir_lower_set_loc(b, site);
    int ia_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_a_ptr);
    IRValue* ia = ir_value_reg(ia_r, IR_TYPE_I64_T);
    int cond_a_r = ir_builder_emit_cmp_lt(b, ia, len_a);
    IRValue* cond_a = ir_value_reg(cond_a_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cond_a, a_body, a_done);

    ir_builder_set_insert_point(b, a_body);
    ir_lower_set_loc(b, site);
    IRValue* cha = ir_lower_load_char_at(ctx, a, ia);
    ir_lower_store_char_at(ctx, dst, ia, cha);
    int ia_inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, ia, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* ia_inc = ir_value_reg(ia_inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ia_inc, idx_a_ptr);
    ir_builder_emit_br(b, a_head);

    ir_builder_set_insert_point(b, a_done);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_b_ptr);
    ir_builder_emit_br(b, b_head);

    ir_builder_set_insert_point(b, b_head);
    ir_lower_set_loc(b, site);
    int ib_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_b_ptr);
    IRValue* ib = ir_value_reg(ib_r, IR_TYPE_I64_T);
    int dst_off_r = ir_builder_emit_add(b, IR_TYPE_I64_T, len_a, ib);
    IRValue* dst_off = ir_value_reg(dst_off_r, IR_TYPE_I64_T);
    IRValue* chb = ir_lower_load_char_at(ctx, c, ib);
    ir_lower_store_char_at(ctx, dst, dst_off, chb);
    IRValue* chb_i64 = ensure_i64(ctx, chb);
    int end_b_r = ir_builder_emit_cmp_eq(b, chb_i64, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* end_b = ir_value_reg(end_b_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, end_b, done, b_body);

    ir_builder_set_insert_point(b, b_body);
    ir_lower_set_loc(b, site);
    int ib_inc_r = ir_builder_emit_add(b, IR_TYPE_I64_T, ib, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* ib_inc = ir_value_reg(ib_inc_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ib_inc, idx_b_ptr);
    ir_builder_emit_br(b, b_head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(out_r, char_ptr_t);
}

static IRValue* ir_lower_builtin_free_value(IRLowerCtx* ctx, const Node* site, IRValue* v)
{
    if (!ctx || !ctx->builder) return ir_builder_const_i64(0);
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* as_char_ptr = ir_lower_as_char_ptr(ctx, v);
    IRValue* as_i8 = cast_to(ctx, as_char_ptr, i8_ptr_t);
    IRValue* args[1] = { as_i8 };
    ir_lower_set_loc(ctx->builder, site);
    ir_builder_emit_call_void(ctx->builder, "free", args, 1);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_read_line_from_file(IRLowerCtx* ctx, const Node* site, IRValue* file_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, get_char_ptr_type(NULL));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* i32_ptr_t = ir_type_ptr(IR_TYPE_I32_T);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);

    IRValue* nulls = ir_value_const_int(0, char_ptr_t);
    IRValue* file = cast_to(ctx, file_in, i8_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, char_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, char_ptr_ptr_t);
    ir_builder_emit_store(b, nulls, res_ptr);

    int buf_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), buf_ptr);

    int len_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int cap_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* len_ptr = ir_value_reg(len_ptr_reg, i64_ptr_t);
    IRValue* cap_ptr = ir_value_reg(cap_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len_ptr);
    ir_builder_emit_store(b, ir_value_const_int(256, IR_TYPE_I64_T), cap_ptr);

    IRValue* cap0 = ir_value_const_int(256, IR_TYPE_I64_T);
    IRValue* margs0[1] = { cap0 };
    ir_lower_set_loc(b, site);
    int buf0_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs0, 1);
    if (buf0_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc في اقرأ_سطر.");
        return nulls;
    }
    IRValue* buf0 = ir_value_reg(buf0_r, i8_ptr_t);
    ir_builder_emit_store(b, buf0, buf_ptr);

    int last_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I32_T);
    IRValue* last_ptr = ir_value_reg(last_ptr_reg, i32_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I32_T), last_ptr);

    IRBlock* head = cf_create_block(ctx, "اقرأ_سطر_تحقق");
    IRBlock* body = cf_create_block(ctx, "اقرأ_سطر_جسم");
    IRBlock* grow = cf_create_block(ctx, "اقرأ_سطر_توسيع");
    IRBlock* after_grow = cf_create_block(ctx, "اقرأ_سطر_بعد_توسيع");
    IRBlock* done_read = cf_create_block(ctx, "اقرأ_سطر_بعد_قراءة");
    IRBlock* eofret = cf_create_block(ctx, "اقرأ_سطر_EOF");
    IRBlock* parse = cf_create_block(ctx, "اقرأ_سطر_تحويل");
    IRBlock* final_done = cf_create_block(ctx, "اقرأ_سطر_نهاية");
    if (!head || !body || !grow || !after_grow || !done_read || !eofret || !parse || !final_done) {
        return nulls;
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* fargs[1] = { file };
    int ch_r = ir_builder_emit_call(b, "fgetc", IR_TYPE_I32_T, fargs, 1);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_I32_T);
    ir_builder_emit_store(b, ch, last_ptr);
    int is_eof_r = ir_builder_emit_cmp_eq(b, ch, ir_value_const_int(-1, IR_TYPE_I32_T));
    int is_nl_r = ir_builder_emit_cmp_eq(b, ch, ir_value_const_int(10, IR_TYPE_I32_T));
    IRValue* stop = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I1_T,
                                                   ir_value_reg(is_eof_r, IR_TYPE_I1_T),
                                                   ir_value_reg(is_nl_r, IR_TYPE_I1_T)),
                                 IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, stop, done_read, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    IRValue* lenv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* capv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, cap_ptr), IR_TYPE_I64_T);
    IRValue* len1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, lenv, ir_value_const_int(1, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    IRValue* need_grow = ir_value_reg(ir_builder_emit_cmp_ge(b, len1, capv), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, need_grow, grow, after_grow);

    ir_builder_set_insert_point(b, grow);
    ir_lower_set_loc(b, site);
    IRValue* cap2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, capv, ir_value_const_int(2, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    IRValue* old_buf = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* rargs[2] = { old_buf, cap2 };
    int new_buf_r = ir_builder_emit_call(b, "realloc", i8_ptr_t, rargs, 2);
    if (new_buf_r < 0) {
        IRValue* f0[1] = { old_buf };
        ir_builder_emit_call_void(b, "free", f0, 1);
        ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), buf_ptr);
        ir_builder_emit_store(b, ir_value_const_int(-1, IR_TYPE_I32_T), last_ptr);
        ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), len_ptr);
        ir_builder_emit_br(b, done_read);
    }
    ir_builder_emit_store(b, ir_value_reg(new_buf_r, i8_ptr_t), buf_ptr);
    ir_builder_emit_store(b, cap2, cap_ptr);
    ir_builder_emit_br(b, after_grow);

    ir_builder_set_insert_point(b, after_grow);
    ir_lower_set_loc(b, site);
    IRValue* buf = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    int dstp = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, lenv);
    ir_builder_emit_store(b, cast_to(ctx, ch, IR_TYPE_I8_T), ir_value_reg(dstp, i8_ptr_t));
    ir_builder_emit_store(b, len1, len_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done_read);
    ir_lower_set_loc(b, site);
    IRValue* lastv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I32_T, last_ptr), IR_TYPE_I32_T);
    IRValue* len2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, len_ptr), IR_TYPE_I64_T);
    IRValue* eof_empty = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T,
                                                        ir_value_reg(ir_builder_emit_cmp_eq(b, lastv, ir_value_const_int(-1, IR_TYPE_I32_T)), IR_TYPE_I1_T),
                                                        ir_value_reg(ir_builder_emit_cmp_eq(b, len2, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T)),
                                      IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, eof_empty, eofret, parse);

    ir_builder_set_insert_point(b, eofret);
    ir_lower_set_loc(b, site);
    IRValue* bf = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    IRValue* fbf[1] = { bf };
    ir_builder_emit_call_void(b, "free", fbf, 1);
    ir_builder_emit_store(b, nulls, res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, parse);
    ir_lower_set_loc(b, site);
    IRValue* buf2 = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, buf_ptr), i8_ptr_t);
    int nul_p = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, len2);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), ir_value_reg(nul_p, i8_ptr_t));

    IRValue* chars_count = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len2, ir_value_const_int(1, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
    IRValue* chars_bytes = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, chars_count, ir_value_const_int(8, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
    IRValue* cargs[1] = { chars_bytes };
    int raw_chars_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, cargs, 1);
    if (raw_chars_r < 0) {
        IRValue* f1[1] = { buf2 };
        ir_builder_emit_call_void(b, "free", f1, 1);
        ir_builder_emit_store(b, nulls, res_ptr);
        ir_builder_emit_br(b, final_done);
    }
    IRValue* chars = cast_to(ctx, ir_value_reg(raw_chars_r, i8_ptr_t), char_ptr_t);

    int i_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int j_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int bits_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* i_ptr = ir_value_reg(i_ptr_reg, i64_ptr_t);
    IRValue* j_ptr = ir_value_reg(j_ptr_reg, i64_ptr_t);
    IRValue* bits_ptr = ir_value_reg(bits_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), i_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), j_ptr);

    uint64_t repl_bits_u = pack_utf8_codepoint(0xFFFDu);
    IRValue* repl_bits = ir_value_const_int((int64_t)repl_bits_u, IR_TYPE_I64_T);

    int next_i_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* next_i_ptr = ir_value_reg(next_i_ptr_reg, i64_ptr_t);

    IRBlock* p_head = cf_create_block(ctx, "اقرأ_سطر_تفكيك_تحقق");
    IRBlock* p_disp = cf_create_block(ctx, "اقرأ_سطر_تفكيك_توزيع");
    IRBlock* p_d2 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د2");
    IRBlock* p_d3 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د3");
    IRBlock* p_d4 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د4");
    IRBlock* p_d5 = cf_create_block(ctx, "اقرأ_سطر_تفكيك_د5");
    IRBlock* c1 = cf_create_block(ctx, "اقرأ_سطر_حالة1");
    IRBlock* c2 = cf_create_block(ctx, "اقرأ_سطر_حالة2");
    IRBlock* c3 = cf_create_block(ctx, "اقرأ_سطر_حالة3");
    IRBlock* c4 = cf_create_block(ctx, "اقرأ_سطر_حالة4");
    IRBlock* inv = cf_create_block(ctx, "اقرأ_سطر_غير_صالح");
    IRBlock* p_store = cf_create_block(ctx, "اقرأ_سطر_تخزين");
    IRBlock* p_done = cf_create_block(ctx, "اقرأ_سطر_تفكيك_نهاية");
    if (!p_head || !p_disp || !p_d2 || !p_d3 || !p_d4 || !p_d5 ||
        !c1 || !c2 || !c3 || !c4 || !inv || !p_store || !p_done) {
        IRValue* f2[1] = { buf2 };
        ir_builder_emit_call_void(b, "free", f2, 1);
        ir_builder_emit_store(b, chars, res_ptr);
        ir_builder_emit_br(b, final_done);
    }

    ir_builder_emit_br(b, p_head);

    ir_builder_set_insert_point(b, p_head);
    ir_lower_set_loc(b, site);
    IRValue* i = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, i_ptr), IR_TYPE_I64_T);
    IRValue* cont = ir_value_reg(ir_builder_emit_cmp_lt(b, i, len2), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cont, p_disp, p_done);

    ir_builder_set_insert_point(b, p_disp);
    ir_lower_set_loc(b, site);
    int bp0 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i);
    IRValue* b0s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp0, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b0u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b0s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);

    // len0 = 1/2/3/4 حسب البايت الأول (UTF-8)
    IRValue* m80 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0x80, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* mE0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* mF0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* mF8 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* is1 = ir_value_reg(ir_builder_emit_cmp_eq(b, m80, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* is2 = ir_value_reg(ir_builder_emit_cmp_eq(b, mE0, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* is3 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF0, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* is4 = ir_value_reg(ir_builder_emit_cmp_eq(b, mF8, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* l1 = cast_i1_to_i64(ctx, is1);
    IRValue* l2 = cast_i1_to_i64(ctx, is2);
    IRValue* l3 = cast_i1_to_i64(ctx, is3);
    IRValue* l4 = cast_i1_to_i64(ctx, is4);
    IRValue* t2 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l2, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* t3 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l3, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* t4 = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, l4, ir_value_const_int(4, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* len0 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, l1, t2), IR_TYPE_I64_T),
                                                    ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, t3, t4), IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);

    int b0z = ir_builder_emit_cmp_eq(b, b0u, ir_value_const_int(0, IR_TYPE_I64_T));
    int oklead = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                    ir_value_reg(ir_builder_emit_not(b, IR_TYPE_I1_T, ir_value_reg(b0z, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                    ir_value_reg(ir_builder_emit_cmp_gt(b, len0, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T));
    ir_builder_emit_br_cond(b, ir_value_reg(oklead, IR_TYPE_I1_T), p_d2, inv);

    ir_builder_set_insert_point(b, p_d2);
    ir_lower_set_loc(b, site);
    int is1r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(1, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is1r, IR_TYPE_I1_T), c1, p_d3);

    ir_builder_set_insert_point(b, p_d3);
    ir_lower_set_loc(b, site);
    int is2r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(2, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is2r, IR_TYPE_I1_T), c2, p_d4);

    ir_builder_set_insert_point(b, p_d4);
    ir_lower_set_loc(b, site);
    int is3r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(3, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is3r, IR_TYPE_I1_T), c3, p_d5);

    ir_builder_set_insert_point(b, p_d5);
    ir_lower_set_loc(b, site);
    int is4r = ir_builder_emit_cmp_eq(b, len0, ir_value_const_int(4, IR_TYPE_I64_T));
    ir_builder_emit_br_cond(b, ir_value_reg(is4r, IR_TYPE_I1_T), c4, inv);

    // حالة 1: بايت واحد
    ir_builder_set_insert_point(b, c1);
    ir_lower_set_loc(b, site);
    IRValue* packed1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, b0u, ir_value_const_int(4294967296LL, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed1, bits_ptr);
    ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T), next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // helpers لقراءة بايت + تحويله لـ i64 (0..255)
    // حالة2
    ir_builder_set_insert_point(b, c2);
    ir_lower_set_loc(b, site);
    IRValue* i2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int in2 = ir_builder_emit_cmp_le(b, i2, len2);
    IRBlock* c2ok = cf_create_block(ctx, "اقرأ_سطر_حالة2_ok");
    if (!c2ok) c2ok = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(in2, IR_TYPE_I1_T), c2ok, inv);
    ir_builder_set_insert_point(b, c2ok);
    ir_lower_set_loc(b, site);
    IRValue* i1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp1 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i1);
    IRValue* b1s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp1, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b1u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b1s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b1m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b1ok = ir_builder_emit_cmp_eq(b, b1m, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b1nz = ir_builder_emit_cmp_ne(b, b1u, ir_value_const_int(0, IR_TYPE_I64_T));
    int ok2 = ir_builder_emit_and(b, IR_TYPE_I1_T,
                                 ir_value_reg(b1ok, IR_TYPE_I1_T),
                                 ir_value_reg(b1nz, IR_TYPE_I1_T));
    IRBlock* c2pack = cf_create_block(ctx, "اقرأ_سطر_حالة2_حزم");
    if (!c2pack) c2pack = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(ok2, IR_TYPE_I1_T), c2pack, inv);
    ir_builder_set_insert_point(b, c2pack);
    ir_lower_set_loc(b, site);
    IRValue* b1sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bytes2 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh), IR_TYPE_I64_T);
    IRValue* l2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, ir_value_const_int(2, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* packed2 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes2, l2sh), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed2, bits_ptr);
    ir_builder_emit_store(b, i2, next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // حالة3
    ir_builder_set_insert_point(b, c3);
    ir_lower_set_loc(b, site);
    IRValue* i3 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int in3 = ir_builder_emit_cmp_le(b, i3, len2);
    IRBlock* c3ok = cf_create_block(ctx, "اقرأ_سطر_حالة3_ok");
    if (!c3ok) c3ok = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(in3, IR_TYPE_I1_T), c3ok, inv);
    ir_builder_set_insert_point(b, c3ok);
    ir_lower_set_loc(b, site);
    IRValue* i1b = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp1b = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i1b);
    IRValue* b1s3 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp1b, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b1u3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_cast(b, b1s3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                    ir_value_const_int(255, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b1m3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b1ok3 = ir_builder_emit_cmp_eq(b, b1m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b1nz3 = ir_builder_emit_cmp_ne(b, b1u3, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* i2b = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp2 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i2b);
    IRValue* b2s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp2, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b2u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b2s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b2m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b2ok = ir_builder_emit_cmp_eq(b, b2m, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b2nz = ir_builder_emit_cmp_ne(b, b2u, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* okb1 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok3, IR_TYPE_I1_T), ir_value_reg(b1nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    IRValue* okb2 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b2ok, IR_TYPE_I1_T), ir_value_reg(b2nz, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    int ok3 = ir_builder_emit_and(b, IR_TYPE_I1_T, okb1, okb2);
    IRBlock* c3pack = cf_create_block(ctx, "اقرأ_سطر_حالة3_حزم");
    if (!c3pack) c3pack = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(ok3, IR_TYPE_I1_T), c3pack, inv);
    ir_builder_set_insert_point(b, c3pack);
    ir_lower_set_loc(b, site);
    IRValue* b1sh3 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u3, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b2sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bytes3 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                     ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh3), IR_TYPE_I64_T),
                                                     b2sh),
                                   IR_TYPE_I64_T);
    IRValue* l3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, ir_value_const_int(3, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* packed3 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes3, l3sh), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed3, bits_ptr);
    ir_builder_emit_store(b, i3, next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // حالة4 (مشابهة، محافظة)
    ir_builder_set_insert_point(b, c4);
    ir_lower_set_loc(b, site);
    IRValue* i4 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(4, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int in4 = ir_builder_emit_cmp_le(b, i4, len2);
    IRBlock* c4ok = cf_create_block(ctx, "اقرأ_سطر_حالة4_ok");
    if (!c4ok) c4ok = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(in4, IR_TYPE_I1_T), c4ok, inv);
    ir_builder_set_insert_point(b, c4ok);
    ir_lower_set_loc(b, site);
    IRValue* i1c = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp1c = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i1c);
    IRValue* b1s4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp1c, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b1u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_cast(b, b1s4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                    ir_value_const_int(255, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b1m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b1ok4 = ir_builder_emit_cmp_eq(b, b1m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b1nz4 = ir_builder_emit_cmp_ne(b, b1u4, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* i2c = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp2c = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i2c);
    IRValue* b2s4 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp2c, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b2u4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                    ir_value_reg(ir_builder_emit_cast(b, b2s4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                    ir_value_const_int(255, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b2m4 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b2ok4 = ir_builder_emit_cmp_eq(b, b2m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b2nz4 = ir_builder_emit_cmp_ne(b, b2u4, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* i3b = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int bp3 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf2, i3b);
    IRValue* b3s = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(bp3, i8_ptr_t)), IR_TYPE_I8_T);
    IRValue* b3u = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T,
                                                   ir_value_reg(ir_builder_emit_cast(b, b3s, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b3m = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b3u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    int b3ok = ir_builder_emit_cmp_eq(b, b3m, ir_value_const_int(0x80, IR_TYPE_I64_T));
    int b3nz = ir_builder_emit_cmp_ne(b, b3u, ir_value_const_int(0, IR_TYPE_I64_T));

    IRValue* okb14 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b1ok4, IR_TYPE_I1_T), ir_value_reg(b1nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    IRValue* okb24 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b2ok4, IR_TYPE_I1_T), ir_value_reg(b2nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    IRValue* okb34 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(b3ok, IR_TYPE_I1_T), ir_value_reg(b3nz, IR_TYPE_I1_T)), IR_TYPE_I1_T);
    int ok12 = ir_builder_emit_and(b, IR_TYPE_I1_T, okb14, okb24);
    int ok4 = ir_builder_emit_and(b, IR_TYPE_I1_T, ir_value_reg(ok12, IR_TYPE_I1_T), okb34);
    IRBlock* c4pack = cf_create_block(ctx, "اقرأ_سطر_حالة4_حزم");
    if (!c4pack) c4pack = inv;
    ir_builder_emit_br_cond(b, ir_value_reg(ok4, IR_TYPE_I1_T), c4pack, inv);
    ir_builder_set_insert_point(b, c4pack);
    ir_lower_set_loc(b, site);
    IRValue* b3sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b3u, ir_value_const_int(24, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b1sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b1u4, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b2sh4 = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, b2u4, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* bytes4 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                     ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T,
                                                                                    ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, b0u, b1sh4), IR_TYPE_I64_T),
                                                                                    b2sh4),
                                                                  IR_TYPE_I64_T),
                                                     b3sh),
                                   IR_TYPE_I64_T);
    IRValue* l4sh = ir_value_reg(ir_builder_emit_shl(b, IR_TYPE_I64_T, ir_value_const_int(4, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* packed4 = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I64_T, bytes4, l4sh), IR_TYPE_I64_T);
    ir_builder_emit_store(b, packed4, bits_ptr);
    ir_builder_emit_store(b, i4, next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // invalid: replacement + advance 1
    ir_builder_set_insert_point(b, inv);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, repl_bits, bits_ptr);
    ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T), next_i_ptr);
    ir_builder_emit_br(b, p_store);

    // store: chars[j]=bits; j++; i=next_i
    ir_builder_set_insert_point(b, p_store);
    ir_lower_set_loc(b, site);
    IRValue* j = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, j_ptr), IR_TYPE_I64_T);
    IRValue* bitsv = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, bits_ptr), IR_TYPE_I64_T);
    int epc = ir_builder_emit_ptr_offset(b, char_ptr_t, chars, j);
    ir_builder_emit_store(b, ir_lower_i64_bits_to_char(ctx, bitsv), ir_value_reg(epc, char_ptr_t));
    ir_builder_emit_store(b, ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, j, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T), j_ptr);
    IRValue* ni = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, next_i_ptr), IR_TYPE_I64_T);
    ir_builder_emit_store(b, ni, i_ptr);
    ir_builder_emit_br(b, p_head);

    ir_builder_set_insert_point(b, p_done);
    ir_lower_set_loc(b, site);
    IRValue* j2 = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, j_ptr), IR_TYPE_I64_T);
    int ept = ir_builder_emit_ptr_offset(b, char_ptr_t, chars, j2);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_CHAR_T), ir_value_reg(ept, char_ptr_t));
    IRValue* f2[1] = { buf2 };
    ir_builder_emit_call_void(b, "free", f2, 1);
    ir_builder_emit_store(b, chars, res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, final_done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, char_ptr_t, res_ptr);
    return ir_value_reg(out_r, char_ptr_t);
}

