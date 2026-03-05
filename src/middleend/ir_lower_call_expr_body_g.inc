    if (allow_file_builtins && expr->data.call.name) {
        const char* n = expr->data.call.name;
        Node* a0 = expr->data.call.args;
        Node* a1 = a0 ? a0->next : NULL;
        Node* a2 = a1 ? a1->next : NULL;

        IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
        IRType* char_ptr_t = get_char_ptr_type(m);

        bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
        const char* fn_tell = is_win ? "_ftelli64" : "ftello";
        const char* fn_seek = is_win ? "_fseeki64" : "fseeko";

        if (strcmp(n, "فتح_ملف") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'فتح_ملف' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }

            IRValue* path_i8 = NULL;
            IRValue* mode_i8 = NULL;
            bool free_path = false;
            bool free_mode = false;

            if (a0->type == NODE_STRING && a0->data.string_lit.value) {
                path_i8 = ir_builder_const_string(ctx->builder, a0->data.string_lit.value);
            } else {
                IRValue* pv = lower_expr(ctx, a0);
                path_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, pv);
                free_path = true;
            }

            if (a1->type == NODE_STRING && a1->data.string_lit.value) {
                const char* ms = a1->data.string_lit.value;
                if (strcmp(ms, "قراءة") == 0) mode_i8 = ir_builder_const_string(ctx->builder, "r");
                else if (strcmp(ms, "كتابة") == 0) mode_i8 = ir_builder_const_string(ctx->builder, "w");
                else if (strcmp(ms, "إضافة") == 0) mode_i8 = ir_builder_const_string(ctx->builder, "a");
                else mode_i8 = ir_builder_const_string(ctx->builder, ms);
            } else {
                IRValue* mv = lower_expr(ctx, a1);
                mode_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, mv);
                free_mode = true;
            }

            int res_ptr_reg = ir_builder_emit_alloca(ctx->builder, i8_ptr_t);
            IRValue* res_ptr = ir_value_reg(res_ptr_reg, ir_type_ptr(i8_ptr_t));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), res_ptr);

            int pnull_r = ir_builder_emit_cmp_eq(ctx->builder, path_i8, ir_value_const_int(0, i8_ptr_t));
            int mnull_r = ir_builder_emit_cmp_eq(ctx->builder, mode_i8, ir_value_const_int(0, i8_ptr_t));
            IRValue* pnull = ir_value_reg(pnull_r, IR_TYPE_I1_T);
            IRValue* mnull = ir_value_reg(mnull_r, IR_TYPE_I1_T);
            int anynull_r = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, pnull, mnull);
            IRValue* anynull = ir_value_reg(anynull_r, IR_TYPE_I1_T);

            IRBlock* callb = cf_create_block(ctx, "فتح_ملف_نداء");
            IRBlock* nullb = cf_create_block(ctx, "فتح_ملف_فارغ");
            IRBlock* done = cf_create_block(ctx, "فتح_ملف_نهاية");
            if (!callb || !nullb || !done) {
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, i8_ptr_t);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, anynull, nullb, callb);
            }

            ir_builder_set_insert_point(ctx->builder, nullb);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), res_ptr);
            if (free_path) {
                IRValue* fargs[1] = { path_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            if (free_mode) {
                IRValue* fargs[1] = { mode_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, callb);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* fargs2[2] = { path_i8, mode_i8 };
            int fr = ir_builder_emit_call(ctx->builder, "fopen", i8_ptr_t, fargs2, 2);
            if (fr < 0) {
                ir_lower_report_error(ctx, expr, "فشل خفض نداء fopen.");
                ir_builder_emit_store(ctx->builder, ir_value_const_int(0, i8_ptr_t), res_ptr);
            } else {
                ir_builder_emit_store(ctx->builder, ir_value_reg(fr, i8_ptr_t), res_ptr);
            }
            if (free_path) {
                IRValue* fargs[1] = { path_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            if (free_mode) {
                IRValue* fargs[1] = { mode_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            int out_r = ir_builder_emit_load(ctx->builder, i8_ptr_t, res_ptr);
            return ir_value_reg(out_r, i8_ptr_t);
        }

        if (strcmp(n, "اغلق_ملف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اغلق_ملف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* fv = lower_expr(ctx, a0);
            IRValue* f_i8 = cast_to(ctx, fv, i8_ptr_t);
            IRValue* args[1] = { f_i8 };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "fclose", IR_TYPE_I32_T, args, 1);
            if (r < 0) return ir_builder_const_i64(-1);
            return cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T);
        }

        if (strcmp(n, "اقرأ_حرف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اقرأ_حرف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_CHAR_T);
            }

            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);

            IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
            int bits_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* bits_ptr = ir_value_reg(bits_ptr_reg, i64_ptr_t);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), bits_ptr);

            uint64_t repl_bits_u = pack_utf8_codepoint(0xFFFDu);
            IRValue* repl_bits = ir_value_const_int((int64_t)repl_bits_u, IR_TYPE_I64_T);

            ir_lower_set_loc(ctx->builder, expr);
            IRValue* fargs0[1] = { fv };
            int ch0_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch0 = ir_value_reg(ch0_r, IR_TYPE_I32_T);
            int eof0_r = ir_builder_emit_cmp_eq(ctx->builder, ch0, ir_value_const_int(-1, IR_TYPE_I32_T));

            IRBlock* b_eof = cf_create_block(ctx, "اقرأ_حرف_EOF");
            IRBlock* b_disp = cf_create_block(ctx, "اقرأ_حرف_توزيع");
            IRBlock* b_d2 = cf_create_block(ctx, "اقرأ_حرف_د2");
            IRBlock* b_d3 = cf_create_block(ctx, "اقرأ_حرف_د3");
            IRBlock* b_d4 = cf_create_block(ctx, "اقرأ_حرف_د4");
            IRBlock* b_c1 = cf_create_block(ctx, "اقرأ_حرف_حالة1");
            IRBlock* b_c2 = cf_create_block(ctx, "اقرأ_حرف_حالة2");
            IRBlock* b_c3 = cf_create_block(ctx, "اقرأ_حرف_حالة3");
            IRBlock* b_c4 = cf_create_block(ctx, "اقرأ_حرف_حالة4");
            IRBlock* b_inv = cf_create_block(ctx, "اقرأ_حرف_غير_صالح");
            IRBlock* b_done = cf_create_block(ctx, "اقرأ_حرف_نهاية");
            if (!b_eof || !b_disp || !b_d2 || !b_d3 || !b_d4 ||
                !b_c1 || !b_c2 || !b_c3 || !b_c4 || !b_inv || !b_done) {
                return ir_value_const_int(0, IR_TYPE_CHAR_T);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, ir_value_reg(eof0_r, IR_TYPE_I1_T), b_eof, b_disp);
            }

            ir_builder_set_insert_point(ctx->builder, b_eof);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_disp);
            ir_lower_set_loc(ctx->builder, expr);

            // b0u = (uint8)ch0
            IRValue* b0u = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                           ir_value_reg(ir_builder_emit_cast(ctx->builder, ch0, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                           ir_value_const_int(255, IR_TYPE_I64_T)),
                                       IR_TYPE_I64_T);

            // len0 = 1/2/3/4 حسب البايت الأول (UTF-8)
            IRValue* m80 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0x80, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* mE0 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* mF0 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* mF8 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(0xF8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* is1 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, m80, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* is2 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, mE0, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* is3 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, mF0, ir_value_const_int(0xE0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* is4 = ir_value_reg(ir_builder_emit_cmp_eq(ctx->builder, mF8, ir_value_const_int(0xF0, IR_TYPE_I64_T)), IR_TYPE_I1_T);
            IRValue* l1 = cast_i1_to_i64(ctx, is1);
            IRValue* l2 = cast_i1_to_i64(ctx, is2);
            IRValue* l3 = cast_i1_to_i64(ctx, is3);
            IRValue* l4 = cast_i1_to_i64(ctx, is4);
            IRValue* t2 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, l2, ir_value_const_int(2, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* t3 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, l3, ir_value_const_int(3, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* t4 = ir_value_reg(ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, l4, ir_value_const_int(4, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* len0 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, l1, t2), IR_TYPE_I64_T),
                                                            ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, t3, t4), IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);

            // نرفض NUL داخل الملف لأنه يكسر نموذج C-string.
            int b0z = ir_builder_emit_cmp_eq(ctx->builder, b0u, ir_value_const_int(0, IR_TYPE_I64_T));
            int oklead = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                            ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b0z, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                            ir_value_reg(ir_builder_emit_cmp_gt(ctx->builder, len0, ir_value_const_int(0, IR_TYPE_I64_T)), IR_TYPE_I1_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(oklead, IR_TYPE_I1_T), b_d2, b_inv);

            ir_builder_set_insert_point(ctx->builder, b_d2);
            ir_lower_set_loc(ctx->builder, expr);
            int is1r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(1, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is1r, IR_TYPE_I1_T), b_c1, b_d3);

            ir_builder_set_insert_point(ctx->builder, b_d3);
            ir_lower_set_loc(ctx->builder, expr);
            int is2r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(2, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is2r, IR_TYPE_I1_T), b_c2, b_d4);

            ir_builder_set_insert_point(ctx->builder, b_d4);
            ir_lower_set_loc(ctx->builder, expr);
            int is3r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(3, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is3r, IR_TYPE_I1_T), b_c3, b_c4);

            // حالة1: بايت واحد
            ir_builder_set_insert_point(ctx->builder, b_c1);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* packed1 = ir_value_reg(ir_builder_emit_add(ctx->builder, IR_TYPE_I64_T, b0u, ir_value_const_int(4294967296LL, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed1, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            // حالة2: بايتان
            ir_builder_set_insert_point(ctx->builder, b_c2);
            ir_lower_set_loc(ctx->builder, expr);
            int ch1_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch1 = ir_value_reg(ch1_r, IR_TYPE_I32_T);
            int eof1 = ir_builder_emit_cmp_eq(ctx->builder, ch1, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b1u = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                           ir_value_reg(ir_builder_emit_cast(ctx->builder, ch1, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                           ir_value_const_int(255, IR_TYPE_I64_T)),
                                       IR_TYPE_I64_T);
            IRValue* b1m = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b1u, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b1ok = ir_builder_emit_cmp_eq(ctx->builder, b1m, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b1nz = ir_builder_emit_cmp_ne(ctx->builder, b1u, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb1 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b1ok, IR_TYPE_I1_T), ir_value_reg(b1nz, IR_TYPE_I1_T)), IR_TYPE_I1_T);
            IRValue* ok2 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                           ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof1, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                           okb1),
                                       IR_TYPE_I1_T);
            IRBlock* b_c2pack = cf_create_block(ctx, "اقرأ_حرف_حالة2_حزم");
            if (!b_c2pack) b_c2pack = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ok2, b_c2pack, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c2pack);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* b1sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b1u, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* bytes2 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, b0u, b1sh), IR_TYPE_I64_T);
            IRValue* l2sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(2, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* packed2 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, bytes2, l2sh), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed2, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            // حالة3: ثلاثة بايتات
            ir_builder_set_insert_point(ctx->builder, b_c3);
            ir_lower_set_loc(ctx->builder, expr);
            int ch1_3_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch1_3 = ir_value_reg(ch1_3_r, IR_TYPE_I32_T);
            int eof1_3 = ir_builder_emit_cmp_eq(ctx->builder, ch1_3, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b1u3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch1_3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b1m3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b1u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b1ok3 = ir_builder_emit_cmp_eq(ctx->builder, b1m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b1nz3 = ir_builder_emit_cmp_ne(ctx->builder, b1u3, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb13 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b1ok3, IR_TYPE_I1_T), ir_value_reg(b1nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            int ch2_3_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch2_3 = ir_value_reg(ch2_3_r, IR_TYPE_I32_T);
            int eof2_3 = ir_builder_emit_cmp_eq(ctx->builder, ch2_3, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b2u3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch2_3, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b2m3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b2u3, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b2ok3 = ir_builder_emit_cmp_eq(ctx->builder, b2m3, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b2nz3 = ir_builder_emit_cmp_ne(ctx->builder, b2u3, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb23 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b2ok3, IR_TYPE_I1_T), ir_value_reg(b2nz3, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            IRValue* noeof3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof1_3, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof2_3, IR_TYPE_I1_T)), IR_TYPE_I1_T)),
                                          IR_TYPE_I1_T);
            IRValue* ok3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, noeof3,
                                                           ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, okb13, okb23), IR_TYPE_I1_T)),
                                       IR_TYPE_I1_T);
            IRBlock* b_c3pack = cf_create_block(ctx, "اقرأ_حرف_حالة3_حزم");
            if (!b_c3pack) b_c3pack = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ok3, b_c3pack, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c3pack);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* b1sh3 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b1u3, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b2sh3 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b2u3, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* bytes3 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T,
                                                             ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, b0u, b1sh3), IR_TYPE_I64_T),
                                                             b2sh3),
                                           IR_TYPE_I64_T);
            IRValue* l3sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(3, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* packed3 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, bytes3, l3sh), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed3, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            // حالة4: أربعة بايتات (مراجعة محافظة)
            ir_builder_set_insert_point(ctx->builder, b_c4);
            ir_lower_set_loc(ctx->builder, expr);
            // إن لم تكن len0 == 4 فعلياً، اعتبره غير صالح.
            int is4r = ir_builder_emit_cmp_eq(ctx->builder, len0, ir_value_const_int(4, IR_TYPE_I64_T));
            IRBlock* b_c4len = cf_create_block(ctx, "اقرأ_حرف_حالة4_len");
            if (!b_c4len) b_c4len = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is4r, IR_TYPE_I1_T), b_c4len, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c4len);
            ir_lower_set_loc(ctx->builder, expr);

            int ch1_4_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch1_4 = ir_value_reg(ch1_4_r, IR_TYPE_I32_T);
            int eof1_4 = ir_builder_emit_cmp_eq(ctx->builder, ch1_4, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b1u4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch1_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b1m4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b1u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b1ok4 = ir_builder_emit_cmp_eq(ctx->builder, b1m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b1nz4 = ir_builder_emit_cmp_ne(ctx->builder, b1u4, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb14 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b1ok4, IR_TYPE_I1_T), ir_value_reg(b1nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            int ch2_4_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch2_4 = ir_value_reg(ch2_4_r, IR_TYPE_I32_T);
            int eof2_4 = ir_builder_emit_cmp_eq(ctx->builder, ch2_4, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b2u4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch2_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b2m4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b2u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b2ok4 = ir_builder_emit_cmp_eq(ctx->builder, b2m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b2nz4 = ir_builder_emit_cmp_ne(ctx->builder, b2u4, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb24 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b2ok4, IR_TYPE_I1_T), ir_value_reg(b2nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            int ch3_4_r = ir_builder_emit_call(ctx->builder, "fgetc", IR_TYPE_I32_T, fargs0, 1);
            IRValue* ch3_4 = ir_value_reg(ch3_4_r, IR_TYPE_I32_T);
            int eof3_4 = ir_builder_emit_cmp_eq(ctx->builder, ch3_4, ir_value_const_int(-1, IR_TYPE_I32_T));
            IRValue* b3u4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                            ir_value_reg(ir_builder_emit_cast(ctx->builder, ch3_4, IR_TYPE_I64_T), IR_TYPE_I64_T),
                                                            ir_value_const_int(255, IR_TYPE_I64_T)),
                                        IR_TYPE_I64_T);
            IRValue* b3m4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, b3u4, ir_value_const_int(0xC0, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int b3ok4 = ir_builder_emit_cmp_eq(ctx->builder, b3m4, ir_value_const_int(0x80, IR_TYPE_I64_T));
            int b3nz4 = ir_builder_emit_cmp_ne(ctx->builder, b3u4, ir_value_const_int(0, IR_TYPE_I64_T));
            IRValue* okb34 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, ir_value_reg(b3ok4, IR_TYPE_I1_T), ir_value_reg(b3nz4, IR_TYPE_I1_T)), IR_TYPE_I1_T);

            IRValue* noeof4 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                              ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof1_4, IR_TYPE_I1_T)), IR_TYPE_I1_T),
                                                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof2_4, IR_TYPE_I1_T)), IR_TYPE_I1_T)),
                                                                           IR_TYPE_I1_T),
                                                              ir_value_reg(ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, ir_value_reg(eof3_4, IR_TYPE_I1_T)), IR_TYPE_I1_T)),
                                          IR_TYPE_I1_T);
            IRValue* okb = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T,
                                                           ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, okb14, okb24), IR_TYPE_I1_T),
                                                           okb34),
                                       IR_TYPE_I1_T);
            IRValue* ok4v = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, noeof4, okb), IR_TYPE_I1_T);

            IRBlock* b_c4pack = cf_create_block(ctx, "اقرأ_حرف_حالة4_حزم");
            if (!b_c4pack) b_c4pack = b_inv;
            ir_builder_emit_br_cond(ctx->builder, ok4v, b_c4pack, b_inv);
            ir_builder_set_insert_point(ctx->builder, b_c4pack);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* b1sh4 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b1u4, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b2sh4 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b2u4, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b3sh4 = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, b3u4, ir_value_const_int(24, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* bytes4 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T,
                                                             ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T,
                                                                                            ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, b0u, b1sh4), IR_TYPE_I64_T),
                                                                                            b2sh4),
                                                                           IR_TYPE_I64_T),
                                                             b3sh4),
                                           IR_TYPE_I64_T);
            IRValue* l4sh = ir_value_reg(ir_builder_emit_shl(ctx->builder, IR_TYPE_I64_T, ir_value_const_int(4, IR_TYPE_I64_T), ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* packed4 = ir_value_reg(ir_builder_emit_or(ctx->builder, IR_TYPE_I64_T, bytes4, l4sh), IR_TYPE_I64_T);
            ir_builder_emit_store(ctx->builder, packed4, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_inv);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, repl_bits, bits_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_done);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* bitsv = ir_value_reg(ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, bits_ptr), IR_TYPE_I64_T);
            return ir_lower_i64_bits_to_char(ctx, bitsv);
        }

        if (strcmp(n, "اكتب_حرف") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اكتب_حرف' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* chv = lower_expr(ctx, a1);
            IRValue* bits = ir_lower_char_bits_to_i64(ctx, chv);

            IRType* i64_ptr_t = ir_type_ptr(IR_TYPE_I64_T);
            int ret_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ret_ptr = ir_value_reg(ret_ptr_reg, i64_ptr_t);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);

            // len = bits >> 32
            IRValue* len = ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(32, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            int len0 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(0, IR_TYPE_I64_T));

            // استخراج البايتات من bits (0..255) قبل إنهاء الكتلة الحالية بالـ branch.
            IRValue* b0 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(255, IR_TYPE_I64_T)), IR_TYPE_I64_T);
            IRValue* b1 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                          ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(8, IR_TYPE_I64_T)), IR_TYPE_I64_T),
                                                          ir_value_const_int(255, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
            IRValue* b2 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                          ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(16, IR_TYPE_I64_T)), IR_TYPE_I64_T),
                                                          ir_value_const_int(255, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);
            IRValue* b3 = ir_value_reg(ir_builder_emit_and(ctx->builder, IR_TYPE_I64_T,
                                                          ir_value_reg(ir_builder_emit_shr(ctx->builder, IR_TYPE_I64_T, bits, ir_value_const_int(24, IR_TYPE_I64_T)), IR_TYPE_I64_T),
                                                          ir_value_const_int(255, IR_TYPE_I64_T)),
                                      IR_TYPE_I64_T);

            IRBlock* b_len0 = cf_create_block(ctx, "اكتب_حرف_len0");
            IRBlock* b_w0 = cf_create_block(ctx, "اكتب_حرف_w0");
            IRBlock* b_a0 = cf_create_block(ctx, "اكتب_حرف_a0");
            IRBlock* b_w1 = cf_create_block(ctx, "اكتب_حرف_w1");
            IRBlock* b_a1 = cf_create_block(ctx, "اكتب_حرف_a1");
            IRBlock* b_w2 = cf_create_block(ctx, "اكتب_حرف_w2");
            IRBlock* b_a2 = cf_create_block(ctx, "اكتب_حرف_a2");
            IRBlock* b_w3 = cf_create_block(ctx, "اكتب_حرف_w3");
            IRBlock* b_a3 = cf_create_block(ctx, "اكتب_حرف_a3");
            IRBlock* b_done = cf_create_block(ctx, "اكتب_حرف_نهاية");
            if (!b_len0 || !b_w0 || !b_a0 || !b_w1 || !b_a1 || !b_w2 || !b_a2 || !b_w3 || !b_a3 || !b_done) {
                return ir_builder_const_i64(-1);
            }

            if (!ir_builder_is_block_terminated(ctx->builder)) {
                ir_builder_emit_br_cond(ctx->builder, ir_value_reg(len0, IR_TYPE_I1_T), b_len0, b_w0);
            }

            ir_builder_set_insert_point(ctx->builder, b_len0);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), ret_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_w0);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a0v[2] = { cast_to(ctx, b0, IR_TYPE_I32_T), fv };
            int r0 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a0v, 2);
            int fail0 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r0, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail0, IR_TYPE_I1_T), b_done, b_a0);

            ir_builder_set_insert_point(ctx->builder, b_a0);
            ir_lower_set_loc(ctx->builder, expr);
            int is1 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(1, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is1, IR_TYPE_I1_T), b_a3, b_w1);

            ir_builder_set_insert_point(ctx->builder, b_w1);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a1v[2] = { cast_to(ctx, b1, IR_TYPE_I32_T), fv };
            int r1 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a1v, 2);
            int fail1 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r1, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail1, IR_TYPE_I1_T), b_done, b_a1);

            ir_builder_set_insert_point(ctx->builder, b_a1);
            ir_lower_set_loc(ctx->builder, expr);
            int is2 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(2, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is2, IR_TYPE_I1_T), b_a3, b_w2);

            ir_builder_set_insert_point(ctx->builder, b_w2);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a2v[2] = { cast_to(ctx, b2, IR_TYPE_I32_T), fv };
            int r2 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a2v, 2);
            int fail2 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r2, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail2, IR_TYPE_I1_T), b_done, b_a2);

            ir_builder_set_insert_point(ctx->builder, b_a2);
            ir_lower_set_loc(ctx->builder, expr);
            int is3 = ir_builder_emit_cmp_eq(ctx->builder, len, ir_value_const_int(3, IR_TYPE_I64_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(is3, IR_TYPE_I1_T), b_a3, b_w3);

            ir_builder_set_insert_point(ctx->builder, b_w3);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* a3v[2] = { cast_to(ctx, b3, IR_TYPE_I32_T), fv };
            int r3 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, a3v, 2);
            int fail3 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r3, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail3, IR_TYPE_I1_T), b_done, b_a3);

            ir_builder_set_insert_point(ctx->builder, b_a3);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), ret_ptr);
            ir_builder_emit_br(ctx->builder, b_done);

            ir_builder_set_insert_point(ctx->builder, b_done);
            ir_lower_set_loc(ctx->builder, expr);
            int out_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, ret_ptr);
            return ir_value_reg(out_r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "اقرأ_ملف") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اقرأ_ملف' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* bv = cast_to(ctx, lower_expr(ctx, a1), i8_ptr_t);
            IRValue* nv = ensure_i64(ctx, lower_expr(ctx, a2));
            IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
            IRValue* args[4] = { bv, one, nv, fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "fread", IR_TYPE_I64_T, args, 4);
            if (r < 0) return ir_builder_const_i64(0);
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "اكتب_ملف") == 0) {
            if (!a0 || !a1 || !a2 || a2->next) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اكتب_ملف' يتطلب ثلاثة وسائط.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(0);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* bv = cast_to(ctx, lower_expr(ctx, a1), i8_ptr_t);
            IRValue* nv = ensure_i64(ctx, lower_expr(ctx, a2));
            IRValue* one = ir_value_const_int(1, IR_TYPE_I64_T);
            IRValue* args[4] = { bv, one, nv, fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "fwrite", IR_TYPE_I64_T, args, 4);
            if (r < 0) return ir_builder_const_i64(0);
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "نهاية_ملف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'نهاية_ملف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, IR_TYPE_I1_T);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* args[1] = { fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, "feof", IR_TYPE_I32_T, args, 1);
            if (r < 0) return ir_value_const_int(0, IR_TYPE_I1_T);
            int ne = ir_builder_emit_cmp_ne(ctx->builder, ir_value_reg(r, IR_TYPE_I32_T), ir_value_const_int(0, IR_TYPE_I32_T));
            return ir_value_reg(ne, IR_TYPE_I1_T);
        }

        if (strcmp(n, "موقع_ملف") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'موقع_ملف' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* args[1] = { fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, fn_tell, IR_TYPE_I64_T, args, 1);
            if (r < 0) return ir_builder_const_i64(-1);
            return ir_value_reg(r, IR_TYPE_I64_T);
        }

        if (strcmp(n, "اذهب_لموقع") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اذهب_لموقع' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            IRValue* pos = ensure_i64(ctx, lower_expr(ctx, a1));
            IRValue* wh = ir_value_const_int(0, IR_TYPE_I64_T); // SEEK_SET
            IRValue* args[3] = { fv, pos, wh };
            ir_lower_set_loc(ctx->builder, expr);
            int r = ir_builder_emit_call(ctx->builder, fn_seek, IR_TYPE_I32_T, args, 3);
            if (r < 0) return ir_builder_const_i64(-1);
            return cast_to(ctx, ir_value_reg(r, IR_TYPE_I32_T), IR_TYPE_I64_T);
        }

        if (strcmp(n, "اقرأ_سطر") == 0) {
            if (!a0 || a1) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اقرأ_سطر' يتطلب وسيطاً واحداً.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_value_const_int(0, char_ptr_t);
            }
            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);
            return ir_lower_builtin_read_line_from_file(ctx, expr, fv);
        }

        if (strcmp(n, "اكتب_سطر") == 0) {
            if (!a0 || !a1 || a2) {
                ir_lower_report_error(ctx, expr, "استدعاء 'اكتب_سطر' يتطلب وسيطين.");
                ir_lower_eval_call_args(ctx, expr->data.call.args);
                return ir_builder_const_i64(-1);
            }

            IRValue* fv = cast_to(ctx, lower_expr(ctx, a0), i8_ptr_t);

            IRValue* s_i8 = NULL;
            bool free_s = false;
            if (a1->type == NODE_STRING && a1->data.string_lit.value) {
                s_i8 = ir_builder_const_string(ctx->builder, a1->data.string_lit.value);
            } else {
                IRValue* sv = lower_expr(ctx, a1);
                s_i8 = ir_lower_baa_string_to_cstr_alloc(ctx, expr, sv);
                free_s = true;
            }

            int ret_ptr_reg = ir_builder_emit_alloca(ctx->builder, IR_TYPE_I64_T);
            IRValue* ret_ptr = ir_value_reg(ret_ptr_reg, ir_type_ptr(IR_TYPE_I64_T));
            ir_builder_emit_store(ctx->builder, ir_value_const_int(-1, IR_TYPE_I64_T), ret_ptr);

            IRBlock* ok1 = cf_create_block(ctx, "اكتب_سطر_بعد_نص");
            IRBlock* ok2 = cf_create_block(ctx, "اكتب_سطر_بعد_سطر");
            IRBlock* done = cf_create_block(ctx, "اكتب_سطر_نهاية");
            if (!ok1 || !ok2 || !done) {
                if (free_s) {
                    IRValue* fargs[1] = { s_i8 };
                    ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
                }
                return ir_builder_const_i64(-1);
            }

            IRValue* args1[2] = { s_i8, fv };
            ir_lower_set_loc(ctx->builder, expr);
            int r1 = ir_builder_emit_call(ctx->builder, "fputs", IR_TYPE_I32_T, args1, 2);
            int fail1 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r1, IR_TYPE_I32_T),
                                              ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail1, IR_TYPE_I1_T), done, ok1);

            ir_builder_set_insert_point(ctx->builder, ok1);
            ir_lower_set_loc(ctx->builder, expr);
            IRValue* nl = ir_value_const_int(10, IR_TYPE_I32_T);
            IRValue* args2[2] = { nl, fv };
            int r2 = ir_builder_emit_call(ctx->builder, "fputc", IR_TYPE_I32_T, args2, 2);
            int fail2 = ir_builder_emit_cmp_lt(ctx->builder, ir_value_reg(r2, IR_TYPE_I32_T),
                                              ir_value_const_int(0, IR_TYPE_I32_T));
            ir_builder_emit_br_cond(ctx->builder, ir_value_reg(fail2, IR_TYPE_I1_T), done, ok2);

            ir_builder_set_insert_point(ctx->builder, ok2);
            ir_lower_set_loc(ctx->builder, expr);
            ir_builder_emit_store(ctx->builder, ir_value_const_int(0, IR_TYPE_I64_T), ret_ptr);
            ir_builder_emit_br(ctx->builder, done);

            ir_builder_set_insert_point(ctx->builder, done);
            ir_lower_set_loc(ctx->builder, expr);
            if (free_s) {
                IRValue* fargs[1] = { s_i8 };
                ir_builder_emit_call_void(ctx->builder, "free", fargs, 1);
            }
            int out_r = ir_builder_emit_load(ctx->builder, IR_TYPE_I64_T, ret_ptr);
            return ir_value_reg(out_r, IR_TYPE_I64_T);
        }
    }

