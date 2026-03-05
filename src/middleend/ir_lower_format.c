// ============================================================================
// تنسيق عربي للإخراج/الإدخال (v0.4.0)
// ============================================================================

#define BAA_FMT_MAX_SPECS 128

typedef enum {
    BAA_FMT_SPEC_I64,
    BAA_FMT_SPEC_U64,
    BAA_FMT_SPEC_HEX,
    BAA_FMT_SPEC_STR,
    BAA_FMT_SPEC_CHAR,
    BAA_FMT_SPEC_F64,
    BAA_FMT_SPEC_F64_SCI,
    BAA_FMT_SPEC_PTR,
} BaaFmtSpecKind;

typedef struct {
    BaaFmtSpecKind kind;
    int width;
    bool has_width;
} BaaFmtSpec;

static bool baa_utf8_decode_one_fmt(const char* s, uint32_t* out_cp, int* out_len)
{
    if (!s || !out_cp || !out_len) return false;
    const unsigned char b0 = (unsigned char)s[0];
    if (b0 == 0) return false;

    if ((b0 & 0x80u) == 0x00u) {
        *out_cp = (uint32_t)b0;
        *out_len = 1;
        return true;
    }
    if ((b0 & 0xE0u) == 0xC0u) {
        const unsigned char b1 = (unsigned char)s[1];
        if ((b1 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        *out_len = 2;
        return true;
    }
    if ((b0 & 0xF0u) == 0xE0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        if ((b1 & 0xC0u) != 0x80u) return false;
        if ((b2 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                  ((uint32_t)(b1 & 0x3Fu) << 6) |
                  (uint32_t)(b2 & 0x3Fu);
        *out_len = 3;
        return true;
    }
    if ((b0 & 0xF8u) == 0xF0u) {
        const unsigned char b1 = (unsigned char)s[1];
        const unsigned char b2 = (unsigned char)s[2];
        const unsigned char b3 = (unsigned char)s[3];
        if ((b1 & 0xC0u) != 0x80u) return false;
        if ((b2 & 0xC0u) != 0x80u) return false;
        if ((b3 & 0xC0u) != 0x80u) return false;
        *out_cp = ((uint32_t)(b0 & 0x07u) << 18) |
                  ((uint32_t)(b1 & 0x3Fu) << 12) |
                  ((uint32_t)(b2 & 0x3Fu) << 6) |
                  (uint32_t)(b3 & 0x3Fu);
        *out_len = 4;
        return true;
    }

    return false;
}

static bool fmt_buf_append(char** buf, size_t* len, size_t* cap, const char* s)
{
    if (!buf || !len || !cap || !s) return false;
    size_t n = strlen(s);
    if (*cap < *len + n + 1) {
        size_t ncap = (*cap == 0) ? 64 : *cap;
        while (ncap < *len + n + 1) ncap *= 2;
        char* nb = (char*)realloc(*buf, ncap);
        if (!nb) return false;
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = 0;
    return true;
}

static bool fmt_buf_append_ch(char** buf, size_t* len, size_t* cap, char ch)
{
    char tmp[2] = { ch, 0 };
    return fmt_buf_append(buf, len, cap, tmp);
}

static bool baa_fmt_translate_ar(IRLowerCtx* ctx, const Node* site, const char* fmt, bool is_input,
                                 char** out_cfmt, BaaFmtSpec* out_specs, int* out_spec_count)
{
    if (!out_cfmt || !out_specs || !out_spec_count) return false;
    *out_cfmt = NULL;
    *out_spec_count = 0;
    if (!fmt) return false;

    char* obuf = NULL;
    size_t olen = 0;
    size_t ocap = 0;

    const char* p = fmt;
    while (*p) {
        if (*p != '%') {
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
            p++;
            continue;
        }

        // %%
        if (p[1] == '%') {
            if (!fmt_buf_append(&obuf, &olen, &ocap, "%%")) goto oom;
            p += 2;
            continue;
        }

        if (!fmt_buf_append_ch(&obuf, &olen, &ocap, '%')) goto oom;
        p++;

        // flags
        while (*p && (*p == '-' || *p == '+' || *p == '0' || *p == ' ' || *p == '#')) {
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
            p++;
        }

        int width = 0;
        bool has_width = false;
        while (*p && isdigit((unsigned char)*p)) {
            has_width = true;
            width = width * 10 + (int)(*p - '0');
            if (width > 1000000) width = 1000000;
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
            p++;
        }

        if (*p == '.') {
            if (!fmt_buf_append_ch(&obuf, &olen, &ocap, '.')) goto oom;
            p++;
            if (*p == '*') {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: الدقة باستخدام '*' غير مدعومة حالياً.");
                free(obuf);
                return false;
            }
            while (*p && isdigit((unsigned char)*p)) {
                if (!fmt_buf_append_ch(&obuf, &olen, &ocap, *p)) goto oom;
                p++;
            }
        }

        uint32_t cp = 0;
        int ulen = 0;
        if (!baa_utf8_decode_one_fmt(p, &cp, &ulen) || ulen <= 0) {
            ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: محرف UTF-8 غير صالح بعد '%%'.");
            free(obuf);
            return false;
        }

        if (*out_spec_count >= BAA_FMT_MAX_SPECS) {
            ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: عدد المواصفات كبير جداً (الحد %d).", BAA_FMT_MAX_SPECS);
            free(obuf);
            return false;
        }

        BaaFmtSpec spec;
        memset(&spec, 0, sizeof(spec));
        spec.width = width;
        spec.has_width = has_width;

        if (cp == 0x0635u) { // ص
            spec.kind = BAA_FMT_SPEC_I64;
            if (!fmt_buf_append(&obuf, &olen, &ocap, "lld")) goto oom;
        } else if (cp == 0x0637u) { // ط
            spec.kind = BAA_FMT_SPEC_U64;
            if (!fmt_buf_append(&obuf, &olen, &ocap, "llu")) goto oom;
        } else if (cp == 0x0633u) { // س
            spec.kind = BAA_FMT_SPEC_HEX;
            if (!fmt_buf_append(&obuf, &olen, &ocap, "llx")) goto oom;
        } else if (cp == 0x0646u) { // ن
            spec.kind = BAA_FMT_SPEC_STR;
            if (is_input && (!has_width || width <= 0)) {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: %%ن في الإدخال يتطلب عرضاً رقمياً (مثلاً %%64ن).");
                free(obuf);
                return false;
            }
            if (!fmt_buf_append(&obuf, &olen, &ocap, "s")) goto oom;
        } else if (cp == 0x062Du) { // ح
            spec.kind = BAA_FMT_SPEC_CHAR;
            if (is_input) {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: %%ح غير مدعوم في الإدخال حالياً.");
                free(obuf);
                return false;
            }
            if (!fmt_buf_append(&obuf, &olen, &ocap, "s")) goto oom;
        } else if (cp == 0x0639u) { // ع
            spec.kind = BAA_FMT_SPEC_F64;
            if (!fmt_buf_append(&obuf, &olen, &ocap, is_input ? "lf" : "f")) goto oom;
        } else if (cp == 0x0623u) { // أ (صيغة علمية)
            spec.kind = BAA_FMT_SPEC_F64_SCI;
            if (!fmt_buf_append(&obuf, &olen, &ocap, is_input ? "le" : "e")) goto oom;
        } else if (cp == 0x0645u) { // م
            spec.kind = BAA_FMT_SPEC_PTR;
            if (is_input) {
                ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: %%م غير مدعوم في الإدخال حالياً.");
                free(obuf);
                return false;
            }
            if (!fmt_buf_append(&obuf, &olen, &ocap, "p")) goto oom;
        } else {
            ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: مواصفة غير معروفة بعد '%%'.");
            free(obuf);
            return false;
        }

        out_specs[(*out_spec_count)++] = spec;
        p += ulen;
    }

    *out_cfmt = obuf;
    return true;

oom:
    ir_lower_report_error(ctx, (Node*)site, "تنسيق عربي: نفدت الذاكرة أثناء بناء نص التنسيق.");
    free(obuf);
    return false;
}

static IRValue* ir_lower_char_to_cstr_tmp(IRLowerCtx* ctx, const Node* site, IRValue* ch)
{
    if (!ctx || !ctx->builder || !ch) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, 5);
    int buf_ptr_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, ir_type_ptr(buf_arr_t));

    int base_reg = ir_builder_emit_cast(b, buf_ptr, i8_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, i8_ptr_t);

    // i64 معبأ = bytes + len*2^32
    int pr = ir_builder_emit_cast(b, ch, IR_TYPE_I64_T);
    IRValue* packed = ir_value_reg(pr, IR_TYPE_I64_T);

    IRValue* c_2p32 = ir_value_const_int(4294967296LL, IR_TYPE_I64_T);
    IRValue* c256 = ir_value_const_int(256, IR_TYPE_I64_T);

    int len_r = ir_builder_emit_div(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* len = ir_value_reg(len_r, IR_TYPE_I64_T);

    int bytes_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, packed, c_2p32);
    IRValue* bytes = ir_value_reg(bytes_r, IR_TYPE_I64_T);

    int b0_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* b0 = ir_value_reg(b0_r, IR_TYPE_I64_T);
    int bytes1_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes, c256);
    IRValue* bytes1 = ir_value_reg(bytes1_r, IR_TYPE_I64_T);

    int b1_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes1, c256);
    IRValue* b1 = ir_value_reg(b1_r, IR_TYPE_I64_T);
    int bytes2_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes1, c256);
    IRValue* bytes2 = ir_value_reg(bytes2_r, IR_TYPE_I64_T);

    int b2_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes2, c256);
    IRValue* b2 = ir_value_reg(b2_r, IR_TYPE_I64_T);
    int bytes3_r = ir_builder_emit_div(b, IR_TYPE_I64_T, bytes2, c256);
    IRValue* bytes3 = ir_value_reg(bytes3_r, IR_TYPE_I64_T);

    int b3_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, bytes3, c256);
    IRValue* b3 = ir_value_reg(b3_r, IR_TYPE_I64_T);

    IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
    IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
    IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
    IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);

    IRValue* b0p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0), i8_ptr_t);
    IRValue* b1p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1), i8_ptr_t);
    IRValue* b2p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2), i8_ptr_t);
    IRValue* b3p = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3), i8_ptr_t);

    if (site) ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, cast_to(ctx, b0, IR_TYPE_I8_T), b0p);
    ir_builder_emit_store(b, cast_to(ctx, b1, IR_TYPE_I8_T), b1p);
    ir_builder_emit_store(b, cast_to(ctx, b2, IR_TYPE_I8_T), b2p);
    ir_builder_emit_store(b, cast_to(ctx, b3, IR_TYPE_I8_T), b3p);

    IRValue* tp = ir_value_reg(ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, len), i8_ptr_t);
    ir_builder_emit_store(b, ir_value_const_int(0, IR_TYPE_I8_T), tp);

    return base_ptr;
}

static IRValue* ir_lower_cstr_or_literal_on_null(IRLowerCtx* ctx, const Node* site, IRValue* cstr, const char* literal)
{
    if (!ctx || !ctx->builder || !literal) return cstr;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);

    IRValue* p = cstr ? cast_to(ctx, cstr, i8_ptr_t) : ir_value_const_int(0, i8_ptr_t);
    IRValue* lit = ir_builder_const_string(b, literal);
    if (!lit) return p;

    int res_ptr_reg = ir_builder_emit_alloca(b, i8_ptr_t);
    IRValue* res_ptr = ir_value_reg(res_ptr_reg, i8_ptr_ptr_t);
    ir_builder_emit_store(b, p, res_ptr);

    int is_null_r = ir_builder_emit_cmp_eq(b, p, ir_value_const_int(0, i8_ptr_t));
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* b_null = cf_create_block(ctx, "تنسيق_نص_NULL");
    IRBlock* b_ok = cf_create_block(ctx, "تنسيق_نص_OK");
    IRBlock* b_ret = cf_create_block(ctx, "تنسيق_نص_إرجاع");
    if (!b_null || !b_ok || !b_ret) {
        int r0 = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
        return ir_value_reg(r0, i8_ptr_t);
    }

    ir_builder_emit_br_cond(b, is_null, b_null, b_ok);

    ir_builder_set_insert_point(b, b_null);
    if (site) ir_lower_set_loc(b, site);
    ir_builder_emit_store(b, lit, res_ptr);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ok);
    if (site) ir_lower_set_loc(b, site);
    ir_builder_emit_br(b, b_ret);

    ir_builder_set_insert_point(b, b_ret);
    if (site) ir_lower_set_loc(b, site);
    int r = ir_builder_emit_load(b, i8_ptr_t, res_ptr);
    return ir_value_reg(r, i8_ptr_t);
}

static IRValue* ir_lower_builtin_print_format_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_format_string_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_scan_format_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_read_line_stdin_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_read_num_stdin_ar(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_variadic_start(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_variadic_next(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_variadic_end(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_assert(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_panic(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_errno_get(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_errno_set(IRLowerCtx* ctx, Node* call_expr);
static IRValue* ir_lower_builtin_errno_text(IRLowerCtx* ctx, Node* call_expr);
static IRValue* lower_lvalue_address(IRLowerCtx* ctx, Node* expr, IRType** out_pointee_type);

static bool ir_lower_variadic_extract_type(Node* type_arg, DataType* out_type)
{
    if (!out_type) return false;
    *out_type = TYPE_INT;
    if (!type_arg) return false;
    if (type_arg->type != NODE_SIZEOF || !type_arg->data.sizeof_expr.has_type_form) return false;
    *out_type = type_arg->data.sizeof_expr.target_type;
    return true;
}

static IRValue* ir_lower_pack_variadic_args(IRLowerCtx* ctx, Node* site, IRValue** args, int arg_count)
{
    if (!ctx || !ctx->builder) return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* f64_ptr_t = ir_type_ptr(IR_TYPE_F64_T);

    int slots = arg_count > 0 ? arg_count : 1;
    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, slots * 8);
    if (!buf_arr_t) {
        ir_lower_report_error(ctx, site, "فشل إنشاء مخزن معاملات متغيرة.");
        return ir_value_const_int(0, i8_ptr_t);
    }

    int buf_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_arr_ptr = ir_value_reg(buf_reg, ir_type_ptr(buf_arr_t));
    int base_cast = ir_builder_emit_cast(b, buf_arr_ptr, i8_ptr_t);
    IRValue* base_i8 = ir_value_reg(base_cast, i8_ptr_t);

    for (int i = 0; i < arg_count; i++) {
        IRValue* av = (args && args[i]) ? args[i] : ir_value_const_int(0, IR_TYPE_I64_T);
        IRType* at = av->type ? av->type : IR_TYPE_I64_T;

        if (site) ir_lower_set_loc(b, site);
        int slot_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_i8,
                                                ir_value_const_int((int64_t)i * 8, IR_TYPE_I64_T));
        IRValue* slot_i8 = ir_value_reg(slot_r, i8_ptr_t);

        if (at->kind == IR_TYPE_F64) {
            int cp = ir_builder_emit_cast(b, slot_i8, f64_ptr_t);
            IRValue* slot_f64 = ir_value_reg(cp, f64_ptr_t);
            IRValue* fv = cast_to(ctx, av, IR_TYPE_F64_T);
            ir_builder_emit_store(b, fv, slot_f64);
            continue;
        }

        if (at->kind == IR_TYPE_PTR) {
            int cp = ir_builder_emit_cast(b, slot_i8, i8_ptr_ptr_t);
            IRValue* slot_ptr = ir_value_reg(cp, i8_ptr_ptr_t);
            IRValue* pv = cast_to(ctx, av, i8_ptr_t);
            ir_builder_emit_store(b, pv, slot_ptr);
            continue;
        }

        int cp = ir_builder_emit_cast(b, slot_i8, i64_ptr_t);
        IRValue* slot_i64 = ir_value_reg(cp, i64_ptr_t);
        IRValue* iv = ensure_i64(ctx, av);
        ir_builder_emit_store(b, iv, slot_i64);
    }

    return base_i8;
}

static IRValue* ir_lower_builtin_variadic_start(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (!ctx->current_func_is_variadic || ctx->current_func_va_base_reg < 0) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'بدء_معاملات' خارج دالة متغيرة المعاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    Node* a0 = call_expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    if (!a0 || !a1 || a1->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'بدء_معاملات' يتطلب معاملين.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    if (a0->type != NODE_VAR_REF) {
        ir_lower_report_error(ctx, a0, "المعامل الأول في 'بدء_معاملات' يجب أن يكون متغيراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* cursor_pointee = IR_TYPE_I64_T;
    IRValue* cursor_addr = lower_lvalue_address(ctx, a0, &cursor_pointee);
    if (!cursor_pointee || cursor_pointee->kind != IR_TYPE_PTR) {
        ir_lower_report_error(ctx, a0, "متغير قائمة المعاملات يجب أن يكون مؤشراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* base = ir_value_reg(ctx->current_func_va_base_reg, i8_ptr_t);
    int64_t off = (int64_t)ctx->current_func_fixed_params * 8;
    if (off > 0) {
        int pr = ir_builder_emit_ptr_offset(ctx->builder, i8_ptr_t, base,
                                            ir_value_const_int(off, IR_TYPE_I64_T));
        base = ir_value_reg(pr, i8_ptr_t);
    }

    if (call_expr) ir_lower_set_loc(ctx->builder, call_expr);
    ir_builder_emit_store(ctx->builder, cast_to(ctx, base, cursor_pointee), cursor_addr);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_variadic_end(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* a0 = call_expr->data.call.args;
    if (!a0 || a0->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'نهاية_معاملات' يتطلب معاملاً واحداً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    if (a0->type != NODE_VAR_REF) {
        ir_lower_report_error(ctx, a0, "المعامل الأول في 'نهاية_معاملات' يجب أن يكون متغيراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* cursor_pointee = IR_TYPE_I64_T;
    IRValue* cursor_addr = lower_lvalue_address(ctx, a0, &cursor_pointee);
    if (!cursor_pointee || cursor_pointee->kind != IR_TYPE_PTR) {
        ir_lower_report_error(ctx, a0, "متغير قائمة المعاملات يجب أن يكون مؤشراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    if (call_expr) ir_lower_set_loc(ctx->builder, call_expr);
    ir_builder_emit_store(ctx->builder, ir_value_const_int(0, cursor_pointee), cursor_addr);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_variadic_next(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (!ctx->current_func_is_variadic) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'معامل_تالي' خارج دالة متغيرة المعاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    Node* a0 = call_expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    if (!a0 || !a1 || a1->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'معامل_تالي' يتطلب معاملين.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    if (a0->type != NODE_VAR_REF) {
        ir_lower_report_error(ctx, a0, "المعامل الأول في 'معامل_تالي' يجب أن يكون متغيراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    DataType want = TYPE_INT;
    if (!ir_lower_variadic_extract_type(a1, &want)) {
        ir_lower_report_error(ctx, a1, "المعامل الثاني في 'معامل_تالي' يجب أن يكون نوعاً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* cursor_pointee = IR_TYPE_I64_T;
    IRValue* cursor_addr = lower_lvalue_address(ctx, a0, &cursor_pointee);
    if (!cursor_pointee || cursor_pointee->kind != IR_TYPE_PTR) {
        ir_lower_report_error(ctx, a0, "متغير قائمة المعاملات يجب أن يكون مؤشراً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
    IRType* f64_ptr_t = ir_type_ptr(IR_TYPE_F64_T);
    IRType* i8_ptr_ptr_t = ir_type_ptr(i8_ptr_t);

    if (call_expr) ir_lower_set_loc(b, call_expr);
    int cur_r = ir_builder_emit_load(b, cursor_pointee, cursor_addr);
    IRValue* cur_raw = ir_value_reg(cur_r, cursor_pointee);
    IRValue* cur_i8 = cast_to(ctx, cur_raw, i8_ptr_t);

    IRType* load_t = IR_TYPE_I64_T;
    IRType* ret_t = ir_type_from_datatype(m, want);
    if (!ret_t || ret_t->kind == IR_TYPE_VOID) ret_t = IR_TYPE_I64_T;

    if (want == TYPE_FLOAT) {
        load_t = IR_TYPE_F64_T;
    } else if (want == TYPE_POINTER || want == TYPE_STRING) {
        load_t = i8_ptr_t;
    }

    IRValue* slot_ptr = NULL;
    if (load_t == IR_TYPE_F64_T) {
        int cp = ir_builder_emit_cast(b, cur_i8, f64_ptr_t);
        slot_ptr = ir_value_reg(cp, f64_ptr_t);
    } else if (load_t == i8_ptr_t) {
        int cp = ir_builder_emit_cast(b, cur_i8, i8_ptr_ptr_t);
        slot_ptr = ir_value_reg(cp, i8_ptr_ptr_t);
    } else {
        int cp = ir_builder_emit_cast(b, cur_i8, i64_ptr_t);
        slot_ptr = ir_value_reg(cp, i64_ptr_t);
    }

    int val_r = ir_builder_emit_load(b, load_t, slot_ptr);
    IRValue* val = ir_value_reg(val_r, load_t);

    int next_r = ir_builder_emit_ptr_offset(b, i8_ptr_t, cur_i8, ir_value_const_int(8, IR_TYPE_I64_T));
    IRValue* next_i8 = ir_value_reg(next_r, i8_ptr_t);
    ir_builder_emit_store(b, cast_to(ctx, next_i8, cursor_pointee), cursor_addr);

    if (load_t != ret_t) {
        val = cast_to(ctx, val, ret_t);
    }
    return val;
}

static const char* ir_lower_errno_symbol(const IRLowerCtx* ctx)
{
    bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
    return is_win ? "_errno" : "__errno_location";
}

static void ir_lower_emit_message_and_free_if_needed(IRLowerCtx* ctx,
                                                     const Node* site,
                                                     IRValue* cstr_msg,
                                                     bool free_msg)
{
    if (!ctx || !ctx->builder || !cstr_msg) return;

    IRBuilder* b = ctx->builder;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* n = ir_value_const_int(0, i8_ptr_t);

    int is_null_r = ir_builder_emit_cmp_eq(b, cstr_msg, n);
    IRValue* is_null = ir_value_reg(is_null_r, IR_TYPE_I1_T);

    IRBlock* skipb = cf_create_block(ctx, "رسالة_تخطي");
    IRBlock* printb = cf_create_block(ctx, "رسالة_طباعة");
    IRBlock* doneb = cf_create_block(ctx, "رسالة_نهاية");
    if (!skipb || !printb || !doneb) {
        if (free_msg) {
            IRValue* fargs[1] = { cstr_msg };
            ir_builder_emit_call_void(b, "free", fargs, 1);
        }
        return;
    }

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, is_null, skipb, printb);
    }

    ir_builder_set_insert_point(b, printb);
    if (site) ir_lower_set_loc(b, site);
    IRValue* pargs[1] = { cstr_msg };
    (void)ir_builder_emit_call(b, "puts", IR_TYPE_I32_T, pargs, 1);
    if (free_msg) {
        IRValue* fargs[1] = { cstr_msg };
        ir_builder_emit_call_void(b, "free", fargs, 1);
    }
    ir_builder_emit_br(b, doneb);

    ir_builder_set_insert_point(b, skipb);
    if (site) ir_lower_set_loc(b, site);
    if (free_msg) {
        IRValue* fargs[1] = { cstr_msg };
        ir_builder_emit_call_void(b, "free", fargs, 1);
    }
    ir_builder_emit_br(b, doneb);

    ir_builder_set_insert_point(b, doneb);
    if (site) ir_lower_set_loc(b, site);
}

static IRValue* ir_lower_build_panic_path(IRLowerCtx* ctx, Node* site, Node* msg_node, const char* header)
{
    if (!ctx || !ctx->builder) return ir_builder_const_i64(0);

    IRBuilder* b = ctx->builder;
    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);

    if (header && header[0]) {
        IRValue* hv = ir_builder_const_string(b, header);
        IRValue* hargs[1] = { hv };
        (void)ir_builder_emit_call(b, "puts", IR_TYPE_I32_T, hargs, 1);
    }

    if (msg_node) {
        IRValue* msg_i8 = NULL;
        bool free_msg = false;

        if (msg_node->type == NODE_STRING && msg_node->data.string_lit.value) {
            msg_i8 = ir_builder_const_string(b, msg_node->data.string_lit.value);
        } else {
            IRValue* msg_baa = lower_expr(ctx, msg_node);
            msg_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, site, msg_baa);
            free_msg = true;
        }

        if (!msg_i8) {
            msg_i8 = ir_value_const_int(0, i8_ptr_t);
        }
        ir_lower_emit_message_and_free_if_needed(ctx, site, msg_i8, free_msg);
    }

    ir_lower_emit_abort_path(ctx);

    IRBlock* cont = cf_create_block(ctx, "ذعر_متابعة");
    if (cont) {
        ir_builder_set_insert_point(b, cont);
        if (site) ir_lower_set_loc(b, site);
    }
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_assert(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* a0 = call_expr->data.call.args;
    Node* a1 = a0 ? a0->next : NULL;
    if (!a0 || !a1 || a1->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'تأكد' يتطلب وسيطين.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRValue* cond_v = cast_to(ctx, lower_expr(ctx, a0), IR_TYPE_I1_T);
    IRBlock* passb = cf_create_block(ctx, "تأكد_سليم");
    IRBlock* failb = cf_create_block(ctx, "تأكد_فشل");
    if (!passb || !failb) {
        ir_lower_report_error(ctx, call_expr, "فشل إنشاء كتل تحكم لـ 'تأكد'.");
        return ir_builder_const_i64(0);
    }

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br_cond(ctx->builder, cond_v, passb, failb);
    }

    ir_builder_set_insert_point(ctx->builder, failb);
    ir_lower_set_loc(ctx->builder, call_expr);
    (void)ir_lower_build_panic_path(ctx, call_expr, a1, "فشل_تأكد");

    ir_builder_set_insert_point(ctx->builder, passb);
    ir_lower_set_loc(ctx->builder, call_expr);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_panic(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* a0 = call_expr->data.call.args;
    if (!a0 || a0->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'توقف_فوري' يتطلب معاملاً واحداً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }
    return ir_lower_build_panic_path(ctx, call_expr, a0, "توقف_فوري");
}

static IRValue* ir_lower_builtin_errno_get(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    if (call_expr->data.call.args) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'كود_خطأ_النظام' لا يقبل معاملات.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* i32_ptr_t = ir_type_ptr(IR_TYPE_I32_T);
    const char* errno_fn = ir_lower_errno_symbol(ctx);
    ir_lower_set_loc(ctx->builder, call_expr);
    int pr = ir_builder_emit_call(ctx->builder, errno_fn, i32_ptr_t, NULL, 0);
    if (pr < 0) {
        ir_lower_report_error(ctx, call_expr, "فشل خفض نداء مؤشر errno.");
        return ir_builder_const_i64(0);
    }

    IRValue* p = ir_value_reg(pr, i32_ptr_t);
    int vr = ir_builder_emit_load(ctx->builder, IR_TYPE_I32_T, p);
    return cast_to(ctx, ir_value_reg(vr, IR_TYPE_I32_T), IR_TYPE_I64_T);
}

static IRValue* ir_lower_builtin_errno_set(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_builder_const_i64(0);

    Node* a0 = call_expr->data.call.args;
    if (!a0 || a0->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'ضبط_كود_خطأ_النظام' يتطلب معاملاً واحداً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_builder_const_i64(0);
    }

    IRType* i32_ptr_t = ir_type_ptr(IR_TYPE_I32_T);
    const char* errno_fn = ir_lower_errno_symbol(ctx);

    IRValue* in_i64 = ensure_i64(ctx, lower_expr(ctx, a0));
    IRValue* in_i32 = cast_to(ctx, in_i64, IR_TYPE_I32_T);

    ir_lower_set_loc(ctx->builder, call_expr);
    int pr = ir_builder_emit_call(ctx->builder, errno_fn, i32_ptr_t, NULL, 0);
    if (pr < 0) {
        ir_lower_report_error(ctx, call_expr, "فشل خفض نداء مؤشر errno.");
        return ir_builder_const_i64(0);
    }

    IRValue* p = ir_value_reg(pr, i32_ptr_t);
    ir_builder_emit_store(ctx->builder, in_i32, p);
    return ir_builder_const_i64(0);
}

static IRValue* ir_lower_builtin_errno_text(IRLowerCtx* ctx, Node* call_expr)
{
    if (!ctx || !ctx->builder || !call_expr) return ir_value_const_int(0, get_char_ptr_type(ctx->builder->module));

    Node* a0 = call_expr->data.call.args;
    if (!a0 || a0->next) {
        ir_lower_report_error(ctx, call_expr, "استدعاء 'نص_كود_خطأ' يتطلب معاملاً واحداً.");
        ir_lower_eval_call_args(ctx, call_expr->data.call.args);
        return ir_value_const_int(0, get_char_ptr_type(ctx->builder->module));
    }

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    IRType* char_ptr_t = get_char_ptr_type(ctx->builder->module);

    IRValue* in_i64 = ensure_i64(ctx, lower_expr(ctx, a0));
    IRValue* in_i32 = cast_to(ctx, in_i64, IR_TYPE_I32_T);
    IRValue* args[1] = { in_i32 };

    ir_lower_set_loc(ctx->builder, call_expr);
    int r = ir_builder_emit_call(ctx->builder, "strerror", i8_ptr_t, args, 1);
    if (r < 0) {
        ir_lower_report_error(ctx, call_expr, "فشل خفض نداء strerror.");
        return ir_value_const_int(0, char_ptr_t);
    }
    return ir_lower_cstr_to_baa_string_alloc(ctx, call_expr, ir_value_reg(r, i8_ptr_t));
}

