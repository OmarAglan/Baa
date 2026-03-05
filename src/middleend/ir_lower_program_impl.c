// ============================================================================
// Program lowering (v0.3.0.7)
// ============================================================================

static uint64_t ir_lower_u64_add_wrap(uint64_t a, uint64_t b) { return a + b; }
static uint64_t ir_lower_u64_sub_wrap(uint64_t a, uint64_t b) { return a - b; }
static uint64_t ir_lower_u64_mul_wrap(uint64_t a, uint64_t b) { return a * b; }

/**
 * @brief تقييم تعبير ثابت للأعداد الصحيحة لاستخدامه في تهيئة العوام.
 *
 * الهدف هنا هو دعم تهيئة العوام بقيم قابلة للإصدار في .data (مثل C):
 * - يسمح بعمليات حسابية بسيطة وتعبيرات مقارنة/منطقية تنتج 0/1.
 * - يطبق التفاف 2's complement لتفادي UB في C عند تجاوز المدى.
 */
static int ir_lower_eval_const_i64(Node* expr, int64_t* out_value) {
    if (!out_value) return 0;
    *out_value = 0;
    if (!expr) return 1;

    switch (expr->type) {
        case NODE_INT:
            *out_value = (int64_t)expr->data.integer.value;
            return 1;
        case NODE_BOOL:
            *out_value = expr->data.bool_lit.value ? 1 : 0;
            return 1;
        case NODE_CHAR:
            *out_value = (int64_t)expr->data.char_lit.value;
            return 1;

        case NODE_UNARY_OP:
        {
            int64_t v = 0;
            if (!ir_lower_eval_const_i64(expr->data.unary_op.operand, &v)) return 0;

            if (expr->data.unary_op.op == UOP_NEG) {
                uint64_t uv = (uint64_t)v;
                *out_value = (int64_t)ir_lower_u64_sub_wrap(0u, uv);
                return 1;
            }
            if (expr->data.unary_op.op == UOP_NOT) {
                *out_value = (v == 0) ? 1 : 0;
                return 1;
            }
            return 0;
        }

        case NODE_BIN_OP:
        {
            int64_t a = 0;
            int64_t b = 0;
            if (!ir_lower_eval_const_i64(expr->data.bin_op.left, &a)) return 0;
            if (!ir_lower_eval_const_i64(expr->data.bin_op.right, &b)) return 0;

            uint64_t ua = (uint64_t)a;
            uint64_t ub = (uint64_t)b;

            switch (expr->data.bin_op.op) {
                case OP_ADD: *out_value = (int64_t)ir_lower_u64_add_wrap(ua, ub); return 1;
                case OP_SUB: *out_value = (int64_t)ir_lower_u64_sub_wrap(ua, ub); return 1;
                case OP_MUL: *out_value = (int64_t)ir_lower_u64_mul_wrap(ua, ub); return 1;

                case OP_DIV:
                    if (b == 0) return 0;
                    if (a == INT64_MIN && b == -1) { *out_value = INT64_MIN; return 1; }
                    *out_value = a / b;
                    return 1;
                case OP_MOD:
                    if (b == 0) return 0;
                    if (a == INT64_MIN && b == -1) { *out_value = 0; return 1; }
                    *out_value = a % b;
                    return 1;

                case OP_EQ:  *out_value = (a == b) ? 1 : 0; return 1;
                case OP_NEQ: *out_value = (a != b) ? 1 : 0; return 1;
                case OP_LT:  *out_value = (a <  b) ? 1 : 0; return 1;
                case OP_GT:  *out_value = (a >  b) ? 1 : 0; return 1;
                case OP_LTE: *out_value = (a <= b) ? 1 : 0; return 1;
                case OP_GTE: *out_value = (a >= b) ? 1 : 0; return 1;

                case OP_AND: *out_value = ((a != 0) && (b != 0)) ? 1 : 0; return 1;
                case OP_OR:  *out_value = ((a != 0) || (b != 0)) ? 1 : 0; return 1;
                default:
                    return 0;
            }
        }

        default:
            return 0;
    }
}

static int64_t ir_lower_trunc_const_to_type(int64_t v, IRType* t)
{
    if (!t) return v;
    if (t->kind == IR_TYPE_I1) return v ? 1 : 0;

    int bits = ir_type_bits(t);
    if (bits <= 0 || bits >= 64) return v;

    uint64_t mask = (1ULL << (unsigned)bits) - 1ULL;
    uint64_t u = ((uint64_t)v) & mask;

    if (t->kind == IR_TYPE_U8 || t->kind == IR_TYPE_U16 || t->kind == IR_TYPE_U32)
        return (int64_t)u;

    uint64_t sign_bit = 1ULL << (unsigned)(bits - 1);
    if (u & sign_bit) u |= ~mask;
    return (int64_t)u;
}

static IRValue* ir_lower_global_init_value(IRBuilder* builder, Node* expr, IRType* expected_type) {
    if (!builder) return NULL;

    if (!expr) {
        return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }

    switch (expr->type) {
        case NODE_INT:
        {
            IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
            int64_t v = (int64_t)expr->data.integer.value;
            v = ir_lower_trunc_const_to_type(v, t);
            return ir_value_const_int(v, t);
        }

        case NODE_BOOL:
        {
            IRType* t = expected_type ? expected_type : IR_TYPE_I1_T;
            int64_t v = expr->data.bool_lit.value ? 1 : 0;
            v = ir_lower_trunc_const_to_type(v, t);
            return ir_value_const_int(v, t);
        }

        case NODE_FLOAT:
            return ir_value_const_int((int64_t)expr->data.float_lit.bits,
                                      expected_type ? expected_type : IR_TYPE_F64_T);

        case NODE_CHAR:
            if (expected_type && expected_type->kind == IR_TYPE_CHAR) {
                uint64_t packed = pack_utf8_codepoint((uint32_t)expr->data.char_lit.value);
                return ir_value_const_int((int64_t)packed, IR_TYPE_CHAR_T);
            }
            {
                IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
                int64_t v = (int64_t)expr->data.char_lit.value;
                v = ir_lower_trunc_const_to_type(v, t);
                return ir_value_const_int(v, t);
            }

        case NODE_STRING:
            // Adds to module string table and returns pointer value.
            return ir_builder_const_baa_string(builder, expr->data.string_lit.value);

        case NODE_NULL:
            return ir_value_const_int(0, expected_type ? expected_type : ir_type_ptr(IR_TYPE_I8_T));

        case NODE_VAR_REF:
            // تهيئة عامة لمؤشر دالة: اسم دالة يُخزن كعنوان ثابت.
            if (expected_type && expected_type->kind == IR_TYPE_FUNC &&
                expr->data.var_ref.name) {
                return ir_value_func_ref(expr->data.var_ref.name, expected_type);
            }
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_MEMBER_ACCESS:
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value,
                                          expected_type ? expected_type : IR_TYPE_I64_T);
            }
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_CAST:
            return ir_lower_global_init_value(builder,
                                              expr->data.cast_expr.expression,
                                              expected_type);

        case NODE_UNARY_OP:
        case NODE_BIN_OP:
        {
            int64_t v = 0;
            if (ir_lower_eval_const_i64(expr, &v)) {
                IRType* t = expected_type ? expected_type : IR_TYPE_I64_T;
                v = ir_lower_trunc_const_to_type(v, t);
                return ir_value_const_int(v, t);
            }
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
        }

        default:
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }
}

IRModule* ir_lower_program(Node* program, const char* module_name,
                           bool enable_bounds_checks, const BaaTarget* target) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    // هل لدينا `الرئيسية(صحيح، نص[])`؟ إن كان نعم سنولد غلاف ABI لـ C ونُعيد تسمية دالة المستخدم.
    bool main_with_args = false;
    const Node* main_site = NULL;
    for (Node* d = program->data.program.declarations; d; d = d->next) {
        if (d->type != NODE_FUNC_DEF) continue;
        if (!ir_lower_ast_func_is_main_with_args(d)) continue;
        main_with_args = true;
        // نفضل موقع التعريف غير النموذجي لربط التشخيصات.
        if (!main_site || (!d->data.func_def.is_prototype && main_site->data.func_def.is_prototype)) {
            main_site = d;
        }
    }

    IRModule* module = ir_module_new(module_name ? module_name : "module");
    if (!module) return NULL;

    IRBuilder* builder = ir_builder_new(module);
    if (!builder) {
        ir_module_free(module);
        return NULL;
    }

    // -----------------------------------------------------------------
    // 0) إنشاء تواقيع الدوال أولاً (يدعم الاستدعاء قبل التعريف)
    // -----------------------------------------------------------------
    for (Node* decl0 = program->data.program.declarations; decl0; decl0 = decl0->next) {
        if (decl0->type != NODE_FUNC_DEF) continue;
        if (!decl0->data.func_def.name) continue;
        int decl0_param_count = 0;
        for (Node* p = decl0->data.func_def.params; p; p = p->next) {
            if (p->type == NODE_VAR_DECL) decl0_param_count++;
        }

        const char* ir_name = decl0->data.func_def.name;
        if (main_with_args && ir_lower_ast_func_is_main_with_args(decl0)) {
            ir_name = BAA_WRAPPED_USER_MAIN_NAME;
        }

        IRFunc* existing = ir_module_find_func(module, ir_name);
        if (existing) {
            existing->is_variadic = decl0->data.func_def.is_variadic;
            if (decl0->data.func_def.is_variadic &&
                existing->param_count == decl0_param_count) {
                ir_builder_set_func(builder, existing);
                (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
            }
            if (!decl0->data.func_def.is_prototype) {
                existing->is_prototype = false;
            }
            continue;
        }

        IRType* ret_type = ir_type_from_datatype_ex(module,
                                                    decl0->data.func_def.return_type,
                                                    decl0->data.func_def.return_func_sig);
        IRFunc* f0 = ir_builder_create_func(builder, ir_name, ret_type);
        if (!f0) continue;
        f0->is_prototype = decl0->data.func_def.is_prototype;
        f0->is_variadic = decl0->data.func_def.is_variadic;

        for (Node* p = decl0->data.func_def.params; p; p = p->next) {
            if (p->type != NODE_VAR_DECL) continue;
            IRType* ptype = ir_type_from_datatype_ex(module,
                                                     p->data.var_decl.type,
                                                     p->data.var_decl.func_sig);
            (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
        }
        if (decl0->data.func_def.is_variadic) {
            (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
        }
    }

    // لا نريد ترك current_func على آخر توقيع.
    ir_builder_set_func(builder, NULL);

    // Walk top-level declarations: globals + functions
    for (Node* decl = program->data.program.declarations; decl; decl = decl->next) {
        // Global arrays
        if (decl->type == NODE_ARRAY_DECL && decl->data.array_decl.is_global) {
            if (!decl->data.array_decl.name) continue;

            if (decl->data.array_decl.total_elems <= 0 || decl->data.array_decl.total_elems > INT_MAX) {
                fprintf(stderr, "IR Lower Error: global array '%s' has invalid total size\n",
                        decl->data.array_decl.name ? decl->data.array_decl.name : "???");
                continue;
            }

            int total_count = (int)decl->data.array_decl.total_elems;
            DataType elem_dt = decl->data.array_decl.element_type;
            bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);

            IRType* elem_t = NULL;
            IRType* arr_t = NULL;

            if (elem_is_agg) {
                int elem_size = decl->data.array_decl.element_struct_size;
                if (elem_size <= 0) {
                    fprintf(stderr, "IR Lower Error: global array '%s' has invalid aggregate element size\n",
                            decl->data.array_decl.name ? decl->data.array_decl.name : "???");
                    continue;
                }
                int64_t total_bytes64 = decl->data.array_decl.total_elems * (int64_t)elem_size;
                if (total_bytes64 <= 0 || total_bytes64 > INT_MAX) {
                    fprintf(stderr, "IR Lower Error: global array '%s' is too large\n",
                            decl->data.array_decl.name ? decl->data.array_decl.name : "???");
                    continue;
                }
                elem_t = IR_TYPE_I8_T;
                arr_t = ir_type_array(IR_TYPE_I8_T, (int)total_bytes64);
            } else {
                elem_t = ir_type_from_datatype(module, elem_dt);
                if (!elem_t || elem_t->kind == IR_TYPE_VOID) elem_t = IR_TYPE_I64_T;
                arr_t = ir_type_array(elem_t, total_count);
            }
            if (!arr_t) continue;

            IRGlobal* g = ir_builder_create_global(builder, decl->data.array_decl.name, arr_t,
                                                   decl->data.array_decl.is_const ? 1 : 0);
            if (!g) continue;
            g->is_internal = decl->data.array_decl.is_static ? true : false;

            g->has_init_list = decl->data.array_decl.has_init ? true : false;

            if (decl->data.array_decl.has_init && !elem_is_agg) {
                int n = decl->data.array_decl.init_count;
                if (n < 0) n = 0;
                if (n > total_count) {
                    n = total_count;
                }

                // نخزن فقط العناصر المعطاة؛ الباقي يُعتبر صفراً كما في C.
                IRValue** elems = NULL;
                if (n > 0) {
                    elems = (IRValue**)ir_arena_calloc(&module->arena, (size_t)n, sizeof(IRValue*), _Alignof(IRValue*));
                    if (!elems) {
                        ir_builder_free(builder);
                        ir_module_free(module);
                        return NULL;
                    }
                }

                int i = 0;
                for (Node* v = decl->data.array_decl.init_values; v && i < n; v = v->next, i++) {
                    elems[i] = ir_lower_global_init_value(builder, v, elem_t);
                }

                g->init_elems = elems;
                g->init_elem_count = n;
            } else if (decl->data.array_decl.has_init && elem_is_agg && decl->data.array_decl.init_count > 0) {
                fprintf(stderr, "IR Lower Error: aggregate array initializer not supported for '%s'\n",
                        decl->data.array_decl.name ? decl->data.array_decl.name : "???");
            }

            continue;
        }

        // Globals
        if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
            if (decl->data.var_decl.type == TYPE_STRUCT || decl->data.var_decl.type == TYPE_UNION) {
                int n = decl->data.var_decl.struct_size;
                if (n <= 0) {
                    fprintf(stderr, "IR Lower Error: global struct '%s' has invalid size\n",
                            decl->data.var_decl.name ? decl->data.var_decl.name : "???");
                    continue;
                }
                IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
                IRGlobal* g = ir_builder_create_global(builder, decl->data.var_decl.name, bytes_t,
                                                       decl->data.var_decl.is_const ? 1 : 0);
                if (g) {
                    g->is_internal = decl->data.var_decl.is_static ? true : false;
                }
                continue;
            }

            IRType* gtype = ir_type_from_datatype_ex(module,
                                                     decl->data.var_decl.type,
                                                     decl->data.var_decl.func_sig);
            IRValue* init = ir_lower_global_init_value(builder, decl->data.var_decl.expression, gtype);

            IRGlobal* g = ir_builder_create_global_init(builder, decl->data.var_decl.name, gtype, init,
                                                        decl->data.var_decl.is_const ? 1 : 0);
            if (g) {
                g->is_internal = decl->data.var_decl.is_static ? true : false;
            }
            continue;
        }

        // Functions
        if (decl->type == NODE_FUNC_DEF) {
            if (!decl->data.func_def.name) continue;
            int decl_param_count = 0;
            for (Node* p = decl->data.func_def.params; p; p = p->next) {
                if (p->type == NODE_VAR_DECL) decl_param_count++;
            }

            const char* ir_name = decl->data.func_def.name;
            if (main_with_args && ir_lower_ast_func_is_main_with_args(decl)) {
                ir_name = BAA_WRAPPED_USER_MAIN_NAME;
            }

            IRFunc* func = ir_module_find_func(module, ir_name);
            if (!func) {
                IRType* ret_type = ir_type_from_datatype_ex(module,
                                                            decl->data.func_def.return_type,
                                                            decl->data.func_def.return_func_sig);
                func = ir_builder_create_func(builder, ir_name, ret_type);
                if (!func) continue;
                func->is_prototype = decl->data.func_def.is_prototype;
                func->is_variadic = decl->data.func_def.is_variadic;
                for (Node* p = decl->data.func_def.params; p; p = p->next) {
                    if (p->type != NODE_VAR_DECL) continue;
                    IRType* ptype = ir_type_from_datatype_ex(module,
                                                             p->data.var_decl.type,
                                                             p->data.var_decl.func_sig);
                    (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
                }
                if (decl->data.func_def.is_variadic) {
                    (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
                }
            } else {
                func->is_variadic = decl->data.func_def.is_variadic;
                if (decl->data.func_def.is_variadic &&
                    func->param_count == decl_param_count) {
                    ir_builder_set_func(builder, func);
                    (void)ir_builder_add_param(builder, "__baa_va_base", ir_type_ptr(IR_TYPE_I8_T));
                }
            }

            // Prototypes don't have a body or blocks.
            if (decl->data.func_def.is_prototype || func->is_prototype) {
                continue;
            }

            ir_builder_set_func(builder, func);

            // Entry block
            IRBlock* entry = ir_builder_create_block(builder, "بداية");
            ir_builder_set_insert_point(builder, entry);

            IRLowerCtx ctx;
            ir_lower_ctx_init(&ctx, builder);
            // نُبقي الاسم المصدرّي لأغراض رسائل/تسميات داخلية حتى إن اختلف اسم IR.
            ctx.current_func_name = decl->data.func_def.name;
            ctx.static_local_counter = 0;
            ctx.enable_bounds_checks = enable_bounds_checks;
            ctx.program_root = program;
            ctx.target = target;
            ctx.current_func_is_variadic = decl->data.func_def.is_variadic;
            ctx.current_func_fixed_params = decl_param_count;
            ctx.current_func_va_base_reg = -1;

            if (ctx.current_func_is_variadic &&
                func->param_count > ctx.current_func_fixed_params &&
                func->params) {
                ctx.current_func_va_base_reg = func->params[ctx.current_func_fixed_params].reg;
            }

            // نطاق الدالة: المعاملات + جسم الدالة
            ir_lower_scope_push(&ctx);

            // Parameters: spill into allocas so existing lowering can treat them like locals.
            int pi = 0;
            for (Node* p = decl->data.func_def.params; p; p = p->next) {
                if (p->type != NODE_VAR_DECL) continue;

                const char* pname = p->data.var_decl.name ? p->data.var_decl.name : NULL;
                if (!pname) { pi++; continue; }
                if (pi < 0 || pi >= func->param_count) { pi++; continue; }

                IRType* ptype = func->params[pi].type;
                int preg = func->params[pi].reg;

                // اربط تعليمات تهيئة معامل الدالة بموقعه في المصدر.
                ir_lower_set_loc(builder, p);

                int ptr_reg = ir_builder_emit_alloca(builder, ptype);
                ir_lower_tag_last_inst(builder, IR_OP_ALLOCA, ptr_reg, pname);
                ir_lower_bind_local(&ctx, pname, ptr_reg, ptype,
                                    p->data.var_decl.type == TYPE_POINTER ? p->data.var_decl.ptr_base_type : TYPE_INT,
                                    p->data.var_decl.type == TYPE_POINTER ? p->data.var_decl.ptr_base_type_name : NULL,
                                    p->data.var_decl.type == TYPE_POINTER ? p->data.var_decl.ptr_depth : 0);

                IRValue* val = ir_value_reg(preg, ptype);
                IRType* ptr_type = ir_type_ptr(ptype ? ptype : IR_TYPE_I64_T);
                IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);
                ir_lower_set_loc(builder, p);
                ir_builder_emit_store(builder, val, ptr);
                ir_lower_tag_last_inst(builder, IR_OP_STORE, -1, pname);

                pi++;
            }

            // Lower function body (NODE_BLOCK)
            if (decl->data.func_def.body) {
                lower_stmt(&ctx, decl->data.func_def.body);
            }

            // إذا وصلنا لنهاية الكتلة الحالية بدون منهي، نُغلق الدالة بشكل صريح.
            if (!ir_builder_is_block_terminated(builder)) {
                if (func->ret_type && func->ret_type->kind == IR_TYPE_VOID) {
                    ir_builder_emit_ret_void(builder);
                } else {
                    ir_lower_report_error(&ctx, decl,
                                          "الدالة '%s' قد تصل إلى النهاية بدون قيمة إرجاع.",
                                          decl->data.func_def.name ? decl->data.func_def.name : "???");
                    IRType* rt = func->ret_type ? func->ret_type : IR_TYPE_I64_T;
                    ir_builder_emit_ret(builder, ir_value_const_int(0, rt));
                }
            }

            // خروج نطاق الدالة
            ir_lower_scope_pop(&ctx);

            if (ctx.had_error) {
                ir_builder_free(builder);
                ir_module_free(module);
                return NULL;
            }
        }
    }

    // إن كانت الرئيسية بوسائط: ولّد غلاف ABI لـ C باسم 'الرئيسية' ليستدعي '__baa_user_main'.
    if (main_with_args) {
        if (!ir_lower_emit_main_args_wrapper(builder, program, main_site, target)) {
            ir_builder_free(builder);
            ir_module_free(module);
            return NULL;
        }
    }

    ir_builder_free(builder);
    return module;
}
