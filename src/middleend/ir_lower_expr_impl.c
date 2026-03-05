// ============================================================================
// Expression lowering (v0.3.0.3)
// ============================================================================

IRValue* lower_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) {
        return ir_builder_const_i64(0);
    }

    // تعيين موقع المصدر لهذه العقدة قبل توليد أي تعليمات.
    ir_lower_set_loc(ctx->builder, expr);

    switch (expr->type) {
        // -----------------------------------------------------------------
        // v0.3.0.3 required nodes
        // -----------------------------------------------------------------
        case NODE_INT:
            return ir_builder_const_i64((int64_t)expr->data.integer.value);

        case NODE_VAR_REF: {
            const char* name = expr->data.var_ref.name;

            // 1) Local variable (stack slot)
            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                const char* storage_name = ir_lower_binding_storage_name(b);
                if (b->value_type && b->value_type->kind == IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", name ? name : "???");
                    return ir_builder_const_i64(0);
                }
                IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

                IRValue* ptr = NULL;
                if (b->is_static_storage) {
                    ptr = ir_value_global(storage_name, value_type);
                } else {
                    IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
                    ptr = ir_value_reg(b->ptr_reg, ptr_type);
                }
                int loaded = ir_builder_emit_load(ctx->builder, value_type, ptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, storage_name);
                return ir_value_reg(loaded, value_type);
            }

            // 2) Global variable (module scope)
            IRGlobal* g = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                g = ir_module_find_global(ctx->builder->module, name);
            }
            if (g) {
                // Globals are addressable; load from @name
                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
                return ir_value_reg(loaded, g->type);
            }

            // 3) Function reference (as a value)
            IRFunc* f = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                f = ir_module_find_func(ctx->builder->module, name);
            }
            if (f) {
                IRType* pts_local[64];
                IRType** pts = NULL;
                int pc = f->param_count;
                if (pc < 0) pc = 0;
                if (pc > (int)(sizeof(pts_local) / sizeof(pts_local[0]))) {
                    pc = (int)(sizeof(pts_local) / sizeof(pts_local[0]));
                }
                for (int i = 0; i < pc; i++) {
                    pts_local[i] = (f->params && f->params[i].type) ? f->params[i].type : IR_TYPE_I64_T;
                }
                if (pc > 0) pts = pts_local;
                IRType* ft = ir_type_func(f->ret_type ? f->ret_type : IR_TYPE_VOID_T, pts, pc);
                return ir_value_func_ref(name, ft);
            }

            ir_lower_report_error(ctx, expr, "متغير غير معروف '%s'.", name ? name : "???");
            return ir_builder_const_i64(0);
        }

        case NODE_ARRAY_ACCESS: {
            const char* name = expr->data.array_op.name;
            int index_count = expr->data.array_op.index_count;
            if (index_count <= 0) index_count = ir_lower_index_count(expr->data.array_op.indices);

            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                const char* storage_name = ir_lower_binding_storage_name(b);

                // نص: الاسم يشير إلى قيمة ptr<char> مخزنة في alloca/رمز ساكن.
                if (!b->is_array &&
                    b->value_type && b->value_type->kind == IR_TYPE_PTR &&
                    b->value_type->data.pointee && b->value_type->data.pointee->kind == IR_TYPE_CHAR)
                {
                    Node* idx_node = expr->data.array_op.indices;
                    if (!idx_node) {
                        ir_lower_report_error(ctx, expr, "فهرس النص '%s' مفقود.", name ? name : "???");
                        return ir_builder_const_i64(0);
                    }

                    IRType* str_t = b->value_type;
                    IRValue* str_ptr = NULL;
                    if (b->is_static_storage) {
                        IRValue* gptr = ir_value_global(storage_name, str_t);
                        int loaded = ir_builder_emit_load(ctx->builder, str_t, gptr);
                        str_ptr = ir_value_reg(loaded, str_t);
                    } else {
                        IRType* ptr_to_str_t = ir_type_ptr(str_t);
                        IRValue* slot = ir_value_reg(b->ptr_reg, ptr_to_str_t);
                        int loaded = ir_builder_emit_load(ctx->builder, str_t, slot);
                        str_ptr = ir_value_reg(loaded, str_t);
                    }

                    IRValue* idx = ensure_i64(ctx, lower_expr(ctx, idx_node));
                    int ep = ir_builder_emit_ptr_offset(ctx->builder, str_t, str_ptr, idx);
                    IRValue* elem_ptr = ir_value_reg(ep, str_t);
                    int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, elem_ptr);
                    return ir_value_reg(ch, IR_TYPE_CHAR_T);
                }

                // مؤشرات عامة: p[i] و p[i][j]...
                if (b->is_pointer) {
                    if (!expr->data.array_op.indices) {
                        ir_lower_report_error(ctx, expr, "فهرس المؤشر '%s' مفقود.", name ? name : "???");
                        return ir_builder_const_i64(0);
                    }

                    IRType* ptr_t = b->value_type;
                    IRValue* ptr_val = NULL;
                    if (b->is_static_storage) {
                        IRValue* gptr = ir_value_global(storage_name, ptr_t);
                        int loaded = ir_builder_emit_load(ctx->builder, ptr_t, gptr);
                        ptr_val = ir_value_reg(loaded, ptr_t);
                    } else {
                        IRType* slot_t = ir_type_ptr(ptr_t);
                        IRValue* slot = ir_value_reg(b->ptr_reg, slot_t);
                        int loaded = ir_builder_emit_load(ctx->builder, ptr_t, slot);
                        ptr_val = ir_value_reg(loaded, ptr_t);
                    }

                    return ir_lower_pointer_index_chain(ctx, expr, ptr_val,
                                                        b->ptr_base_type, b->ptr_base_type_name, b->ptr_depth,
                                                        expr->data.array_op.indices, index_count,
                                                        false, NULL);
                }

                if (!b->is_array || !b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
                    for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                        (void)lower_expr(ctx, idx);
                    }
                    return ir_builder_const_i64(0);
                }

                bool elem_is_agg = (b->array_elem_type == TYPE_STRUCT || b->array_elem_type == TYPE_UNION);
                IRType* elem_t = elem_is_agg
                    ? IR_TYPE_I8_T
                    : (b->value_type->data.array.element ? b->value_type->data.array.element : IR_TYPE_I64_T);
                IRType* elem_ptr_t = ir_type_ptr(elem_t);
                bool elem_is_string = (!elem_is_agg &&
                                       elem_t && elem_t->kind == IR_TYPE_PTR &&
                                       elem_t->data.pointee &&
                                       elem_t->data.pointee->kind == IR_TYPE_CHAR);

                IRValue* base_ptr = NULL;
                if (b->is_static_storage) {
                    base_ptr = ir_value_global(storage_name, elem_t);
                } else {
                    IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                    int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
                    base_ptr = ir_value_reg(base_reg, elem_ptr_t);
                }

                if (!base_ptr) {
                    ir_lower_report_error(ctx, expr, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
                    for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                        (void)lower_expr(ctx, idx);
                    }
                    return ir_builder_const_i64(0);
                }

                if (elem_is_string && b->array_rank > 0 && index_count == b->array_rank + 1) {
                    IRValue* outer_linear = ir_lower_build_linear_index(ctx, expr,
                                                                        expr->data.array_op.indices,
                                                                        b->array_rank,
                                                                        b->array_dims,
                                                                        b->array_rank);
                    int ep_outer = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, outer_linear);
                    IRValue* outer_ptr = ir_value_reg(ep_outer, elem_ptr_t);
                    int ld_str = ir_builder_emit_load(ctx->builder, elem_t, outer_ptr);
                    IRValue* str_ptr = ir_value_reg(ld_str, elem_t);

                    Node* inner_node = ir_lower_nth_index(expr->data.array_op.indices, b->array_rank);
                    IRValue* inner_idx = ensure_i64(ctx, lower_expr(ctx, inner_node));
                    int ep_inner = ir_builder_emit_ptr_offset(ctx->builder, elem_t, str_ptr, inner_idx);
                    IRValue* ch_ptr = ir_value_reg(ep_inner, elem_t);
                    int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, ch_ptr);
                    return ir_value_reg(ch, IR_TYPE_CHAR_T);
                }

                IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                              expr->data.array_op.indices,
                                                              index_count,
                                                              b->array_dims,
                                                              b->array_rank);
                int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
                IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

                if (elem_is_agg) {
                    ir_lower_report_error(ctx, expr, "استخدام عنصر مصفوفة مركبة كقيمة غير مدعوم.");
                    return ir_builder_const_i64(0);
                }

                int loaded = ir_builder_emit_load(ctx->builder, elem_t, elem_ptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
                return ir_value_reg(loaded, elem_t);
            }

            IRGlobal* g = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                g = ir_module_find_global(ctx->builder->module, name);
            }
            if (!g || !g->type) {
                ir_lower_report_error(ctx, expr, "مصفوفة/نص غير معرّف '%s'.", name ? name : "???");
                for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                    (void)lower_expr(ctx, idx);
                }
                return ir_builder_const_i64(0);
            }

            Node* vdecl = ir_lower_find_global_var_decl(ctx, name);
            if (vdecl && vdecl->type == NODE_VAR_DECL &&
                vdecl->data.var_decl.type == TYPE_POINTER &&
                g && g->type) {
                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                IRValue* p = ir_value_reg(loaded, g->type);
                return ir_lower_pointer_index_chain(ctx, expr, p,
                                                    vdecl->data.var_decl.ptr_base_type,
                                                    vdecl->data.var_decl.ptr_base_type_name,
                                                    vdecl->data.var_decl.ptr_depth,
                                                    expr->data.array_op.indices, index_count,
                                                    false, NULL);
            }

            // نص عام: global يحمل ptr<char>، لذا نحمّله أولاً ثم نفهرسه.
            if (g->type->kind == IR_TYPE_PTR && g->type->data.pointee &&
                g->type->data.pointee->kind == IR_TYPE_CHAR)
            {
                Node* idx_node = expr->data.array_op.indices;
                if (!idx_node) {
                    ir_lower_report_error(ctx, expr, "فهرس النص '%s' مفقود.", name ? name : "???");
                    return ir_builder_const_i64(0);
                }

                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                IRValue* str_ptr = ir_value_reg(loaded, g->type);

                IRValue* idx = ensure_i64(ctx, lower_expr(ctx, idx_node));
                int ep = ir_builder_emit_ptr_offset(ctx->builder, g->type, str_ptr, idx);
                IRValue* elem_ptr = ir_value_reg(ep, g->type);
                int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, elem_ptr);
                return ir_value_reg(ch, IR_TYPE_CHAR_T);
            }

            if (g->type->kind != IR_TYPE_ARRAY) {
                ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة.", name ? name : "???");
                for (Node* idx = expr->data.array_op.indices; idx; idx = idx->next) {
                    (void)lower_expr(ctx, idx);
                }
                return ir_builder_const_i64(0);
            }

            Node* gdecl = ir_lower_find_global_array_decl(ctx, name);
            int rank = 1;
            const int* dims = NULL;
            DataType elem_dt = TYPE_INT;
            if (gdecl && gdecl->type == NODE_ARRAY_DECL) {
                rank = gdecl->data.array_decl.dim_count > 0 ? gdecl->data.array_decl.dim_count : 1;
                dims = gdecl->data.array_decl.dims;
                elem_dt = gdecl->data.array_decl.element_type;
            }

            bool elem_is_agg = (elem_dt == TYPE_STRUCT || elem_dt == TYPE_UNION);
            IRType* elem_t = elem_is_agg
                ? IR_TYPE_I8_T
                : (g->type->data.array.element ? g->type->data.array.element : IR_TYPE_I64_T);
            IRType* elem_ptr_t = ir_type_ptr(elem_t);
            bool elem_is_string = (!elem_is_agg &&
                                   elem_t && elem_t->kind == IR_TYPE_PTR &&
                                   elem_t->data.pointee &&
                                   elem_t->data.pointee->kind == IR_TYPE_CHAR);
            IRValue* base_ptr = ir_value_global(name, elem_t);

            if (elem_is_string && rank > 0 && index_count == rank + 1) {
                IRValue* outer_linear = ir_lower_build_linear_index(ctx, expr,
                                                                    expr->data.array_op.indices,
                                                                    rank,
                                                                    dims,
                                                                    rank);
                int ep_outer = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, outer_linear);
                IRValue* outer_ptr = ir_value_reg(ep_outer, elem_ptr_t);
                int ld_str = ir_builder_emit_load(ctx->builder, elem_t, outer_ptr);
                IRValue* str_ptr = ir_value_reg(ld_str, elem_t);

                Node* inner_node = ir_lower_nth_index(expr->data.array_op.indices, rank);
                IRValue* inner_idx = ensure_i64(ctx, lower_expr(ctx, inner_node));
                int ep_inner = ir_builder_emit_ptr_offset(ctx->builder, elem_t, str_ptr, inner_idx);
                IRValue* ch_ptr = ir_value_reg(ep_inner, elem_t);
                int ch = ir_builder_emit_load(ctx->builder, IR_TYPE_CHAR_T, ch_ptr);
                return ir_value_reg(ch, IR_TYPE_CHAR_T);
            }

            IRValue* linear = ir_lower_build_linear_index(ctx, expr,
                                                          expr->data.array_op.indices,
                                                          index_count,
                                                          dims,
                                                          rank);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, linear);
            IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

            if (elem_is_agg) {
                ir_lower_report_error(ctx, expr, "استخدام عنصر مصفوفة مركبة كقيمة غير مدعوم.");
                return ir_builder_const_i64(0);
            }

            int loaded = ir_builder_emit_load(ctx->builder, elem_t, elem_ptr);
            ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
            return ir_value_reg(loaded, elem_t);
        }

        case NODE_BIN_OP: {
            OpType op = expr->data.bin_op.op;

            // Arithmetic
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                if (expr->inferred_type == TYPE_POINTER) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                    bool left_ptr = expr->data.bin_op.left && expr->data.bin_op.left->inferred_type == TYPE_POINTER;
                    IRValue* base_ptr = left_ptr ? lhs0 : rhs0;
                    IRValue* delta_v = left_ptr ? rhs0 : lhs0;
                    IRType* ptr_t = (base_ptr && base_ptr->type && base_ptr->type->kind == IR_TYPE_PTR)
                        ? base_ptr->type
                        : ir_type_ptr(IR_TYPE_I8_T);
                    base_ptr = cast_to(ctx, base_ptr, ptr_t);
                    IRValue* delta = ensure_i64(ctx, delta_v);

                    Node* pnode = left_ptr ? expr->data.bin_op.left : expr->data.bin_op.right;
                    int64_t elem_size = ir_lower_pointer_elem_size(pnode);
                    if (elem_size > 1) {
                        int mr = ir_builder_emit_mul(ctx->builder, IR_TYPE_I64_T, delta,
                                                     ir_value_const_int(elem_size, IR_TYPE_I64_T));
                        delta = ir_value_reg(mr, IR_TYPE_I64_T);
                    }

                    if (op == OP_SUB) {
                        int nr = ir_builder_emit_neg(ctx->builder, IR_TYPE_I64_T, delta);
                        delta = ir_value_reg(nr, IR_TYPE_I64_T);
                    }
                    int pr = ir_builder_emit_ptr_offset(ctx->builder, ptr_t, base_ptr, delta);
                    return ir_value_reg(pr, ptr_t);
                }

                if (op == OP_SUB &&
                    expr->data.bin_op.left && expr->data.bin_op.right &&
                    expr->data.bin_op.left->inferred_type == TYPE_POINTER &&
                    expr->data.bin_op.right->inferred_type == TYPE_POINTER &&
                    expr->inferred_type != TYPE_POINTER) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRValue* lhs = cast_to(ctx, lhs0, IR_TYPE_I64_T);
                    IRValue* rhs = cast_to(ctx, rhs0, IR_TYPE_I64_T);

                    int dr = ir_builder_emit_sub(ctx->builder, IR_TYPE_I64_T, lhs, rhs);
                    IRValue* diff_bytes = ir_value_reg(dr, IR_TYPE_I64_T);

                    int64_t elem_size = ir_lower_pointer_elem_size(expr->data.bin_op.left);
                    if (elem_size > 1) {
                        int qr = ir_builder_emit_div(ctx->builder, IR_TYPE_I64_T,
                                                     diff_bytes,
                                                     ir_value_const_int(elem_size, IR_TYPE_I64_T));
                        return ir_value_reg(qr, IR_TYPE_I64_T);
                    }

                    return diff_bytes;
                }

                // عشري
                if (expr->inferred_type == TYPE_FLOAT) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRValue* lhs = cast_to(ctx, lhs0, IR_TYPE_F64_T);
                    IRValue* rhs = cast_to(ctx, rhs0, IR_TYPE_F64_T);

                    IRType* type = IR_TYPE_F64_T;
                    int dest = -1;

                    switch (ir_binop_to_irop(op)) {
                        case IR_OP_ADD: dest = ir_builder_emit_add(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_SUB: dest = ir_builder_emit_sub(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_MUL: dest = ir_builder_emit_mul(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_DIV: dest = ir_builder_emit_div(ctx->builder, type, lhs, rhs); break;
                        case IR_OP_MOD:
                            ir_lower_report_error(ctx, expr, "عملية باقي غير مدعومة على 'عشري'.");
                            return ir_value_const_int(0, IR_TYPE_F64_T);
                        default:
                            ir_lower_report_error(ctx, expr, "عملية حسابية غير مدعومة.");
                            return ir_value_const_int(0, IR_TYPE_F64_T);
                    }

                    return ir_value_reg(dest, type);
                }

                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                DataType lt = expr->data.bin_op.left ? expr->data.bin_op.left->inferred_type : TYPE_INT;
                DataType rt = expr->data.bin_op.right ? expr->data.bin_op.right->inferred_type : TYPE_INT;

                IRType* type = lower_common_int_type(ctx->builder ? ctx->builder->module : NULL, lt, rt);
                IRValue* lhs = cast_to(ctx, lhs0, type);
                IRValue* rhs = cast_to(ctx, rhs0, type);
                int dest = -1;

                switch (ir_binop_to_irop(op)) {
                    case IR_OP_ADD: dest = ir_builder_emit_add(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_SUB: dest = ir_builder_emit_sub(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MUL: dest = ir_builder_emit_mul(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_DIV: dest = ir_builder_emit_div(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MOD: dest = ir_builder_emit_mod(ctx->builder, type, lhs, rhs); break;
                    default:
                        ir_lower_report_error(ctx, expr, "عملية حسابية غير مدعومة.");
                        return ir_builder_const_i64(0);
                }

                return ir_value_reg(dest, type);
            }

            // Bitwise / Shift
            if (op == OP_BIT_AND || op == OP_BIT_OR || op == OP_BIT_XOR ||
                op == OP_SHL || op == OP_SHR) {
                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                DataType lt = expr->data.bin_op.left ? expr->data.bin_op.left->inferred_type : TYPE_INT;
                DataType rt = expr->data.bin_op.right ? expr->data.bin_op.right->inferred_type : TYPE_INT;

                IRType* type = NULL;
                if (op == OP_SHL || op == OP_SHR) {
                    DataType left_promoted = lower_int_promote_datatype(lt);
                    type = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, left_promoted);
                } else {
                    type = lower_common_int_type(ctx->builder ? ctx->builder->module : NULL, lt, rt);
                }
                if (!type) {
                    type = IR_TYPE_I64_T;
                }

                IRValue* lhs = cast_to(ctx, lhs0, type);
                IRValue* rhs = cast_to(ctx, rhs0, type);
                int dest = -1;

                switch (op) {
                    case OP_BIT_AND:
                        dest = ir_builder_emit_and(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_BIT_OR:
                        dest = ir_builder_emit_or(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_BIT_XOR:
                        dest = ir_builder_emit_xor(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_SHL:
                        dest = ir_builder_emit_shl(ctx->builder, type, lhs, rhs);
                        break;
                    case OP_SHR:
                        dest = ir_builder_emit_shr(ctx->builder, type, lhs, rhs);
                        break;
                    default:
                        ir_lower_report_error(ctx, expr, "عملية بتية غير مدعومة.");
                        return ir_builder_const_i64(0);
                }

                return ir_value_reg(dest, type);
            }

            // Comparison
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                if ((op == OP_EQ || op == OP_NEQ) &&
                    expr->data.bin_op.left && expr->data.bin_op.right &&
                    (expr->data.bin_op.left->inferred_type == TYPE_STRING || expr->data.bin_op.right->inferred_type == TYPE_STRING)) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRType* st = get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL);
                    if (!st) st = ir_type_ptr(IR_TYPE_CHAR_T);
                    IRValue* lhs = cast_to(ctx, lhs0, st);
                    IRValue* rhs = cast_to(ctx, rhs0, st);
                    IRCmpPred pred = ir_cmp_to_pred(op, false);
                    int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                    return ir_value_reg(dest, IR_TYPE_I1_T);
                }

                if (expr->data.bin_op.left && expr->data.bin_op.right &&
                    (expr->data.bin_op.left->inferred_type == TYPE_POINTER || expr->data.bin_op.right->inferred_type == TYPE_POINTER)) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                    IRType* ptr_t = (lhs0 && lhs0->type && lhs0->type->kind == IR_TYPE_PTR)
                        ? lhs0->type
                        : ((rhs0 && rhs0->type && rhs0->type->kind == IR_TYPE_PTR) ? rhs0->type : ir_type_ptr(IR_TYPE_I8_T));
                    IRValue* lhs = cast_to(ctx, lhs0, ptr_t);
                    IRValue* rhs = cast_to(ctx, rhs0, ptr_t);
                    IRCmpPred pred = ir_cmp_to_pred(op, false);
                    int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                    return ir_value_reg(dest, IR_TYPE_I1_T);
                }

                if (expr->data.bin_op.left && expr->data.bin_op.right &&
                    (expr->data.bin_op.left->inferred_type == TYPE_FUNC_PTR || expr->data.bin_op.right->inferred_type == TYPE_FUNC_PTR)) {
                    IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                    IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);

                    // توحيد ثابت "عدم" (0) ليأخذ نفس نوع مؤشر الدالة للطرف الآخر.
                    // هذا يمنع فشل متحقق الـ IR عندما يحمل "عدم" توقيعاً افتراضياً مختلفاً.
                    if (lhs0 && rhs0 && lhs0->type && rhs0->type) {
                        if (lhs0->type->kind == IR_TYPE_FUNC && !ir_types_equal(lhs0->type, rhs0->type)) {
                            if (rhs0->kind == IR_VAL_CONST_INT && rhs0->data.const_int == 0) {
                                rhs0 = ir_value_const_int(0, lhs0->type);
                            }
                        } else if (rhs0->type->kind == IR_TYPE_FUNC && !ir_types_equal(lhs0->type, rhs0->type)) {
                            if (lhs0->kind == IR_VAL_CONST_INT && lhs0->data.const_int == 0) {
                                lhs0 = ir_value_const_int(0, rhs0->type);
                            }
                        }
                    }

                    IRCmpPred pred = ir_cmp_to_pred(op, false);
                    int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs0, rhs0);
                    return ir_value_reg(dest, IR_TYPE_I1_T);
                }

                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                IRValue* lhs = lhs0;
                IRValue* rhs = rhs0;

                DataType lt = expr->data.bin_op.left ? expr->data.bin_op.left->inferred_type : TYPE_INT;
                DataType rt = expr->data.bin_op.right ? expr->data.bin_op.right->inferred_type : TYPE_INT;
                IRType* ct = NULL;
                if (lt == TYPE_FLOAT || rt == TYPE_FLOAT) {
                    ct = IR_TYPE_F64_T;
                } else {
                    ct = lower_common_int_type(ctx->builder ? ctx->builder->module : NULL, lt, rt);
                }

                // ترتيب الحروف يعتمد على نقطة-الكود، لا على تمثيل البايتات المعبأ.
                if (lhs0 && rhs0 && lhs0->type && rhs0->type &&
                    lhs0->type->kind == IR_TYPE_CHAR && rhs0->type->kind == IR_TYPE_CHAR &&
                    (op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE))
                {
                    lhs = ensure_i64(ctx, lhs0);
                    rhs = ensure_i64(ctx, rhs0);
                    ct = IR_TYPE_I64_T;
                }

                // طبّق الترقيات/التحويلات C-like قبل المقارنة.
                lhs = cast_to(ctx, lhs, ct);
                rhs = cast_to(ctx, rhs, ct);

                bool is_unsigned = false;
                if (ct && (ct->kind == IR_TYPE_U8 || ct->kind == IR_TYPE_U16 || ct->kind == IR_TYPE_U32 || ct->kind == IR_TYPE_U64))
                    is_unsigned = true;

                IRCmpPred pred = ir_cmp_to_pred(op, is_unsigned);
                int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            // Logical AND/OR (non-short-circuit form for now; proper short-circuit will be handled
            // during control-flow lowering by introducing blocks).
            if (op == OP_AND || op == OP_OR) {
                IRValue* lhs = lower_to_bool(ctx, lower_expr(ctx, expr->data.bin_op.left));
                IRValue* rhs = lower_to_bool(ctx, lower_expr(ctx, expr->data.bin_op.right));

                int dest = -1;
                if (op == OP_AND) dest = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, lhs, rhs);
                else dest = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, lhs, rhs);

                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            ir_lower_report_error(ctx, expr, "عملية ثنائية غير مدعومة.");
            return ir_builder_const_i64(0);
        }

        case NODE_UNARY_OP: {
            UnaryOpType op = expr->data.unary_op.op;

            if (op == UOP_ADDR) {
                IRType* pointee = IR_TYPE_I64_T;
                IRValue* addr = lower_lvalue_address(ctx, expr->data.unary_op.operand, &pointee);
                IRType* ptr_t = ir_type_ptr(pointee ? pointee : IR_TYPE_I64_T);
                if (!addr->type || !ir_types_equal(addr->type, ptr_t)) {
                    addr = cast_to(ctx, addr, ptr_t);
                }
                return addr;
            }

            if (op == UOP_DEREF) {
                IRValue* p0 = lower_expr(ctx, expr->data.unary_op.operand);
                IRType* target_t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, expr->inferred_type);
                if (!target_t || target_t->kind == IR_TYPE_VOID) {
                    target_t = IR_TYPE_I64_T;
                }
                IRType* ptr_t = ir_type_ptr(target_t);
                IRValue* p = cast_to(ctx, p0, ptr_t);
                int lr = ir_builder_emit_load(ctx->builder, target_t, p);
                return ir_value_reg(lr, target_t);
            }

            if (op == UOP_NEG) {
                IRValue* operand0 = lower_expr(ctx, expr->data.unary_op.operand);
                DataType dt = expr->inferred_type;
                if (dt == TYPE_FLOAT) {
                    IRValue* operand = cast_to(ctx, operand0, IR_TYPE_F64_T);
                    int dest = ir_builder_emit_neg(ctx->builder, IR_TYPE_F64_T, operand);
                    return ir_value_reg(dest, IR_TYPE_F64_T);
                }

                IRType* t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, dt);
                if (!t) t = IR_TYPE_I64_T;
                IRValue* operand = cast_to(ctx, operand0, t);
                int dest = ir_builder_emit_neg(ctx->builder, t, operand);
                return ir_value_reg(dest, t);
            }

            if (op == UOP_NOT) {
                // نفي منطقي: !(x) == (x == 0)
                IRValue* operand = lower_to_bool(ctx, lower_expr(ctx, expr->data.unary_op.operand));
                IRValue* zero = ir_value_const_int(0, IR_TYPE_I1_T);
                int dest = ir_builder_emit_cmp_eq(ctx->builder, operand, zero);
                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            if (op == UOP_BIT_NOT) {
                IRValue* operand0 = lower_expr(ctx, expr->data.unary_op.operand);
                DataType dt = expr->inferred_type;
                IRType* t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL, dt);
                if (!t || t->kind == IR_TYPE_VOID) {
                    t = IR_TYPE_I64_T;
                }
                IRValue* operand = cast_to(ctx, operand0, t);
                int dest = ir_builder_emit_not(ctx->builder, t, operand);
                return ir_value_reg(dest, t);
            }

            // ++ / -- in expression form will be addressed later (statement lowering / lvalues).
            ir_lower_report_error(ctx, expr, "عملية أحادية غير مدعومة (%d).", (int)op);
            return ir_builder_const_i64(0);
        }

        case NODE_POSTFIX_OP: {
            UnaryOpType op = expr->data.unary_op.op;
            if (op != UOP_INC && op != UOP_DEC) {
                ir_lower_report_error(ctx, expr, "عملية لاحقة غير مدعومة (%d).", (int)op);
                return ir_builder_const_i64(0);
            }

            IRType* pointee = IR_TYPE_I64_T;
            IRValue* addr0 = lower_lvalue_address(ctx, expr->data.unary_op.operand, &pointee);
            if (!pointee || pointee->kind == IR_TYPE_VOID) {
                ir_lower_report_error(ctx, expr, "نوع العملية اللاحقة غير صالح.");
                return ir_builder_const_i64(0);
            }
            if (pointee->kind == IR_TYPE_F64) {
                ir_lower_report_error(ctx, expr, "الزيادة/النقصان للعشري غير مدعومة حالياً.");
                return ir_builder_const_i64(0);
            }

            IRType* ptr_t = ir_type_ptr(pointee);
            IRValue* addr = cast_to(ctx, addr0, ptr_t);

            int old_r = ir_builder_emit_load(ctx->builder, pointee, addr);
            IRValue* old_v = ir_value_reg(old_r, pointee);
            IRValue* one = ir_value_const_int(1, pointee);

            int new_r = -1;
            if (op == UOP_INC) new_r = ir_builder_emit_add(ctx->builder, pointee, old_v, one);
            else new_r = ir_builder_emit_sub(ctx->builder, pointee, old_v, one);

            IRValue* new_v = ir_value_reg(new_r, pointee);
            ir_builder_emit_store(ctx->builder, new_v, addr);
            return old_v;
        }

        case NODE_CALL_EXPR:
            return lower_call_expr(ctx, expr);

        // -----------------------------------------------------------------
        // Extra nodes (not in v0.3.0.3 checklist, but useful for robustness)
        // -----------------------------------------------------------------
        case NODE_BOOL:
            return ir_value_const_int(expr->data.bool_lit.value ? 1 : 0, IR_TYPE_I1_T);

        case NODE_NULL:
            if (expr->inferred_type == TYPE_FUNC_PTR) {
                IRType* ft = ir_type_from_funcsig(ctx->builder ? ctx->builder->module : NULL,
                                                  expr->inferred_func_sig);
                return ir_value_const_int(0, ft ? ft : ir_type_func(IR_TYPE_VOID_T, NULL, 0));
            }
            if (expr->inferred_type == TYPE_STRING) {
                IRType* char_ptr_t = get_char_ptr_type(ctx->builder ? ctx->builder->module : NULL);
                return ir_value_const_int(0, char_ptr_t ? char_ptr_t : ir_type_ptr(IR_TYPE_CHAR_T));
            }
            return ir_value_const_int(0, ir_type_ptr(IR_TYPE_I8_T));

        case NODE_FLOAT:
            return ir_value_const_int((int64_t)expr->data.float_lit.bits, IR_TYPE_F64_T);

        case NODE_CHAR:
        {
            uint64_t packed = pack_utf8_codepoint((uint32_t)expr->data.char_lit.value);
            return ir_value_const_int((int64_t)packed, IR_TYPE_CHAR_T);
        }

        case NODE_STRING:
            return ir_builder_const_baa_string(ctx->builder, expr->data.string_lit.value);

        case NODE_SIZEOF:
            if (!expr->data.sizeof_expr.size_known) {
                ir_lower_report_error(ctx, expr, "فشل حساب 'حجم' أثناء التحليل الدلالي.");
                return ir_value_const_int(0, IR_TYPE_I64_T);
            }
            return ir_value_const_int(expr->data.sizeof_expr.size_bytes, IR_TYPE_I64_T);

        case NODE_CAST: {
            IRValue* src = lower_expr(ctx, expr->data.cast_expr.expression);
            IRType* to_t = ir_type_from_datatype(ctx->builder ? ctx->builder->module : NULL,
                                                 expr->data.cast_expr.target_type);
            if (!to_t) to_t = IR_TYPE_I64_T;
            return cast_to(ctx, src, to_t);
        }

        case NODE_MEMBER_ACCESS: {
            // ملاحظة: التحليل الدلالي يملأ معلومات الوصول (root/offset/type).
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value, IR_TYPE_I64_T);
            }

            if (!expr->data.member_access.is_struct_member || !expr->data.member_access.root_var) {
                ir_lower_report_error(ctx, expr, "وصول عضو غير صالح في مسار IR.");
                return ir_builder_const_i64(0);
            }

            if (expr->data.member_access.member_type == TYPE_STRUCT ||
                expr->data.member_access.member_type == TYPE_UNION) {
                ir_lower_report_error(ctx, expr, "قراءة عضو هيكل من نوع هيكل كقيمة غير مدعومة.");
                return ir_builder_const_i64(0);
            }

            IRModule* m = ctx->builder->module;
            if (m) ir_module_set_current(m);

            IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
            IRValue* base_ptr = NULL;

            if (expr->data.member_access.root_is_global) {
                base_ptr = ir_value_global(expr->data.member_access.root_var, IR_TYPE_I8_T);
            } else {
                IRLowerBinding* b = find_local(ctx, expr->data.member_access.root_var);
                if (!b || !b->value_type) {
                    ir_lower_report_error(ctx, expr, "هيكل محلي غير معروف '%s'.",
                                          expr->data.member_access.root_var);
                    return ir_builder_const_i64(0);
                }
                if (b->is_static_storage) {
                    base_ptr = ir_value_global(ir_lower_binding_storage_name(b), IR_TYPE_I8_T);
                } else {
                    IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                    int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
                    base_ptr = ir_value_reg(c, ptr_i8_t);
                }
            }

            IRValue* idx = ir_value_const_int((int64_t)expr->data.member_access.member_offset, IR_TYPE_I64_T);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
            IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

            IRType* field_val_t = ir_type_from_datatype(m, expr->data.member_access.member_type);
            if (!field_val_t || field_val_t->kind == IR_TYPE_VOID) field_val_t = IR_TYPE_I64_T;

            IRType* field_ptr_t = ir_type_ptr(field_val_t);
            int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, field_ptr_t);
            IRValue* field_ptr = ir_value_reg(fp, field_ptr_t);
            int r = ir_builder_emit_load(ctx->builder, field_val_t, field_ptr);
            return ir_value_reg(r, field_val_t);
        }

        default:
            ir_lower_report_error(ctx, expr, "عقدة تعبير غير مدعومة (%d).", (int)expr->type);
            return ir_builder_const_i64(0);
    }
}

