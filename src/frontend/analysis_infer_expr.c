static bool builtin_dispatch_known_call(Node* call_node,
                                        const char* fname,
                                        Node* args,
                                        DataType* out_return_type,
                                        bool clear_ptr_after_string)
{
    DataType built_ret = TYPE_INT;

    if (builtin_check_format_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_variadic_runtime_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_string_call(call_node, fname, args, &built_ret)) {
        if (clear_ptr_after_string && call_node) node_clear_inferred_ptr(call_node);
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_mem_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_file_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_math_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_system_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_time_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }
    if (builtin_check_error_call(call_node, fname, args, &built_ret)) {
        if (out_return_type) *out_return_type = built_ret;
        return true;
    }

    return false;
}

/**
 * @brief التحقق الموحد من معاملات استدعاء دالة مستخدم.
 */
static void analyze_user_function_call_args(Node* call_node, FuncSymbol* fs, Node* args)
{
    if (!call_node || !fs) return;

    int i = 0;
    Node* arg = args;
    while (arg) {
        if (i < fs->param_count) {
            DataType expected0 = fs->param_types[i];
            DataType at = infer_type_allow_null_string(arg, expected0);
            if (fs->param_types[i] == TYPE_POINTER) {
                if (at != TYPE_POINTER ||
                    !ptr_type_compatible(arg->inferred_ptr_base_type,
                                         arg->inferred_ptr_base_type_name,
                                         arg->inferred_ptr_depth,
                                         fs->param_ptr_base_types ? fs->param_ptr_base_types[i] : TYPE_INT,
                                         (fs->param_ptr_base_type_names && fs->param_ptr_base_type_names[i]) ? fs->param_ptr_base_type_names[i] : NULL,
                                         fs->param_ptr_depths ? fs->param_ptr_depths[i] : 0,
                                         true)) {
                    semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
                }
            } else if (fs->param_types[i] == TYPE_FUNC_PTR) {
                FuncPtrSig* expected_sig = (fs->param_func_sigs) ? fs->param_func_sigs[i] : NULL;
                if (!expected_sig) {
                    semantic_error(arg, "توقيع مؤشر الدالة لمعامل %d في '%s' مفقود.", i + 1, fs->name);
                } else if (arg->type == NODE_NULL) {
                    // سماح null لمؤشر الدالة: نُحدد نوعه كي يُخفض بشكل صحيح.
                    arg->inferred_type = TYPE_FUNC_PTR;
                    node_set_inferred_funcptr(arg, expected_sig);
                } else if (at != TYPE_FUNC_PTR || !funcsig_equal(arg->inferred_func_sig, expected_sig)) {
                    semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق (توقيع مؤشر دالة مختلف).", i + 1, fs->name);
                }
            } else if (!types_compatible(at, fs->param_types[i])) {
                semantic_error(arg, "نوع المعامل %d في '%s' غير متوافق.", i + 1, fs->name);
            } else {
                maybe_warn_implicit_narrowing(at, fs->param_types[i], arg);
            }
        } else {
            DataType at = infer_type(arg);
            if (fs->is_variadic) {
                if (!variadic_extra_arg_type_supported(at)) {
                    semantic_error(arg, "نوع المعامل المتغير %d في '%s' غير مدعوم.", i + 1, fs->name);
                }
            }
        }
        i++;
        arg = arg->next;
    }
    if (fs->is_variadic) {
        if (i < fs->param_count) {
            semantic_error(call_node, "عدد معاملات '%s' غير صحيح (المتوقع على الأقل %d).",
                           fs->name, fs->param_count);
        }
    } else if (i != fs->param_count) {
        semantic_error(call_node, "عدد معاملات '%s' غير صحيح (المتوقع %d).", fs->name, fs->param_count);
    }
}

/**
 * @brief استنتاج نوع التعبير.
 * @return DataType نوع التعبير الناتج.
 */
static DataType infer_type_internal(Node* node);

static DataType infer_type_internal(Node* node) {
    if (!node) return TYPE_INT; // افتراضي لتجنب الانهيار

    switch (node->type) {
        case NODE_INT:
        {
            // قاعدة نمط C للأعداد العشرية بدون لاحقة:
            // إذا كانت ضمن مدى 32-بت موقّع → اعتبرها ص٣٢، وإلا اعتبرها صحيح/ص٦٤.
            int64_t v = (int64_t)node->data.integer.value;
            if (v >= INT32_MIN && v <= INT32_MAX)
                return TYPE_I32;
            return TYPE_INT;
        }

        case NODE_FLOAT:
            return TYPE_FLOAT;

        case NODE_CHAR:
            return TYPE_CHAR;
        
        case NODE_STRING:
            return TYPE_STRING;
        
        case NODE_BOOL:
            return TYPE_BOOL;

        case NODE_NULL:
            node_set_inferred_ptr(node, TYPE_INT, NULL, 0);
            return TYPE_POINTER;

        case NODE_SIZEOF: {
            int64_t size = 0;
            bool ok = false;

            if (node->data.sizeof_expr.has_type_form) {
                ok = datatype_size_bytes(node->data.sizeof_expr.target_type,
                                         node->data.sizeof_expr.target_type_name,
                                         &size);
            } else {
                if (!node->data.sizeof_expr.expression) {
                    semantic_error(node, "تعبير 'حجم' يتطلب نوعاً أو تعبيراً صالحاً.");
                    ok = false;
                } else {
                    ok = sizeof_expr_bytes(node->data.sizeof_expr.expression, &size);
                }
            }

            if (!ok) {
                semantic_error(node, "لا يمكن حساب 'حجم' لهذا التعبير/النوع.");
                node->data.sizeof_expr.size_known = false;
                node->data.sizeof_expr.size_bytes = 0;
            } else {
                node->data.sizeof_expr.size_known = true;
                node->data.sizeof_expr.size_bytes = size;
            }

            return TYPE_INT;
        }
            
        case NODE_VAR_REF: {
            Symbol* sym = lookup(node->data.var_ref.name, true); // تعليم كمستخدم
            if (!sym) {
                // قد يكون الاسم اسم دالة مستخدمة كقيمة (مؤشر دالة).
                FuncSymbol* fs = func_lookup(node->data.var_ref.name);
                if (fs && fs->ref_funcptr_sig) {
                    node_set_inferred_funcptr(node, fs->ref_funcptr_sig);
                    return TYPE_FUNC_PTR;
                }
                if (fs && !fs->ref_funcptr_sig) {
                    semantic_error(node,
                                   "لا يمكن أخذ مرجع الدالة '%s' كمؤشر دالة حالياً (Higher-order غير مدعوم).",
                                   node->data.var_ref.name ? node->data.var_ref.name : "???");
                    return TYPE_FUNC_PTR;
                }
                semantic_error(node, "متغير غير معرّف '%s'.", node->data.var_ref.name);
                return TYPE_INT; // استرداد الخطأ
            }
            if (sym->is_array) {
                semantic_error(node, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", node->data.var_ref.name);
                return sym->type;
            }
            if (sym->type == TYPE_STRUCT || sym->type == TYPE_UNION) {
                semantic_error(node, "لا يمكن استخدام النوع المركب '%s' كقيمة مباشرة (استخدم ':' للوصول لعضو).", sym->name);
                return sym->type;
            }
            if (sym->type == TYPE_POINTER) {
                node_set_inferred_ptr(node,
                                      sym->ptr_base_type,
                                      sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                      sym->ptr_depth);
            } else if (sym->type == TYPE_FUNC_PTR) {
                node_set_inferred_funcptr(node, sym->func_sig);
            } else {
                node_clear_inferred_ptr(node);
            }
            return sym->type;
        }

        case NODE_MEMBER_ACCESS: {
            resolve_member_access(node);
            if (node->data.member_access.is_enum_value) {
                return TYPE_ENUM;
            }
            if (node->data.member_access.is_struct_member) {
                if (node->data.member_access.member_type == TYPE_STRUCT ||
                    node->data.member_access.member_type == TYPE_UNION) {
                    semantic_error(node, "لا يمكن استخدام عضو مركب كقيمة (استخدم ':' للتعشيش).");
                }
                if (node->data.member_access.member_type == TYPE_POINTER) {
                    node_set_inferred_ptr(node,
                                          node->data.member_access.member_ptr_base_type,
                                          node->data.member_access.member_ptr_base_type_name,
                                          node->data.member_access.member_ptr_depth);
                } else {
                    node_clear_inferred_ptr(node);
                }
                return node->data.member_access.member_type;
            }
            semantic_error(node, "وصول عضو غير صالح.");
            return TYPE_INT;
        }

        case NODE_ARRAY_ACCESS: {
            Symbol* sym = lookup(node->data.array_op.name, true);
            if (!sym) {
                semantic_error(node, "مصفوفة غير معرّفة '%s'.", node->data.array_op.name ? node->data.array_op.name : "???");
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                return TYPE_INT;
            }

            int supplied = node->data.array_op.index_count;
            if (supplied <= 0) supplied = node_list_count(node->data.array_op.indices);

            if (!sym->is_array) {
                // السماح بفهرسة النص على أنه حرف[] (v0.3.5)
                if (sym->type == TYPE_STRING) {
                    if (supplied != 1) {
                        semantic_error(node, "فهرسة النص '%s' تتطلب فهرساً واحداً.", sym->name);
                    }
                    for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                        DataType it = infer_type(idx);
                        if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                            semantic_error(idx, "فهرس النص يجب أن يكون صحيحاً.");
                        }
                    }
                    node_clear_inferred_ptr(node);
                    return TYPE_CHAR;
                }

                // فهرسة المؤشر: p[i] ⇢ *(p + i)
                if (sym->type == TYPE_POINTER) {
                    DataType cur_t = TYPE_POINTER;
                    DataType cur_base = sym->ptr_base_type;
                    const char* cur_base_name = (sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL);
                    int cur_depth = sym->ptr_depth;

                    if (cur_depth <= 0) {
                        semantic_error(node, "عمق المؤشر غير صالح في '%s'.", sym->name);
                        node_clear_inferred_ptr(node);
                        return TYPE_INT;
                    }

                    Node* idx_node = node->data.array_op.indices;
                    for (int i = 0; i < supplied && idx_node; i++, idx_node = idx_node->next) {
                        DataType it = infer_type(idx_node);
                        if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                            semantic_error(idx_node, "فهرس المؤشر يجب أن يكون صحيحاً.");
                        }

                        if (cur_t == TYPE_POINTER) {
                            if (!ptr_arith_allowed(cur_base, cur_depth)) {
                                semantic_error(node, "فهرسة هذا المؤشر غير مسموحة (نوع الأساس غير قابل للحساب).");
                                cur_t = TYPE_INT;
                                break;
                            }
                            if (cur_depth <= 0) {
                                semantic_error(node, "عمق المؤشر غير صالح أثناء فهرسة '%s'.", sym->name);
                                cur_t = TYPE_INT;
                                break;
                            }
                            if (cur_depth == 1) {
                                cur_t = cur_base;
                            } else {
                                cur_depth--;
                                cur_t = TYPE_POINTER;
                            }
                        } else if (cur_t == TYPE_STRING) {
                            cur_t = TYPE_CHAR;
                        } else {
                            semantic_error(node, "لا يمكن فهرسة الناتج من '%s' بهذه الطريقة.", sym->name);
                            cur_t = TYPE_INT;
                            break;
                        }

                        if (cur_t == TYPE_CHAR && i != supplied - 1) {
                            semantic_error(node, "فهرسة متعددة بعد محرف غير مدعومة.");
                            break;
                        }
                    }

                    if (cur_t == TYPE_POINTER) {
                        node_set_inferred_ptr(node, cur_base, cur_base_name, cur_depth);
                    } else {
                        node_clear_inferred_ptr(node);
                    }

                    if (cur_t == TYPE_STRUCT || cur_t == TYPE_UNION) {
                        semantic_error(node, "استخدام نوع مركب كناتج فهرسة '%s' كقيمة غير مدعوم.", sym->name);
                    }
                    return cur_t;
                }

                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                return sym->type;
            }

            int expected = (sym->array_rank > 0) ? sym->array_rank : 1;
            if (sym->type == TYPE_STRING && supplied == expected + 1) {
                Node* idx = node->data.array_op.indices;
                for (int i = 0; i < expected && idx; i++, idx = idx->next) {
                    DataType it = infer_type(idx);
                    if (!types_compatible(it, TYPE_INT) || it == TYPE_FLOAT) {
                        semantic_error(idx, "فهرس المصفوفة يجب أن يكون صحيحاً.");
                    }
                    if (idx->type == NODE_INT && sym->array_dims && i < sym->array_rank) {
                        int64_t iv = (int64_t)idx->data.integer.value;
                        int dim = sym->array_dims[i];
                        if (iv < 0 || (dim > 0 && iv >= dim)) {
                            semantic_error(node,
                                           "فهرس خارج الحدود للمصفوفة '%s' في البعد %d (الحجم %d).",
                                           sym->name, i + 1, dim);
                        }
                    }
                }
                if (!idx) {
                    semantic_error(node, "فهرس النص داخل مصفوفة النصوص مفقود.");
                    node_clear_inferred_ptr(node);
                    return TYPE_CHAR;
                }
                DataType sit = infer_type(idx);
                if (!types_compatible(sit, TYPE_INT) || sit == TYPE_FLOAT) {
                    semantic_error(idx, "فهرس النص يجب أن يكون صحيحاً.");
                }
                node_clear_inferred_ptr(node);
                return TYPE_CHAR;
            }
            (void)analyze_indices_type_and_bounds(sym, node, node->data.array_op.indices, expected);
            if (sym->type == TYPE_POINTER) {
                node_set_inferred_ptr(node,
                                      sym->ptr_base_type,
                                      sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                      sym->ptr_depth);
            } else {
                node_clear_inferred_ptr(node);
            }
            return sym->type; // نوع العنصر
        }

        case NODE_CALL_EXPR:
        {
            const char* fname = node->data.call.name;
            // 1) أعطِ أولوية للرموز في النطاق (متغيرات/معاملات) حتى تعمل الـ shadowing بشكل صحيح.
            Symbol* callee_sym = lookup(fname, true);
            if (callee_sym) {
                if (callee_sym->is_array) {
                    semantic_error(node, "لا يمكن نداء المصفوفة '%s'.", callee_sym->name);
                    analyze_consume_call_args(node->data.call.args);
                    return TYPE_INT;
                }

                if (callee_sym->type != TYPE_FUNC_PTR) {
                    semantic_error(node, "المعرف '%s' ليس دالة ولا مؤشر دالة قابل للنداء.", fname ? fname : "???");
                    analyze_consume_call_args(node->data.call.args);
                    return TYPE_INT;
                }

                FuncPtrSig* sig = callee_sym->func_sig;
                if (!sig) {
                    semantic_error(node, "توقيع مؤشر الدالة للرمز '%s' مفقود.", fname ? fname : "???");
                    analyze_consume_call_args(node->data.call.args);
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }
                analyze_funcptr_call_args(node, fname, node->data.call.args, sig);

                if (sig->return_type == TYPE_POINTER) {
                    node_set_inferred_ptr(node,
                                          sig->return_ptr_base_type,
                                          sig->return_ptr_base_type_name,
                                          sig->return_ptr_depth);
                } else if (sig->return_type == TYPE_FUNC_PTR) {
                    semantic_error(node, "Higher-order غير مدعوم: نوع الإرجاع في توقيع مؤشر الدالة هو مؤشر دالة.");
                    node_clear_inferred_ptr(node);
                } else {
                    node_clear_inferred_ptr(node);
                }

                return sig->return_type;
            }

            // 2) ثم نحل الدوال المعرفة (أو builtins) إذا لم يوجد رمز بنفس الاسم.
            FuncSymbol* fs = func_lookup(fname);
            if (!fs || !fs->is_defined) {
                DataType built_ret = TYPE_INT;
                if (builtin_dispatch_known_call(node, fname, node->data.call.args, &built_ret, true)) {
                    return built_ret;
                }

                if (!fs) {
                    semantic_error(node, "استدعاء دالة غير معرّفة '%s'.", fname ? fname : "???");
                    // ما زلنا نستنتج أنواع الوسائط لاكتشاف أخطاء أخرى.
                    analyze_consume_call_args(node->data.call.args);
                    return TYPE_INT;
                }
            }
            // تحقق عدد وأنواع المعاملات
            analyze_user_function_call_args(node, fs, node->data.call.args);
            if (fs->return_type == TYPE_POINTER) {
                node_set_inferred_ptr(node,
                                      fs->return_ptr_base_type,
                                      fs->return_ptr_base_type_name,
                                      fs->return_ptr_depth);
            } else if (fs->return_type == TYPE_FUNC_PTR) {
                node_set_inferred_funcptr(node, fs->return_func_sig);
            } else {
                node_clear_inferred_ptr(node);
            }
            return fs->return_type;
        }

        case NODE_CAST: {
            Node* src = node->data.cast_expr.expression;
            DataType src_t = infer_type(src);
            DataType dst_t = node->data.cast_expr.target_type;

            if (src_t == TYPE_FUNC_PTR || dst_t == TYPE_FUNC_PTR) {
                semantic_error(node, "التحويل الصريح من/إلى مؤشر دالة غير مدعوم حالياً.");
                node_clear_inferred_ptr(node);
                return dst_t;
            }

            if (dst_t == TYPE_VOID) {
                semantic_error(node, "التحويل الصريح إلى 'عدم' غير مدعوم حالياً.");
                node_clear_inferred_ptr(node);
                return TYPE_VOID;
            }

            if (dst_t == TYPE_STRUCT || dst_t == TYPE_UNION) {
                semantic_error(node, "التحويل الصريح إلى نوع مركب غير مدعوم حالياً.");
                node_clear_inferred_ptr(node);
                return dst_t;
            }

            if (dst_t == TYPE_POINTER) {
                if (node->data.cast_expr.target_ptr_depth <= 0) {
                    semantic_error(node, "نوع المؤشر الهدف في التحويل غير صالح.");
                }

                if ((node->data.cast_expr.target_ptr_base_type == TYPE_STRUCT &&
                     (!node->data.cast_expr.target_ptr_base_type_name || !struct_lookup_def(node->data.cast_expr.target_ptr_base_type_name))) ||
                    (node->data.cast_expr.target_ptr_base_type == TYPE_UNION &&
                     (!node->data.cast_expr.target_ptr_base_type_name || !union_lookup_def(node->data.cast_expr.target_ptr_base_type_name))) ||
                    (node->data.cast_expr.target_ptr_base_type == TYPE_ENUM &&
                     (!node->data.cast_expr.target_ptr_base_type_name || !enum_lookup_def(node->data.cast_expr.target_ptr_base_type_name)))) {
                    semantic_error(node, "أساس المؤشر الهدف في التحويل غير معرّف.");
                }

                if (!(src_t == TYPE_POINTER || datatype_is_intlike(src_t))) {
                    semantic_error(node, "التحويل إلى مؤشر يتطلب مؤشراً أو عدداً صحيحاً.");
                }

                node_set_inferred_ptr(node,
                                      node->data.cast_expr.target_ptr_base_type,
                                      node->data.cast_expr.target_ptr_base_type_name,
                                      node->data.cast_expr.target_ptr_depth);
                return TYPE_POINTER;
            }

            if (src_t == TYPE_POINTER) {
                if (!datatype_is_intlike(dst_t)) {
                    semantic_error(node, "التحويل من مؤشر يدعم الأنواع الصحيحة فقط حالياً.");
                }
                node_clear_inferred_ptr(node);
                return dst_t;
            }

            if (!(datatype_is_numeric_scalar(src_t) && datatype_is_numeric_scalar(dst_t))) {
                semantic_error(node, "التحويل الصريح يدعم التحويلات العددية أو المؤشرية فقط.");
            }

            node_clear_inferred_ptr(node);
            return dst_t;
        }

        case NODE_BIN_OP: {
            DataType left = infer_type(node->data.bin_op.left);
            DataType right = infer_type(node->data.bin_op.right);
            
            // العمليات الحسابية تتطلب أعداداً صحيحة
            OpType op = node->data.bin_op.op;

            // مقارنة مؤشرات الدوال (==/!=) فقط، مع السماح بالمقارنة مع عدم.
            if (left == TYPE_FUNC_PTR || right == TYPE_FUNC_PTR) {
                if (!(op == OP_EQ || op == OP_NEQ)) {
                    semantic_error(node, "مقارنة مؤشرات الدوال تدعم فقط (==) أو (!=).");
                }

                Node* l = node->data.bin_op.left;
                Node* r = node->data.bin_op.right;
                bool l_null = (l && l->type == NODE_NULL);
                bool r_null = (r && r->type == NODE_NULL);

                if (l_null && right == TYPE_FUNC_PTR) {
                    l->inferred_type = TYPE_FUNC_PTR;
                    node_set_inferred_funcptr(l, r ? r->inferred_func_sig : NULL);
                }
                if (r_null && left == TYPE_FUNC_PTR) {
                    r->inferred_type = TYPE_FUNC_PTR;
                    node_set_inferred_funcptr(r, l ? l->inferred_func_sig : NULL);
                }

                if (!l_null && !r_null) {
                    if (left != TYPE_FUNC_PTR || right != TYPE_FUNC_PTR) {
                        semantic_error(node, "مقارنة مؤشر دالة تتطلب مؤشراً مقابلاً أو قيمة عدم.");
                    } else if (!funcsig_equal(l ? l->inferred_func_sig : NULL, r ? r->inferred_func_sig : NULL)) {
                        semantic_error(node, "مقارنة مؤشري دالة تتطلب تطابق التوقيع.");
                    }
                }

                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }

            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                if (left == TYPE_POINTER || right == TYPE_POINTER) {
                    if ((op == OP_ADD || op == OP_SUB) &&
                        ((left == TYPE_POINTER && datatype_is_intlike(right)) ||
                         (op == OP_ADD && right == TYPE_POINTER && datatype_is_intlike(left)))) {
                        Node* pnode = (left == TYPE_POINTER) ? node->data.bin_op.left : node->data.bin_op.right;
                        if (!pnode || !ptr_arith_allowed(pnode->inferred_ptr_base_type, pnode->inferred_ptr_depth)) {
                            semantic_error(node, "الحساب على هذا النوع من المؤشرات غير مدعوم حالياً.");
                        }
                        node_set_inferred_ptr(node,
                                              pnode ? pnode->inferred_ptr_base_type : TYPE_INT,
                                              pnode ? pnode->inferred_ptr_base_type_name : NULL,
                                              pnode ? pnode->inferred_ptr_depth : 1);
                        return TYPE_POINTER;
                    }
                    if (op == OP_SUB && left == TYPE_POINTER && right == TYPE_POINTER) {
                        Node* lp = node->data.bin_op.left;
                        Node* rp = node->data.bin_op.right;
                        if (!lp || !rp || lp->inferred_ptr_depth <= 0 || rp->inferred_ptr_depth <= 0) {
                            semantic_error(node, "طرح المؤشر الفارغ غير مدعوم.");
                        } else if (!ptr_type_compatible(lp->inferred_ptr_base_type,
                                                        lp->inferred_ptr_base_type_name,
                                                        lp->inferred_ptr_depth,
                                                        rp->inferred_ptr_base_type,
                                                        rp->inferred_ptr_base_type_name,
                                                        rp->inferred_ptr_depth,
                                                        false)) {
                            semantic_error(node, "طرح مؤشرين يتطلب توافق نوع المؤشر.");
                        }
                        if (!ptr_arith_allowed(lp ? lp->inferred_ptr_base_type : TYPE_INT,
                                               lp ? lp->inferred_ptr_depth : 0)) {
                            semantic_error(node, "الحساب على هذا النوع من المؤشرات غير مدعوم حالياً.");
                        }
                        node_clear_inferred_ptr(node);
                        return TYPE_INT;
                    }
                    semantic_error(node, "عمليات المؤشرات المدعومة: pointer +/- int أو pointer - pointer.");
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }

                if (op == OP_MOD && (left == TYPE_FLOAT || right == TYPE_FLOAT)) {
                    semantic_error(node, "عملية باقي غير مدعومة على 'عشري'.");
                }

                // إذا كان أحد الطرفين عشرياً → النتيجة عشري (بعد تحويل الطرف الآخر).
                if (left == TYPE_FLOAT || right == TYPE_FLOAT) {
                    if (!((datatype_is_intlike(left) || left == TYPE_FLOAT) && (datatype_is_intlike(right) || right == TYPE_FLOAT))) {
                        semantic_error(node, "عمليات العشري تتطلب معاملات عددية.");
                    }
                    return TYPE_FLOAT;
                }

                if (!datatype_is_intlike(left) || !datatype_is_intlike(right)) {
                    semantic_error(node, "العمليات الحسابية تتطلب معاملات صحيحة.");
                }
                node_clear_inferred_ptr(node);
                return datatype_usual_arith(left, right);
            }

            // العمليات البتية والإزاحة
            if (op == OP_BIT_AND || op == OP_BIT_OR || op == OP_BIT_XOR ||
                op == OP_SHL || op == OP_SHR) {
                if (!datatype_is_intlike(left) || !datatype_is_intlike(right)) {
                    semantic_error(node, "العمليات البتية تتطلب معاملات عددية صحيحة.");
                    return TYPE_INT;
                }

                if ((op == OP_SHL || op == OP_SHR) &&
                    node->data.bin_op.right &&
                    node->data.bin_op.right->type == NODE_INT) {
                    int64_t sh = node->data.bin_op.right->data.integer.value;
                    if (sh < 0 || sh >= 64) {
                        semantic_error(node->data.bin_op.right, "قيمة الإزاحة يجب أن تكون بين ٠ و ٦٣.");
                    }
                }

                if (op == OP_SHL || op == OP_SHR) {
                    node_clear_inferred_ptr(node);
                    return datatype_int_promote(left);
                }
                node_clear_inferred_ptr(node);
                return datatype_usual_arith(left, right);
            }
            
            // عمليات المقارنة تتطلب أنواعاً متوافقة وتعيد منطقي
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                // سماح v0.3.12: مقارنة نص مع نص (==/!=) وكذلك نص مع عدم (==/!=) لتسهيل نمط EOF في اقرأ_سطر.
                if (left == TYPE_STRING || right == TYPE_STRING) {
                    if (!(op == OP_EQ || op == OP_NEQ)) {
                        semantic_error(node, "مقارنة النص تدعم فقط (==) أو (!=).");
                    }

                    Node* l = node->data.bin_op.left;
                    Node* r = node->data.bin_op.right;
                    bool l_null = (l && l->type == NODE_NULL);
                    bool r_null = (r && r->type == NODE_NULL);

                    // الحالات المدعومة:
                    // - نص == نص
                    // - نص == عدم (أو العكس)
                    if (left == TYPE_STRING && !(right == TYPE_STRING || r_null)) {
                        semantic_error(node, "مقارنة النص تتطلب نصاً أو 'عدم' على الطرف الآخر.");
                    }
                    if (right == TYPE_STRING && !(left == TYPE_STRING || l_null)) {
                        semantic_error(node, "مقارنة النص تتطلب نصاً أو 'عدم' على الطرف الآخر.");
                    }

                    // اجعل NODE_NULL يُعامل كنص فارغ (NULL) في سياق مقارنة النص.
                    if (l_null) {
                        l->inferred_type = TYPE_STRING;
                        node_clear_inferred_ptr(l);
                    }
                    if (r_null) {
                        r->inferred_type = TYPE_STRING;
                        node_clear_inferred_ptr(r);
                    }

                    node_clear_inferred_ptr(node);
                    return TYPE_BOOL;
                }

                if (left == TYPE_POINTER || right == TYPE_POINTER) {
                    if (left == TYPE_POINTER && right == TYPE_POINTER) {
                        int ldepth = node->data.bin_op.left->inferred_ptr_depth;
                        int rdepth = node->data.bin_op.right->inferred_ptr_depth;
                        if (!(ldepth == 0 || rdepth == 0) &&
                            !ptr_type_compatible(node->data.bin_op.left->inferred_ptr_base_type,
                                                 node->data.bin_op.left->inferred_ptr_base_type_name,
                                                 ldepth,
                                                 node->data.bin_op.right->inferred_ptr_base_type,
                                                 node->data.bin_op.right->inferred_ptr_base_type_name,
                                                 rdepth,
                                                 true)) {
                            semantic_error(node, "مقارنة المؤشرات تتطلب توافق نوع المؤشر.");
                        }
                        node_clear_inferred_ptr(node);
                        return TYPE_BOOL;
                    }
                    semantic_error(node, "مقارنة المؤشر تتطلب مؤشراً مقابلاً أو قيمة عدم.");
                    node_clear_inferred_ptr(node);
                    return TYPE_BOOL;
                }

                // مقارنة العشري: نسمح بمقارنة عشري مع عدد (يحوّل العدد إلى عشري).
                if (left == TYPE_FLOAT || right == TYPE_FLOAT) {
                    if (!((datatype_is_intlike(left) || left == TYPE_FLOAT) && (datatype_is_intlike(right) || right == TYPE_FLOAT))) {
                        semantic_error(node, "مقارنات العشري تتطلب معاملات عددية.");
                    }
                    node_clear_inferred_ptr(node);
                    return TYPE_BOOL;
                }
                maybe_warn_signed_unsigned_compare(left, right, node);
                if (left != right) {
                    // سماح C-like: مقارنة حرف مع صحيح.
                    if (!((datatype_is_intlike(left) && datatype_is_intlike(right)) ||
                          (left == TYPE_ENUM && right == TYPE_ENUM))) {
                        semantic_error(node, "عمليات المقارنة تتطلب تطابق نوعي المعاملات.");
                    }
                } else if (left == TYPE_ENUM) {
                    const char* ln = expr_enum_name(node->data.bin_op.left);
                    const char* rn = expr_enum_name(node->data.bin_op.right);
                    if (!ln || !rn || strcmp(ln, rn) != 0) {
                        semantic_error(node, "لا يمكن مقارنة قيم تعداد من أنواع مختلفة.");
                    }
                } else if (left == TYPE_STRUCT) {
                    semantic_error(node, "لا يمكن مقارنة الهياكل.");
                }
                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }
            
            // العمليات المنطقية (AND/OR) تتطلب منطقي أو صحيح وتعيد منطقي
            if (op == OP_AND || op == OP_OR) {
                // نقبل INT أو BOOL لأن C تعامل القيم غير الصفرية كـ true
                if ((!datatype_is_intlike(left) && left != TYPE_BOOL && left != TYPE_FLOAT) ||
                    (!datatype_is_intlike(right) && right != TYPE_BOOL && right != TYPE_FLOAT)) {
                    semantic_error(node, "العمليات المنطقية تتطلب معاملات صحيحة أو منطقية.");
                }
                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }
            
            return TYPE_INT;
        }

        case NODE_UNARY_OP:
        case NODE_POSTFIX_OP: {
            DataType ot = infer_type(node->data.unary_op.operand);

            if (node->data.unary_op.op == UOP_ADDR) {
                Node* target = node->data.unary_op.operand;
                if (!target ||
                    (target->type != NODE_VAR_REF &&
                     target->type != NODE_MEMBER_ACCESS &&
                     target->type != NODE_ARRAY_ACCESS &&
                     target->type != NODE_UNARY_OP)) {
                    semantic_error(node, "أخذ العنوان '&' يتطلب قيمة قابلة للإسناد.");
                }

                if (ot == TYPE_STRUCT || ot == TYPE_UNION || ot == TYPE_VOID) {
                    semantic_error(node, "لا يمكن أخذ عنوان هذا النوع.");
                    node_set_inferred_ptr(node, TYPE_INT, NULL, 1);
                    return TYPE_POINTER;
                }

                if (ot == TYPE_POINTER) {
                    node_set_inferred_ptr(node,
                                          node->data.unary_op.operand->inferred_ptr_base_type,
                                          node->data.unary_op.operand->inferred_ptr_base_type_name,
                                          node->data.unary_op.operand->inferred_ptr_depth + 1);
                } else {
                    const char* base_name = NULL;
                    if (target->type == NODE_MEMBER_ACCESS &&
                        target->data.member_access.member_type == TYPE_ENUM) {
                        base_name = target->data.member_access.member_type_name;
                    }
                    node_set_inferred_ptr(node, ot, base_name, 1);
                }
                return TYPE_POINTER;
            }

            if (node->data.unary_op.op == UOP_DEREF) {
                if (ot != TYPE_POINTER) {
                    semantic_error(node, "فك الإشارة '*' يتطلب مؤشراً.");
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }

                int depth = node->data.unary_op.operand->inferred_ptr_depth;
                if (depth <= 0) {
                    semantic_error(node, "لا يمكن فك مؤشر فارغ مباشرة.");
                    node_clear_inferred_ptr(node);
                    return TYPE_INT;
                }

                if (depth == 1) {
                    node_clear_inferred_ptr(node);
                    return node->data.unary_op.operand->inferred_ptr_base_type;
                }

                node_set_inferred_ptr(node,
                                      node->data.unary_op.operand->inferred_ptr_base_type,
                                      node->data.unary_op.operand->inferred_ptr_base_type_name,
                                      depth - 1);
                return TYPE_POINTER;
            }

            if (ot == TYPE_FLOAT) {
                if (node->data.unary_op.op == UOP_NEG) {
                    // السماح بسالب عشري كحالة دنيا (بدون عمليات عشري أخرى).
                    node_clear_inferred_ptr(node);
                    return TYPE_FLOAT;
                }
                semantic_error(node, "عمليات العشري غير مدعومة حالياً.");
                node_clear_inferred_ptr(node);
                return TYPE_FLOAT;
            }

            if (!datatype_is_intlike(ot)) {
                semantic_error(node, "Unary operations require INTEGER operand.");
            }
            // -x و ~x يتبعان ترقيات الأعداد الصحيحة (قد يصبحان أوسع).
            if (node->data.unary_op.op == UOP_NEG || node->data.unary_op.op == UOP_BIT_NOT) {
                node_clear_inferred_ptr(node);
                return datatype_int_promote(ot);
            }
            if (node->data.unary_op.op == UOP_NOT) {
                node_clear_inferred_ptr(node);
                return TYPE_BOOL;
            }
            node_clear_inferred_ptr(node);
            return ot;
        }

        default:
            return TYPE_INT;
    }
}

