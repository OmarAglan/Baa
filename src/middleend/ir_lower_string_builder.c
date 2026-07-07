/**
 * @file ir_lower_string_builder.c
 * @brief خفض واجهة باني النص القياسية إلى IR مدمج.
 */

#define BAA_STRING_BUILDER_HEADER_BYTES 24
#define BAA_STRING_BUILDER_INITIAL_CAPACITY 32

static bool ir_lower_string_builder_name_known(const char* name)
{
    return name &&
           (strcmp(name, "أنشئ_باني_نص") == 0 ||
            strcmp(name, "حرر_باني_نص") == 0 ||
            strcmp(name, "طول_باني_نص") == 0 ||
            strcmp(name, "أضف_نص_للباني") == 0 ||
            strcmp(name, "امسح_باني_نص") == 0 ||
            strcmp(name, "نص_الباني") == 0);
}

static bool ir_lower_string_builder_builtin_shadowed(IRLowerCtx* ctx, IRModule* m, const char* name)
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

static IRValue* ir_lower_sb_null_i8(void)
{
    return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));
}

static IRValue* ir_lower_sb_const_bool(bool value)
{
    return ir_value_const_int(value ? 1 : 0, IR_TYPE_I1_T);
}

static void ir_lower_sb_free_cstr(IRLowerCtx* ctx, IRValue* cstr)
{
    if (!ctx || !ctx->builder || !cstr) return;
    IRValue* args[1] = { cast_to(ctx, cstr, ir_type_ptr(IR_TYPE_I8_T)) };
    ir_builder_emit_call_void(ctx->builder, "free", args, 1);
}

static IRValue* ir_lower_sb_strlen(IRLowerCtx* ctx, const Node* site, IRValue* cstr)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b || !cstr) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* args[1] = { cast_to(ctx, cstr, ir_type_ptr(IR_TYPE_I8_T)) };
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_call(b, "strlen", IR_TYPE_I64_T, args, 1);
    if (r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء strlen لباني النص.");
        return ir_value_const_int(0, IR_TYPE_I64_T);
    }
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_sb_i64_field_ptr(IRLowerCtx* ctx, IRValue* builder, int field_index)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return NULL;

    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* base = cast_to(ctx, builder, i64_ptr_t);
    int r = ir_builder_emit_ptr_offset(b, i64_ptr_t, base, ir_value_const_int(field_index, IR_TYPE_I64_T));
    return ir_value_reg(r, i64_ptr_t);
}

static IRValue* ir_lower_sb_data_field_ptr(IRLowerCtx* ctx, IRValue* builder)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return NULL;

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRValue* base = cast_to(ctx, builder, i8_ptr_ptr_t);
    int r = ir_builder_emit_ptr_offset(b, i8_ptr_ptr_t, base, ir_value_const_int(2, IR_TYPE_I64_T));
    return ir_value_reg(r, i8_ptr_ptr_t);
}

static IRValue* ir_lower_sb_load_i64_field(IRLowerCtx* ctx, IRValue* builder, int field_index)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* ptr = ir_lower_sb_i64_field_ptr(ctx, builder, field_index);
    int r = ir_builder_emit_load(b, IR_TYPE_I64_T, ptr);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static void ir_lower_sb_store_i64_field(IRLowerCtx* ctx, IRValue* builder, int field_index, IRValue* value)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return;

    IRValue* ptr = ir_lower_sb_i64_field_ptr(ctx, builder, field_index);
    ir_builder_emit_store(b, ensure_i64(ctx, value), ptr);
}

static IRValue* ir_lower_sb_load_data(IRLowerCtx* ctx, IRValue* builder)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_sb_null_i8();

    IRValue* ptr = ir_lower_sb_data_field_ptr(ctx, builder);
    int r = ir_builder_emit_load(b, i8_ptr_t, ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static void ir_lower_sb_store_data(IRLowerCtx* ctx, IRValue* builder, IRValue* data)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return;

    IRValue* ptr = ir_lower_sb_data_field_ptr(ctx, builder);
    ir_builder_emit_store(b, cast_to(ctx, data, ir_type_ptr(IR_TYPE_I8_T)), ptr);
}

static void ir_lower_sb_store_byte(IRLowerCtx* ctx, IRValue* base, IRValue* idx, IRValue* byte_value)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b || !base || !idx || !byte_value) return;

    int p = ir_builder_emit_ptr_offset(b, i8_ptr_t, base, ensure_i64(ctx, idx));
    ir_builder_emit_store(b, cast_to(ctx, byte_value, IR_TYPE_I8_T), ir_value_reg(p, i8_ptr_t));
}

static IRValue* ir_lower_sb_create(IRLowerCtx* ctx, const Node* site)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    if (!b) return ir_lower_sb_null_i8();

    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, i8_ptr_t), i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_lower_sb_null_i8(), result_ptr);

    IRValue* header_args[1] = { ir_value_const_int(BAA_STRING_BUILDER_HEADER_BYTES, IR_TYPE_I64_T) };
    ir_lower_set_loc(b, site);
    int header_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, header_args, 1);
    if (header_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc لإنشاء باني نص.");
        return ir_lower_sb_null_i8();
    }

    IRValue* builder = ir_value_reg(header_r, i8_ptr_t);
    IRValue* header_null = ir_value_reg(ir_builder_emit_cmp_eq(b, builder, ir_lower_sb_null_i8()), IR_TYPE_I1_T);

    IRBlock* alloc_data = cf_create_block(ctx, "باني_نص_إنشاء_حجز_بيانات");
    IRBlock* init = cf_create_block(ctx, "باني_نص_إنشاء_تهيئة");
    IRBlock* release_header = cf_create_block(ctx, "باني_نص_إنشاء_تحرير_رأس");
    IRBlock* done = cf_create_block(ctx, "باني_نص_إنشاء_نهاية");
    if (!alloc_data || !init || !release_header || !done) return ir_lower_sb_null_i8();

    ir_builder_emit_br_cond(b, header_null, done, alloc_data);

    ir_builder_set_insert_point(b, alloc_data);
    ir_lower_set_loc(b, site);
    IRValue* data_args[1] = { ir_value_const_int(BAA_STRING_BUILDER_INITIAL_CAPACITY, IR_TYPE_I64_T) };
    int data_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, data_args, 1);
    if (data_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc لبيانات باني النص.");
        ir_builder_emit_br(b, release_header);
    } else {
        IRValue* data = ir_value_reg(data_r, i8_ptr_t);
        IRValue* data_null = ir_value_reg(ir_builder_emit_cmp_eq(b, data, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
        ir_builder_emit_br_cond(b, data_null, release_header, init);

        ir_builder_set_insert_point(b, init);
        ir_lower_set_loc(b, site);
        ir_lower_sb_store_i64_field(ctx, builder, 0, ir_value_const_int(0, IR_TYPE_I64_T));
        ir_lower_sb_store_i64_field(ctx, builder, 1,
                                    ir_value_const_int(BAA_STRING_BUILDER_INITIAL_CAPACITY, IR_TYPE_I64_T));
        ir_lower_sb_store_data(ctx, builder, data);
        ir_lower_sb_store_byte(ctx, data, ir_value_const_int(0, IR_TYPE_I64_T),
                               ir_value_const_int(0, IR_TYPE_I8_T));
        ir_builder_emit_store(b, builder, result_ptr);
        ir_builder_emit_br(b, done);
    }

    ir_builder_set_insert_point(b, release_header);
    ir_lower_set_loc(b, site);
    ir_lower_sb_free_cstr(ctx, builder);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, result_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_sb_free(IRLowerCtx* ctx, const Node* site, IRValue* builder_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return ir_builder_const_i64(0);

    IRValue* builder = cast_to(ctx, builder_in, ir_type_ptr(IR_TYPE_I8_T));
    IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, builder, ir_lower_sb_null_i8()), IR_TYPE_I1_T);

    IRBlock* release = cf_create_block(ctx, "باني_نص_تحرير_عمل");
    IRBlock* done = cf_create_block(ctx, "باني_نص_تحرير_نهاية");
    if (!release || !done) return ir_builder_const_i64(0);

    ir_builder_emit_br_cond(b, is_null, done, release);

    ir_builder_set_insert_point(b, release);
    ir_lower_set_loc(b, site);
    IRValue* data = ir_lower_sb_load_data(ctx, builder);
    ir_lower_sb_free_cstr(ctx, data);
    ir_lower_sb_free_cstr(ctx, builder);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_sb_length(IRLowerCtx* ctx, const Node* site, IRValue* builder_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* builder = cast_to(ctx, builder_in, ir_type_ptr(IR_TYPE_I8_T));
    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), result_ptr);

    IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, builder, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
    IRBlock* read = cf_create_block(ctx, "باني_نص_طول_قراءة");
    IRBlock* done = cf_create_block(ctx, "باني_نص_طول_نهاية");
    if (!read || !done) return ir_value_const_int(0, IR_TYPE_I64_T);

    ir_builder_emit_br_cond(b, is_null, done, read);

    ir_builder_set_insert_point(b, read);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_sb_load_i64_field(ctx, builder, 0), result_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, IR_TYPE_I64_T, result_ptr);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* ir_lower_sb_clear(IRLowerCtx* ctx, const Node* site, IRValue* builder_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return ir_builder_const_i64(0);

    IRValue* builder = cast_to(ctx, builder_in, ir_type_ptr(IR_TYPE_I8_T));
    IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, builder, ir_lower_sb_null_i8()), IR_TYPE_I1_T);

    IRBlock* clear = cf_create_block(ctx, "باني_نص_مسح_عمل");
    IRBlock* done = cf_create_block(ctx, "باني_نص_مسح_نهاية");
    if (!clear || !done) return ir_builder_const_i64(0);

    ir_builder_emit_br_cond(b, is_null, done, clear);

    ir_builder_set_insert_point(b, clear);
    ir_lower_set_loc(b, site);
    ir_lower_sb_store_i64_field(ctx, builder, 0, ir_value_const_int(0, IR_TYPE_I64_T));
    IRValue* data = ir_lower_sb_load_data(ctx, builder);
    IRValue* data_null = ir_value_reg(ir_builder_emit_cmp_eq(b, data, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
    IRBlock* write_nul = cf_create_block(ctx, "باني_نص_مسح_صفر");
    if (!write_nul) return ir_builder_const_i64(0);
    ir_builder_emit_br_cond(b, data_null, done, write_nul);

    ir_builder_set_insert_point(b, write_nul);
    ir_lower_set_loc(b, site);
    ir_lower_sb_store_byte(ctx, data, ir_value_const_int(0, IR_TYPE_I64_T), ir_value_const_int(0, IR_TYPE_I8_T));
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_sb_append(IRLowerCtx* ctx, const Node* site, IRValue* builder_in, IRValue* text_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i1_ptr_t = ir_type_ptr(IR_TYPE_I1_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    if (!b) return ir_lower_sb_const_bool(false);

    IRValue* builder = cast_to(ctx, builder_in, i8_ptr_t);
    IRValue* src = ir_lower_baa_string_to_cstr_alloc(ctx, site, text_in);
    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I1_T), i1_ptr_t);
    IRValue* new_cap_ptr = ir_value_reg(ir_builder_emit_alloca(b, IR_TYPE_I64_T), i64_ptr_t);
    IRValue* data_for_copy_ptr = ir_value_reg(ir_builder_emit_alloca(b, i8_ptr_t), i8_ptr_ptr_t);
    ir_builder_emit_store(b, ir_lower_sb_const_bool(false), result_ptr);
    ir_builder_emit_store(b, ir_lower_sb_null_i8(), data_for_copy_ptr);

    IRValue* builder_null = ir_value_reg(ir_builder_emit_cmp_eq(b, builder, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
    IRValue* src_null = ir_value_reg(ir_builder_emit_cmp_eq(b, src, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
    IRValue* invalid = ir_value_reg(ir_builder_emit_or(b, IR_TYPE_I1_T, builder_null, src_null), IR_TYPE_I1_T);

    IRBlock* measure = cf_create_block(ctx, "باني_نص_إضافة_قياس");
    IRBlock* use_existing = cf_create_block(ctx, "باني_نص_إضافة_استخدم_الحالي");
    IRBlock* grow_init = cf_create_block(ctx, "باني_نص_إضافة_بدء_توسيع");
    IRBlock* grow_head = cf_create_block(ctx, "باني_نص_إضافة_تحقق_توسيع");
    IRBlock* grow_step = cf_create_block(ctx, "باني_نص_إضافة_ضاعف");
    IRBlock* realloc_block = cf_create_block(ctx, "باني_نص_إضافة_إعادة_حجز");
    IRBlock* realloc_ok = cf_create_block(ctx, "باني_نص_إضافة_إعادة_حجز_نجاح");
    IRBlock* copy = cf_create_block(ctx, "باني_نص_إضافة_نسخ");
    IRBlock* cleanup = cf_create_block(ctx, "باني_نص_إضافة_تنظيف");
    IRBlock* done = cf_create_block(ctx, "باني_نص_إضافة_نهاية");
    if (!measure || !use_existing || !grow_init || !grow_head || !grow_step || !realloc_block ||
        !realloc_ok || !copy || !cleanup || !done) {
        return ir_lower_sb_const_bool(false);
    }

    ir_builder_emit_br_cond(b, invalid, cleanup, measure);

    ir_builder_set_insert_point(b, measure);
    ir_lower_set_loc(b, site);
    IRValue* add_len = ir_lower_sb_strlen(ctx, site, src);
    IRValue* old_len = ir_lower_sb_load_i64_field(ctx, builder, 0);
    IRValue* old_cap = ir_lower_sb_load_i64_field(ctx, builder, 1);
    IRValue* data = ir_lower_sb_load_data(ctx, builder);
    IRValue* len_sum = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, old_len, add_len), IR_TYPE_I64_T);
    IRValue* required = ir_value_reg(ir_builder_emit_add(b, IR_TYPE_I64_T, len_sum,
                                                         ir_value_const_int(1, IR_TYPE_I64_T)),
                                     IR_TYPE_I64_T);
    IRValue* enough = ir_value_reg(ir_builder_emit_cmp_le(b, required, old_cap), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, enough, use_existing, grow_init);

    ir_builder_set_insert_point(b, use_existing);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, data, data_for_copy_ptr);
    ir_builder_emit_br(b, copy);

    ir_builder_set_insert_point(b, grow_init);
    ir_lower_set_loc(b, site);
    IRValue* cap_positive = ir_value_reg(ir_builder_emit_cmp_gt(b, old_cap, ir_value_const_int(0, IR_TYPE_I64_T)),
                                         IR_TYPE_I1_T);
    IRBlock* keep_cap = cf_create_block(ctx, "باني_نص_إضافة_احتفظ_بسعة");
    IRBlock* default_cap = cf_create_block(ctx, "باني_نص_إضافة_سعة_ابتدائية");
    if (!keep_cap || !default_cap) return ir_lower_sb_const_bool(false);
    ir_builder_emit_br_cond(b, cap_positive, keep_cap, default_cap);

    ir_builder_set_insert_point(b, keep_cap);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, old_cap, new_cap_ptr);
    ir_builder_emit_br(b, grow_head);

    ir_builder_set_insert_point(b, default_cap);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(BAA_STRING_BUILDER_INITIAL_CAPACITY, IR_TYPE_I64_T), new_cap_ptr);
    ir_builder_emit_br(b, grow_head);

    ir_builder_set_insert_point(b, grow_head);
    ir_lower_set_loc(b, site);
    IRValue* new_cap = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, new_cap_ptr), IR_TYPE_I64_T);
    IRValue* grown_enough = ir_value_reg(ir_builder_emit_cmp_ge(b, new_cap, required), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, grown_enough, realloc_block, grow_step);

    ir_builder_set_insert_point(b, grow_step);
    ir_lower_set_loc(b, site);
    IRValue* doubled = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, new_cap,
                                                        ir_value_const_int(2, IR_TYPE_I64_T)),
                                    IR_TYPE_I64_T);
    ir_builder_emit_store(b, doubled, new_cap_ptr);
    ir_builder_emit_br(b, grow_head);

    ir_builder_set_insert_point(b, realloc_block);
    ir_lower_set_loc(b, site);
    IRValue* final_cap = ir_value_reg(ir_builder_emit_load(b, IR_TYPE_I64_T, new_cap_ptr), IR_TYPE_I64_T);
    IRValue* rargs[2] = { data, final_cap };
    int rr = ir_builder_emit_call(b, "realloc", i8_ptr_t, rargs, 2);
    if (rr < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء realloc لباني النص.");
        ir_builder_emit_br(b, cleanup);
    } else {
        IRValue* new_data = ir_value_reg(rr, i8_ptr_t);
        IRValue* realloc_null = ir_value_reg(ir_builder_emit_cmp_eq(b, new_data, ir_lower_sb_null_i8()),
                                             IR_TYPE_I1_T);
        ir_builder_emit_br_cond(b, realloc_null, cleanup, realloc_ok);

        ir_builder_set_insert_point(b, realloc_ok);
        ir_lower_set_loc(b, site);
        ir_lower_sb_store_data(ctx, builder, new_data);
        ir_lower_sb_store_i64_field(ctx, builder, 1, final_cap);
        ir_builder_emit_store(b, new_data, data_for_copy_ptr);
        ir_builder_emit_br(b, copy);
    }

    ir_builder_set_insert_point(b, copy);
    ir_lower_set_loc(b, site);
    IRValue* selected_data = ir_value_reg(ir_builder_emit_load(b, i8_ptr_t, data_for_copy_ptr), i8_ptr_t);
    int dst_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, selected_data, old_len);
    IRValue* dst = ir_value_reg(dst_r, i8_ptr_t);
    IRValue* margs[3] = { dst, src, add_len };
    int mr = ir_builder_emit_call(b, "memcpy", i8_ptr_t, margs, 3);
    if (mr < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء memcpy لباني النص.");
    }
    ir_lower_sb_store_i64_field(ctx, builder, 0, len_sum);
    ir_lower_sb_store_byte(ctx, selected_data, len_sum, ir_value_const_int(0, IR_TYPE_I8_T));
    ir_builder_emit_store(b, ir_lower_sb_const_bool(true), result_ptr);
    ir_builder_emit_br(b, cleanup);

    ir_builder_set_insert_point(b, cleanup);
    ir_lower_set_loc(b, site);
    ir_lower_sb_free_cstr(ctx, src);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int out_r = ir_builder_emit_load(b, IR_TYPE_I1_T, result_ptr);
    return ir_value_reg(out_r, IR_TYPE_I1_T);
}

static IRValue* ir_lower_sb_to_string(IRLowerCtx* ctx, const Node* site, IRValue* builder_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* char_ptr_t = get_char_ptr_type(b ? b->module : NULL);
    IRType* char_ptr_ptr_t = ir_type_ptr(char_ptr_t);
    if (!b) return ir_value_const_int(0, char_ptr_t);

    IRValue* builder = cast_to(ctx, builder_in, ir_type_ptr(IR_TYPE_I8_T));
    IRValue* result_ptr = ir_value_reg(ir_builder_emit_alloca(b, char_ptr_t), char_ptr_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, char_ptr_t), result_ptr);

    IRValue* builder_null = ir_value_reg(ir_builder_emit_cmp_eq(b, builder, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
    IRBlock* load_data = cf_create_block(ctx, "باني_نص_تحويل_بيانات");
    IRBlock* make = cf_create_block(ctx, "باني_نص_تحويل_نسخ");
    IRBlock* done = cf_create_block(ctx, "باني_نص_تحويل_نهاية");
    if (!load_data || !make || !done) return ir_value_const_int(0, char_ptr_t);

    ir_builder_emit_br_cond(b, builder_null, done, load_data);

    ir_builder_set_insert_point(b, load_data);
    ir_lower_set_loc(b, site);
    IRValue* data = ir_lower_sb_load_data(ctx, builder);
    IRValue* data_null = ir_value_reg(ir_builder_emit_cmp_eq(b, data, ir_lower_sb_null_i8()), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, data_null, done, make);

    ir_builder_set_insert_point(b, make);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_cstr_to_baa_string_alloc(ctx, site, data), result_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, char_ptr_t, result_ptr);
    return ir_value_reg(r, char_ptr_t);
}

static bool ir_lower_try_string_builder_builtin(IRLowerCtx* ctx, Node* expr, IRValue** out_value)
{
    if (out_value) *out_value = NULL;
    if (!ctx || !ctx->builder || !expr || !expr->data.call.name) return false;

    const char* name = expr->data.call.name;
    if (!ir_lower_string_builder_name_known(name)) return false;

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    if (ir_lower_string_builder_builtin_shadowed(ctx, m, name)) return false;

    Node* a0 = expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    Node* a2 = a1 ? a1->next : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_t = get_char_ptr_type(m);

    if (strcmp(name, "أنشئ_باني_نص") == 0) {
        if (a0) {
            ir_lower_report_error(ctx, expr, "استدعاء 'أنشئ_باني_نص' لا يتطلب وسائط.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_sb_null_i8();
            return true;
        }
        if (out_value) *out_value = ir_lower_sb_create(ctx, expr);
        return true;
    }

    if (strcmp(name, "حرر_باني_نص") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'حرر_باني_نص' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_builder_const_i64(0);
            return true;
        }
        IRValue* builder = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_sb_free(ctx, expr, builder);
        return true;
    }

    if (strcmp(name, "طول_باني_نص") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'طول_باني_نص' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_value_const_int(0, IR_TYPE_I64_T);
            return true;
        }
        IRValue* builder = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_sb_length(ctx, expr, builder);
        return true;
    }

    if (strcmp(name, "امسح_باني_نص") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'امسح_باني_نص' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_builder_const_i64(0);
            return true;
        }
        IRValue* builder = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_sb_clear(ctx, expr, builder);
        return true;
    }

    if (strcmp(name, "نص_الباني") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'نص_الباني' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_value_const_int(0, char_ptr_t);
            return true;
        }
        IRValue* builder = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_sb_to_string(ctx, expr, builder);
        return true;
    }

    if (strcmp(name, "أضف_نص_للباني") == 0) {
        if (!a0 || !a1 || a2) {
            ir_lower_report_error(ctx, expr, "استدعاء 'أضف_نص_للباني' يتطلب وسيطين.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_sb_const_bool(false);
            return true;
        }
        IRValue* builder = lower_expr(ctx, a0);
        IRValue* text = lower_expr(ctx, a1);
        if (out_value) *out_value = ir_lower_sb_append(ctx, expr, builder, text);
        return true;
    }

    (void)i8_ptr_t;
    return false;
}

#undef BAA_STRING_BUILDER_INITIAL_CAPACITY
#undef BAA_STRING_BUILDER_HEADER_BYTES
