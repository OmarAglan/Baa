/**
 * @file ir_lower_path.c
 * @brief خفض دوال المسارات القياسية إلى IR مدمج.
 */

#define BAA_PATH_SEP_FWD 47
#define BAA_PATH_SEP_BACK 92
#define BAA_PATH_DOT 46

static bool ir_lower_path_name_known(const char* name)
{
    return name &&
           (strcmp(name, "ضم_مسار") == 0 ||
            strcmp(name, "مجلد_مسار") == 0 ||
            strcmp(name, "اسم_ملف_مسار") == 0 ||
            strcmp(name, "امتداد_مسار") == 0 ||
            strcmp(name, "طبع_مسار") == 0);
}

static IRValue* ir_lower_path_null_i8(void)
{
    return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));
}

static IRValue* ir_lower_path_load_byte(IRLowerCtx* ctx, IRValue* base, IRValue* idx)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b || !base || !idx) return ir_value_const_int(0, IR_TYPE_I8_T);

    IRValue* i64_idx = ensure_i64(ctx, idx);
    int p = ir_builder_emit_ptr_offset(b, i8_ptr_t, base, i64_idx);
    int r = ir_builder_emit_load(b, IR_TYPE_I8_T, ir_value_reg(p, i8_ptr_t));
    return ir_value_reg(r, IR_TYPE_I8_T);
}

static IRValue* ir_lower_path_load_byte_i64(IRLowerCtx* ctx, IRValue* base, IRValue* idx)
{
    IRValue* b8 = ir_lower_path_load_byte(ctx, base, idx);
    return ensure_i64(ctx, b8);
}

static void ir_lower_path_store_byte(IRLowerCtx* ctx, IRValue* base, IRValue* idx, IRValue* byte_value)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b || !base || !idx || !byte_value) return;

    IRValue* i64_idx = ensure_i64(ctx, idx);
    int p = ir_builder_emit_ptr_offset(b, i8_ptr_t, base, i64_idx);
    ir_builder_emit_store(b, cast_to(ctx, byte_value, IR_TYPE_I8_T), ir_value_reg(p, i8_ptr_t));
}

static IRValue* ir_lower_path_is_sep(IRLowerCtx* ctx, IRValue* byte_i64)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b || !byte_i64) return ir_value_const_int(0, IR_TYPE_I1_T);

    int slash = ir_builder_emit_cmp_eq(b, byte_i64, ir_value_const_int(BAA_PATH_SEP_FWD, IR_TYPE_I64_T));
    int back = ir_builder_emit_cmp_eq(b, byte_i64, ir_value_const_int(BAA_PATH_SEP_BACK, IR_TYPE_I64_T));
    int either = ir_builder_emit_or(b, IR_TYPE_I1_T,
                                    ir_value_reg(slash, IR_TYPE_I1_T),
                                    ir_value_reg(back, IR_TYPE_I1_T));
    return ir_value_reg(either, IR_TYPE_I1_T);
}

static IRValue* ir_lower_path_i1_to_count(IRLowerCtx* ctx, const Node* site, IRValue* cond)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b || !cond) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* slot = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), slot);

    IRBlock* one = cf_create_block(ctx, "مسار_تحويل_منطقي_إلى_واحد");
    IRBlock* done = cf_create_block(ctx, "مسار_تحويل_منطقي_نهاية");
    if (!one || !done) return ir_value_const_int(0, IR_TYPE_I64_T);

    ir_builder_emit_br_cond(b, cond, one, done);

    ir_builder_set_insert_point(b, one);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(1, IR_TYPE_I64_T), slot);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, IR_TYPE_I64_T, slot);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static void ir_lower_path_free_cstr(IRLowerCtx* ctx, IRValue* cstr)
{
    if (!ctx || !ctx->builder || !cstr) return;
    IRValue* args[1] = { cast_to(ctx, cstr, ir_type_ptr(IR_TYPE_I8_T)) };
    ir_builder_emit_call_void(ctx->builder, "free", args, 1);
}

static IRValue* ir_lower_path_strlen(IRLowerCtx* ctx, const Node* site, IRValue* cstr)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* args[1] = { cast_to(ctx, cstr, ir_type_ptr(IR_TYPE_I8_T)) };
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_call(b, "strlen", IR_TYPE_I64_T, args, 1);
    if (r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء strlen لمسار.");
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_path_malloc_cstr(IRLowerCtx* ctx, const Node* site, IRValue* len)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_path_null_i8();

    IRValue* bytes = ir_value_reg(
        ir_builder_emit_add(b, IR_TYPE_I64_T, ensure_i64(ctx, len), ir_value_const_int(1, IR_TYPE_I64_T)),
        IR_TYPE_I64_T);
    IRValue* args[1] = { bytes };
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_call(b, "malloc", i8_ptr_t, args, 1);
    if (r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc لمسار.");
        return ir_lower_path_null_i8();
    }
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_path_make_slice(IRLowerCtx* ctx,
                                         const Node* site,
                                         IRValue* src,
                                         IRValue* start,
                                         IRValue* len)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    if (!b) return ir_lower_path_null_i8();

    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, i8_ptr_t), i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_lower_path_null_i8(), result_ptr);

    IRValue* out = ir_lower_path_malloc_cstr(ctx, site, len);
    IRValue* out_null = ir_value_reg(ir_builder_emit_cmp_eq(b, out, ir_lower_path_null_i8()), IR_TYPE_I1_T);

    IRBlock* copy = cf_create_block(ctx, "مسار_نسخ_شريحة");
    IRBlock* done = cf_create_block(ctx, "مسار_نسخ_نهاية");
    if (!copy || !done) return ir_lower_path_null_i8();

    ir_builder_emit_br_cond(b, out_null, done, copy);

    ir_builder_set_insert_point(b, copy);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, out, result_ptr);
    int sp = ir_builder_emit_ptr_offset(b, i8_ptr_t, src, ensure_i64(ctx, start));
    IRValue* src_start = ir_value_reg(sp, i8_ptr_t);
    IRValue* margs[3] = { out, src_start, ensure_i64(ctx, len) };
    int mr = ir_builder_emit_call(b, "memcpy", i8_ptr_t, margs, 3);
    if (mr < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء memcpy لشريحة مسار.");
    }
    ir_lower_path_store_byte(ctx, out, len, ir_value_const_int(0, IR_TYPE_I8_T));
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, result_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_path_baa_from_literal(IRLowerCtx* ctx, const Node* site, const char* literal)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, get_char_ptr_type(NULL));
    IRValue* c = ir_builder_const_string(ctx->builder, literal ? literal : "");
    return ir_lower_cstr_to_baa_string_alloc(ctx, site, c);
}

static IRValue* ir_lower_path_baa_from_owned_cstr(IRLowerCtx* ctx, const Node* site, IRValue* cstr)
{
    IRValue* out = ir_lower_cstr_to_baa_string_alloc(ctx, site, cstr);
    ir_lower_path_free_cstr(ctx, cstr);
    return out;
}

static IRValue* ir_lower_path_normalize_cstr(IRLowerCtx* ctx, const Node* site, IRValue* src)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_lower_path_null_i8();

    IRValue* len = ir_lower_path_strlen(ctx, site, src);
    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, i8_ptr_t), i8_ptr_ptr_t);
    IRValue* in_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    IRValue* out_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    IRValue* prev_sep_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ir_lower_path_null_i8(), result_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), in_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), out_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), prev_sep_ptr);

    IRValue* out = ir_lower_path_malloc_cstr(ctx, site, len);
    IRValue* out_null = ir_value_reg(ir_builder_emit_cmp_eq(b, out, ir_lower_path_null_i8()), IR_TYPE_I1_T);

    IRBlock* head = cf_create_block(ctx, "مسار_تطبيع_تحقق");
    IRBlock* body = cf_create_block(ctx, "مسار_تطبيع_جسم");
    IRBlock* sep = cf_create_block(ctx, "مسار_تطبيع_فاصل");
    IRBlock* maybe_store_sep = cf_create_block(ctx, "مسار_تطبيع_احفظ_فاصل");
    IRBlock* nonsep = cf_create_block(ctx, "مسار_تطبيع_غير_فاصل");
    IRBlock* step = cf_create_block(ctx, "مسار_تطبيع_خطوة");
    IRBlock* check_trailing = cf_create_block(ctx, "مسار_تطبيع_تحقق_النهاية");
    IRBlock* maybe_trim = cf_create_block(ctx, "مسار_تطبيع_ربما_قص");
    IRBlock* trim = cf_create_block(ctx, "مسار_تطبيع_قص");
    IRBlock* write_nul = cf_create_block(ctx, "مسار_تطبيع_صفر");
    IRBlock* done = cf_create_block(ctx, "مسار_تطبيع_نهاية");
    if (!head || !body || !sep || !maybe_store_sep || !nonsep || !step ||
        !check_trailing || !maybe_trim || !trim || !write_nul || !done) {
        return ir_lower_path_null_i8();
    }

    ir_builder_emit_br_cond(b, out_null, done, head);

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, out, result_ptr);
    IRValue* in_i = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, in_ptr), IR_TYPE_I64_T);
    IRValue* more = ir_value_reg(ir_builder_emit_cmp_lt(b, in_i, len), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, more, body, check_trailing);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    IRValue* ch = ir_lower_path_load_byte_i64(ctx, src, in_i);
    IRValue* is_sep = ir_lower_path_is_sep(ctx, ch);
    ir_builder_emit_br_cond(b, is_sep, sep, nonsep);

    ir_builder_set_insert_point(b, sep);
    ir_lower_set_loc(b, site);
    IRValue* out_i_sep = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr), IR_TYPE_I64_T);
    IRValue* prev_sep = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, prev_sep_ptr), IR_TYPE_I64_T);
    IRValue* out_zero = ir_value_reg(ir_builder_emit_cmp_eq(b, out_i_sep, ir_value_const_int(0, IR_TYPE_I64_T)),
                                     IR_TYPE_I1_T);
    IRValue* prev_zero = ir_value_reg(ir_builder_emit_cmp_eq(b, prev_sep, ir_value_const_int(0, IR_TYPE_I64_T)),
                                      IR_TYPE_I1_T);
    IRValue* should_store_sep = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I1_T, out_zero, prev_zero), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, should_store_sep, maybe_store_sep, step);

    ir_builder_set_insert_point(b, maybe_store_sep);
    ir_lower_set_loc(b, site);
    ir_lower_path_store_byte(ctx, out, out_i_sep, ir_value_const_int(BAA_PATH_SEP_FWD, IR_TYPE_I8_T));
    IRValue* out_i_sep_next = ir_value_reg(
        ir_builder_emit_add(b, IR_TYPE_I64_T, out_i_sep, ir_value_const_int(1, IR_TYPE_I64_T)),
        IR_TYPE_I64_T);
    ir_builder_emit_store(b, out_i_sep_next, out_ptr);
    ir_builder_emit_store(b, ir_value_const_int(1, IR_TYPE_I64_T), prev_sep_ptr);
    ir_builder_emit_br(b, step);

    ir_builder_set_insert_point(b, nonsep);
    ir_lower_set_loc(b, site);
    IRValue* out_i = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr), IR_TYPE_I64_T);
    IRValue* raw_ch = ir_lower_path_load_byte(ctx, src, in_i);
    ir_lower_path_store_byte(ctx, out, out_i, raw_ch);
    IRValue* out_next = ir_value_reg(
        ir_builder_emit_add(b, IR_TYPE_I64_T, out_i, ir_value_const_int(1, IR_TYPE_I64_T)),
        IR_TYPE_I64_T);
    ir_builder_emit_store(b, out_next, out_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), prev_sep_ptr);
    ir_builder_emit_br(b, step);

    ir_builder_set_insert_point(b, step);
    ir_lower_set_loc(b, site);
    IRValue* in_next = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, in_i, ir_value_const_int(1, IR_TYPE_I64_T)),
                                    IR_TYPE_I64_T);
    ir_builder_emit_store(b, in_next, in_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, check_trailing);
    ir_lower_set_loc(b, site);
    IRValue* out_count = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr), IR_TYPE_I64_T);
    IRValue* can_trim = ir_value_reg(ir_builder_emit_cmp_gt(b, out_count, ir_value_const_int(1, IR_TYPE_I64_T)),
                                     IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, can_trim, maybe_trim, write_nul);

    ir_builder_set_insert_point(b, maybe_trim);
    ir_lower_set_loc(b, site);
    IRValue* last_idx = ir_value_reg(
        ir_builder_emit_sub(b, IR_TYPE_I64_T, out_count, ir_value_const_int(1, IR_TYPE_I64_T)),
        IR_TYPE_I64_T);
    IRValue* last = ir_lower_path_load_byte_i64(ctx, out, last_idx);
    IRValue* last_is_sep = ir_lower_path_is_sep(ctx, last);
    ir_builder_emit_br_cond(b, last_is_sep, trim, write_nul);

    ir_builder_set_insert_point(b, trim);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, last_idx, out_ptr);
    ir_builder_emit_br(b, write_nul);

    ir_builder_set_insert_point(b, write_nul);
    ir_lower_set_loc(b, site);
    IRValue* final_len = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, out_ptr), IR_TYPE_I64_T);
    ir_lower_path_store_byte(ctx, out, final_len, ir_value_const_int(0, IR_TYPE_I8_T));
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, result_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_path_find_last_sep(IRLowerCtx* ctx, const Node* site, IRValue* cstr, IRValue* end)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_value_const_int(-1, IR_TYPE_I64_T);

    IRValue* i_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    IRValue* found_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ensure_i64(ctx, end), i_ptr);
    ir_builder_emit_store(b, ir_value_const_int(-1, IR_TYPE_I64_T), found_ptr);

    IRBlock* head = cf_create_block(ctx, "مسار_بحث_فاصل_تحقق");
    IRBlock* body = cf_create_block(ctx, "مسار_بحث_فاصل_جسم");
    IRBlock* store = cf_create_block(ctx, "مسار_بحث_فاصل_حفظ");
    IRBlock* done = cf_create_block(ctx, "مسار_بحث_فاصل_نهاية");
    if (!head || !body || !store || !done) return ir_value_const_int(-1, IR_TYPE_I64_T);

    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* i = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, i_ptr), IR_TYPE_I64_T);
    IRValue* found = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, found_ptr), IR_TYPE_I64_T);
    IRValue* has_i = ir_value_reg(ir_builder_emit_cmp_gt(b, i, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    IRValue* not_found = ir_value_reg(ir_builder_emit_cmp_lt(b, found, ir_value_const_int(0, IR_TYPE_I64_T)),
                                      IR_TYPE_I1_T);
    IRValue* cont = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, has_i, not_found), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cont, body, done);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    IRValue* next_i = ir_value_reg(ir_builder_emit_sub(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)),
                                   IR_TYPE_I64_T);
    ir_builder_emit_store(b, next_i, i_ptr);
    IRValue* ch = ir_lower_path_load_byte_i64(ctx, cstr, next_i);
    IRValue* is_sep = ir_lower_path_is_sep(ctx, ch);
    ir_builder_emit_br_cond(b, is_sep, store, head);

    ir_builder_set_insert_point(b, store);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, next_i, found_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, IR_TYPE_I64_T, found_ptr);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_path_find_last_dot_after_start(IRLowerCtx* ctx,
                                                        const Node* site,
                                                        IRValue* cstr,
                                                        IRValue* start,
                                                        IRValue* end)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_value_const_int(-1, IR_TYPE_I64_T);

    IRValue* i_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    IRValue* found_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ensure_i64(ctx, end), i_ptr);
    ir_builder_emit_store(b, ir_value_const_int(-1, IR_TYPE_I64_T), found_ptr);

    IRValue* min_i = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T,
                                                      ensure_i64(ctx, start),
                                                      ir_value_const_int(1, IR_TYPE_I64_T)),
                                  IR_TYPE_I64_T);

    IRBlock* head = cf_create_block(ctx, "مسار_بحث_امتداد_تحقق");
    IRBlock* body = cf_create_block(ctx, "مسار_بحث_امتداد_جسم");
    IRBlock* store = cf_create_block(ctx, "مسار_بحث_امتداد_حفظ");
    IRBlock* done = cf_create_block(ctx, "مسار_بحث_امتداد_نهاية");
    if (!head || !body || !store || !done) return ir_value_const_int(-1, IR_TYPE_I64_T);

    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, head);
    ir_lower_set_loc(b, site);
    IRValue* i = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, i_ptr), IR_TYPE_I64_T);
    IRValue* found = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, found_ptr), IR_TYPE_I64_T);
    IRValue* has_i = ir_value_reg(ir_builder_emit_cmp_gt(b, i, min_i), IR_TYPE_I1_T);
    IRValue* not_found = ir_value_reg(ir_builder_emit_cmp_lt(b, found, ir_value_const_int(0, IR_TYPE_I64_T)),
                                      IR_TYPE_I1_T);
    IRValue* cont = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, has_i, not_found), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cont, body, done);

    ir_builder_set_insert_point(b, body);
    ir_lower_set_loc(b, site);
    IRValue* next_i = ir_value_reg(ir_builder_emit_sub(b, IR_TYPE_I64_T, i, ir_value_const_int(1, IR_TYPE_I64_T)),
                                   IR_TYPE_I64_T);
    ir_builder_emit_store(b, next_i, i_ptr);
    IRValue* ch = ir_lower_path_load_byte_i64(ctx, cstr, next_i);
    IRValue* is_dot = ir_value_reg(ir_builder_emit_cmp_eq(b, ch, ir_value_const_int(BAA_PATH_DOT, IR_TYPE_I64_T)),
                                   IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_dot, store, head);

    ir_builder_set_insert_point(b, store);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, next_i, found_ptr);
    ir_builder_emit_br(b, head);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, IR_TYPE_I64_T, found_ptr);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_path_join_cstr(IRLowerCtx* ctx, const Node* site, IRValue* lhs, IRValue* rhs)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    if (!b) return ir_lower_path_null_i8();

    IRValue* a = ir_lower_path_normalize_cstr(ctx, site, lhs);
    IRValue* c = ir_lower_path_normalize_cstr(ctx, site, rhs);
    IRValue* len_a = ir_lower_path_strlen(ctx, site, a);
    IRValue* len_b = ir_lower_path_strlen(ctx, site, c);

    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, i8_ptr_t), i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_lower_path_null_i8(), result_ptr);

    IRBlock* a_empty = cf_create_block(ctx, "مسار_ضم_أ_فارغ");
    IRBlock* b_check = cf_create_block(ctx, "مسار_ضم_تحقق_ب");
    IRBlock* b_empty = cf_create_block(ctx, "مسار_ضم_ب_فارغ");
    IRBlock* abs_check = cf_create_block(ctx, "مسار_ضم_تحقق_مطلق");
    IRBlock* rhs_abs = cf_create_block(ctx, "مسار_ضم_ب_مطلق");
    IRBlock* join = cf_create_block(ctx, "مسار_ضم_عمل");
    IRBlock* copy_join = cf_create_block(ctx, "مسار_ضم_نسخ");
    IRBlock* sep = cf_create_block(ctx, "مسار_ضم_فاصل");
    IRBlock* copy_rhs = cf_create_block(ctx, "مسار_ضم_نسخ_ب");
    IRBlock* cleanup = cf_create_block(ctx, "مسار_ضم_تنظيف");
    IRBlock* done = cf_create_block(ctx, "مسار_ضم_نهاية");
    if (!a_empty || !b_check || !b_empty || !abs_check || !rhs_abs || !join ||
        !copy_join || !sep || !copy_rhs || !cleanup || !done) {
        return ir_lower_path_null_i8();
    }

    IRValue* is_a_empty = ir_value_reg(ir_builder_emit_cmp_eq(b, len_a, ir_value_const_int(0, IR_TYPE_I64_T)),
                                       IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_a_empty, a_empty, b_check);

    ir_builder_set_insert_point(b, a_empty);
    ir_lower_set_loc(b, site);
    IRValue* only_b = ir_lower_path_make_slice(ctx, site, c, ir_value_const_int(0, IR_TYPE_I64_T), len_b);
    ir_builder_emit_store(b, only_b, result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, b_check);
    ir_lower_set_loc(b, site);
    IRValue* is_b_empty = ir_value_reg(ir_builder_emit_cmp_eq(b, len_b, ir_value_const_int(0, IR_TYPE_I64_T)),
                                       IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_b_empty, b_empty, abs_check);

    ir_builder_set_insert_point(b, b_empty);
    ir_lower_set_loc(b, site);
    IRValue* only_a = ir_lower_path_make_slice(ctx, site, a, ir_value_const_int(0, IR_TYPE_I64_T), len_a);
    ir_builder_emit_store(b, only_a, result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, abs_check);
    ir_lower_set_loc(b, site);
    IRValue* first_b = ir_lower_path_load_byte_i64(ctx, c, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* b_is_abs = ir_lower_path_is_sep(ctx, first_b);
    ir_builder_emit_br_cond(b, b_is_abs, rhs_abs, join);

    ir_builder_set_insert_point(b, rhs_abs);
    ir_lower_set_loc(b, site);
    IRValue* abs_b = ir_lower_path_make_slice(ctx, site, c, ir_value_const_int(0, IR_TYPE_I64_T), len_b);
    ir_builder_emit_store(b, abs_b, result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, join);
    ir_lower_set_loc(b, site);
    IRValue* last_a_idx = ir_value_reg(ir_builder_emit_sub(b, IR_TYPE_I64_T, len_a, ir_value_const_int(1, IR_TYPE_I64_T)),
                                       IR_TYPE_I64_T);
    IRValue* last_a = ir_lower_path_load_byte_i64(ctx, a, last_a_idx);
    IRValue* last_is_sep = ir_lower_path_is_sep(ctx, last_a);
    IRValue* need_sep = ir_value_reg(ir_builder_emit_not(b, IR_TYPE_I1_T, last_is_sep), IR_TYPE_I1_T);
    IRValue* sep_i64 = ir_lower_path_i1_to_count(ctx, site, need_sep);
    IRValue* base_sum = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len_a, len_b), IR_TYPE_I64_T);
    IRValue* total = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, base_sum, sep_i64), IR_TYPE_I64_T);
    IRValue* out = ir_lower_path_malloc_cstr(ctx, site, total);
    IRValue* out_null = ir_value_reg(ir_builder_emit_cmp_eq(b, out, ir_lower_path_null_i8()), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, out_null, cleanup, copy_join);

    ir_builder_set_insert_point(b, copy_join);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, out, result_ptr);
    IRValue* margs_a[3] = { out, a, len_a };
    (void)ir_builder_emit_call(b, "memcpy", i8_ptr_t, margs_a, 3);
    ir_builder_emit_br_cond(b, need_sep, sep, copy_rhs);

    ir_builder_set_insert_point(b, sep);
    ir_lower_set_loc(b, site);
    ir_lower_path_store_byte(ctx, out, len_a, ir_value_const_int(BAA_PATH_SEP_FWD, IR_TYPE_I8_T));
    ir_builder_emit_br(b, copy_rhs);

    ir_builder_set_insert_point(b, copy_rhs);
    ir_lower_set_loc(b, site);
    IRValue* rhs_off = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len_a, sep_i64), IR_TYPE_I64_T);
    int dst_rhs_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, out, rhs_off);
    IRValue* dst_rhs = ir_value_reg(dst_rhs_r, i8_ptr_t);
    IRValue* margs_b[3] = { dst_rhs, c, len_b };
    (void)ir_builder_emit_call(b, "memcpy", i8_ptr_t, margs_b, 3);
    ir_lower_path_store_byte(ctx, out, total, ir_value_const_int(0, IR_TYPE_I8_T));
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, cleanup);
    ir_lower_set_loc(b, site);
    ir_lower_path_free_cstr(ctx, a);
    ir_lower_path_free_cstr(ctx, c);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, result_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_path_dirname_baa(IRLowerCtx* ctx, const Node* site, IRValue* cstr)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* char_ptr_t = get_char_ptr_type(b ? b->module : NULL);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    if (!b) return ir_value_const_int(0, char_ptr_t);

    IRValue* norm = ir_lower_path_normalize_cstr(ctx, site, cstr);
    IRValue* len = ir_lower_path_strlen(ctx, site, norm);
    IRValue* sep = ir_lower_path_find_last_sep(ctx, site, norm, len);

    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, char_ptr_t), char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), result_ptr);

    IRBlock* no_sep = cf_create_block(ctx, "مسار_مجلد_بلا_فاصل");
    IRBlock* root_check = cf_create_block(ctx, "مسار_مجلد_تحقق_جذر");
    IRBlock* root_value = cf_create_block(ctx, "مسار_مجلد_جذر");
    IRBlock* slice = cf_create_block(ctx, "مسار_مجلد_شريحة");
    IRBlock* cleanup = cf_create_block(ctx, "مسار_مجلد_تنظيف");
    IRBlock* done = cf_create_block(ctx, "مسار_مجلد_نهاية");
    if (!no_sep || !root_check || !root_value || !slice || !cleanup || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    IRValue* sep_missing = ir_value_reg(ir_builder_emit_cmp_lt(b, sep, ir_value_const_int(0, IR_TYPE_I64_T)),
                                        IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, sep_missing, no_sep, root_check);

    ir_builder_set_insert_point(b, no_sep);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_path_baa_from_literal(ctx, site, "."), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, root_check);
    ir_lower_set_loc(b, site);
    IRValue* is_root = ir_value_reg(ir_builder_emit_cmp_eq(b, sep, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_root, root_value, slice);

    ir_builder_set_insert_point(b, root_value);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_path_baa_from_literal(ctx, site, "/"), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, slice);
    ir_lower_set_loc(b, site);
    IRValue* s = ir_lower_path_make_slice(ctx, site, norm, ir_value_const_int(0, IR_TYPE_I64_T), sep);
    ir_builder_emit_store(b, ir_lower_path_baa_from_owned_cstr(ctx, site, s), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, cleanup);
    ir_lower_set_loc(b, site);
    ir_lower_path_free_cstr(ctx, norm);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, char_ptr_t, result_ptr);
    return ir_value_reg(r, char_ptr_t);
}

static IRValue* ir_lower_path_basename_baa(IRLowerCtx* ctx, const Node* site, IRValue* cstr)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* char_ptr_t = get_char_ptr_type(b ? b->module : NULL);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_value_const_int(0, char_ptr_t);

    IRValue* norm = ir_lower_path_normalize_cstr(ctx, site, cstr);
    IRValue* len = ir_lower_path_strlen(ctx, site, norm);
    IRValue* sep = ir_lower_path_find_last_sep(ctx, site, norm, len);

    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, char_ptr_t), char_ptr_ptr_t);
    IRValue* start_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), result_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), start_ptr);

    IRBlock* root_check = cf_create_block(ctx, "مسار_اسم_تحقق_جذر");
    IRBlock* root_value = cf_create_block(ctx, "مسار_اسم_جذر");
    IRBlock* sep_check = cf_create_block(ctx, "مسار_اسم_تحقق_فاصل");
    IRBlock* set_start = cf_create_block(ctx, "مسار_اسم_بداية");
    IRBlock* make = cf_create_block(ctx, "مسار_اسم_شريحة");
    IRBlock* cleanup = cf_create_block(ctx, "مسار_اسم_تنظيف");
    IRBlock* done = cf_create_block(ctx, "مسار_اسم_نهاية");
    if (!root_check || !root_value || !sep_check || !set_start || !make || !cleanup || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    ir_builder_emit_br(b, root_check);

    ir_builder_set_insert_point(b, root_check);
    ir_lower_set_loc(b, site);
    IRValue* len_one = ir_value_reg(ir_builder_emit_cmp_eq(b, len, ir_value_const_int(1, IR_TYPE_I64_T)),
                                    IR_TYPE_I1_T);
    IRValue* first = ir_lower_path_load_byte_i64(ctx, norm, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* first_sep = ir_lower_path_is_sep(ctx, first);
    IRValue* is_root = ir_value_reg(ir_builder_emit_and(b, IR_TYPE_I1_T, len_one, first_sep), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, is_root, root_value, sep_check);

    ir_builder_set_insert_point(b, root_value);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_path_baa_from_literal(ctx, site, "/"), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, sep_check);
    ir_lower_set_loc(b, site);
    IRValue* has_sep = ir_value_reg(ir_builder_emit_cmp_ge(b, sep, ir_value_const_int(0, IR_TYPE_I64_T)),
                                    IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, has_sep, set_start, make);

    ir_builder_set_insert_point(b, set_start);
    ir_lower_set_loc(b, site);
    IRValue* start = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, sep, ir_value_const_int(1, IR_TYPE_I64_T)),
                                  IR_TYPE_I64_T);
    ir_builder_emit_store(b, start, start_ptr);
    ir_builder_emit_br(b, make);

    ir_builder_set_insert_point(b, make);
    ir_lower_set_loc(b, site);
    IRValue* start_v = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, start_ptr), IR_TYPE_I64_T);
    IRValue* slice_len = ir_value_reg(ir_builder_emit_sub(b, IR_TYPE_I64_T, len, start_v), IR_TYPE_I64_T);
    IRValue* s = ir_lower_path_make_slice(ctx, site, norm, start_v, slice_len);
    ir_builder_emit_store(b, ir_lower_path_baa_from_owned_cstr(ctx, site, s), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, cleanup);
    ir_lower_set_loc(b, site);
    ir_lower_path_free_cstr(ctx, norm);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, char_ptr_t, result_ptr);
    return ir_value_reg(r, char_ptr_t);
}

static IRValue* ir_lower_path_extension_baa(IRLowerCtx* ctx, const Node* site, IRValue* cstr)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* char_ptr_t = get_char_ptr_type(b ? b->module : NULL);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_value_const_int(0, char_ptr_t);

    IRValue* norm = ir_lower_path_normalize_cstr(ctx, site, cstr);
    IRValue* len = ir_lower_path_strlen(ctx, site, norm);
    IRValue* sep = ir_lower_path_find_last_sep(ctx, site, norm, len);

    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, char_ptr_t), char_ptr_ptr_t);
    IRValue* start_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), result_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), start_ptr);

    IRBlock* sep_check = cf_create_block(ctx, "مسار_امتداد_تحقق_فاصل");
    IRBlock* set_start = cf_create_block(ctx, "مسار_امتداد_بداية");
    IRBlock* find_dot = cf_create_block(ctx, "مسار_امتداد_بحث");
    IRBlock* no_ext = cf_create_block(ctx, "مسار_امتداد_لا_يوجد");
    IRBlock* slice = cf_create_block(ctx, "مسار_امتداد_شريحة");
    IRBlock* cleanup = cf_create_block(ctx, "مسار_امتداد_تنظيف");
    IRBlock* done = cf_create_block(ctx, "مسار_امتداد_نهاية");
    if (!sep_check || !set_start || !find_dot || !no_ext || !slice || !cleanup || !done) {
        return ir_value_const_int(0, char_ptr_t);
    }

    ir_builder_emit_br(b, sep_check);

    ir_builder_set_insert_point(b, sep_check);
    ir_lower_set_loc(b, site);
    IRValue* has_sep = ir_value_reg(ir_builder_emit_cmp_ge(b, sep, ir_value_const_int(0, IR_TYPE_I64_T)),
                                    IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, has_sep, set_start, find_dot);

    ir_builder_set_insert_point(b, set_start);
    ir_lower_set_loc(b, site);
    IRValue* start = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, sep, ir_value_const_int(1, IR_TYPE_I64_T)),
                                  IR_TYPE_I64_T);
    ir_builder_emit_store(b, start, start_ptr);
    ir_builder_emit_br(b, find_dot);

    ir_builder_set_insert_point(b, find_dot);
    ir_lower_set_loc(b, site);
    IRValue* start_v = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, start_ptr), IR_TYPE_I64_T);
    IRValue* dot = ir_lower_path_find_last_dot_after_start(ctx, site, norm, start_v, len);
    IRValue* dot_missing = ir_value_reg(ir_builder_emit_cmp_lt(b, dot, ir_value_const_int(0, IR_TYPE_I64_T)),
                                        IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, dot_missing, no_ext, slice);

    ir_builder_set_insert_point(b, no_ext);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_path_baa_from_literal(ctx, site, ""), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, slice);
    ir_lower_set_loc(b, site);
    IRValue* slice_len = ir_value_reg(ir_builder_emit_sub(b, IR_TYPE_I64_T, len, dot), IR_TYPE_I64_T);
    IRValue* s = ir_lower_path_make_slice(ctx, site, norm, dot, slice_len);
    ir_builder_emit_store(b, ir_lower_path_baa_from_owned_cstr(ctx, site, s), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, cleanup);
    ir_lower_set_loc(b, site);
    ir_lower_path_free_cstr(ctx, norm);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, char_ptr_t, result_ptr);
    return ir_value_reg(r, char_ptr_t);
}

static bool ir_lower_path_builtin_shadowed(IRLowerCtx* ctx, IRModule* m, const char* name)
{
    if (!name) return true;
    if (find_local(ctx, name)) return true;
    if (m && ir_module_find_global(m, name)) return true;
    if (m) {
        IRFunc* f = ir_module_find_func(m, name);
        if (f && !f->is_prototype) return true;
    }
    return false;
}

static bool ir_lower_try_path_builtin(IRLowerCtx* ctx, Node* expr, IRValue** out_value)
{
    if (out_value) *out_value = NULL;
    if (!ctx || !ctx->builder || !expr || !expr->data.call.name) return false;

    const char* name = expr->data.call.name;
    if (!ir_lower_path_name_known(name)) return false;

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    if (ir_lower_path_builtin_shadowed(ctx, m, name)) return false;

    Node* a0 = expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    Node* a2 = a1 ? a1->next : NULL;
    IRType* char_ptr_t = get_char_ptr_type(m);

    if (strcmp(name, "ضم_مسار") == 0) {
        if (!a0 || !a1 || a2) {
            ir_lower_report_error(ctx, expr, "استدعاء 'ضم_مسار' يتطلب وسيطين.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_value_const_int(0, char_ptr_t);
            return true;
        }
        IRValue* lhs_baa = lower_expr(ctx, a0);
        IRValue* rhs_baa = lower_expr(ctx, a1);
        IRValue* lhs = ir_lower_baa_string_to_cstr_alloc(ctx, expr, lhs_baa);
        IRValue* rhs = ir_lower_baa_string_to_cstr_alloc(ctx, expr, rhs_baa);
        IRValue* joined = ir_lower_path_join_cstr(ctx, expr, lhs, rhs);
        IRValue* baa = ir_lower_path_baa_from_owned_cstr(ctx, expr, joined);
        ir_lower_path_free_cstr(ctx, lhs);
        ir_lower_path_free_cstr(ctx, rhs);
        if (out_value) *out_value = baa;
        return true;
    }

    if (strcmp(name, "طبع_مسار") == 0 ||
        strcmp(name, "مجلد_مسار") == 0 ||
        strcmp(name, "اسم_ملف_مسار") == 0 ||
        strcmp(name, "امتداد_مسار") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء '%s' يتطلب وسيطاً واحداً.", name);
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_value_const_int(0, char_ptr_t);
            return true;
        }

        IRValue* path_baa = lower_expr(ctx, a0);
        IRValue* path_c = ir_lower_baa_string_to_cstr_alloc(ctx, expr, path_baa);
        IRValue* baa = NULL;
        if (strcmp(name, "طبع_مسار") == 0) {
            IRValue* norm = ir_lower_path_normalize_cstr(ctx, expr, path_c);
            baa = ir_lower_path_baa_from_owned_cstr(ctx, expr, norm);
        } else if (strcmp(name, "مجلد_مسار") == 0) {
            baa = ir_lower_path_dirname_baa(ctx, expr, path_c);
        } else if (strcmp(name, "اسم_ملف_مسار") == 0) {
            baa = ir_lower_path_basename_baa(ctx, expr, path_c);
        } else {
            baa = ir_lower_path_extension_baa(ctx, expr, path_c);
        }
        ir_lower_path_free_cstr(ctx, path_c);
        if (out_value) *out_value = baa ? baa : ir_value_const_int(0, char_ptr_t);
        return true;
    }

    return false;
}

#undef BAA_PATH_DOT
#undef BAA_PATH_SEP_BACK
#undef BAA_PATH_SEP_FWD
