/**
 * @file ir_lower_vector.c
 * @brief خفض دوال المتجه الديناميكي القياسية إلى IR مدمج.
 */

#define BAA_VECTOR_HEADER_BYTES 32
#define BAA_VECTOR_INITIAL_CAPACITY 4

/**
 * @brief هل اسم دالة stdlib محجوب بتعريف مستخدم أو تخزين محلي؟
 */
static bool ir_lower_vector_builtin_shadowed(IRLowerCtx* ctx, IRModule* m, const char* name)
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

static bool ir_lower_vector_name_known(const char* name)
{
    return name &&
           (strcmp(name, "أنشئ_متجه") == 0 ||
            strcmp(name, "حرر_متجه") == 0 ||
            strcmp(name, "طول_متجه") == 0 ||
            strcmp(name, "سعة_متجه") == 0 ||
            strcmp(name, "بيانات_متجه") == 0 ||
            strcmp(name, "ادفع_متجه") == 0 ||
            strcmp(name, "اسحب_متجه") == 0);
}

static bool ir_lower_byte_buffer_name_known(const char* name)
{
    return name &&
           (strcmp(name, "أنشئ_مخزن_بايتات") == 0 ||
            strcmp(name, "حرر_مخزن_بايتات") == 0 ||
            strcmp(name, "طول_مخزن_بايتات") == 0 ||
            strcmp(name, "سعة_مخزن_بايتات") == 0 ||
            strcmp(name, "بيانات_مخزن_بايتات") == 0 ||
            strcmp(name, "أضف_بايت") == 0);
}

static IRValue* ir_lower_vector_const_bool(bool value)
{
    return ir_value_const_int(value ? 1 : 0, IR_TYPE_I1_T);
}

static IRValue* ir_lower_vector_const_ptr(IRType* ptr_type)
{
    return ir_value_const_int(0, ptr_type ? ptr_type : ir_type_ptr(IR_TYPE_I8_T));
}

static IRValue* ir_lower_vector_i64_field_ptr(IRLowerCtx* ctx, IRValue* vec, int field_index)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return NULL;

    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRValue* base = cast_to(ctx, vec, i64_ptr_t);
    IRValue* index = ir_value_const_int(field_index, IR_TYPE_I64_T);
    int r = ir_builder_emit_ptr_offset(b, i64_ptr_t, base, index);
    return ir_value_reg(r, i64_ptr_t);
}

static IRValue* ir_lower_vector_data_field_ptr(IRLowerCtx* ctx, IRValue* vec)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return NULL;

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRValue* base = cast_to(ctx, vec, i8_ptr_ptr_t);
    IRValue* index = ir_value_const_int(3, IR_TYPE_I64_T);
    int r = ir_builder_emit_ptr_offset(b, i8_ptr_ptr_t, base, index);
    return ir_value_reg(r, i8_ptr_ptr_t);
}

static IRValue* ir_lower_vector_load_i64_field(IRLowerCtx* ctx, IRValue* vec, int field_index)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* ptr = ir_lower_vector_i64_field_ptr(ctx, vec, field_index);
    int r = ir_builder_emit_load(b, IR_TYPE_I64_T, ptr);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static void ir_lower_vector_store_i64_field(IRLowerCtx* ctx, IRValue* vec, int field_index, IRValue* value)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return;

    IRValue* ptr = ir_lower_vector_i64_field_ptr(ctx, vec, field_index);
    ir_builder_emit_store(b, ensure_i64(ctx, value), ptr);
}

static IRValue* ir_lower_vector_load_data(IRLowerCtx* ctx, IRValue* vec)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_vector_const_ptr(i8_ptr_t);

    IRValue* ptr = ir_lower_vector_data_field_ptr(ctx, vec);
    int r = ir_builder_emit_load(b, i8_ptr_t, ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static void ir_lower_vector_store_data(IRLowerCtx* ctx, IRValue* vec, IRValue* data)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return;

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* ptr = ir_lower_vector_data_field_ptr(ctx, vec);
    ir_builder_emit_store(b, cast_to(ctx, data, i8_ptr_t), ptr);
}

static IRValue* ir_lower_vector_alloca_result(IRLowerCtx* ctx, IRType* value_type)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b) return NULL;

    int ptr_reg = ir_builder_emit_alloca(b, value_type);
    return ir_value_reg(ptr_reg, ir_type_ptr(value_type));
}

static IRValue* ir_lower_vector_load_result(IRLowerCtx* ctx, IRValue* result_ptr, IRType* value_type)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    if (!b || !result_ptr) return ir_value_const_int(0, value_type);

    int r = ir_builder_emit_load(b, value_type, result_ptr);
    return ir_value_reg(r, value_type);
}

static IRValue* ir_lower_vector_create(IRLowerCtx* ctx, const Node* site, IRValue* elem_size_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_vector_const_ptr(i8_ptr_t);

    IRValue* result_ptr = ir_lower_vector_alloca_result(ctx, i8_ptr_t);

    IRValue* elem_size = ensure_i64(ctx, elem_size_in);
    IRValue* invalid_size = ir_value_reg(
        ir_builder_emit_cmp_le(b, elem_size, ir_value_const_int(0, IR_TYPE_I64_T)),
        IR_TYPE_I1_T);

    IRBlock* invalid = cf_create_block(ctx, "متجه_إنشاء_حجم_مرفوض");
    IRBlock* alloc = cf_create_block(ctx, "متجه_إنشاء_حجز");
    IRBlock* init = cf_create_block(ctx, "متجه_إنشاء_تهيئة");
    IRBlock* done = cf_create_block(ctx, "متجه_إنشاء_نهاية");
    if (!invalid || !alloc || !init || !done) {
        return ir_lower_vector_const_ptr(i8_ptr_t);
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, invalid_size, invalid, alloc);
    }

    ir_builder_set_insert_point(b, invalid);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_lower_vector_const_ptr(i8_ptr_t), result_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, alloc);
    ir_lower_set_loc(b, site);
    IRValue* malloc_args[1] = { ir_value_const_int(BAA_VECTOR_HEADER_BYTES, IR_TYPE_I64_T) };
    int raw_r = ir_builder_emit_call(b, "malloc", i8_ptr_t, malloc_args, 1);
    if (raw_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء malloc لإنشاء متجه.");
        ir_builder_emit_store(b, ir_lower_vector_const_ptr(i8_ptr_t), result_ptr);
        ir_builder_emit_br(b, done);
    } else {
        IRValue* vec = ir_value_reg(raw_r, i8_ptr_t);
        IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, vec, ir_lower_vector_const_ptr(i8_ptr_t)),
                                        IR_TYPE_I1_T);
        ir_builder_emit_store(b, ir_lower_vector_const_ptr(i8_ptr_t), result_ptr);
        ir_builder_emit_br_cond(b, is_null, done, init);

        ir_builder_set_insert_point(b, init);
        ir_lower_set_loc(b, site);
        ir_lower_vector_store_i64_field(ctx, vec, 0, ir_value_const_int(0, IR_TYPE_I64_T));
        ir_lower_vector_store_i64_field(ctx, vec, 1, ir_value_const_int(0, IR_TYPE_I64_T));
        ir_lower_vector_store_i64_field(ctx, vec, 2, elem_size);
        ir_lower_vector_store_data(ctx, vec, ir_lower_vector_const_ptr(i8_ptr_t));
        ir_builder_emit_store(b, vec, result_ptr);
        ir_builder_emit_br(b, done);
    }

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_lower_vector_load_result(ctx, result_ptr, i8_ptr_t);
}

static IRValue* ir_lower_vector_free(IRLowerCtx* ctx, const Node* site, IRValue* vec_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_builder_const_i64(0);

    IRValue* vec = cast_to(ctx, vec_in, i8_ptr_t);
    IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, vec, ir_lower_vector_const_ptr(i8_ptr_t)),
                                    IR_TYPE_I1_T);

    IRBlock* release = cf_create_block(ctx, "متجه_تحرير_عمل");
    IRBlock* done = cf_create_block(ctx, "متجه_تحرير_نهاية");
    if (!release || !done) return ir_builder_const_i64(0);

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, release);
    }

    ir_builder_set_insert_point(b, release);
    ir_lower_set_loc(b, site);
    IRValue* data = ir_lower_vector_load_data(ctx, vec);
    IRValue* data_args[1] = { data };
    ir_builder_emit_call_void(b, "free", data_args, 1);
    IRValue* vec_args[1] = { vec };
    ir_builder_emit_call_void(b, "free", vec_args, 1);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_vector_query_i64(IRLowerCtx* ctx, const Node* site, IRValue* vec_in, int field_index)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_value_const_int(0, IR_TYPE_I64_T);

    IRValue* result_ptr = ir_lower_vector_alloca_result(ctx, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), result_ptr);

    IRValue* vec = cast_to(ctx, vec_in, i8_ptr_t);
    IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, vec, ir_lower_vector_const_ptr(i8_ptr_t)),
                                    IR_TYPE_I1_T);

    IRBlock* load = cf_create_block(ctx, "متجه_قراءة_قيمة");
    IRBlock* done = cf_create_block(ctx, "متجه_قراءة_نهاية");
    if (!load || !done) return ir_value_const_int(0, IR_TYPE_I64_T);

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, load);
    }

    ir_builder_set_insert_point(b, load);
    ir_lower_set_loc(b, site);
    IRValue* value = ir_lower_vector_load_i64_field(ctx, vec, field_index);
    ir_builder_emit_store(b, value, result_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_lower_vector_load_result(ctx, result_ptr, IR_TYPE_I64_T);
}

static IRValue* ir_lower_vector_data(IRLowerCtx* ctx, const Node* site, IRValue* vec_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_vector_const_ptr(i8_ptr_t);

    IRValue* result_ptr = ir_lower_vector_alloca_result(ctx, i8_ptr_t);
    ir_builder_emit_store(b, ir_lower_vector_const_ptr(i8_ptr_t), result_ptr);

    IRValue* vec = cast_to(ctx, vec_in, i8_ptr_t);
    IRValue* is_null = ir_value_reg(ir_builder_emit_cmp_eq(b, vec, ir_lower_vector_const_ptr(i8_ptr_t)),
                                    IR_TYPE_I1_T);

    IRBlock* load = cf_create_block(ctx, "متجه_بيانات_قراءة");
    IRBlock* done = cf_create_block(ctx, "متجه_بيانات_نهاية");
    if (!load || !done) return ir_lower_vector_const_ptr(i8_ptr_t);

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, done, load);
    }

    ir_builder_set_insert_point(b, load);
    ir_lower_set_loc(b, site);
    IRValue* data = ir_lower_vector_load_data(ctx, vec);
    ir_builder_emit_store(b, data, result_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_lower_vector_load_result(ctx, result_ptr, i8_ptr_t);
}

static IRValue* ir_lower_vector_push(IRLowerCtx* ctx, const Node* site, IRValue* vec_in, IRValue* elem_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_vector_const_bool(false);

    IRValue* result_ptr = ir_lower_vector_alloca_result(ctx, IR_TYPE_I1_T);
    IRValue* new_cap_ptr = ir_lower_vector_alloca_result(ctx, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ir_lower_vector_const_bool(false), result_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), new_cap_ptr);

    IRValue* vec = cast_to(ctx, vec_in, i8_ptr_t);
    IRValue* elem = cast_to(ctx, elem_in, i8_ptr_t);

    IRBlock* check_elem = cf_create_block(ctx, "متجه_إدخال_تحقق_عنصر");
    IRBlock* work = cf_create_block(ctx, "متجه_إدخال_عمل");
    IRBlock* grow = cf_create_block(ctx, "متجه_إدخال_توسيع");
    IRBlock* cap_zero = cf_create_block(ctx, "متجه_إدخال_سعة_أولية");
    IRBlock* cap_double = cf_create_block(ctx, "متجه_إدخال_مضاعفة");
    IRBlock* after_cap = cf_create_block(ctx, "متجه_إدخال_بعد_السعة");
    IRBlock* copy = cf_create_block(ctx, "متجه_إدخال_نسخ");
    IRBlock* done = cf_create_block(ctx, "متجه_إدخال_نهاية");
    if (!check_elem || !work || !grow || !cap_zero || !cap_double || !after_cap || !copy || !done) {
        return ir_lower_vector_const_bool(false);
    }

    IRValue* vec_null = ir_value_reg(ir_builder_emit_cmp_eq(b, vec, ir_lower_vector_const_ptr(i8_ptr_t)),
                                     IR_TYPE_I1_T);
    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, vec_null, done, check_elem);
    }

    ir_builder_set_insert_point(b, check_elem);
    ir_lower_set_loc(b, site);
    IRValue* elem_null = ir_value_reg(ir_builder_emit_cmp_eq(b, elem, ir_lower_vector_const_ptr(i8_ptr_t)),
                                      IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, elem_null, done, work);

    ir_builder_set_insert_point(b, work);
    ir_lower_set_loc(b, site);
    IRValue* len = ir_lower_vector_load_i64_field(ctx, vec, 0);
    IRValue* cap = ir_lower_vector_load_i64_field(ctx, vec, 1);
    IRValue* elem_size = ir_lower_vector_load_i64_field(ctx, vec, 2);
    IRValue* bad_elem_size = ir_value_reg(
        ir_builder_emit_cmp_le(b, elem_size, ir_value_const_int(0, IR_TYPE_I64_T)),
        IR_TYPE_I1_T);
    IRBlock* check_cap = cf_create_block(ctx, "متجه_إدخال_تحقق_السعة");
    if (!check_cap) check_cap = done;
    ir_builder_emit_br_cond(b, bad_elem_size, done, check_cap);

    ir_builder_set_insert_point(b, check_cap);
    ir_lower_set_loc(b, site);
    IRValue* need_grow = ir_value_reg(ir_builder_emit_cmp_ge(b, len, cap), IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, need_grow, grow, copy);

    ir_builder_set_insert_point(b, grow);
    ir_lower_set_loc(b, site);
    IRValue* cap_is_zero = ir_value_reg(
        ir_builder_emit_cmp_eq(b, cap, ir_value_const_int(0, IR_TYPE_I64_T)),
        IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, cap_is_zero, cap_zero, cap_double);

    ir_builder_set_insert_point(b, cap_zero);
    ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, ir_value_const_int(BAA_VECTOR_INITIAL_CAPACITY, IR_TYPE_I64_T), new_cap_ptr);
    ir_builder_emit_br(b, after_cap);

    ir_builder_set_insert_point(b, cap_double);
    ir_lower_set_loc(b, site);
    IRValue* doubled = ir_value_reg(
        ir_builder_emit_mul(b, IR_TYPE_I64_T, cap, ir_value_const_int(2, IR_TYPE_I64_T)),
        IR_TYPE_I64_T);
    ir_builder_emit_store(b, doubled, new_cap_ptr);
    ir_builder_emit_br(b, after_cap);

    ir_builder_set_insert_point(b, after_cap);
    ir_lower_set_loc(b, site);
    IRValue* new_cap = ir_lower_vector_load_result(ctx, new_cap_ptr, IR_TYPE_I64_T);
    IRValue* bytes = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, new_cap, elem_size),
                                  IR_TYPE_I64_T);
    IRValue* old_data = ir_lower_vector_load_data(ctx, vec);
    IRValue* realloc_args[2] = { old_data, bytes };
    int new_data_r = ir_builder_emit_call(b, "realloc", i8_ptr_t, realloc_args, 2);
    if (new_data_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء realloc لتوسيع متجه.");
        ir_builder_emit_br(b, done);
    } else {
        IRValue* new_data = ir_value_reg(new_data_r, i8_ptr_t);
        IRValue* realloc_failed = ir_value_reg(
            ir_builder_emit_cmp_eq(b, new_data, ir_lower_vector_const_ptr(i8_ptr_t)),
            IR_TYPE_I1_T);
        IRBlock* store_growth = cf_create_block(ctx, "متجه_إدخال_حفظ_التوسيع");
        if (!store_growth) store_growth = done;
        ir_builder_emit_br_cond(b, realloc_failed, done, store_growth);

        ir_builder_set_insert_point(b, store_growth);
        ir_lower_set_loc(b, site);
        ir_lower_vector_store_data(ctx, vec, new_data);
        ir_lower_vector_store_i64_field(ctx, vec, 1, new_cap);
        ir_builder_emit_br(b, copy);
    }

    ir_builder_set_insert_point(b, copy);
    ir_lower_set_loc(b, site);
    IRValue* data_now = ir_lower_vector_load_data(ctx, vec);
    IRValue* len_now = ir_lower_vector_load_i64_field(ctx, vec, 0);
    IRValue* elem_size_now = ir_lower_vector_load_i64_field(ctx, vec, 2);
    IRValue* byte_off = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, len_now, elem_size_now),
                                     IR_TYPE_I64_T);
    int dst_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, data_now, byte_off);
    IRValue* dst = ir_value_reg(dst_r, i8_ptr_t);
    IRValue* memcpy_args[3] = { dst, elem, elem_size_now };
    int memcpy_r = ir_builder_emit_call(b, "memcpy", i8_ptr_t, memcpy_args, 3);
    if (memcpy_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء memcpy لإدخال عنصر في متجه.");
        ir_builder_emit_br(b, done);
    } else {
        IRValue* next_len = ir_value_reg(
            ir_builder_emit_add(b, IR_TYPE_I64_T, len_now, ir_value_const_int(1, IR_TYPE_I64_T)),
            IR_TYPE_I64_T);
        ir_lower_vector_store_i64_field(ctx, vec, 0, next_len);
        ir_builder_emit_store(b, ir_lower_vector_const_bool(true), result_ptr);
        ir_builder_emit_br(b, done);
    }

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_lower_vector_load_result(ctx, result_ptr, IR_TYPE_I1_T);
}

static IRValue* ir_lower_vector_pop(IRLowerCtx* ctx, const Node* site, IRValue* vec_in, IRValue* out_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_vector_const_bool(false);

    IRValue* result_ptr = ir_lower_vector_alloca_result(ctx, IR_TYPE_I1_T);
    IRValue* new_len_ptr = ir_lower_vector_alloca_result(ctx, IR_TYPE_I64_T);
    ir_builder_emit_store(b, ir_lower_vector_const_bool(false), result_ptr);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I64_T), new_len_ptr);

    IRValue* vec = cast_to(ctx, vec_in, i8_ptr_t);
    IRValue* out = cast_to(ctx, out_in, i8_ptr_t);

    IRBlock* work = cf_create_block(ctx, "متجه_سحب_عمل");
    IRBlock* check_size = cf_create_block(ctx, "متجه_سحب_تحقق_حجم");
    IRBlock* choose_copy = cf_create_block(ctx, "متجه_سحب_اختيار_النسخ");
    IRBlock* copy = cf_create_block(ctx, "متجه_سحب_نسخ");
    IRBlock* store_len = cf_create_block(ctx, "متجه_سحب_حفظ_الطول");
    IRBlock* done = cf_create_block(ctx, "متجه_سحب_نهاية");
    if (!work || !check_size || !choose_copy || !copy || !store_len || !done) {
        return ir_lower_vector_const_bool(false);
    }

    IRValue* vec_null = ir_value_reg(ir_builder_emit_cmp_eq(b, vec, ir_lower_vector_const_ptr(i8_ptr_t)),
                                     IR_TYPE_I1_T);
    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, vec_null, done, work);
    }

    ir_builder_set_insert_point(b, work);
    ir_lower_set_loc(b, site);
    IRValue* len = ir_lower_vector_load_i64_field(ctx, vec, 0);
    IRValue* empty = ir_value_reg(ir_builder_emit_cmp_le(b, len, ir_value_const_int(0, IR_TYPE_I64_T)),
                                  IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, empty, done, check_size);

    ir_builder_set_insert_point(b, check_size);
    ir_lower_set_loc(b, site);
    IRValue* elem_size = ir_lower_vector_load_i64_field(ctx, vec, 2);
    IRValue* bad_elem_size = ir_value_reg(
        ir_builder_emit_cmp_le(b, elem_size, ir_value_const_int(0, IR_TYPE_I64_T)),
        IR_TYPE_I1_T);
    IRValue* new_len = ir_value_reg(
        ir_builder_emit_sub(b, IR_TYPE_I64_T, len, ir_value_const_int(1, IR_TYPE_I64_T)),
        IR_TYPE_I64_T);
    ir_builder_emit_store(b, new_len, new_len_ptr);
    IRValue* out_null = ir_value_reg(ir_builder_emit_cmp_eq(b, out, ir_lower_vector_const_ptr(i8_ptr_t)),
                                     IR_TYPE_I1_T);
    ir_builder_emit_br_cond(b, bad_elem_size, done, choose_copy);

    ir_builder_set_insert_point(b, choose_copy);
    ir_lower_set_loc(b, site);
    ir_builder_emit_br_cond(b, out_null, store_len, copy);

    ir_builder_set_insert_point(b, copy);
    ir_lower_set_loc(b, site);
    IRValue* data = ir_lower_vector_load_data(ctx, vec);
    IRValue* elem_size_copy = ir_lower_vector_load_i64_field(ctx, vec, 2);
    IRValue* copy_new_len = ir_lower_vector_load_result(ctx, new_len_ptr, IR_TYPE_I64_T);
    IRValue* byte_off = ir_value_reg(ir_builder_emit_mul(b, IR_TYPE_I64_T, copy_new_len, elem_size_copy),
                                     IR_TYPE_I64_T);
    int src_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, data, byte_off);
    IRValue* src = ir_value_reg(src_r, i8_ptr_t);
    IRValue* memcpy_args[3] = { out, src, elem_size_copy };
    int memcpy_r = ir_builder_emit_call(b, "memcpy", i8_ptr_t, memcpy_args, 3);
    if (memcpy_r < 0) {
        ir_lower_report_error(ctx, site, "فشل خفض نداء memcpy لسحب عنصر من متجه.");
        ir_builder_emit_br(b, done);
    } else {
        ir_builder_emit_br(b, store_len);
    }

    ir_builder_set_insert_point(b, store_len);
    ir_lower_set_loc(b, site);
    IRValue* final_len = ir_lower_vector_load_result(ctx, new_len_ptr, IR_TYPE_I64_T);
    ir_lower_vector_store_i64_field(ctx, vec, 0, final_len);
    ir_builder_emit_store(b, ir_lower_vector_const_bool(true), result_ptr);
    ir_builder_emit_br(b, done);

    ir_builder_set_insert_point(b, done);
    ir_lower_set_loc(b, site);
    return ir_lower_vector_load_result(ctx, result_ptr, IR_TYPE_I1_T);
}

static IRValue* ir_lower_byte_buffer_push_byte(IRLowerCtx* ctx,
                                               const Node* site,
                                               IRValue* buffer_in,
                                               IRValue* byte_in)
{
    IRBuilder* b = ctx ? ctx->builder : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    if (!b) return ir_lower_vector_const_bool(false);

    IRValue* byte_slot = ir_lower_vector_alloca_result(ctx, IR_TYPE_U8_T);
    IRValue* byte_value = cast_to(ctx, byte_in, IR_TYPE_U8_T);
    ir_builder_emit_store(b, byte_value, byte_slot);
    IRValue* byte_ptr = cast_to(ctx, byte_slot, i8_ptr_t);
    return ir_lower_vector_push(ctx, site, buffer_in, byte_ptr);
}

static bool ir_lower_try_vector_builtin(IRLowerCtx* ctx, Node* expr, IRValue** out_value)
{
    if (out_value) *out_value = NULL;
    if (!ctx || !ctx->builder || !expr || !expr->data.call.name) return false;

    const char* name = expr->data.call.name;
    if (!ir_lower_vector_name_known(name) && !ir_lower_byte_buffer_name_known(name)) return false;

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);
    if (ir_lower_vector_builtin_shadowed(ctx, m, name)) return false;

    Node* a0 = expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    Node* a2 = a1 ? a1->next : NULL;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    if (strcmp(name, "أنشئ_متجه") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'أنشئ_متجه' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_ptr(i8_ptr_t);
            return true;
        }
        IRValue* elem_size = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_vector_create(ctx, expr, elem_size);
        return true;
    }

    if (strcmp(name, "حرر_متجه") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'حرر_متجه' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_builder_const_i64(0);
            return true;
        }
        IRValue* vec = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_vector_free(ctx, expr, vec);
        return true;
    }

    if (strcmp(name, "طول_متجه") == 0 || strcmp(name, "سعة_متجه") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr,
                                  strcmp(name, "طول_متجه") == 0
                                      ? "استدعاء 'طول_متجه' يتطلب وسيطاً واحداً."
                                      : "استدعاء 'سعة_متجه' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_value_const_int(0, IR_TYPE_I64_T);
            return true;
        }
        IRValue* vec = lower_expr(ctx, a0);
        int field = (strcmp(name, "طول_متجه") == 0) ? 0 : 1;
        if (out_value) *out_value = ir_lower_vector_query_i64(ctx, expr, vec, field);
        return true;
    }

    if (strcmp(name, "بيانات_متجه") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'بيانات_متجه' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_ptr(i8_ptr_t);
            return true;
        }
        IRValue* vec = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_vector_data(ctx, expr, vec);
        return true;
    }

    if (strcmp(name, "ادفع_متجه") == 0) {
        if (!a0 || !a1 || a2) {
            ir_lower_report_error(ctx, expr, "استدعاء 'ادفع_متجه' يتطلب وسيطين.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_bool(false);
            return true;
        }
        IRValue* vec = lower_expr(ctx, a0);
        IRValue* elem = lower_expr(ctx, a1);
        if (out_value) *out_value = ir_lower_vector_push(ctx, expr, vec, elem);
        return true;
    }

    if (strcmp(name, "اسحب_متجه") == 0) {
        if (!a0 || !a1 || a2) {
            ir_lower_report_error(ctx, expr, "استدعاء 'اسحب_متجه' يتطلب وسيطين.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_bool(false);
            return true;
        }
        IRValue* vec = lower_expr(ctx, a0);
        IRValue* out = lower_expr(ctx, a1);
        if (out_value) *out_value = ir_lower_vector_pop(ctx, expr, vec, out);
        return true;
    }

    if (strcmp(name, "أنشئ_مخزن_بايتات") == 0) {
        if (a0) {
            ir_lower_report_error(ctx, expr, "استدعاء 'أنشئ_مخزن_بايتات' لا يتطلب وسائط.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_ptr(i8_ptr_t);
            return true;
        }
        if (out_value) {
            *out_value = ir_lower_vector_create(ctx, expr, ir_value_const_int(1, IR_TYPE_I64_T));
        }
        return true;
    }

    if (strcmp(name, "حرر_مخزن_بايتات") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'حرر_مخزن_بايتات' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_builder_const_i64(0);
            return true;
        }
        IRValue* buffer = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_vector_free(ctx, expr, buffer);
        return true;
    }

    if (strcmp(name, "طول_مخزن_بايتات") == 0 || strcmp(name, "سعة_مخزن_بايتات") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr,
                                  strcmp(name, "طول_مخزن_بايتات") == 0
                                      ? "استدعاء 'طول_مخزن_بايتات' يتطلب وسيطاً واحداً."
                                      : "استدعاء 'سعة_مخزن_بايتات' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_value_const_int(0, IR_TYPE_I64_T);
            return true;
        }
        IRValue* buffer = lower_expr(ctx, a0);
        int field = (strcmp(name, "طول_مخزن_بايتات") == 0) ? 0 : 1;
        if (out_value) *out_value = ir_lower_vector_query_i64(ctx, expr, buffer, field);
        return true;
    }

    if (strcmp(name, "بيانات_مخزن_بايتات") == 0) {
        if (!a0 || a1) {
            ir_lower_report_error(ctx, expr, "استدعاء 'بيانات_مخزن_بايتات' يتطلب وسيطاً واحداً.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_ptr(i8_ptr_t);
            return true;
        }
        IRValue* buffer = lower_expr(ctx, a0);
        if (out_value) *out_value = ir_lower_vector_data(ctx, expr, buffer);
        return true;
    }

    if (strcmp(name, "أضف_بايت") == 0) {
        if (!a0 || !a1 || a2) {
            ir_lower_report_error(ctx, expr, "استدعاء 'أضف_بايت' يتطلب وسيطين.");
            ir_lower_eval_call_args(ctx, expr->data.call.args);
            if (out_value) *out_value = ir_lower_vector_const_bool(false);
            return true;
        }
        IRValue* buffer = lower_expr(ctx, a0);
        IRValue* byte_value = lower_expr(ctx, a1);
        if (out_value) *out_value = ir_lower_byte_buffer_push_byte(ctx, expr, buffer, byte_value);
        return true;
    }

    return false;
}

#undef BAA_VECTOR_INITIAL_CAPACITY
#undef BAA_VECTOR_HEADER_BYTES
