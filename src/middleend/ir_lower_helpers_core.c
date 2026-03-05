// ============================================================================
// Expression/Statement lowering helpers
// ============================================================================

static void ir_lower_set_loc(IRBuilder* builder, const Node* node) {
    if (!builder) return;

    if (!node || !node->filename || node->line <= 0) {
        ir_builder_clear_loc(builder);
        return;
    }

    ir_builder_set_loc(builder, node->filename, node->line, node->col);
}

static void ir_lower_tag_last_inst(IRBuilder* builder, IROp expected_op, int expected_dest,
                                   const char* dbg_name) {
    if (!builder || !builder->insert_block) return;
    if (!dbg_name) return;

    IRInst* last = builder->insert_block->last;
    if (!last) return;
    if (last->op != expected_op) return;
    if (expected_dest >= 0 && last->dest != expected_dest) return;

    ir_inst_set_dbg_name(last, dbg_name);
}

// forward decls
static IRValue* ensure_i64(IRLowerCtx* ctx, IRValue* v);
static IRValue* cast_to(IRLowerCtx* ctx, IRValue* v, IRType* to);
static IRValue* ir_lower_global_init_value(IRBuilder* builder, Node* expr, IRType* expected_type);

static IRType* get_char_ptr_type(IRModule* module) {
    if (!module) return ir_type_ptr(IR_TYPE_CHAR_T);
    ir_module_set_current(module);
    return ir_type_ptr(IR_TYPE_CHAR_T);
}

static const char* ir_lower_binding_storage_name(const IRLowerBinding* b)
{
    if (!b) return NULL;
    if (b->is_static_storage && b->global_name) return b->global_name;
    return b->name;
}

static char* ir_lower_make_static_label(IRLowerCtx* ctx, const char* local_name)
{
    if (!ctx || !ctx->builder || !ctx->builder->module) return NULL;

    IRModule* m = ctx->builder->module;
    const char* func_name = ctx->current_func_name ? ctx->current_func_name : "func";
    const char* name = local_name ? local_name : "var";
    int id = ctx->static_local_counter++;

    int n = snprintf(NULL, 0, "__baa_static_%s_%s_%d", func_name, name, id);
    if (n <= 0) {
        ir_lower_report_error(ctx, NULL, "فشل توليد اسم متغير ساكن.");
        return NULL;
    }

    char* out = (char*)ir_arena_alloc(&m->arena, (size_t)n + 1, _Alignof(char));
    if (!out) {
        ir_lower_report_error(ctx, NULL, "فشل تخصيص اسم متغير ساكن.");
        return NULL;
    }

    (void)snprintf(out, (size_t)n + 1, "__baa_static_%s_%s_%d", func_name, name, id);
    return out;
}

static uint64_t pack_utf8_codepoint(uint32_t cp)
{
    // استبدال المحرف غير الصالح بـ U+FFFD
    const uint32_t repl = 0xFFFDu;
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        cp = repl;
    }

    unsigned char b[4] = {0, 0, 0, 0};
    unsigned len = 0;

    if (cp <= 0x7Fu) {
        b[0] = (unsigned char)cp;
        len = 1;
    } else if (cp <= 0x7FFu) {
        b[0] = (unsigned char)(0xC0u | ((cp >> 6) & 0x1Fu));
        b[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
        len = 2;
    } else if (cp <= 0xFFFFu) {
        b[0] = (unsigned char)(0xE0u | ((cp >> 12) & 0x0Fu));
        b[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
        len = 3;
    } else {
        b[0] = (unsigned char)(0xF0u | ((cp >> 18) & 0x07u));
        b[1] = (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
        len = 4;
    }

    uint64_t bytes_field = (uint64_t)b[0] |
                           ((uint64_t)b[1] << 8) |
                           ((uint64_t)b[2] << 16) |
                           ((uint64_t)b[3] << 24);
    return bytes_field | ((uint64_t)len << 32);
}

static IRType* ir_type_from_datatype(IRModule* module, DataType t) {
    switch (t) {
        case TYPE_BOOL:   return IR_TYPE_I1_T;
        case TYPE_I8:     return IR_TYPE_I8_T;
        case TYPE_I16:    return IR_TYPE_I16_T;
        case TYPE_I32:    return IR_TYPE_I32_T;
        case TYPE_U8:     return IR_TYPE_U8_T;
        case TYPE_U16:    return IR_TYPE_U16_T;
        case TYPE_U32:    return IR_TYPE_U32_T;
        case TYPE_U64:    return IR_TYPE_U64_T;
        case TYPE_STRING: return get_char_ptr_type(module);
        case TYPE_POINTER: return ir_type_ptr(IR_TYPE_I8_T);
        case TYPE_FUNC_PTR: return ir_type_func(IR_TYPE_VOID_T, NULL, 0);
        case TYPE_CHAR:   return IR_TYPE_CHAR_T;
        case TYPE_FLOAT:  return IR_TYPE_F64_T;
        case TYPE_VOID:   return IR_TYPE_VOID_T;
        case TYPE_ENUM:   return IR_TYPE_I64_T;
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
    }
}

static IRType* ir_type_from_funcsig(IRModule* module, const FuncPtrSig* sig)
{
    if (!sig) {
        return ir_type_func(IR_TYPE_VOID_T, NULL, 0);
    }

    IRType* ret_t = ir_type_from_datatype(module, sig->return_type);
    if (!ret_t) ret_t = IR_TYPE_VOID_T;

    int pc = sig->param_count;
    if (pc < 0) pc = 0;

    IRType* params_local[64];
    IRType** params = NULL;
    if (pc > 0) {
        if (pc > (int)(sizeof(params_local) / sizeof(params_local[0]))) {
            pc = (int)(sizeof(params_local) / sizeof(params_local[0]));
        }
        for (int i = 0; i < pc; i++) {
            DataType pt = sig->param_types ? sig->param_types[i] : TYPE_INT;
            // لا ندعم حالياً أنواع مؤشر دالة داخل توقيع مؤشر دالة.
            if (pt == TYPE_FUNC_PTR) {
                params_local[i] = ir_type_func(IR_TYPE_VOID_T, NULL, 0);
            } else {
                params_local[i] = ir_type_from_datatype(module, pt);
            }
            if (!params_local[i]) params_local[i] = IR_TYPE_I64_T;
        }
        params = params_local;
    }

    return ir_type_func(ret_t, params, pc);
}

static IRType* ir_type_from_datatype_ex(IRModule* module, DataType t, const FuncPtrSig* sig)
{
    if (t == TYPE_FUNC_PTR) {
        return ir_type_from_funcsig(module, sig);
    }
    return ir_type_from_datatype(module, t);
}

static int64_t ir_lower_pointer_elem_size(const Node* ptr_expr)
{
    if (!ptr_expr) return 1;

    int depth = ptr_expr->inferred_ptr_depth;
    DataType base = ptr_expr->inferred_ptr_base_type;

    if (depth > 1) return 8;
    if (depth <= 0) return 1;

    switch (base)
    {
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

static IRValue* cast_i1_to_i64(IRLowerCtx* ctx, IRValue* v)
{
    if (!ctx || !ctx->builder || !v) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (v->type && v->type->kind == IR_TYPE_I64) return v;
    int r = ir_builder_emit_cast(ctx->builder, v, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* lower_char_decode_to_i64(IRLowerCtx* ctx, IRValue* ch)
{
    if (!ctx || !ctx->builder || !ch) return ir_value_const_int(0, IR_TYPE_I64_T);

    // التمثيل المعبأ: i64 = bytes(32-bit) + len * 2^32
    IRValue* packed = ch;
    if (!packed->type || packed->type->kind != IR_TYPE_I64) {
        int pr = ir_builder_emit_cast(ctx->builder, ch, IR_TYPE_I64_T);
        packed = ir_value_reg(pr, IR_TYPE_I64_T);
    }

    IRValue* c_2p32 = ir_value_const_int(4294967296LL, IR_TYPE_I64_T);
    int len_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    int bytes_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    IRValue* c256 = ir_value_const_int(256, IR_TYPE_I64_T);

    int b0_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, bytes, c256);
    IRValue* b0 = ir_value_reg(b0_r, IR_TYPE_I64_T);
    int t1_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, bytes, c256);
    IRValue* t1 = ir_value_reg(t1_r, IR_TYPE_I64_T);
    int b1_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, t1, c256);
    IRValue* b1 = ir_value_reg(b1_r, IR_TYPE_I64_T);
    int t2_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, t1, c256);
    IRValue* t2 = ir_value_reg(t2_r, IR_TYPE_I64_T);
    int b2_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, t2, c256);
    IRValue* b2 = ir_value_reg(b2_r, IR_TYPE_I64_T);
    int t3_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, t2, c256);
    IRValue* t3 = ir_value_reg(t3_r, IR_TYPE_I64_T);
    int b3_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, t3, c256);
    IRValue* b3 = ir_value_reg(b3_r, IR_TYPE_I64_T);

    // flags f1..f4
    IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* two = ir_value_const_int(2, IR_TYPE_I64_T);
    IRValue* three = ir_value_const_int(3, IR_TYPE_I64_T);
    IRValue* four = ir_value_const_int(4, IR_TYPE_I64_T);

    int f1b = ir_builder_emit_cmp_eq(ctx->builder, len, one);
    int f2b = ir_builder_emit_cmp_eq(ctx->builder, len, two);
    int f3b = ir_builder_emit_cmp_eq(ctx->builder, len, three);
    int f4b = ir_builder_emit_cmp_eq(ctx->builder, len, four);

    IRValue* f1 = cast_i1_to_i64(ctx, ir_value_reg(f1b, IR_TYPE_I1_T));
    IRValue* f2 = cast_i1_to_i64(ctx, ir_value_reg(f2b, IR_TYPE_I1_T));
    IRValue* f3 = cast_i1_to_i64(ctx, ir_value_reg(f3b, IR_TYPE_I1_T));
    IRValue* f4 = cast_i1_to_i64(ctx, ir_value_reg(f4b, IR_TYPE_I1_T));

    // cp1 = b0
    IRValue* cp1 = b0;

    // cp2 = (b0-0xC0)*64 + (b1-0x80)
    int a0_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b0, ir_value_const_int(0xC0, IR_TYPE_I64_T));
    IRValue* a0 = ir_value_reg(a0_r, IR_TYPE_I64_T);
    int a0s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, a0, ir_value_const_int(64, IR_TYPE_I64_T));
    IRValue* a0s = ir_value_reg(a0s_r, IR_TYPE_I64_T);
    int a1_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* a1 = ir_value_reg(a1_r, IR_TYPE_I64_T);
    int cp2_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, a0s, a1);
    IRValue* cp2 = ir_value_reg(cp2_r, IR_TYPE_I64_T);

    // cp3 = (b0-0xE0)*4096 + (b1-0x80)*64 + (b2-0x80)
    int b0m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b0, ir_value_const_int(0xE0, IR_TYPE_I64_T));
    IRValue* b0m = ir_value_reg(b0m_r, IR_TYPE_I64_T);
    int b0s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0m, ir_value_const_int(4096, IR_TYPE_I64_T));
    IRValue* b0s = ir_value_reg(b0s_r, IR_TYPE_I64_T);
    int b1m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* b1m = ir_value_reg(b1m_r, IR_TYPE_I64_T);
    int b1s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1m, ir_value_const_int(64, IR_TYPE_I64_T));
    IRValue* b1s = ir_value_reg(b1s_r, IR_TYPE_I64_T);
    int b2m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b2, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* b2m = ir_value_reg(b2m_r, IR_TYPE_I64_T);
    int t3a_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0s, b1s);
    IRValue* t3a = ir_value_reg(t3a_r, IR_TYPE_I64_T);
    int cp3_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t3a, b2m);
    IRValue* cp3 = ir_value_reg(cp3_r, IR_TYPE_I64_T);

    // cp4 = (b0-0xF0)*262144 + (b1-0x80)*4096 + (b2-0x80)*64 + (b3-0x80)
    int c0m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b0, ir_value_const_int(0xF0, IR_TYPE_I64_T));
    IRValue* c0m = ir_value_reg(c0m_r, IR_TYPE_I64_T);
    int c0s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, c0m, ir_value_const_int(262144, IR_TYPE_I64_T));
    IRValue* c0s = ir_value_reg(c0s_r, IR_TYPE_I64_T);
    int c1m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* c1m = ir_value_reg(c1m_r, IR_TYPE_I64_T);
    int c1s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, c1m, ir_value_const_int(4096, IR_TYPE_I64_T));
    IRValue* c1s = ir_value_reg(c1s_r, IR_TYPE_I64_T);
    int c2m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b2, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* c2m = ir_value_reg(c2m_r, IR_TYPE_I64_T);
    int c2s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, c2m, ir_value_const_int(64, IR_TYPE_I64_T));
    IRValue* c2s = ir_value_reg(c2s_r, IR_TYPE_I64_T);
    int c3m_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, b3, ir_value_const_int(0x80, IR_TYPE_I64_T));
    IRValue* c3m = ir_value_reg(c3m_r, IR_TYPE_I64_T);
    int t4a_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, c0s, c1s);
    IRValue* t4a = ir_value_reg(t4a_r, IR_TYPE_I64_T);
    int t4b_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t4a, c2s);
    IRValue* t4b = ir_value_reg(t4b_r, IR_TYPE_I64_T);
    int cp4_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t4b, c3m);
    IRValue* cp4 = ir_value_reg(cp4_r, IR_TYPE_I64_T);

    // اختيار حسب الأعلام: cp = cp1*f1 + cp2*f2 + cp3*f3 + cp4*f4
    int cp1m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp1, f1);
    int cp2m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp2, f2);
    int cp3m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp3, f3);
    int cp4m_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, cp4, f4);
    IRValue* cp1m = ir_value_reg(cp1m_r, IR_TYPE_I64_T);
    IRValue* cp2m = ir_value_reg(cp2m_r, IR_TYPE_I64_T);
    IRValue* cp3m = ir_value_reg(cp3m_r, IR_TYPE_I64_T);
    IRValue* cp4m = ir_value_reg(cp4m_r, IR_TYPE_I64_T);
    int s12_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, cp1m, cp2m);
    IRValue* s12 = ir_value_reg(s12_r, IR_TYPE_I64_T);
    int s34_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, cp3m, cp4m);
    IRValue* s34 = ir_value_reg(s34_r, IR_TYPE_I64_T);
    int cp_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, s12, s34);
    return ir_value_reg(cp_r, IR_TYPE_I64_T);
}

static IRValue* lower_codepoint_encode_to_char(IRLowerCtx* ctx, IRValue* cp_in)
{
    if (!ctx || !ctx->builder || !cp_in) return ir_value_const_int(0, IR_TYPE_CHAR_T);

    IRValue* cp = ensure_i64(ctx, cp_in);

    // invalid = (cp > 0x10FFFF) || (0xD800 <= cp <= 0xDFFF)
    int gtmax_b = ir_builder_emit_cmp_gt(ctx->builder, cp, ir_value_const_int(0x10FFFF, IR_TYPE_I64_T));
    IRValue* gtmax = ir_value_reg(gtmax_b, IR_TYPE_I1_T);
    int ge_b = ir_builder_emit_cmp_ge(ctx->builder, cp, ir_value_const_int(0xD800, IR_TYPE_I64_T));
    IRValue* ge = ir_value_reg(ge_b, IR_TYPE_I1_T);
    int le_b = ir_builder_emit_cmp_le(ctx->builder, cp, ir_value_const_int(0xDFFF, IR_TYPE_I64_T));
    IRValue* le = ir_value_reg(le_b, IR_TYPE_I1_T);
    int sur_b = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ge, le);
    IRValue* sur = ir_value_reg(sur_b, IR_TYPE_I1_T);
    int inv_b = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, gtmax, sur);
    IRValue* inv = ir_value_reg(inv_b, IR_TYPE_I1_T);
    IRValue* inv64 = cast_i1_to_i64(ctx, inv);

    // cp = cp + inv*(0xFFFD - cp)
    int dcp_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xFFFD, IR_TYPE_I64_T), cp);
    IRValue* dcp = ir_value_reg(dcp_r, IR_TYPE_I64_T);
    int adj_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, inv64, dcp);
    IRValue* adj = ir_value_reg(adj_r, IR_TYPE_I64_T);
    int cpv_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, cp, adj);
    IRValue* cpv = ir_value_reg(cpv_r, IR_TYPE_I64_T);

    // الطول = 1 + (cpv > 0x7F) + (cpv > 0x7FF) + (cpv > 0xFFFF)
    int g1_b = ir_builder_emit_cmp_gt(ctx->builder, cpv, ir_value_const_int(0x7F, IR_TYPE_I64_T));
    int g2_b = ir_builder_emit_cmp_gt(ctx->builder, cpv, ir_value_const_int(0x7FF, IR_TYPE_I64_T));
    int g3_b = ir_builder_emit_cmp_gt(ctx->builder, cpv, ir_value_const_int(0xFFFF, IR_TYPE_I64_T));
    IRValue* g1 = cast_i1_to_i64(ctx, ir_value_reg(g1_b, IR_TYPE_I1_T));
    IRValue* g2 = cast_i1_to_i64(ctx, ir_value_reg(g2_b, IR_TYPE_I1_T));
    IRValue* g3 = cast_i1_to_i64(ctx, ir_value_reg(g3_b, IR_TYPE_I1_T));
    int tlen1_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(1, IR_TYPE_I64_T), g1);
    IRValue* tlen1 = ir_value_reg(tlen1_r, IR_TYPE_I64_T);
    int tlen2_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tlen1, g2);
    IRValue* tlen2 = ir_value_reg(tlen2_r, IR_TYPE_I64_T);
    int len_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tlen2, g3);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    // if invalid, force len = 3
    int dlen_r = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(3, IR_TYPE_I64_T), len);
    IRValue* dlen = ir_value_reg(dlen_r, IR_TYPE_I64_T);
    int ladj_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, inv64, dlen);
    IRValue* ladj = ir_value_reg(ladj_r, IR_TYPE_I64_T);
    int lenv_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, len, ladj);
    IRValue* lenv = ir_value_reg(lenv_r, IR_TYPE_I64_T);

    // flags for lenv
    int f1b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(1, IR_TYPE_I64_T));
    int f2b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(2, IR_TYPE_I64_T));
    int f3b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(3, IR_TYPE_I64_T));
    int f4b = ir_builder_emit_cmp_eq(ctx->builder, lenv, ir_value_const_int(4, IR_TYPE_I64_T));
    IRValue* f1 = cast_i1_to_i64(ctx, ir_value_reg(f1b, IR_TYPE_I1_T));
    IRValue* f2 = cast_i1_to_i64(ctx, ir_value_reg(f2b, IR_TYPE_I1_T));
    IRValue* f3 = cast_i1_to_i64(ctx, ir_value_reg(f3b, IR_TYPE_I1_T));
    IRValue* f4 = cast_i1_to_i64(ctx, ir_value_reg(f4b, IR_TYPE_I1_T));

    // compute bytes for each length
    IRValue* d64 = ir_value_const_int(64, IR_TYPE_I64_T);
    IRValue* d4096 = ir_value_const_int(4096, IR_TYPE_I64_T);
    IRValue* d262144 = ir_value_const_int(262144, IR_TYPE_I64_T);

    // طول 1
    IRValue* b0_1 = cpv;
    IRValue* b1_1 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b2_1 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b3_1 = ir_value_const_int(0, IR_TYPE_I64_T);

    // طول 2
    int q2_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    int r2_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* q2 = ir_value_reg(q2_r, IR_TYPE_I64_T);
    IRValue* r2 = ir_value_reg(r2_r, IR_TYPE_I64_T);
    int b0_2r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xC0, IR_TYPE_I64_T), q2);
    int b1_2r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r2);
    IRValue* b0_2 = ir_value_reg(b0_2r, IR_TYPE_I64_T);
    IRValue* b1_2 = ir_value_reg(b1_2r, IR_TYPE_I64_T);
    IRValue* b2_2 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* b3_2 = ir_value_const_int(0, IR_TYPE_I64_T);

    // طول 3
    int q31_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d4096);
    IRValue* q31 = ir_value_reg(q31_r, IR_TYPE_I64_T);
    int q32_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* q32 = ir_value_reg(q32_r, IR_TYPE_I64_T);
    int q32m_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, q32, d64);
    IRValue* q32m = ir_value_reg(q32m_r, IR_TYPE_I64_T);
    int r3_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* r3 = ir_value_reg(r3_r, IR_TYPE_I64_T);
    int b0_3r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xE0, IR_TYPE_I64_T), q31);
    int b1_3r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q32m);
    int b2_3r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r3);
    IRValue* b0_3 = ir_value_reg(b0_3r, IR_TYPE_I64_T);
    IRValue* b1_3 = ir_value_reg(b1_3r, IR_TYPE_I64_T);
    IRValue* b2_3 = ir_value_reg(b2_3r, IR_TYPE_I64_T);
    IRValue* b3_3 = ir_value_const_int(0, IR_TYPE_I64_T);

    // طول 4
    int q41_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d262144);
    IRValue* q41 = ir_value_reg(q41_r, IR_TYPE_I64_T);
    int q42_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d4096);
    IRValue* q42 = ir_value_reg(q42_r, IR_TYPE_I64_T);
    int q42m_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, q42, d64);
    IRValue* q42m = ir_value_reg(q42m_r, IR_TYPE_I64_T);
    int q43_r = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* q43 = ir_value_reg(q43_r, IR_TYPE_I64_T);
    int q43m_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, q43, d64);
    IRValue* q43m = ir_value_reg(q43m_r, IR_TYPE_I64_T);
    int r4_r = ir_builder_emit_mod(ctx->builder, IR_TYPE_I64_T, cpv, d64);
    IRValue* r4 = ir_value_reg(r4_r, IR_TYPE_I64_T);
    int b0_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0xF0, IR_TYPE_I64_T), q41);
    int b1_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q42m);
    int b2_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q43m);
    int b3_4r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r4);
    IRValue* b0_4 = ir_value_reg(b0_4r, IR_TYPE_I64_T);
    IRValue* b1_4 = ir_value_reg(b1_4r, IR_TYPE_I64_T);
    IRValue* b2_4 = ir_value_reg(b2_4r, IR_TYPE_I64_T);
    IRValue* b3_4 = ir_value_reg(b3_4r, IR_TYPE_I64_T);

    // اختيار البايتات حسب الأعلام
    IRValue* b0s1 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_1, f1), IR_TYPE_I64_T);
    IRValue* b0s2 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_2, f2), IR_TYPE_I64_T);
    IRValue* b0s3 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_3, f3), IR_TYPE_I64_T);
    IRValue* b0s4 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b0_4, f4), IR_TYPE_I64_T);
    IRValue* b1s1v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_1, f1), IR_TYPE_I64_T);
    IRValue* b1s2v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_2, f2), IR_TYPE_I64_T);
    IRValue* b1s3v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_3, f3), IR_TYPE_I64_T);
    IRValue* b1s4v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1_4, f4), IR_TYPE_I64_T);
    IRValue* b2s1v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_1, f1), IR_TYPE_I64_T);
    IRValue* b2s2v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_2, f2), IR_TYPE_I64_T);
    IRValue* b2s3v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_3, f3), IR_TYPE_I64_T);
    IRValue* b2s4v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2_4, f4), IR_TYPE_I64_T);
    IRValue* b3s1v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_1, f1), IR_TYPE_I64_T);
    IRValue* b3s2v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_2, f2), IR_TYPE_I64_T);
    IRValue* b3s3v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_3, f3), IR_TYPE_I64_T);
    IRValue* b3s4v = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3_4, f4), IR_TYPE_I64_T);

    IRValue* b0 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0s1, b0s2), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0s3, b0s4), IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b1 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b1s1v, b1s2v), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b1s3v, b1s4v), IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b2 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b2s1v, b2s2v), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b2s3v, b2s4v), IR_TYPE_I64_T)), IR_TYPE_I64_T);
    IRValue* b3 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b3s1v, b3s2v), IR_TYPE_I64_T),
                                                  ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b3s3v, b3s4v), IR_TYPE_I64_T)), IR_TYPE_I64_T);

    // bytes_field = b0 + b1*256 + b2*65536 + b3*16777216
    int b1s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b1, ir_value_const_int(256, IR_TYPE_I64_T));
    int b2s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b2, ir_value_const_int(65536, IR_TYPE_I64_T));
    int b3s_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, b3, ir_value_const_int(16777216, IR_TYPE_I64_T));
    IRValue* b1s = ir_value_reg(b1s_r, IR_TYPE_I64_T);
    IRValue* b2s = ir_value_reg(b2s_r, IR_TYPE_I64_T);
    IRValue* b3s = ir_value_reg(b3s_r, IR_TYPE_I64_T);
    int tbf_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0, b1s);
    IRValue* tbf = ir_value_reg(tbf_r, IR_TYPE_I64_T);
    int tbf2_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tbf, b2s);
    IRValue* tbf2 = ir_value_reg(tbf2_r, IR_TYPE_I64_T);
    int bytesf_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, tbf2, b3s);
    IRValue* bytesf = ir_value_reg(bytesf_r, IR_TYPE_I64_T);

    int len_shift_r = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, lenv, ir_value_const_int(4294967296LL, IR_TYPE_I64_T));
    IRValue* lsh = ir_value_reg(len_shift_r, IR_TYPE_I64_T);
    int packed_r = ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, bytesf, lsh);
    IRValue* packedv = ir_value_reg(packed_r, IR_TYPE_I64_T);

    int cr = ir_builder_emit_cast(ctx->builder, packedv, IR_TYPE_CHAR_T);
    return ir_value_reg(cr, IR_TYPE_CHAR_T);
}

static IRValue* ensure_i64(IRLowerCtx* ctx, IRValue* v)
{
    if (!ctx || !ctx->builder || !v) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (v->type && v->type->kind == IR_TYPE_CHAR) {
        return lower_char_decode_to_i64(ctx, v);
    }
    if (v->type && v->type->kind == IR_TYPE_I64) return v;
    int r = ir_builder_emit_cast(ctx->builder, v, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* cast_to(IRLowerCtx* ctx, IRValue* v, IRType* to)
{
    if (!ctx || !ctx->builder || !v || !to) return v;
    if (v->type && ir_types_equal(v->type, to)) return v;

    if (v->type && v->type->kind == IR_TYPE_CHAR && to->kind != IR_TYPE_CHAR)
    {
        // فك الترميز إلى i64 ثم التحويل إلى الوجهة المطلوبة.
        IRValue* cp64 = lower_char_decode_to_i64(ctx, v);
        if (to->kind == IR_TYPE_I64) return cp64;
        int r = ir_builder_emit_cast(ctx->builder, cp64, to);
        return ir_value_reg(r, to);
    }
    if (to->kind == IR_TYPE_CHAR) {
        // نعتبر التحويل إلى حرف على أنه ترميز UTF-8 من نقطة-كود (i64).
        if (v->type && v->type->kind == IR_TYPE_I64) {
            return lower_codepoint_encode_to_char(ctx, v);
        }
        // دعم: i32 -> حرف
        if (v->type && v->type->kind == IR_TYPE_I32) {
            IRValue* cp64 = ensure_i64(ctx, v);
            return lower_codepoint_encode_to_char(ctx, cp64);
        }
    }

    int r = ir_builder_emit_cast(ctx->builder, v, to);
    return ir_value_reg(r, to);
}

