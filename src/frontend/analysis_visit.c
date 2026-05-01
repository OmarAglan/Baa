static DataType infer_type(Node* node)
{
    if (!node) return TYPE_INT;
    DataType t = infer_type_internal(node);
    node->inferred_type = t;
    if (t != TYPE_POINTER && t != TYPE_FUNC_PTR) {
        node_clear_inferred_ptr(node);
    }
    return t;
}

/**
 * @brief التحقق مما إذا كانت الجملة توقف التدفق (return/break/continue).
 */
static bool is_terminating_statement(Node* node) {
    if (!node) return false;
    return (node->type == NODE_RETURN ||
            node->type == NODE_BREAK ||
            node->type == NODE_CONTINUE);
}

/**
 * @brief تحليل كتلة من الجمل مع اكتشاف الكود الميت.
 * @param statements قائمة الجمل
 * @param context سياق الكتلة (للرسائل)
 */
static void analyze_statements_with_dead_code_check(Node* statements, const char* context) {
    bool found_terminator = false;
    int terminator_line = 0;
    
    Node* stmt = statements;
    while (stmt) {
        if (found_terminator) {
            // كود ميت بعد return/break/continue
            warning_report(WARN_DEAD_CODE, current_filename, terminator_line, 1,
                "Unreachable code after '%s' statement.", context);
            // نستمر في التحليل لاكتشاف أخطاء أخرى
            found_terminator = false; // تجنب تحذيرات متعددة
        }
        
        analyze_node(stmt);
        
        if (is_terminating_statement(stmt)) {
            found_terminator = true;
            terminator_line = (stmt->line > 0) ? stmt->line : 1;
        }
        
        stmt = stmt->next;
    }
}

static void analyze_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            // 0) تسجيل تعريفات الأنواع (تعداد/هيكل) أولاً
            Node* td = node->data.program.declarations;
            while (td) {
                if (td->type == NODE_ENUM_DECL) {
                    enum_register_decl(td);
                } else if (td->type == NODE_STRUCT_DECL) {
                    struct_register_decl(td);
                } else if (td->type == NODE_UNION_DECL) {
                    union_register_decl(td);
                }
                td = td->next;
            }

            // حساب تخطيط الذاكرة للهياكل/الاتحادات مبكراً لالتقاط الأخطاء بسرعة
            for (int i = 0; i < struct_count; i++) {
                (void)struct_compute_layout(&struct_defs[i]);
            }

            for (int i = 0; i < union_count; i++) {
                (void)union_compute_layout(&union_defs[i]);
            }

            // 0.5) تسجيل أسماء الأنواع البديلة (نوع) بعد تعريفات الأنواع المركبة
            Node* ta = node->data.program.declarations;
            while (ta) {
                if (ta->type == NODE_TYPE_ALIAS) {
                    type_alias_register_decl(ta);
                }
                ta = ta->next;
            }

            // 1) تسجيل تواقيع الدوال أولاً لدعم الاستدعاء قبل التعريف
            Node* decl0 = node->data.program.declarations;
            while (decl0) {
                if (decl0->type == NODE_FUNC_DEF) {
                    func_register(decl0);
                }
                decl0 = decl0->next;
            }

            Node* decl = node->data.program.declarations;
            while (decl) {
                analyze_node(decl);
                decl = decl->next;
            }
            // في نهاية البرنامج، التحقق من المتغيرات العامة غير المستخدمة
            check_unused_global_variables();
            break;
        }

        case NODE_VAR_DECL: {
            DataType declType = node->data.var_decl.type;
            const char* declTypeName = node->data.var_decl.type_name;
            bool has_static_storage = node->data.var_decl.is_global || node->data.var_decl.is_static;

            if (declType == TYPE_VOID) {
                semantic_error(node,
                               node->data.var_decl.is_global
                                   ? "لا يمكن تعريف متغير عام من نوع 'عدم'."
                                   : "لا يمكن تعريف متغير من نوع 'عدم'.");
                if (node->data.var_decl.expression) {
                    (void)infer_type(node->data.var_decl.expression);
                }
                break;
            }

            if (declType == TYPE_POINTER) {
                if (node->data.var_decl.ptr_depth <= 0) {
                    semantic_error(node, "عمق المؤشر غير صالح في تعريف '%s'.",
                                   node->data.var_decl.name ? node->data.var_decl.name : "???");
                }
                if ((node->data.var_decl.ptr_base_type == TYPE_STRUCT &&
                     (!node->data.var_decl.ptr_base_type_name || !struct_lookup_def(node->data.var_decl.ptr_base_type_name))) ||
                    (node->data.var_decl.ptr_base_type == TYPE_UNION &&
                     (!node->data.var_decl.ptr_base_type_name || !union_lookup_def(node->data.var_decl.ptr_base_type_name))) ||
                    (node->data.var_decl.ptr_base_type == TYPE_ENUM &&
                     (!node->data.var_decl.ptr_base_type_name || !enum_lookup_def(node->data.var_decl.ptr_base_type_name)))) {
                    semantic_error(node, "أساس المؤشر غير معرّف في '%s'.",
                                   node->data.var_decl.name ? node->data.var_decl.name : "???");
                }
            }

            // 1) تحقق الأنواع المركبة
            if (declType == TYPE_ENUM) {
                if (!declTypeName || !enum_lookup_def(declTypeName)) {
                    semantic_error(node, "تعداد غير معرّف '%s'.", declTypeName ? declTypeName : "???");
                }

                if (node->data.var_decl.expression) {
                    DataType exprType = infer_type(node->data.var_decl.expression);
                    const char* en = expr_enum_name(node->data.var_decl.expression);
                    if (!types_compatible_named(exprType, en, TYPE_ENUM, declTypeName)) {
                        semantic_error(node,
                                       "عدم تطابق نوع التعداد في تهيئة '%s'.", node->data.var_decl.name);
                    }
                    if (has_static_storage &&
                        !static_storage_initializer_is_const(node->data.var_decl.expression, declType)) {
                        semantic_error(node, "تهيئة المتغير الساكن '%s' يجب أن تكون ثابتة وقت الترجمة.",
                                       node->data.var_decl.name ? node->data.var_decl.name : "???");
                    }
                } else {
                    if (!has_static_storage) {
                        semantic_error(node, "متغير التعداد '%s' يجب أن يُهيّأ.", node->data.var_decl.name);
                    }
                }
            }
            else if (declType == TYPE_STRUCT || declType == TYPE_UNION) {
                if (!declTypeName) {
                    semantic_error(node, "اسم نوع مركب مفقود.");
                }
                if (declType == TYPE_STRUCT) {
                    StructDef* sd = struct_lookup_def(declTypeName);
                    if (!sd) {
                        semantic_error(node, "هيكل غير معرّف '%s'.", declTypeName ? declTypeName : "???");
                    } else {
                        (void)struct_compute_layout(sd);
                        node->data.var_decl.struct_size = sd->size;
                        node->data.var_decl.struct_align = sd->align;
                    }
                } else {
                    UnionDef* ud = union_lookup_def(declTypeName);
                    if (!ud) {
                        semantic_error(node, "اتحاد غير معرّف '%s'.", declTypeName ? declTypeName : "???");
                    } else {
                        (void)union_compute_layout(ud);
                        node->data.var_decl.struct_size = ud->size;
                        node->data.var_decl.struct_align = ud->align;
                    }
                }

                if (node->data.var_decl.expression) {
                    semantic_error(node, "تهيئة النوع المركب بهذه الصيغة غير مدعومة حالياً.");
                }
                if (node->data.var_decl.is_const) {
                    semantic_error(node, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
                }
            }
            else {
                // 2) الأنواع البدائية
                if (node->data.var_decl.expression) {
                    DataType exprType = infer_type_allow_null_string(node->data.var_decl.expression, declType);
                    if (declType == TYPE_POINTER) {
                        if (!ptr_type_compatible(node->data.var_decl.expression->inferred_ptr_base_type,
                                                 node->data.var_decl.expression->inferred_ptr_base_type_name,
                                                 node->data.var_decl.expression->inferred_ptr_depth,
                                                 node->data.var_decl.ptr_base_type,
                                                 node->data.var_decl.ptr_base_type_name,
                                                 node->data.var_decl.ptr_depth,
                                                 true)) {
                            semantic_error(node, "عدم تطابق نوع المؤشر في تعريف '%s'.",
                                           node->data.var_decl.name ? node->data.var_decl.name : "???");
                        }
                    } else if (declType == TYPE_FUNC_PTR) {
                        FuncPtrSig* expected_sig = node->data.var_decl.func_sig;
                        if (!expected_sig) {
                            semantic_error(node, "توقيع مؤشر الدالة في تعريف '%s' مفقود.",
                                           node->data.var_decl.name ? node->data.var_decl.name : "???");
                        } else if (node->data.var_decl.expression->type == NODE_NULL) {
                            // تثبيت نوع null كي يُخفض إلى IR_TYPE_FUNC بشكل صحيح.
                            node->data.var_decl.expression->inferred_type = TYPE_FUNC_PTR;
                            node_set_inferred_funcptr(node->data.var_decl.expression, expected_sig);
                        } else if (exprType != TYPE_FUNC_PTR ||
                                   !funcsig_equal(node->data.var_decl.expression->inferred_func_sig, expected_sig)) {
                            semantic_error(node,
                                           "عدم تطابق توقيع مؤشر الدالة في تعريف '%s'.",
                                           node->data.var_decl.name ? node->data.var_decl.name : "???");
                        }
                    } else if (!types_compatible(exprType, declType)) {
                        semantic_error(node,
                                      "عدم تطابق النوع في تعريف '%s' (المتوقع %s لكن وُجد %s).",
                                      node->data.var_decl.name,
                                      datatype_to_str(declType),
                                      datatype_to_str(exprType));
                    } else {
                        maybe_warn_implicit_narrowing(exprType, declType, node->data.var_decl.expression);
                    }
                    if (has_static_storage &&
                        !static_storage_initializer_is_const(node->data.var_decl.expression, declType)) {
                        semantic_error(node, "تهيئة المتغير الساكن '%s' يجب أن تكون ثابتة وقت الترجمة.",
                                       node->data.var_decl.name ? node->data.var_decl.name : "???");
                    }
                }
                if (node->data.var_decl.is_const &&
                    !has_static_storage &&
                    !node->data.var_decl.expression) {
                    semantic_error(node, "الثابت '%s' يجب تهيئته.", node->data.var_decl.name);
                }
            }

            // 3) إضافة الرمز (مع معلومات الموقع للتحذيرات)
            add_symbol(node->data.var_decl.name,
                       node->data.var_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       declType,
                       declTypeName,
                       node->data.var_decl.ptr_base_type,
                       node->data.var_decl.ptr_base_type_name,
                       node->data.var_decl.ptr_depth,
                       node->data.var_decl.func_sig,
                       node->data.var_decl.is_const,
                       node->data.var_decl.is_static,
                       false,
                       0,
                       NULL,
                       0,
                       node->line, node->col, node->filename ? node->filename : current_filename);
            break;
         }

        case NODE_TYPE_ALIAS:
            // تم تسجيل أسماء الأنواع البديلة في مرحلة NODE_PROGRAM.
            break;

        case NODE_FUNC_DEF: {
            // الدخول في نطاق دالة جديدة (تصفير المحلي + إنشاء نطاق)
            local_count = 0;
            symbol_hash_reset_heads(local_symbol_hash_head, ANALYSIS_SYMBOL_HASH_BUCKETS);
            symbol_hash_reset_next(local_symbol_hash_next, ANALYSIS_MAX_SYMBOLS);
            scope_depth = 0;
            scope_push();

            bool prev_in_func = in_function;
            DataType prev_ret = current_func_return_type;
            DataType prev_ret_ptr_base = current_func_return_ptr_base_type;
            const char* prev_ret_ptr_base_name = current_func_return_ptr_base_type_name;
            int prev_ret_ptr_depth = current_func_return_ptr_depth;
            FuncPtrSig* prev_ret_func_sig = current_func_return_func_sig;
            bool prev_is_variadic = current_func_is_variadic;
            const char* prev_variadic_anchor = current_func_variadic_anchor_name;
            in_function = true;
            current_func_return_type = node->data.func_def.return_type;
            current_func_return_ptr_base_type = node->data.func_def.return_ptr_base_type;
            current_func_return_ptr_base_type_name = node->data.func_def.return_ptr_base_type_name;
            current_func_return_ptr_depth = node->data.func_def.return_ptr_depth;
            current_func_return_func_sig = node->data.func_def.return_func_sig;
            current_func_is_variadic = node->data.func_def.is_variadic;
            current_func_variadic_anchor_name = NULL;

            // إضافة المعاملات كمتغيرات محلية (المعاملات ليست ثوابت افتراضياً)
            Node* param = node->data.func_def.params;
            while (param) {
                 if (param->type == NODE_VAR_DECL) {
                    if (param->data.var_decl.type == TYPE_VOID) {
                        semantic_error(param, "لا يمكن استخدام 'عدم' كنوع معامل.");
                        param = param->next;
                        continue;
                    }
                    if (param->data.var_decl.type == TYPE_POINTER && param->data.var_decl.ptr_depth <= 0) {
                        semantic_error(param, "عمق مؤشر المعامل غير صالح.");
                    }
                      // المعاملات تُعتبر "مستخدمة" ضمنياً (لتجنب تحذيرات خاطئة)
                     add_symbol(param->data.var_decl.name, SCOPE_LOCAL,
                                param->data.var_decl.type, param->data.var_decl.type_name,
                                param->data.var_decl.ptr_base_type,
                                param->data.var_decl.ptr_base_type_name,
                                param->data.var_decl.ptr_depth,
                                param->data.var_decl.func_sig,
                                false,
                                false,
                                false, 0, NULL, 0,
                                param->line, param->col, param->filename ? param->filename : current_filename);
                     // تعليم المعامل كمستخدم مباشرة
                     if (local_count > 0) {
                         local_symbols[local_count - 1].is_used = true;
                     }
                     current_func_variadic_anchor_name = param->data.var_decl.name;
                 }
                param = param->next;
            }

            if (current_func_is_variadic && !current_func_variadic_anchor_name) {
                semantic_error(node, "الدالة المتغيرة المعاملات تتطلب معاملاً ثابتاً واحداً على الأقل قبل '...'.");
            }

            // تحليل جسم الدالة فقط إذا لم تكن نموذجاً أولياً
            if (!node->data.func_def.is_prototype) {
                analyze_node(node->data.func_def.body);
            }

            // خروج نطاق الدالة (يشمل تحذيرات غير المستخدم)
            scope_pop();

            in_function = prev_in_func;
            current_func_return_type = prev_ret;
            current_func_return_ptr_base_type = prev_ret_ptr_base;
            current_func_return_ptr_base_type_name = prev_ret_ptr_base_name;
            current_func_return_ptr_depth = prev_ret_ptr_depth;
            current_func_return_func_sig = prev_ret_func_sig;
            current_func_is_variadic = prev_is_variadic;
            current_func_variadic_anchor_name = prev_variadic_anchor;
            break;
        }

        case NODE_BLOCK: {
            // دخول/خروج نطاق كتلة
            scope_push();
            analyze_statements_with_dead_code_check(node->data.block.statements, "return/break");
            scope_pop();
            break;
        }

        case NODE_ASSIGN: {
            Symbol* sym = lookup(node->data.assign_stmt.name, true); // تعليم كمستخدم
            if (!sym) {
                semantic_error_hint(node,
                                    "عرّف المتغير قبل استخدامه أو صحح اسم المتغير.",
                                    "إسناد إلى متغير غير معرّف '%s'.",
                                    node->data.assign_stmt.name);
            } else {
                if (sym->is_array) {
                    semantic_error(node, "لا يمكن تعيين قيمة للمصفوفة '%s' مباشرة (استخدم الفهرسة).", sym->name);
                }
                if (sym->type == TYPE_STRUCT) {
                    semantic_error(node, "لا يمكن إسناد قيمة إلى هيكل '%s' مباشرة.", sym->name);
                }
                // التحقق من الثوابت: لا يمكن إعادة تعيين قيمة ثابت
                if (sym->is_const) {
                    semantic_error_hint(node,
                                        "استخدم متغيراً غير ثابت إذا كانت القيمة ستتغير.",
                                        "لا يمكن إعادة إسناد الثابت '%s'.",
                                        node->data.assign_stmt.name);
                }
                DataType exprType = infer_type_allow_null_string(node->data.assign_stmt.expression, sym->type);
                if (sym->type == TYPE_ENUM) {
                    const char* en = expr_enum_name(node->data.assign_stmt.expression);
                    if (!types_compatible_named(exprType, en, TYPE_ENUM, sym->type_name)) {
                        semantic_error(node, "عدم تطابق نوع التعداد في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (sym->type == TYPE_POINTER) {
                    if (!ptr_type_compatible(node->data.assign_stmt.expression->inferred_ptr_base_type,
                                             node->data.assign_stmt.expression->inferred_ptr_base_type_name,
                                             node->data.assign_stmt.expression->inferred_ptr_depth,
                                             sym->ptr_base_type,
                                             sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                             sym->ptr_depth,
                                             true)) {
                        semantic_error(node, "عدم تطابق نوع المؤشر في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (sym->type == TYPE_FUNC_PTR) {
                    FuncPtrSig* expected_sig = sym->func_sig;
                    if (!expected_sig) {
                        semantic_error(node, "توقيع مؤشر الدالة للرمز '%s' مفقود.", node->data.assign_stmt.name);
                    } else if (node->data.assign_stmt.expression &&
                               node->data.assign_stmt.expression->type == NODE_NULL) {
                        node->data.assign_stmt.expression->inferred_type = TYPE_FUNC_PTR;
                        node_set_inferred_funcptr(node->data.assign_stmt.expression, expected_sig);
                    } else if (exprType != TYPE_FUNC_PTR ||
                               !funcsig_equal(node->data.assign_stmt.expression->inferred_func_sig, expected_sig)) {
                        semantic_error(node, "عدم تطابق توقيع مؤشر الدالة في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                    }
                } else if (!types_compatible(exprType, sym->type)) {
                    semantic_error(node, "عدم تطابق النوع في الإسناد إلى '%s'.", node->data.assign_stmt.name);
                } else {
                    maybe_warn_implicit_narrowing(exprType, sym->type, node->data.assign_stmt.expression);
                }
            }
            break;
        }

        case NODE_MEMBER_ASSIGN: {
            Node* target = node->data.member_assign.target;
            Node* value = node->data.member_assign.value;

            if (!target || target->type != NODE_MEMBER_ACCESS) {
                semantic_error(node, "الهدف في إسناد العضو غير صالح.");
                (void)infer_type(value);
                break;
            }

            resolve_member_access(target);

            if (target->data.member_access.is_enum_value) {
                semantic_error(node, "لا يمكن الإسناد إلى قيمة تعداد.");
                (void)infer_type(value);
                break;
            }
            if (!target->data.member_access.is_struct_member) {
                semantic_error(node, "إسناد عضو يتطلب هيكلاً.");
                (void)infer_type(value);
                break;
            }
            if (target->data.member_access.member_type == TYPE_STRUCT) {
                semantic_error(node, "إسناد عضو من نوع هيكل غير مدعوم حالياً.");
                (void)infer_type(value);
                break;
            }

            if (target->data.member_access.member_is_const) {
                semantic_error(node, "لا يمكن تعديل عضو ثابت داخل الهيكل.");
            }

            // تحقق ثابتية الهيكل الجذري
            Symbol* root = NULL;
            if (target->data.member_access.root_var) {
                root = lookup(target->data.member_access.root_var, true);
            }
            if (root && root->is_const) {
                semantic_error(node, "لا يمكن تعديل هيكل ثابت '%s'.", root->name);
            }

            DataType got = infer_type(value);
            DataType expected = target->data.member_access.member_type;
            if (expected == TYPE_ENUM) {
                const char* en = expr_enum_name(value);
                const char* expn = target->data.member_access.member_type_name;
                if (!types_compatible_named(got, en, TYPE_ENUM, expn)) {
                    semantic_error(node, "عدم تطابق نوع التعداد في إسناد العضو.");
                }
            } else if (expected == TYPE_POINTER) {
                if (!ptr_type_compatible(value->inferred_ptr_base_type,
                                         value->inferred_ptr_base_type_name,
                                         value->inferred_ptr_depth,
                                         target->data.member_access.member_ptr_base_type,
                                         target->data.member_access.member_ptr_base_type_name,
                                         target->data.member_access.member_ptr_depth,
                                         true)) {
                    semantic_error(node, "عدم تطابق نوع المؤشر في إسناد العضو.");
                }
            } else if (!types_compatible(got, expected)) {
                semantic_error(node, "عدم تطابق النوع في إسناد العضو.");
            } else {
                maybe_warn_implicit_narrowing(got, expected, value);
            }
            break;
        }

        case NODE_DEREF_ASSIGN: {
            Node* target = node->data.deref_assign.target;
            Node* value = node->data.deref_assign.value;
            if (!target || target->type != NODE_UNARY_OP || target->data.unary_op.op != UOP_DEREF) {
                semantic_error(node, "هدف الإسناد عبر المؤشر غير صالح.");
                if (value) (void)infer_type(value);
                break;
            }

            DataType tt = infer_type(target);
            if (tt == TYPE_STRUCT || tt == TYPE_UNION || tt == TYPE_VOID) {
                semantic_error(node, "الإسناد عبر المؤشر لهذا النوع غير مدعوم.");
                if (value) (void)infer_type(value);
                break;
            }

            DataType vt = infer_type(value);
            if (!types_compatible(vt, tt)) {
                semantic_error(node, "نوع القيمة في الإسناد عبر المؤشر غير متوافق.");
            }
            break;
        }

        case NODE_IF: {
            DataType condType = infer_type(node->data.if_stmt.condition);
            if (!datatype_is_intlike(condType)) {
                semantic_error(node->data.if_stmt.condition, "شرط 'إذا' يجب أن يكون عدداً صحيحاً أو منطقياً.");
            }

            // كل فرع يعتبر نطاقاً مستقلاً حتى بدون أقواس
            scope_push();
            analyze_node(node->data.if_stmt.then_branch);
            scope_pop();

            if (node->data.if_stmt.else_branch) {
                scope_push();
                analyze_node(node->data.if_stmt.else_branch);
                scope_pop();
            }
            break;
        }

        case NODE_WHILE: {
            DataType condType = infer_type(node->data.while_stmt.condition);
            if (!datatype_is_intlike(condType)) {
                semantic_error(node->data.while_stmt.condition, "شرط 'طالما' يجب أن يكون عدداً صحيحاً أو منطقياً.");
            }
            bool prev_loop = inside_loop;
            inside_loop = true;

            scope_push();
            analyze_node(node->data.while_stmt.body);
            scope_pop();

            inside_loop = prev_loop;
            break;
        }

        case NODE_FOR: {
            // نطاق حلقة for يشمل init/cond/incr/body لمنع تسرب متغيرات init خارج الحلقة.
            scope_push();

            if (node->data.for_stmt.init) analyze_node(node->data.for_stmt.init);
            if (node->data.for_stmt.condition) {
                DataType condType = infer_type(node->data.for_stmt.condition);
                if (!datatype_is_intlike(condType)) {
                    semantic_error(node->data.for_stmt.condition, "شرط 'لكل' يجب أن يكون عدداً صحيحاً أو منطقياً.");
                }
            }
            if (node->data.for_stmt.increment) analyze_node(node->data.for_stmt.increment);

            bool prev_loop = inside_loop;
            inside_loop = true;
            analyze_node(node->data.for_stmt.body);
            inside_loop = prev_loop;

            scope_pop();
            break;
        }

        case NODE_SWITCH: {
            DataType st = infer_type(node->data.switch_stmt.expression);
            if (!datatype_is_intlike(st)) {
                semantic_error(node->data.switch_stmt.expression, "تعبير 'اختر' يجب أن يكون عدداً صحيحاً.");
            }
            bool prev_switch = inside_switch;
            inside_switch = true;

            // نطاق switch
            scope_push();
            
            Node* current_case = node->data.switch_stmt.cases;
            while (current_case) {
                if (!current_case->data.case_stmt.is_default) {
                    DataType ct = infer_type(current_case->data.case_stmt.value);
                    if (!datatype_is_intlike(ct)) {
                        semantic_error(current_case->data.case_stmt.value, "قيمة 'حالة' يجب أن تكون ثابتاً صحيحاً.");
                    }
                }
                // جسم الحالة عبارة عن قائمة جمل
                scope_push();
                analyze_statements_with_dead_code_check(current_case->data.case_stmt.body, "return/break");
                scope_pop();
                current_case = current_case->next;
            }

            scope_pop();
            
            inside_switch = prev_switch;
            break;
        }

        case NODE_BREAK:
            if (!inside_loop && !inside_switch) {
                semantic_error(node, "تم استخدام 'توقف' خارج حلقة أو 'اختر'.");
            }
            break;

        case NODE_CONTINUE:
            if (!inside_loop) {
                semantic_error(node, "تم استخدام 'استمر' خارج حلقة.");
            }
            break;

        case NODE_RETURN:
            if (!in_function) {
                semantic_error(node, "جملة 'إرجع' خارج دالة.");
                if (node->data.return_stmt.expression) {
                    (void)infer_type(node->data.return_stmt.expression);
                }
                break;
            }

            if (current_func_return_type == TYPE_VOID) {
                if (node->data.return_stmt.expression) {
                    (void)infer_type(node->data.return_stmt.expression);
                    semantic_error(node, "الدالة من نوع 'عدم' لا تعيد قيمة.");
                }
                break;
            }

            if (!node->data.return_stmt.expression) {
                semantic_error(node, "الدالة غير 'عدم' يجب أن تعيد قيمة.");
                break;
            }

            {
                DataType rt = infer_type_allow_null_string(node->data.return_stmt.expression, current_func_return_type);
                if (current_func_return_type == TYPE_POINTER) {
                    if (!ptr_type_compatible(node->data.return_stmt.expression->inferred_ptr_base_type,
                                             node->data.return_stmt.expression->inferred_ptr_base_type_name,
                                             node->data.return_stmt.expression->inferred_ptr_depth,
                                             current_func_return_ptr_base_type,
                                             current_func_return_ptr_base_type_name,
                                             current_func_return_ptr_depth,
                                             true)) {
                        semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة.");
                    }
                } else if (current_func_return_type == TYPE_FUNC_PTR) {
                    FuncPtrSig* expected_sig = current_func_return_func_sig;
                    if (!expected_sig) {
                        semantic_error(node, "توقيع مؤشر الدالة كنوع إرجاع للدالة مفقود.");
                    } else if (node->data.return_stmt.expression->type == NODE_NULL) {
                        node->data.return_stmt.expression->inferred_type = TYPE_FUNC_PTR;
                        node_set_inferred_funcptr(node->data.return_stmt.expression, expected_sig);
                    } else if (rt != TYPE_FUNC_PTR ||
                               !funcsig_equal(node->data.return_stmt.expression->inferred_func_sig, expected_sig)) {
                        semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة (توقيع مؤشر دالة مختلف).");
                    }
                } else if (!types_compatible(rt, current_func_return_type)) {
                    semantic_error(node, "نوع الإرجاع غير متوافق مع نوع الدالة.");
                } else {
                    maybe_warn_implicit_narrowing(rt, current_func_return_type, node->data.return_stmt.expression);
                }
            }
            break;

        case NODE_PRINT:
        {
            DataType pt = infer_type(node->data.print_stmt.expression);
            (void)pt;
            break;
        }
        
        case NODE_READ: {
            // التحقق من أن المتغير معرف وقابل للتعديل
            Symbol* sym = lookup(node->data.read_stmt.var_name, true);
            if (!sym) {
                semantic_error(node, "القراءة إلى متغير غير معرّف '%s'.", node->data.read_stmt.var_name);
            } else {
                if (sym->is_const) {
                    semantic_error(node, "لا يمكن القراءة إلى متغير ثابت '%s'.", node->data.read_stmt.var_name);
                }
                // نقبل القراءة في الأنواع الصحيحة (بما فيها الأحجام المختلفة).
                if (!datatype_is_int(sym->type)) {
                    semantic_error(node, "'اقرأ' يدعم حالياً المتغيرات الصحيحة فقط.");
                }
            }
            break;
        }

        case NODE_INLINE_ASM: {
            if (!node->data.inline_asm.templates) {
                semantic_error(node, "جملة 'مجمع' تتطلب سطر تجميع واحداً على الأقل.");
            }

            for (Node* tpl = node->data.inline_asm.templates; tpl; tpl = tpl->next) {
                if (tpl->type != NODE_STRING || !tpl->data.string_lit.value) {
                    semantic_error(tpl ? tpl : node, "أسطر 'مجمع' يجب أن تكون نصوصاً ثابتة.");
                }
            }

            analyze_inline_asm_operands(node, node->data.inline_asm.outputs, true);
            analyze_inline_asm_operands(node, node->data.inline_asm.inputs, false);
            break;
        }
            
        case NODE_CALL_STMT:
        {
            // تحقق استدعاء دالة كجملة
            const char* fname = node->data.call.name;
            // 1) أعطِ أولوية للرموز في النطاق (متغيرات/معاملات) حتى تعمل الـ shadowing بشكل صحيح.
            Symbol* callee_sym = lookup(fname, true);
            if (callee_sym) {
                if (callee_sym->is_array) {
                    semantic_error(node, "لا يمكن نداء المصفوفة '%s'.", callee_sym->name);
                    analyze_consume_call_args(node->data.call.args);
                    break;
                }

                if (callee_sym->type != TYPE_FUNC_PTR) {
                    semantic_error(node, "المعرف '%s' ليس دالة ولا مؤشر دالة قابل للنداء.", fname ? fname : "???");
                    analyze_consume_call_args(node->data.call.args);
                    break;
                }

                FuncPtrSig* sig = callee_sym->func_sig;
                if (!sig) {
                    semantic_error(node, "توقيع مؤشر الدالة للرمز '%s' مفقود.", fname ? fname : "???");
                    analyze_consume_call_args(node->data.call.args);
                    break;
                }
                analyze_funcptr_call_args(node, fname, node->data.call.args, sig);
                break;
            }

            // 2) ثم نحل الدوال المعرفة (أو builtins) إذا لم يوجد رمز بنفس الاسم.
            FuncSymbol* fs = func_lookup(fname);
            if (!fs || !fs->is_defined) {
                if (builtin_dispatch_known_call(node, fname, node->data.call.args, NULL, false)) {
                    break;
                }
                if (!fs) {
                    semantic_error_hint(node,
                                        "عرّف الدالة قبل ندائها أو أضف نموذجها الأولي في ملف مضمّن.",
                                        "استدعاء دالة غير معرّفة '%s'.",
                                        fname ? fname : "???");
                    analyze_consume_call_args(node->data.call.args);
                    break;
                }
            }

            analyze_user_function_call_args(node, fs, node->data.call.args);
            break;
        }

        case NODE_ARRAY_DECL: {
            if (node->data.array_decl.dim_count <= 0 ||
                !node->data.array_decl.dims ||
                node->data.array_decl.total_elems <= 0) {
                semantic_error(node, "حجم المصفوفة غير صالح.");
                break;
            }
            for (int i = 0; i < node->data.array_decl.dim_count; i++) {
                if (node->data.array_decl.dims[i] <= 0) {
                    semantic_error(node, "البعد %d للمصفوفة غير صالح.", i + 1);
                }
            }

            bool has_static_storage = node->data.array_decl.is_global || node->data.array_decl.is_static;
            DataType elem_type = node->data.array_decl.element_type;
            const char* elem_type_name = node->data.array_decl.element_type_name;

            if (elem_type == TYPE_VOID) {
                semantic_error(node, "لا يمكن تعريف مصفوفة من نوع 'عدم'.");
            }
            if (elem_type == TYPE_POINTER && node->data.array_decl.element_ptr_depth <= 0) {
                semantic_error(node, "عمق مؤشر عنصر المصفوفة غير صالح.");
            }
            if (elem_type == TYPE_ENUM) {
                if (!elem_type_name || !enum_lookup_def(elem_type_name)) {
                    semantic_error(node, "تعداد عنصر المصفوفة غير معرّف '%s'.",
                                   elem_type_name ? elem_type_name : "???");
                }
            } else if (elem_type == TYPE_STRUCT) {
                StructDef* sd = elem_type_name ? struct_lookup_def(elem_type_name) : NULL;
                if (!sd) {
                    semantic_error(node, "هيكل عنصر المصفوفة غير معرّف '%s'.",
                                   elem_type_name ? elem_type_name : "???");
                } else {
                    (void)struct_compute_layout(sd);
                    node->data.array_decl.element_struct_size = sd->size;
                    node->data.array_decl.element_struct_align = sd->align;
                }
            } else if (elem_type == TYPE_UNION) {
                UnionDef* ud = elem_type_name ? union_lookup_def(elem_type_name) : NULL;
                if (!ud) {
                    semantic_error(node, "اتحاد عنصر المصفوفة غير معرّف '%s'.",
                                   elem_type_name ? elem_type_name : "???");
                } else {
                    (void)union_compute_layout(ud);
                    node->data.array_decl.element_struct_size = ud->size;
                    node->data.array_decl.element_struct_align = ud->align;
                }
            } else if (elem_type == TYPE_POINTER) {
                if ((node->data.array_decl.element_ptr_base_type == TYPE_STRUCT &&
                     (!node->data.array_decl.element_ptr_base_type_name || !struct_lookup_def(node->data.array_decl.element_ptr_base_type_name))) ||
                    (node->data.array_decl.element_ptr_base_type == TYPE_UNION &&
                     (!node->data.array_decl.element_ptr_base_type_name || !union_lookup_def(node->data.array_decl.element_ptr_base_type_name))) ||
                    (node->data.array_decl.element_ptr_base_type == TYPE_ENUM &&
                     (!node->data.array_decl.element_ptr_base_type_name || !enum_lookup_def(node->data.array_decl.element_ptr_base_type_name)))) {
                    semantic_error(node, "أساس مؤشر عنصر المصفوفة غير معرّف.");
                }
            }

            // الثابت يجب أن يُهيّأ صراحةً (حتى `{}` مقبول ويعني أصفار).
            if (node->data.array_decl.is_const && !has_static_storage && !node->data.array_decl.has_init) {
                semantic_error(node, "المصفوفة الثابتة '%s' يجب تهيئتها.",
                               node->data.array_decl.name ? node->data.array_decl.name : "???");
            }

            add_symbol(node->data.array_decl.name,
                       node->data.array_decl.is_global ? SCOPE_GLOBAL : SCOPE_LOCAL,
                       elem_type,
                       elem_type_name,
                       node->data.array_decl.element_ptr_base_type,
                       node->data.array_decl.element_ptr_base_type_name,
                       node->data.array_decl.element_ptr_depth,
                       NULL,
                       node->data.array_decl.is_const,
                       node->data.array_decl.is_static,
                       true,
                       node->data.array_decl.dim_count,
                       node->data.array_decl.dims,
                       node->data.array_decl.total_elems,
                       node->line,
                       node->col,
                       node->filename ? node->filename : current_filename);

            // التحقق من قائمة التهيئة: تسمح بالتهيئة الجزئية كما في C.
            if (node->data.array_decl.has_init) {
                int n = node->data.array_decl.init_count;
                int64_t cap64 = node->data.array_decl.total_elems;
                int cap = (cap64 > (int64_t)INT_MAX) ? INT_MAX : (int)cap64;
                if (n < 0) n = 0;
                if (n > cap) {
                    semantic_error(node, "عدد عناصر تهيئة المصفوفة '%s' (%d) أكبر من الحجم (%d).",
                                   node->data.array_decl.name ? node->data.array_decl.name : "???",
                                   n, cap);
                }

                int idx0 = 0;
                for (Node* v = node->data.array_decl.init_values; v; v = v->next) {
                    DataType vt = infer_type(v);
                    if (elem_type == TYPE_POINTER) {
                        if (!ptr_type_compatible(v->inferred_ptr_base_type,
                                                 v->inferred_ptr_base_type_name,
                                                 v->inferred_ptr_depth,
                                                 node->data.array_decl.element_ptr_base_type,
                                                 node->data.array_decl.element_ptr_base_type_name,
                                                 node->data.array_decl.element_ptr_depth,
                                                 true)) {
                            semantic_error(v, "نوع عنصر التهيئة %d للمصفوفة '%s' غير متوافق.",
                                           idx0 + 1,
                                           node->data.array_decl.name ? node->data.array_decl.name : "???");
                        }
                    } else if (!types_compatible(vt, elem_type)) {
                        semantic_error(v, "نوع عنصر التهيئة %d للمصفوفة '%s' غير متوافق.",
                                       idx0 + 1,
                                       node->data.array_decl.name ? node->data.array_decl.name : "???");
                    } else {
                        maybe_warn_implicit_narrowing(vt, elem_type, v);
                    }

                    if (has_static_storage) {
                        if (!static_storage_initializer_is_const(v, elem_type)) {
                            semantic_error(v,
                                           "تهيئة المصفوفة الساكنة '%s' يجب أن تكون تعبيراً ثابتاً.",
                                           node->data.array_decl.name ? node->data.array_decl.name : "???");
                        }
                    }

                    idx0++;
                }
            }
            break;
        }

        case NODE_ARRAY_ASSIGN: {
            Symbol* sym = lookup(node->data.array_op.name, true);
            if (!sym) {
                semantic_error(node, "تعيين إلى مصفوفة غير معرّفة '%s'.", node->data.array_op.name ? node->data.array_op.name : "???");
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                (void)infer_type(node->data.array_op.value);
                break;
            }

            int supplied = node->data.array_op.index_count;
            if (supplied <= 0) supplied = node_list_count(node->data.array_op.indices);

            if (!sym->is_array) {
                if (sym->type == TYPE_STRING) {
                    semantic_error(node, "لا يمكن تعديل عناصر النص '%s' حالياً.", sym->name);
                    for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                        (void)infer_type(idx);
                    }
                    (void)infer_type(node->data.array_op.value);
                    break;
                }
                semantic_error(node, "'%s' ليس مصفوفة.", sym->name);
                for (Node* idx = node->data.array_op.indices; idx; idx = idx->next) {
                    (void)infer_type(idx);
                }
                (void)infer_type(node->data.array_op.value);
                break;
            }
            if (sym->is_const) {
                semantic_error(node, "لا يمكن تعديل مصفوفة ثابتة '%s'.", sym->name);
            }

            int expected = (sym->array_rank > 0) ? sym->array_rank : 1;
            (void)analyze_indices_type_and_bounds(sym, node, node->data.array_op.indices, expected);

            DataType vt = infer_type(node->data.array_op.value);
            if (sym->type == TYPE_POINTER) {
                if (!ptr_type_compatible(node->data.array_op.value->inferred_ptr_base_type,
                                         node->data.array_op.value->inferred_ptr_base_type_name,
                                         node->data.array_op.value->inferred_ptr_depth,
                                         sym->ptr_base_type,
                                         sym->ptr_base_type_name[0] ? sym->ptr_base_type_name : NULL,
                                         sym->ptr_depth,
                                         true)) {
                    semantic_error(node, "نوع القيمة في تعيين '%s' غير متوافق.", sym->name);
                }
            } else if (!types_compatible(vt, sym->type)) {
                semantic_error(node, "نوع القيمة في تعيين '%s' غير متوافق.", sym->name);
            } else {
                maybe_warn_implicit_narrowing(vt, sym->type, node->data.array_op.value);
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief نقطة الدخول الرئيسية للتحليل.
 * @param program عقدة البرنامج
 * @return true إذا لم تحدث أخطاء
 */
bool analyze(Node* program) {
    reset_analysis();
    
    // تعيين اسم الملف الحالي (للتحذيرات)
    if (program && program->filename) {
        current_filename = program->filename;
    } else {
        current_filename = "المصدر";
    }
    
    analyze_node(program);
    return !has_error;
}
