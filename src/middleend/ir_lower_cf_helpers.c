// ============================================================================
// Control-flow helpers (v0.3.0.5)
// ============================================================================

static int next_label_id(IRLowerCtx* ctx) {
    if (!ctx) return 0;
    return ctx->label_counter++;
}

static IRBlock* cf_create_block(IRLowerCtx* ctx, const char* base_label) {
    if (!ctx || !ctx->builder || !base_label) return NULL;

    char label[128];
    snprintf(label, sizeof(label), "%s_%d", base_label, next_label_id(ctx));
    return ir_builder_create_block(ctx->builder, label);
}

static void cf_push(IRLowerCtx* ctx, IRBlock* break_target, IRBlock* continue_target) {
    if (!ctx) return;
    if (ctx->cf_depth >= (int)(sizeof(ctx->break_targets) / sizeof(ctx->break_targets[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد مكدس التحكم (break/continue).");
        return;
    }
    ctx->break_targets[ctx->cf_depth] = break_target;
    ctx->continue_targets[ctx->cf_depth] = continue_target;
    ctx->cf_depth++;
}

static void cf_pop(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->cf_depth <= 0) {
        ir_lower_report_error(ctx, NULL, "نقص في مكدس التحكم (break/continue).");
        return;
    }
    ctx->cf_depth--;
}

static IRBlock* cf_current_break(IRLowerCtx* ctx) {
    if (!ctx || ctx->cf_depth <= 0) return NULL;
    return ctx->break_targets[ctx->cf_depth - 1];
}

static IRBlock* cf_current_continue(IRLowerCtx* ctx) {
    if (!ctx || ctx->cf_depth <= 0) return NULL;
    return ctx->continue_targets[ctx->cf_depth - 1];
}

static int ir_lower_index_count(Node* indices)
{
    int n = 0;
    for (Node* p = indices; p; p = p->next) n++;
    return n;
}

static Node* ir_lower_nth_index(Node* indices, int n)
{
    if (n < 0) return NULL;
    int i = 0;
    for (Node* p = indices; p; p = p->next, i++) {
        if (i == n) return p;
    }
    return NULL;
}

static Node* ir_lower_find_global_array_decl(IRLowerCtx* ctx, const char* name)
{
    if (!ctx || !ctx->program_root || !name) return NULL;
    if (ctx->program_root->type != NODE_PROGRAM) return NULL;

    for (Node* d = ctx->program_root->data.program.declarations; d; d = d->next) {
        if (d->type != NODE_ARRAY_DECL) continue;
        if (!d->data.array_decl.is_global) continue;
        if (!d->data.array_decl.name) continue;
        if (strcmp(d->data.array_decl.name, name) == 0) {
            return d;
        }
    }
    return NULL;
}

static Node* ir_lower_find_global_var_decl(IRLowerCtx* ctx, const char* name)
{
    if (!ctx || !ctx->program_root || !name) return NULL;
    if (ctx->program_root->type != NODE_PROGRAM) return NULL;

    for (Node* d = ctx->program_root->data.program.declarations; d; d = d->next) {
        if (d->type != NODE_VAR_DECL) continue;
        if (!d->data.var_decl.is_global) continue;
        if (!d->data.var_decl.name) continue;
        if (strcmp(d->data.var_decl.name, name) == 0) {
            return d;
        }
    }
    return NULL;
}

static void ir_lower_emit_abort_path(IRLowerCtx* ctx)
{
    if (!ctx || !ctx->builder) return;

    ir_builder_emit_call_void(ctx->builder, "abort", NULL, 0);

    IRFunc* f = ctx->builder->current_func;
    if (!f || !f->ret_type || f->ret_type->kind == IR_TYPE_VOID) {
        ir_builder_emit_ret_void(ctx->builder);
    } else {
        IRValue* z = ir_value_const_int(0, f->ret_type);
        ir_builder_emit_ret(ctx->builder, z);
    }
}

static void ir_lower_emit_debug_bounds_check(IRLowerCtx* ctx, const Node* site, IRValue* idx, int dim)
{
    if (!ctx || !ctx->builder || !idx || !ctx->enable_bounds_checks) return;
    if (dim <= 0) return;

    IRValue* i64_idx = ensure_i64(ctx, idx);
    IRValue* zero = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* dimv = ir_value_const_int((int64_t)dim, IR_TYPE_I64_T);

    int ge = ir_builder_emit_cmp_ge(ctx->builder, i64_idx, zero);
    int lt = ir_builder_emit_cmp_lt(ctx->builder, i64_idx, dimv);
    IRValue* ge_v = ir_value_reg(ge, IR_TYPE_I1_T);
    IRValue* lt_v = ir_value_reg(lt, IR_TYPE_I1_T);
    int okb = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ge_v, lt_v);
    IRValue* ok = ir_value_reg(okb, IR_TYPE_I1_T);

    IRBlock* pass = cf_create_block(ctx, "حدود_سليم");
    IRBlock* fail = cf_create_block(ctx, "حدود_فشل");
    if (!pass || !fail) return;

    ir_builder_emit_br_cond(ctx->builder, ok, pass, fail);

    ir_builder_set_insert_point(ctx->builder, fail);
    ir_lower_set_loc(ctx->builder, site);
    ir_lower_emit_abort_path(ctx);

    ir_builder_set_insert_point(ctx->builder, pass);
    ir_lower_set_loc(ctx->builder, site);
}

static IRValue* ir_lower_build_linear_index(IRLowerCtx* ctx,
                                            const Node* site,
                                            Node* indices,
                                            int index_count,
                                            const int* dims,
                                            int rank)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (!indices) return ir_value_const_int(0, IR_TYPE_I64_T);

    int effective_count = index_count > 0 ? index_count : ir_lower_index_count(indices);
    if (effective_count <= 0) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* linear = NULL;
    int i = 0;
    for (Node* idx_node = indices; idx_node && i < effective_count; idx_node = idx_node->next, i++) {
        IRValue* idx0 = lower_expr(ctx, idx_node);
        IRValue* idx = ensure_i64(ctx, idx0);

        if (dims && i < rank) {
            ir_lower_emit_debug_bounds_check(ctx, site, idx, dims[i]);
        }

        if (!linear) {
            linear = idx;
            continue;
        }

        int dim = (dims && i < rank) ? dims[i] : 0;
        if (dim > 0) {
            int mr = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, linear,
                                         ir_value_const_int((int64_t)dim, IR_TYPE_I64_T));
            linear = ir_value_reg(mr, IR_TYPE_I64_T);
        }

        int ar = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, linear, idx);
        linear = ir_value_reg(ar, IR_TYPE_I64_T);
    }

    if (rank > 0 && effective_count != rank) {
        ir_lower_report_error(ctx, site,
                              "عدد فهارس المصفوفة غير صحيح أثناء خفض IR (expected=%d, got=%d).",
                              rank, effective_count);
    }

    return linear ? linear : ir_value_const_int(0, IR_TYPE_I64_T);
}

static IRValue* lower_to_bool(IRLowerCtx* ctx, IRValue* value) {
    if (!ctx || !ctx->builder || !value) return ir_value_const_int(0, IR_TYPE_I1_T);

    // If already i1, keep it.
    if (value->type && value->type->kind == IR_TYPE_I1) {
        return value;
    }

    // Compare value != 0 to produce i1.
    IRType* vt = value->type ? value->type : IR_TYPE_I64_T;
    IRValue* zero = ir_value_const_int(0, vt);

    int reg = ir_builder_emit_cmp_ne(ctx->builder, value, zero);
    return ir_value_reg(reg, IR_TYPE_I1_T);
}

static IROp ir_binop_to_irop(OpType op) {
    switch (op) {
        case OP_ADD: return IR_OP_ADD;
        case OP_SUB: return IR_OP_SUB;
        case OP_MUL: return IR_OP_MUL;
        case OP_DIV: return IR_OP_DIV;
        case OP_MOD: return IR_OP_MOD;
        case OP_BIT_AND: return IR_OP_AND;
        case OP_BIT_OR:  return IR_OP_OR;
        case OP_BIT_XOR: return IR_OP_XOR;
        case OP_SHL:     return IR_OP_SHL;
        case OP_SHR:     return IR_OP_SHR;
        default:     return IR_OP_NOP;
    }
}

static IRCmpPred ir_cmp_to_pred(OpType op, bool is_unsigned) {
    switch (op) {
        case OP_EQ:  return IR_CMP_EQ;
        case OP_NEQ: return IR_CMP_NE;
        case OP_GT:  return is_unsigned ? IR_CMP_UGT : IR_CMP_GT;
        case OP_LT:  return is_unsigned ? IR_CMP_ULT : IR_CMP_LT;
        case OP_GTE: return is_unsigned ? IR_CMP_UGE : IR_CMP_GE;
        case OP_LTE: return is_unsigned ? IR_CMP_ULE : IR_CMP_LE;
        default:     return IR_CMP_EQ;
    }
}

static bool lower_datatype_is_unsigned_int(DataType t)
{
    return t == TYPE_U8 || t == TYPE_U16 || t == TYPE_U32 || t == TYPE_U64;
}

static int lower_datatype_int_rank(DataType t)
{
    switch (t)
    {
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_ENUM:
            return 32;
        case TYPE_INT:
        case TYPE_U64:
            return 64;
        default:
            return 0;
    }
}

static DataType lower_int_promote_datatype(DataType t)
{
    switch (t)
    {
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_ENUM:
        case TYPE_I8:
        case TYPE_U8:
        case TYPE_I16:
        case TYPE_U16:
            return TYPE_I32;
        case TYPE_I32:
        case TYPE_U32:
        case TYPE_INT:
        case TYPE_U64:
            return t;
        default:
            return t;
    }
}

static DataType lower_unsigned_for_rank(int rank)
{
    return (rank >= 64) ? TYPE_U64 : TYPE_U32;
}

static DataType lower_usual_arith_datatype(DataType a, DataType b)
{
    DataType pa = lower_int_promote_datatype(a);
    DataType pb = lower_int_promote_datatype(b);

    if (pa == pb)
        return pa;

    int ra = lower_datatype_int_rank(pa);
    int rb = lower_datatype_int_rank(pb);
    if (ra == 0 || rb == 0)
        return TYPE_INT;

    bool ua = lower_datatype_is_unsigned_int(pa);
    bool ub = lower_datatype_is_unsigned_int(pb);
    if (ua == ub)
        return (ra >= rb) ? pa : pb;

    DataType u = ua ? pa : pb;
    DataType s = ua ? pb : pa;
    int ru = ua ? ra : rb;
    int rs = ua ? rb : ra;

    if (ru > rs)
        return u;

    if (rs > ru)
        return s;

    return lower_unsigned_for_rank(rs);
}

static IRType* lower_common_int_type(IRModule* module, DataType a, DataType b)
{
    DataType ct = lower_usual_arith_datatype(a, b);
    IRType* irt = ir_type_from_datatype(module, ct);
    if (!irt)
        return IR_TYPE_I64_T;
    return irt;
}

static void ir_lower_eval_call_args(IRLowerCtx* ctx, Node* args)
{
    for (Node* a = args; a; a = a->next) {
        (void)lower_expr(ctx, a);
    }
}

static IRValue* ir_lower_as_char_ptr(IRLowerCtx* ctx, IRValue* v)
{
    IRModule* m = (ctx && ctx->builder) ? ctx->builder->module : NULL;
    if (m) ir_module_set_current(m);
    IRType* char_ptr_t = get_char_ptr_type(m);
    if (!v) return ir_value_const_int(0, char_ptr_t);
    if (v->type && ir_types_equal(v->type, char_ptr_t)) return v;
    int cr = ir_builder_emit_cast(ctx->builder, v, char_ptr_t);
    return ir_value_reg(cr, char_ptr_t);
}

static IRValue* ir_lower_load_char_at(IRLowerCtx* ctx, IRValue* base, IRValue* idx)
{
    if (!ctx || !ctx->builder || !base || !idx) {
        return ir_value_const_int(0, IR_TYPE_CHAR_T);
    }
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRValue* i64_idx = ensure_i64(ctx, idx);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, char_ptr_t, base, i64_idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    int lr = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, elem_ptr);
    return ir_value_reg(lr, IR_TYPE_CHAR_T);
}

static void ir_lower_store_char_at(IRLowerCtx* ctx, IRValue* base, IRValue* idx, IRValue* ch)
{
    if (!ctx || !ctx->builder || !base || !idx || !ch) return;
    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    IRType* char_ptr_t = get_char_ptr_type(m);
    IRValue* i64_idx = ensure_i64(ctx, idx);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, char_ptr_t, base, i64_idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    IRValue* v = cast_to(ctx, ch, IR_TYPE_CHAR_T);
    ir_builder_emit_store(ctx->builder, v, elem_ptr);
}

static IRValue* ir_lower_char_bits_to_i64(IRLowerCtx* ctx, IRValue* ch)
{
    if (!ctx || !ctx->builder || !ch) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (ch->type && ch->type->kind == IR_TYPE_I64) return ch;
    // ملاحظة: هذا تحويل "بتّي" للحصول على الحقول المعبأة (bytes + len) بدون فك ترميز الحرف.
    int r = ir_builder_emit_cast(ctx->builder, ch, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_i64_bits_to_char(IRLowerCtx* ctx, IRValue* bits)
{
    if (!ctx || !ctx->builder || !bits) return ir_value_const_int(0, IR_TYPE_CHAR_T);
    if (bits->type && bits->type->kind == IR_TYPE_CHAR) return bits;
    int r = ir_builder_emit_cast(ctx->builder, bits, IR_TYPE_CHAR_T);
    return ir_value_reg(r, IR_TYPE_CHAR_T);
}

/**
 * @brief تحويل نص باء (حرف[]) إلى C-string (char*) UTF-8 مخصص على heap.
 *
 * ملاحظة: إن كان النص NULL (عدم)، يعيد NULL.
 * الملكية: يجب تحرير النتيجة عبر free/تحرير_ذاكرة.
 */
static IRValue* ir_lower_baa_string_to_cstr_alloc(IRLowerCtx* ctx, const Node* site, IRValue* str_in)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* char_ptr_t = get_char_ptr_type(m);
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    IRValue* base = ir_lower_as_char_ptr(ctx, str_in);
    if (!base) base = ir_value_const_int(0, char_ptr_t);

    int res_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, ir_type_ptr(i8_ptr_t));
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), res_ptr);

    IRValue* base_i8 = cast_to(ctx, base, i8_ptr_t);
    int is_null_r = ir_builder_emit_cmp_eq(b, base_i8, ir_value_const_int(0, i8_ptr_t));
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* nullb = cf_create_block(ctx, "نص_إلى_سي_فارغ");
    IRBlock* notnull = cf_create_block(ctx, "نص_إلى_سي_غير_فارغ");
    IRBlock* final_done = cf_create_block(ctx, "نص_إلى_سي_نهاية");
    if (!nullb || !notnull || !final_done) {
        return ir_value_const_int(0, i8_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, nullb, notnull);
    }

    ir_builder_set_insert_point(b, nullb);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, notnull);
    ir_lower_set_loc(b, site);

    // 1) حساب طول البايتات
    int idx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int blen_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* idx_ptr = ir_value_reg(idx_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    IRValue* blen_ptr = ir_value_reg(blen_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), idx_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), blen_ptr);

    IRBlock* count_head = cf_create_block(ctx, "نص_إلى_سي_عد_تحقق");
    IRBlock* count_step = cf_create_block(ctx, "نص_إلى_سي_عد_خطوة");
    IRBlock* count_done = cf_create_block(ctx, "نص_إلى_سي_عد_نهاية");
    if (!count_head || !count_step || !count_done) {
        ir_builder_emit_br(b, final_done);
    } else {
        ir_builder_emit_br(b, count_head);
    }

    ir_builder_set_insert_point(b, count_head);
    ir_lower_set_loc(b, site);
    int idx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, idx_ptr);
    IRValue* idx = ir_value_reg(idx_r, IR_TYPE_I64_T);
    int ep = ir_builder_emit_ptr_offset(b, char_ptr_t, base, idx);
    IRValue* elem_ptr = ir_value_reg(ep, char_ptr_t);
    int ch_r = ir_builder_emit_load(b, IR_TYPE_CHAR_T, elem_ptr);
    IRValue* ch = ir_value_reg(ch_r, IR_TYPE_CHAR_T);
    IRValue* ch_bits = ir_lower_char_bits_to_i64(ctx, ch);
    int end_r = ir_builder_emit_cmp_eq(b, ch_bits, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* is_end = ir_value_reg(end_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_end, count_done, count_step);

    ir_builder_set_insert_point(b, count_step);
    ir_lower_set_loc(b, site);
    IRValue* len64 = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, ch_bits,
                                                      ir_value_const_int(32, IR_TYPE_I64_T)),
                                  IR_TYPE_I64_T);
    int blen_r = ir_builder_emit_load(b, IR_TYPE_I64_T, blen_ptr);
    IRValue* blen = ir_value_reg(blen_r, IR_TYPE_I64_T);
    int blen2_r = ir_builder_emit_add(b, IR_TYPE_I64_T, blen, len64);
    IRValue* blen2 = ir_value_reg(blen2_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, blen2, blen_ptr);
    int idx2_r = ir_builder_emit_add(b, IR_TYPE_I64_T, idx, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* idx2 = ir_value_reg(idx2_r, IR_TYPE_I64_T);
    ir_builder_emit_store(b, idx2, idx_ptr);
    ir_builder_emit_br(b, count_head);

    // 2) تخصيص وملء المخزن
    ir_builder_set_insert_point(b, count_done);
    ir_lower_set_loc(b, site);
    int blen3_r = ir_builder_emit_load(b, IR_TYPE_I64_T, blen_ptr);
    IRValue* blen3 = ir_value_reg(blen3_r, IR_TYPE_I64_T);
    int alloc_n_r = ir_builder_emit_add(b, IR_TYPE_I64_T, blen3, ir_value_const_int(1, IR_TYPE_I64_T));
    IRValue* alloc_n = ir_value_reg(alloc_n_r, IR_TYPE_I64_T);
    IRValue* margs[1] = { alloc_n };
    int buf_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, margs, 1);
    if (buf_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc أثناء تحويل نص إلى C-string.");
        ir_builder_emit_store(b, ir_value_const_int(0, i8_ptr_t), res_ptr);
        ir_builder_emit_br(b, final_done);
        ir_builder_set_insert_point(b, final_done);
        ir_lower_set_loc(b, site);
        int r_fail = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
        return ir_value_reg(r_fail, i8_ptr_t);
    }
    IRValue* buf = ir_value_reg(buf_r, i8_ptr_t);

    int fidx_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    int out_ptr_reg = ir_builder_emit_alloca(b, IR_TYPE_I64_T);
    IRValue* fidx_ptr = ir_value_reg(fidx_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    IRValue* out_ptr = ir_value_reg(out_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), fidx_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), out_ptr);

    IRBlock* fill_head = cf_create_block(ctx, "نص_إلى_سي_ملء_تحقق");
    IRBlock* fill_body = cf_create_block(ctx, "نص_إلى_سي_ملء_جسم");
    IRBlock* fill_done = cf_create_block(ctx, "نص_إلى_سي_ملء_نهاية");
    if (!fill_head || !fill_body || !fill_done) {
        ir_builder_emit_store(b, buf, res_ptr);
        ir_builder_emit_br(b, final_done);
    } else {
        ir_builder_emit_br(b, fill_head);
    }

    ir_builder_set_insert_point(b, fill_head);
    ir_lower_set_loc(b, site);
    int fidx_r = ir_builder_emit_load(b, IR_TYPE_I64_T, fidx_ptr);
    IRValue* fidx = ir_value_reg(fidx_r, IR_TYPE_I64_T);
    int ep2 = ir_builder_emit_ptr_offset(b, char_ptr_t, base, fidx);
    IRValue* elem_ptr2 = ir_value_reg(ep2, char_ptr_t);
    int ch2_r = ir_builder_emit_load(b, IR_TYPE_CHAR_T, elem_ptr2);
    IRValue* ch2 = ir_value_reg(ch2_r, IR_TYPE_CHAR_T);
    IRValue* ch2_bits = ir_lower_char_bits_to_i64(ctx, ch2);
    int end2_r = ir_builder_emit_cmp_eq(b, ch2_bits, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* end2 = ir_value_reg(end2_r, IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, end2, fill_done, fill_body);

    ir_builder_set_insert_point(b, fill_body);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
    IRValue* out = ir_value_reg(out_r, IR_TYPE_I64_T);

    IRValue* len2 = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, ch2_bits,
                                                     ir_value_const_int(32, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    IRValue* bytes_field = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, ch2_bits,
                                                           ir_value_const_int(4294967295LL, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);

    IRValue* b0 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, bytes_field,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b1s = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, bytes_field,
                                                    ir_value_const_int(8, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b1 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b1s,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b2s = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, bytes_field,
                                                    ir_value_const_int(16, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b2 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b2s,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);
    IRValue* b3s = ir_value_reg(ir_builder_emit_shr(b, IR_TYPE_I64_T, bytes_field,
                                                    ir_value_const_int(24, IR_TYPE_I64_T)),
                                IR_TYPE_I64_T);
    IRValue* b3 = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I64_T, b3s,
                                                   ir_value_const_int(255, IR_TYPE_I64_T)),
                               IR_TYPE_I64_T);

    // اكتب b0 دائماً
    int dp0 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out);
    IRValue* p0 = ir_value_reg(dp0, i8_ptr_t);
    IRValue* b0_i8 = cast_to(ctx, b0, IR_TYPE_I8_T);
    ir_builder_emit_store(b, b0_i8, p0);
    IRValue* out1 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out,
                                                     ir_value_const_int(1, IR_TYPE_I64_T)),
                                 IR_TYPE_I64_T);
    ir_builder_emit_store(b, out1, out_ptr);

    IRBlock* b1b = cf_create_block(ctx, "نص_إلى_سي_ملء_b1");
    IRBlock* a1b = cf_create_block(ctx, "نص_إلى_سي_ملء_بعد_b1");
    IRBlock* b2b = cf_create_block(ctx, "نص_إلى_سي_ملء_b2");
    IRBlock* a2b = cf_create_block(ctx, "نص_إلى_سي_ملء_بعد_b2");
    IRBlock* b3b = cf_create_block(ctx, "نص_إلى_سي_ملء_b3");
    IRBlock* a3b = cf_create_block(ctx, "نص_إلى_سي_ملء_بعد_b3");
    if (!b1b || !a1b || !b2b || !a2b || !b3b || !a3b) {
        // في حالة فشل إنشاء الكتل، نكمل بشكل محافظ.
        int fidx2_r = ir_builder_emit_add(b, IR_TYPE_I64_T, fidx, ir_value_const_int(1, IR_TYPE_I64_T));
        ir_builder_emit_store(b, ir_value_reg(fidx2_r, IR_TYPE_I64_T), fidx_ptr);
        ir_builder_emit_br(b, fill_head);
    } else {
        int c2_r = ir_builder_emit_cmp_ge(b, len2, ir_value_const_int(2, IR_TYPE_I64_T));
        ir_builder_emit_br_cond(b, ir_value_reg(c2_r, IR_TYPE_I1_T), b1b, a1b);

        ir_builder_set_insert_point(b, b1b);
        ir_lower_set_loc(b, site);
        int out1r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
        IRValue* out1v = ir_value_reg(out1r, IR_TYPE_I64_T);
        int dp1 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out1v);
        IRValue* p1 = ir_value_reg(dp1, i8_ptr_t);
        ir_builder_emit_store(b, cast_to(ctx, b1, IR_TYPE_I8_T), p1);
        IRValue* out2v = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out1v,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, out2v, out_ptr);
        ir_builder_emit_br(b, a1b);

        ir_builder_set_insert_point(b, a1b);
        ir_lower_set_loc(b, site);
        int c3_r = ir_builder_emit_cmp_ge(b, len2, ir_value_const_int(3, IR_TYPE_I64_T));
        ir_builder_emit_br_cond(b, ir_value_reg(c3_r, IR_TYPE_I1_T), b2b, a2b);

        ir_builder_set_insert_point(b, b2b);
        ir_lower_set_loc(b, site);
        int out2r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
        IRValue* out2 = ir_value_reg(out2r, IR_TYPE_I64_T);
        int dp2 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out2);
        IRValue* p2 = ir_value_reg(dp2, i8_ptr_t);
        ir_builder_emit_store(b, cast_to(ctx, b2, IR_TYPE_I8_T), p2);
        IRValue* out3v = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out2,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, out3v, out_ptr);
        ir_builder_emit_br(b, a2b);

        ir_builder_set_insert_point(b, a2b);
        ir_lower_set_loc(b, site);
        int c4_r = ir_builder_emit_cmp_ge(b, len2, ir_value_const_int(4, IR_TYPE_I64_T));
        ir_builder_emit_br_cond(b, ir_value_reg(c4_r, IR_TYPE_I1_T), b3b, a3b);

        ir_builder_set_insert_point(b, b3b);
        ir_lower_set_loc(b, site);
        int out3r = ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr);
        IRValue* out3 = ir_value_reg(out3r, IR_TYPE_I64_T);
        int dp3 = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, out3);
        IRValue* p3 = ir_value_reg(dp3, i8_ptr_t);
        ir_builder_emit_store(b, cast_to(ctx, b3, IR_TYPE_I8_T), p3);
        IRValue* out4v = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, out3,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, out4v, out_ptr);
        ir_builder_emit_br(b, a3b);

        ir_builder_set_insert_point(b, a3b);
        ir_lower_set_loc(b, site);
        // التالي
        IRValue* fidx2 = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, fidx,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
        ir_builder_emit_store(b, fidx2, fidx_ptr);
        ir_builder_emit_br(b, fill_head);
    }

    ir_builder_set_insert_point(b, fill_done);
    ir_lower_set_loc(b, site);
    // ضع NULL-terminator
    int dpn = ir_builder_emit_ptr_offset(b, i8_ptr_t, buf, blen3);
    IRValue* pn = ir_value_reg(dpn, i8_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), pn);
    ir_builder_emit_store(b, buf, res_ptr);
    ir_builder_emit_br(b, final_done);

    ir_builder_set_insert_point(b, final_done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

