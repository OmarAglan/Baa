static IRValue* ir_lower_as_i8_ptr(IRLowerCtx* ctx, IRValue* v)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!v) return ir_value_const_int(0, i8_ptr_t);
    if (v->type && ir_types_equal(v->type, i8_ptr_t)) return v;
    int cr = ir_builder_emit_cast(ctx->builder, v, i8_ptr_t);
    return ir_value_reg(cr, i8_ptr_t);
}

static IRValue* ir_lower_load_raw_byte_i64(IRLowerCtx* ctx, IRValue* base, IRValue* idx)
{
    if (!ctx || !ctx->builder || !base || !idx) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* base_i8 = ir_lower_as_i8_ptr(ctx, base);
    IRValue* i64_idx = ensure_i64(ctx, idx);
    int ep = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_i8, i64_idx);
    IRValue* elem_ptr = ir_value_reg(ep, i8_ptr_t);
    int lr = ir_builder_emit_load(b, IR_TYPE_I8_T, elem_ptr);
    IRValue* raw = ir_value_reg(lr, IR_TYPE_I8_T);
    int wide_r = ir_builder_emit_cast(b, raw, IR_TYPE_I64_T);
    IRValue* wide = ir_value_reg(wide_r, IR_TYPE_I64_T);
    int masked_r = ir_builder_emit_and(b, IR_TYPE_I64_T, wide, ir_value_const_int(255, IR_TYPE_I64_T));
    return ir_value_reg(masked_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_raw_len_from_value(IRLowerCtx* ctx, const Node* site, IRValue* raw_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRValue* raw = ir_lower_as_i8_ptr(ctx, raw_in);
    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRType* idx_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, idx_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    IRBlock* head = cf_create_block(ctx, "طول_خام_تحقق");
    IRBlock* body = cf_create_block(ctx, "طول_خام_جسم");
    IRBlock* done = cf_create_block(ctx, "طول_خام_نهاية");
    if (!head || !body || !done) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr), IR_TYPE_I64_T);
    IRValue* byte = ir_lower_load_raw_byte_i64(ctx, raw, idx);
    IRValue* is_end = ir_value_reg(ir_builder_emit_cmp_eq(b, byte, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, body);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    IRValue* inc = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int len_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    return ir_value_reg(len_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_raw_cmp_from_values(IRLowerCtx* ctx, const Node* site, IRValue* a_in, IRValue* b_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRValue* a = ir_lower_as_i8_ptr(ctx, a_in);
    IRValue* c = ir_lower_as_i8_ptr(ctx, b_in);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    int res_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), res_ptr);

    IRBlock* head = cf_create_block(ctx, "قارن_خام_تحقق");
    IRBlock* same = cf_create_block(ctx, "قارن_خام_متساوي");
    IRBlock* diff = cf_create_block(ctx, "قارن_خام_مختلف");
    IRBlock* step = cf_create_block(ctx, "قارن_خام_زيادة");
    IRBlock* done = cf_create_block(ctx, "قارن_خام_نهاية");
    if (!head || !same || !diff || !step || !done) {
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br(b, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr), IR_TYPE_I64_T);
    IRValue* ab = ir_lower_load_raw_byte_i64(ctx, a, idx);
    IRValue* cb = ir_lower_load_raw_byte_i64(ctx, c, idx);
    IRValue* eq = ir_value_reg(ir_builder_emit_cmp_eq(b, ab, cb), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, eq, same, diff);

    ir_builder_set_insert_point(b, same);
    ir_lower_set_loc(b, site);
    IRValue* is_end = ir_value_reg(ir_builder_emit_cmp_eq(b, ab, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, done, step);

    ir_builder_set_insert_point(b, step);
    ir_lower_set_loc(b, site);
    IRValue* inc = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, diff);
    ir_lower_set_loc(b, site);
    IRValue* delta = ir_value_reg(ir_builder_emit_sub(b, IR_TYPE_I64_T, ab, cb), IR_TYPE_I64_T);
    ir_builder_emit_store(b, delta, res_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I64_T, res_ptr);
    return ir_value_reg(out_r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_raw_eq_len_from_values(IRLowerCtx* ctx, const Node* site,
                                                        IRValue* a_in, IRValue* len_in, IRValue* b_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I1_T);

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRValue* a = ir_lower_as_i8_ptr(ctx, a_in);
    IRValue* c = ir_lower_as_i8_ptr(ctx, b_in);
    IRValue* len = ensure_i64(ctx, len_in);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* i1_ptr_t = ir_type_ptr(IR_TYPE_I1_T);

    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);

    int res_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I1_T);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, i1_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I1_T), res_ptr);

    IRBlock* head = cf_create_block(ctx, "قارن_خام_بطول_تحقق");
    IRBlock* cmp = cf_create_block(ctx, "قارن_خام_بطول_قارن");
    IRBlock* lit_end = cf_create_block(ctx, "قارن_خام_بطول_نهاية_القالب");
    IRBlock* step = cf_create_block(ctx, "قارن_خام_بطول_زيادة");
    IRBlock* fail = cf_create_block(ctx, "قارن_خام_بطول_فشل");
    IRBlock* done = cf_create_block(ctx, "قارن_خام_بطول_نهاية");
    if (!head || !cmp || !lit_end || !step || !fail || !done) {
        return ir_value_const_int(0, IR_TYPE_I1_T);
    }

    IRValue* len_neg = ir_value_reg(ir_builder_emit_cmp_lt(b, len, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, len_neg, fail, head);
    }

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* idx = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr), IR_TYPE_I64_T);
    IRValue* at_len = ir_value_reg(ir_builder_emit_cmp_eq(b, idx, len), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, at_len, lit_end, cmp);

    ir_builder_set_insert_point(b, cmp);
    ir_lower_set_loc(b, site);
    IRValue* ab = ir_lower_load_raw_byte_i64(ctx, a, idx);
    IRValue* cb = ir_lower_load_raw_byte_i64(ctx, c, idx);
    IRValue* eq = ir_value_reg(ir_builder_emit_cmp_eq(b, ab, cb), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, eq, step, fail);

    ir_builder_set_insert_point(b, step);
    ir_lower_set_loc(b, site);
    IRValue* inc = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T)), IR_TYPE_I64_T);
    ir_builder_emit_store(b, inc, idx_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, lit_end);
    ir_lower_set_loc(b, site);
    IRValue* end_byte = ir_lower_load_raw_byte_i64(ctx, c, idx);
    IRValue* is_end = ir_value_reg(ir_builder_emit_cmp_eq(b, end_byte, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    ir_builder_emit_store(b, is_end, res_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, fail);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I1_T), res_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I1_T, res_ptr);
    return ir_value_reg(out_r, IR_TYPE_I1_T);
}

