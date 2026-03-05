static IRValue* ir_text_parse_value(IRModule* module, IRFunc* func, IRTextBlockMap* bmap,
                                   const char** p, IRType* expected_type) {
    (void)func;
    ir_text_skip_ws(p);

    // رقم ثابت
    if (**p == '-' || isdigit((unsigned char)**p)) {
        int64_t v = 0;
        if (!ir_text_parse_int64(p, &v)) return NULL;
        // إذا لم يتوفر نوع متوقع، نفترض i64 لأن نص IR لا يحمل نوعاً للثوابت.
        IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
        return ir_value_const_int(v, t);
    }

    // سلسلة: @.strN
    if (ir_text_match(p, "@.str")) {
        int id = 0;
        if (!ir_text_parse_int32(p, &id)) return NULL;
        const char* s = module ? ir_module_get_string(module, id) : NULL;
        return ir_value_const_str(s, id);
    }

    // سلسلة باء: @.bsN
    if (ir_text_match(p, "@.bs")) {
        int id = 0;
        if (!ir_text_parse_int32(p, &id)) return NULL;
        const char* s = module ? ir_module_get_baa_string(module, id) : NULL;
        return ir_value_baa_str(s, id);
    }

    // مرجع global/function: @name
    if (**p == '@') {
        (*p)++;
        char* name = ir_text_parse_token(p);
        if (!name) return NULL;

        if (expected_type && expected_type->kind == IR_TYPE_FUNC) {
            IRValue* v = ir_value_func_ref(name, expected_type);
            free(name);
            return v;
        }

        // إن لم يوجد نوع متوقع، نحاول التمييز من محتوى الوحدة:
        // - إذا وجد global بنفس الاسم: نعامله كمرجع global.
        // - وإلا إذا وجدت دالة بنفس الاسم: نعامله كمرجع دالة (مؤشر دالة) مع توقيع معروف.
        if (!expected_type && module) {
            IRGlobal* g = ir_module_find_global(module, name);
            if (!g) {
                IRFunc* fn = ir_module_find_func(module, name);
                if (fn) {
                    IRType* params_local[64];
                    IRType** pp = NULL;
                    int pc = fn->param_count;
                    if (pc > 0) {
                        if (pc <= (int)(sizeof(params_local) / sizeof(params_local[0]))) {
                            for (int i = 0; i < pc; i++) params_local[i] = fn->params[i].type;
                            pp = params_local;
                        } else {
                            free(name);
                            return NULL;
                        }
                    }
                    IRType* sig = ir_type_func(fn->ret_type, pp, pc);
                    IRValue* v = ir_value_func_ref(name, sig);
                    free(name);
                    return v;
                }
            }
        }

        IRType* base = NULL;
        if (expected_type && expected_type->kind == IR_TYPE_PTR) base = expected_type->data.pointee;
        IRValue* v = ir_value_global(name, base);
        free(name);
        return v;
    }

    // سجل أو كتلة
    char* tok = ir_text_parse_token(p);
    if (!tok) return NULL;

    int reg = -1;
    if (ir_text_parse_reg_token(tok, &reg)) {
        free(tok);
        IRType* rt = expected_type ? expected_type : ir_text_reg_type_get(bmap, reg);
        return ir_value_reg(reg, rt);
    }

    int bid = -1;
    if (ir_text_parse_blockref_token(tok, &bid)) {
        free(tok);
        if (!func || !bmap) return NULL;
        IRBlock* b = ir_text_get_or_create_block(func, bmap, bid);
        return b ? ir_value_block(b) : NULL;
    }

    free(tok);
    return NULL;
}

static int ir_text_consume_char(const char** p, char c) {
    ir_text_skip_ws(p);
    if (**p != c) return 0;
    (*p)++;
    return 1;
}

static int ir_text_parse_inst_attrs(const char** p, IRInst* inst, IRFunc* func) {
    if (!p || !*p || !inst || !func) return 0;
    while (!ir_text_is_eol(*p)) {
        ir_text_skip_ws(p);
        if (**p == '\0') break;

        if (ir_text_match(p, "@id")) {
            int id = -1;
            if (!ir_text_parse_int32(p, &id)) return 0;
            inst->id = id;
            if (id + 1 > func->next_inst_id) func->next_inst_id = id + 1;
            continue;
        }

        // سمات غير معروفة: نوقف بشكل صارم.
        return 0;
    }
    return 1;
}

static int ir_text_parse_instruction_line(IRModule* module, IRFunc* func, IRBlock* block,
                                         IRTextBlockMap* bmap, const char* line) {
    if (!module || !func || !block || !bmap || !line) return 0;

    const char* p = line;
    ir_text_skip_ws(&p);
    if (*p == '\0') return 1;

    int dest = -1;
    if (*p == '%') {
        char* lhs = ir_text_parse_token(&p);
        if (!lhs) return 0;
        if (!ir_text_parse_reg_token(lhs, &dest)) { free(lhs); return 0; }
        free(lhs);
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "=")) return 0;
    }

    char* op_tok = ir_text_parse_token(&p);
    if (!op_tok) return 0;
    IROp op = ir_text_parse_op(op_tok);
    free(op_tok);

    IRInst* inst = NULL;

    if (op == IR_OP_NOP) {
        inst = ir_inst_new(IR_OP_NOP, IR_TYPE_VOID_T, -1);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        return 1;
    }

    if (op == IR_OP_BR) {
        IRType* bt = NULL;
        IRValue* tgtv = ir_text_parse_value(module, func, bmap, &p, bt);
        if (!tgtv || tgtv->kind != IR_VAL_BLOCK) return 0;
        inst = ir_inst_br(tgtv->data.block);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        ir_block_add_succ(block, tgtv->data.block);
        return 1;
    }

    if (op == IR_OP_BR_COND) {
        IRValue* cond = ir_text_parse_value(module, func, bmap, &p, IR_TYPE_I1_T);
        if (!cond) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRValue* t = ir_text_parse_value(module, func, bmap, &p, NULL);
        if (!t || t->kind != IR_VAL_BLOCK) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRValue* f = ir_text_parse_value(module, func, bmap, &p, NULL);
        if (!f || f->kind != IR_VAL_BLOCK) return 0;

        inst = ir_inst_br_cond(cond, t->data.block, f->data.block);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        ir_block_add_succ(block, t->data.block);
        ir_block_add_succ(block, f->data.block);
        return 1;
    }

    if (op == IR_OP_RET) {
        IRType* rt = ir_text_parse_type_rec(&p);
        if (!rt) return 0;
        if (rt->kind == IR_TYPE_VOID) {
            inst = ir_inst_ret(NULL);
            if (!inst) return 0;
            if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
            ir_block_append(block, inst);
            return 1;
        }
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, rt);
        if (!v) return 0;
        inst = ir_inst_ret(v);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        return 1;
    }

    if (op == IR_OP_CALL) {
        IRType* ret_t = ir_text_parse_type_rec(&p);
        if (!ret_t) return 0;
        ir_text_skip_ws(&p);
        bool is_direct = false;
        char* fn = NULL;
        IRValue* callee = NULL;
        IRType* callee_sig = NULL;

        // صيغة التسلسل:
        // - مباشر: call <ret_type> @name(args)
        // - غير مباشر (الموصى به): call <ret_type> <callee>(args)
        // - غير مباشر (قديم): call <ret_type> %rN(args)
        if (ir_text_match(&p, "@")) {
            is_direct = true;
            fn = ir_text_parse_token(&p);
            if (!fn) return 0;

            // إن كانت الدالة موجودة في الوحدة، نستخدم توقيعها لتعيين أنواع الوسائط.
            IRFunc* fnd = ir_module_find_func(module, fn);
            if (fnd) {
                IRType* params_local[64];
                IRType** pp = NULL;
                int pc = fnd->param_count;
                if (pc > 0) {
                    if (pc <= (int)(sizeof(params_local) / sizeof(params_local[0]))) {
                        for (int i = 0; i < pc; i++) params_local[i] = fnd->params[i].type;
                        pp = params_local;
                    } else {
                        free(fn);
                        return 0;
                    }
                }
                callee_sig = ir_type_func(fnd->ret_type, pp, pc);
            }
        } else if (ir_text_match(&p, "<")) {
            callee = ir_text_parse_value(module, func, bmap, &p, NULL);
            if (!callee) return 0;
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, ">")) return 0;
        } else {
            // قديم: بدون <> (عادةً %rN)
            callee = ir_text_parse_value(module, func, bmap, &p, NULL);
            if (!callee) return 0;
        }

        if (!is_direct) {
            if (!callee->type || callee->type->kind != IR_TYPE_FUNC) return 0;
            callee_sig = callee->type;
        }

        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "(")) { free(fn); return 0; }

        // جمع الوسائط
        IRValue* args_local[128];
        int ac = 0;
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, ")")) {
            while (1) {
                if (ac >= 128) { free(fn); return 0; }
                IRType* exp = NULL;
                if (callee_sig && callee_sig->kind == IR_TYPE_FUNC) {
                    int pc = callee_sig->data.func.param_count;
                    if (ac < pc) exp = callee_sig->data.func.params[ac];
                }
                IRValue* a = ir_text_parse_value(module, func, bmap, &p, exp);
                if (!a) { free(fn); return 0; }
                args_local[ac++] = a;

                ir_text_skip_ws(&p);
                if (ir_text_match(&p, ")")) break;
                if (!ir_text_match(&p, ",")) { free(fn); return 0; }
            }
        }

        IRValue** args = NULL;
        if (ac > 0) {
            args = (IRValue**)malloc((size_t)ac * sizeof(IRValue*));
            if (!args) { free(fn); return 0; }
            for (int i = 0; i < ac; i++) args[i] = args_local[i];
        }

        if (is_direct) {
            inst = ir_inst_call(fn, ret_t, dest, args, ac);
        } else {
            inst = ir_inst_call_indirect(callee, ret_t, dest, args, ac);
        }
        free(fn);
        free(args);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        if (dest >= 0) {
            if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
        }
        return 1;
    }

    if (op == IR_OP_ALLOCA) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        inst = ir_inst_alloca(t, dest);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        if (dest >= 0) {
            if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
        }
        return 1;
    }

    if (op == IR_OP_LOAD) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRType* ptr_t = ir_type_ptr(t);
        IRValue* ptr = ir_text_parse_value(module, func, bmap, &p, ptr_t);
        if (!ptr) return 0;
        inst = ir_inst_load(t, dest, ptr);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        if (dest >= 0) {
            if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
        }
        return 1;
    }

    if (op == IR_OP_STORE) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, t);
        if (!v) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRType* ptr_t = ir_type_ptr(t);
        IRValue* ptr = ir_text_parse_value(module, func, bmap, &p, ptr_t);
        if (!ptr) return 0;
        inst = ir_inst_store(v, ptr);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        return 1;
    }

    if (op == IR_OP_CMP) {
        char* pred_tok = ir_text_parse_token(&p);
        if (!pred_tok) return 0;
        IRCmpPred pred = ir_text_parse_pred(pred_tok);
        free(pred_tok);
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        IRValue* lhs = ir_text_parse_value(module, func, bmap, &p, t);
        if (!lhs) return 0;
        if (!ir_text_consume_char(&p, ',')) return 0;
        IRValue* rhs = ir_text_parse_value(module, func, bmap, &p, t);
        if (!rhs) return 0;
        inst = ir_inst_cmp(pred, dest, lhs, rhs);
        if (!inst) return 0;
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        if (dest >= 0) {
            if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
        }
        return 1;
    }

    if (op == IR_OP_PHI) {
        IRType* t = ir_text_parse_type_rec(&p);
        if (!t) return 0;
        inst = ir_inst_phi(t, dest);
        if (!inst) return 0;

        // phi entries: [v, %blockN], ...
        while (1) {
            ir_text_skip_ws(&p);
            if (ir_text_is_eol(p)) break;
            if (!ir_text_match(&p, "[")) break;
            IRValue* v = ir_text_parse_value(module, func, bmap, &p, t);
            if (!v) return 0;
            if (!ir_text_consume_char(&p, ',')) return 0;
            char* br = ir_text_parse_token(&p);
            if (!br) return 0;
            int bid = -1;
            if (!ir_text_parse_blockref_token(br, &bid)) { free(br); return 0; }
            free(br);
            IRBlock* pb = ir_text_get_or_create_block(func, bmap, bid);
            if (!pb) return 0;
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "]")) return 0;
            ir_inst_phi_add(inst, v, pb);

            ir_text_skip_ws(&p);
            if (ir_text_match(&p, ",")) continue;
            break;
        }

        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        if (dest >= 0) {
            if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
        }
        return 1;
    }

    if (op == IR_OP_CAST) {
        IRType* from_t = ir_text_parse_type_rec(&p);
        if (!from_t) return 0;
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, from_t);
        if (!v) return 0;
        ir_text_skip_ws(&p);
        if (!ir_text_match(&p, "to")) return 0;
        IRType* to_t = ir_text_parse_type_rec(&p);
        if (!to_t) return 0;

        inst = ir_inst_new(IR_OP_CAST, to_t, dest);
        if (!inst) return 0;
        ir_inst_add_operand(inst, v);
        if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
        ir_block_append(block, inst);
        if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
        if (dest >= 0) {
            if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
        }
        return 1;
    }

    // العمليات القياسية ذات النوع
    IRType* t = ir_text_parse_type_rec(&p);
    if (!t) return 0;

    inst = ir_inst_new(op, t, dest);
    if (!inst) return 0;

    // تحميل/نسخ/حساب...: معاملات مفصولة بفواصل
    int first = 1;
    while (!ir_text_is_eol(p)) {
        if (!first) {
            if (!ir_text_consume_char(&p, ',')) break;
        }
        first = 0;
        IRValue* v = ir_text_parse_value(module, func, bmap, &p, t);
        if (!v) break;
        ir_inst_add_operand(inst, v);
        ir_text_skip_ws(&p);
        if (*p != ',') break;
    }

    if (!ir_text_parse_inst_attrs(&p, inst, func)) return 0;
    ir_block_append(block, inst);
    if (dest >= 0 && dest + 1 > func->next_reg) func->next_reg = dest + 1;
    if (dest >= 0 && inst->type) {
        if (!ir_text_reg_type_set(bmap, dest, inst->type)) return 0;
    }
    return 1;
}

static char* ir_text_read_line(FILE* in) {
    if (!in) return NULL;
    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int ch;
    while ((ch = fgetc(in)) != EOF) {
        if (len + 2 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
        if (ch == '\n') break;
    }

    if (len == 0 && ch == EOF) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

IRModule* ir_text_read_module_file(const char* filename) {
    if (!filename) return NULL;
    FILE* in = fopen(filename, "r");
    if (!in) return NULL;

    IRModule* module = ir_module_new(filename);
    if (!module) { fclose(in); return NULL; }

    IRFunc* cur_func = NULL;
    IRBlock* cur_block = NULL;
    IRTextBlockMap bmap = {0};

    int ok = 1;

    for (;;) {
        char* line = ir_text_read_line(in);
        if (!line) break;

        const char* p = line;
        ir_text_skip_ws(&p);

        // تجاهل التعليقات بأسلوب ";;" أو ";".
        if (ir_text_match(&p, ";;") || ir_text_match(&p, ";")) {
            free(line);
            continue;
        }

        // سطر فارغ
        if (ir_text_is_eol(p)) {
            free(line);
            continue;
        }

        // جدول النصوص: @.strN = "..."
        if (ir_text_match(&p, "@.str")) {
            int id = 0;
            if (!ir_text_parse_int32(&p, &id)) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "=")) { ok = 0; free(line); break; }
            char* s = NULL;
            if (!ir_text_parse_string_lit(&p, &s)) { ok = 0; free(line); break; }

            int got = ir_module_add_string(module, s);
            free(s);
            if (got != id) { ok = 0; free(line); break; }

            free(line);
            continue;
        }

        // global
        if (strncmp(p, "const global ", 13) == 0 ||
            strncmp(p, "global ", 7) == 0 ||
            strncmp(p, "internal ", 9) == 0) {
            int is_internal = 0;
            int is_const = 0;
            if (ir_text_match(&p, "internal")) {
                is_internal = 1;
                ir_text_skip_ws(&p);
            }
            if (ir_text_match(&p, "const")) {
                is_const = 1;
                ir_text_skip_ws(&p);
            }
            if (!ir_text_match(&p, "global")) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "@")) { ok = 0; free(line); break; }
            char* name = ir_text_parse_token(&p);
            if (!name) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "=")) { free(name); ok = 0; free(line); break; }
            IRType* t = ir_text_parse_type_rec(&p);
            if (!t) { free(name); ok = 0; free(line); break; }

            if (t->kind == IR_TYPE_ARRAY)
            {
                IRType* elem_t = t->data.array.element;

                ir_text_skip_ws(&p);
                int has_list = 0;
                IRValue** tmp = NULL;
                int tmp_count = 0;
                int tmp_cap = 0;

                if (ir_text_match(&p, "zeroinit"))
                {
                    has_list = 0;
                }
                else if (ir_text_match(&p, "{"))
                {
                    has_list = 1;
                    ir_text_skip_ws(&p);

                    if (!ir_text_match(&p, "}"))
                    {
                        for (;;)
                        {
                            IRValue* v = ir_text_parse_value(module, NULL, NULL, &p, elem_t);
                            if (!v) { ok = 0; break; }

                            if (tmp_count >= tmp_cap)
                            {
                                int new_cap = (tmp_cap == 0) ? 8 : (tmp_cap * 2);
                                IRValue** new_tmp = (IRValue**)realloc(tmp, (size_t)new_cap * sizeof(IRValue*));
                                if (!new_tmp) { ok = 0; break; }
                                tmp = new_tmp;
                                tmp_cap = new_cap;
                            }
                            tmp[tmp_count++] = v;

                            ir_text_skip_ws(&p);
                            if (ir_text_match(&p, "}")) break;
                            if (!ir_text_match(&p, ",")) { ok = 0; break; }
                            ir_text_skip_ws(&p);
                            // السماح بفاصلة أخيرة قبل '}'
                            if (ir_text_match(&p, "}")) break;
                        }
                    }
                }
                else
                {
                    ok = 0;
                }

                IRGlobal* g = NULL;
                if (ok)
                {
                    g = ir_global_new(name, t, is_const);
                }
                free(name);
                if (!ok || !g) { free(tmp); ok = 0; free(line); break; }
                g->is_internal = is_internal ? true : false;

                g->has_init_list = has_list ? true : false;
                if (tmp_count > 0)
                {
                    IRValue** elems = (IRValue**)ir_arena_calloc(&module->arena, (size_t)tmp_count,
                                                               sizeof(IRValue*), _Alignof(IRValue*));
                    if (!elems) {
                        free(tmp);
                        ok = 0;
                        free(line);
                        break;
                    }
                    for (int i = 0; i < tmp_count; i++) elems[i] = tmp[i];
                    g->init_elems = elems;
                    g->init_elem_count = tmp_count;
                }
                free(tmp);

                ir_module_add_global(module, g);
            }
            else
            {
                IRValue* init = ir_text_parse_value(module, NULL, NULL, &p, t);
                if (!init) { free(name); ok = 0; free(line); break; }

                IRGlobal* g = ir_global_new(name, t, is_const);
                free(name);
                if (!g) { ok = 0; free(line); break; }
                g->is_internal = is_internal ? true : false;
                ir_global_set_init(g, init);
                ir_module_add_global(module, g);
            }

            free(line);
            continue;
        }

        // func header
        if (ir_text_match(&p, "func")) {
            // إعادة تهيئة سياق الكتل
            free(bmap.blocks);
            free(bmap.reg_types);
            memset(&bmap, 0, sizeof(bmap));
            cur_block = NULL;

            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "@")) { ok = 0; free(line); break; }
            char* fname = ir_text_parse_token(&p);
            if (!fname) { ok = 0; free(line); break; }
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "(")) { free(fname); ok = 0; free(line); break; }

            IRFunc* fn = ir_func_new(fname, IR_TYPE_VOID_T);
            free(fname);
            if (!fn) { ok = 0; free(line); break; }

            // params
            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, ")")) {
                if (ir_text_match(&p, "...")) {
                    fn->is_variadic = true;
                    ir_text_skip_ws(&p);
                    if (!ir_text_match(&p, ")")) { ok = 0; free(line); break; }
                } else {
                    while (1) {
                        IRType* pt = ir_text_parse_type_rec(&p);
                        if (!pt) { ok = 0; free(line); break; }
                        char* reg_tok = ir_text_parse_token(&p);
                        if (!reg_tok) { ok = 0; free(line); break; }
                        int preg = -1;
                        if (!ir_text_parse_reg_token(reg_tok, &preg)) { free(reg_tok); ok = 0; free(line); break; }
                        free(reg_tok);

                        ir_func_add_param(fn, NULL, pt);
                        // ضبط رقم السجل كما ورد في النص (إن اختلف)
                        fn->params[fn->param_count - 1].reg = preg;
                        if (preg + 1 > fn->next_reg) fn->next_reg = preg + 1;
                        if (!ir_text_reg_type_set(&bmap, preg, pt)) { ok = 0; break; }

                        ir_text_skip_ws(&p);
                        if (ir_text_match(&p, ")")) break;
                        if (!ir_text_match(&p, ",")) { ok = 0; free(line); break; }

                        ir_text_skip_ws(&p);
                        if (ir_text_match(&p, "...")) {
                            fn->is_variadic = true;
                            ir_text_skip_ws(&p);
                            if (!ir_text_match(&p, ")")) { ok = 0; free(line); break; }
                            break;
                        }
                    }
                }
                if (!ok) { free(line); break; }
            }

            ir_text_skip_ws(&p);
            if (!ir_text_match(&p, "->")) { ok = 0; free(line); break; }
            IRType* rt = ir_text_parse_type_rec(&p);
            if (!rt) { ok = 0; free(line); break; }
            fn->ret_type = rt;

            ir_text_skip_ws(&p);
            if (ir_text_match(&p, ";")) {
                fn->is_prototype = true;
                ir_module_add_func(module, fn);
                cur_func = NULL;
                free(line);
                continue;
            }

            if (!ir_text_match(&p, "{")) { ok = 0; free(line); break; }
            fn->is_prototype = false;
            ir_module_add_func(module, fn);
            cur_func = fn;

            free(line);
            continue;
        }

        // نهاية دالة
        if (*p == '}' && cur_func) {
            cur_func = NULL;
            cur_block = NULL;
            free(line);
            continue;
        }

        // block header: blockN:
        if (strncmp(p, "block", 5) == 0) {
            const char* q = p + 5;
            int id = 0;
            errno = 0;
            char* end = NULL;
            long v = strtol(q, &end, 10);
            if (end == q || errno != 0) { ok = 0; free(line); break; }
            if (!end || *end != ':') { ok = 0; free(line); break; }
            id = (int)v;
            if (!cur_func) { ok = 0; free(line); break; }
            cur_block = ir_text_get_or_create_block(cur_func, &bmap, id);
            if (!cur_block) { ok = 0; free(line); break; }
            free(line);
            continue;
        }

        // instruction
        if (cur_func && cur_block) {
            if (!ir_text_parse_instruction_line(module, cur_func, cur_block, &bmap, p)) {
                ok = 0;
                free(line);
                break;
            }
            free(line);
            continue;
        }

        // سطر غير معروف
        ok = 0;
        free(line);
        break;
    }

    free(bmap.blocks);
    free(bmap.reg_types);
    fclose(in);

    if (!ok) {
        ir_module_free(module);
        return NULL;
    }
    return module;
}
