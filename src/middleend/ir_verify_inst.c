static void ir_verify_inst(IRVerifyDiag* diag,
                           const IRModule* module,
                           const IRFunc* func,
                           const IRBlock* block,
                           const IRInst* inst,
                           int seen_non_phi,
                           const IRModule* verify_module_for_calls) {
    if (!diag || !func || !block || !inst) return;

    // قاعدة: بعد رؤية تعليمة غير فاي، لا يُسمح بوجود فاي.
    if (inst->op == IR_OP_PHI && seen_non_phi) {
        ir_report(diag, module, func, block, inst, "تعليمة `فاي` ليست في بداية الكتلة.");
    }

    // تحقق عام للنوع/الوجهة.
    if (inst->op != IR_OP_STORE && inst->op != IR_OP_BR && inst->op != IR_OP_BR_COND && inst->op != IR_OP_RET) {
        // معظم التعليمات المنتجة للقيم يجب أن تملك type غير void.
        if (!inst->type) {
            ir_report(diag, module, func, block, inst, "تعليمة بدون نوع (type=NULL).");
        }
    }

    switch (inst->op) {
        // --------------------------------------------------------------------
        // تعليمات ثنائية (2 operands + dest)
        // --------------------------------------------------------------------
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_XOR:
        case IR_OP_SHL:
        case IR_OP_SHR: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة ثنائية بدون نوع نتيجة صالح.");
                break;
            }

            bool is_float = (inst->type && inst->type->kind == IR_TYPE_F64);
            bool is_int = ir_type_is_int(inst->type);

            // العمليات البتية/الإزاحة/باقي القسمة: أعداد صحيحة فقط.
            if (inst->op == IR_OP_AND || inst->op == IR_OP_OR || inst->op == IR_OP_XOR ||
                inst->op == IR_OP_SHL || inst->op == IR_OP_SHR || inst->op == IR_OP_MOD) {
                if (!is_int) {
                    ir_report(diag, module, func, block, inst, "تعليمة ثنائية تتوقع نوعاً صحيحاً.");
                    break;
                }
            } else {
                // ADD/SUB/MUL/DIV: صحيح أو عشري.
                if (!(is_int || is_float)) {
                    ir_report(diag, module, func, block, inst, "تعليمة ثنائية تتوقع نوعاً عددياً (صحيح/عشري).");
                    break;
                }
            }

            if (inst->operands[0]) ir_verify_value_type_eq(diag, module, func, block, inst, inst->type, inst->operands[0], "lhs");
            if (inst->operands[1]) ir_verify_value_type_eq(diag, module, func, block, inst, inst->type, inst->operands[1], "rhs");
            break;
        }

        // --------------------------------------------------------------------
        // تعليمات أحادية (1 operand + dest)
        // --------------------------------------------------------------------
        case IR_OP_NEG:
        case IR_OP_NOT:
        case IR_OP_COPY: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة أحادية بدون نوع نتيجة صالح.");
                break;
            }

            if (inst->operands[0]) {
                ir_verify_value_type_eq(diag, module, func, block, inst, inst->type, inst->operands[0], "operand");
            }
            break;
        }

        // --------------------------------------------------------------------
        // alloca: dest + type = ptr(pointee), operand_count=0
        // --------------------------------------------------------------------
        case IR_OP_ALLOCA: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);

            if (inst->operand_count != 0) {
                ir_report(diag, module, func, block, inst, "تعليمة `حجز` يجب ألا تملك معاملات.");
            }

            if (!inst->type || inst->type->kind != IR_TYPE_PTR || !inst->type->data.pointee) {
                ir_report(diag, module, func, block, inst, "تعليمة `حجز` يجب أن تنتج نوع مؤشر صالح.");
            }

            if (inst->type && inst->type->kind == IR_TYPE_PTR &&
                inst->type->data.pointee && inst->type->data.pointee->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `حجز` لا يجب أن تحجز نوع فراغ.");
            }
            break;
        }

        // --------------------------------------------------------------------
        // load: dest + 1 operand (ptr), type == pointee(ptr)
        // --------------------------------------------------------------------
        case IR_OP_LOAD: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            IRValue* ptr = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            if (!ptr || !ir_value_is_ptr_value(ptr)) {
                ir_report(diag, module, func, block, inst, "تعليمة `حمل` تتطلب معاملاً من نوع مؤشر.");
                break;
            }

            IRType* pointee = ir_ptr_pointee(ptr->type);
            if (!pointee) {
                ir_report(diag, module, func, block, inst, "تعليمة `حمل`: نوع المؤشر بدون pointee.");
                break;
            }

            if (!inst->type) {
                ir_report(diag, module, func, block, inst, "تعليمة `حمل` بدون نوع نتيجة.");
                break;
            }

            if (!ir_types_equal(inst->type, pointee)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `حمل`: نوع النتيجة لا يطابق pointee للمؤشر (load=%s, ptr=%s).",
                          ir_type_to_arabic(inst->type),
                          ir_type_to_arabic(pointee));
            }
            break;
        }

        // --------------------------------------------------------------------
        // store: no dest + 2 operands (value, ptr), ptr pointee == value.type
        // --------------------------------------------------------------------
        case IR_OP_STORE: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind != IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن` يجب أن يكون نوعها فراغ.");
            }

            IRValue* val = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            IRValue* ptr = (inst->operand_count >= 2) ? inst->operands[1] : NULL;

            if (!val || !val->type) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن`: قيمة بدون نوع.");
                break;
            }

            if (!ptr || !ir_value_is_ptr_value(ptr)) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن` تتطلب مؤشر كمعامل ثانٍ.");
                break;
            }

            IRType* pointee = ir_ptr_pointee(ptr->type);
            if (!pointee) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن`: نوع المؤشر بدون pointee.");
                break;
            }

            if (!ir_types_equal(pointee, val->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `خزن`: نوع pointee لا يطابق نوع القيمة (val=%s, ptr=%s).",
                          ir_type_to_arabic(val->type),
                          ir_type_to_arabic(pointee));
            }
            break;
        }

        // --------------------------------------------------------------------
        // ptr.offset: dest + 2 operands (base_ptr, index_int), result ptr
        // --------------------------------------------------------------------
        case IR_OP_PTR_OFFSET: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind != IR_TYPE_PTR || !inst->type->data.pointee) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر` يجب أن تنتج نوع مؤشر صالح.");
                break;
            }

            IRValue* base = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            IRValue* idx  = (inst->operand_count >= 2) ? inst->operands[1] : NULL;

            if (!base || !ir_value_is_ptr_value(base)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: المعامل الأول يجب أن يكون مؤشراً.");
                break;
            }

            if (base->type && inst->type && !ir_types_equal(base->type, inst->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: نوع base لا يطابق نوع النتيجة (base=%s, res=%s).",
                          ir_type_to_arabic(base->type),
                          ir_type_to_arabic(inst->type));
            }

            if (!idx || !idx->type || !ir_type_is_int(idx->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: المعامل الثاني (index) يجب أن يكون عدداً صحيحاً.");
            }

            if (base && !ir_value_reg_in_range(base, func)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: base سجل خارج نطاق الدالة.");
            }
            if (idx && !ir_value_reg_in_range(idx, func)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: index سجل خارج نطاق الدالة.");
            }

            break;
        }

        // --------------------------------------------------------------------
        // cmp: dest + 2 operands, result i1.
        // يدعم:
        // - المقارنات العددية (int/f64) مع تطابق النوع.
        // - مقارنة المؤشرات فقط مع (== أو !=) ومع تطابق النوع.
        // --------------------------------------------------------------------
        case IR_OP_CMP: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind != IR_TYPE_I1) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن` يجب أن تنتج ص١.");
            }

            IRValue* a = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            IRValue* b = (inst->operand_count >= 2) ? inst->operands[1] : NULL;

            bool a_int = (a && a->type) ? ir_type_is_int(a->type) : 0;
            bool b_int = (b && b->type) ? ir_type_is_int(b->type) : 0;
            bool a_f64 = (a && a->type && a->type->kind == IR_TYPE_F64);
            bool b_f64 = (b && b->type && b->type->kind == IR_TYPE_F64);
            bool a_ptr = (a && a->type) ? ir_type_is_ptr(a->type) : 0;
            bool b_ptr = (b && b->type) ? ir_type_is_ptr(b->type) : 0;
            bool a_fun = (a && a->type && a->type->kind == IR_TYPE_FUNC);
            bool b_fun = (b && b->type && b->type->kind == IR_TYPE_FUNC);

            if (a_ptr || b_ptr || a_fun || b_fun) {
                if (!a || !a->type) {
                    ir_report(diag, module, func, block, inst, "تعليمة `قارن`: المعامل الأول بدون نوع.");
                    break;
                }
                if (!b || !b->type) {
                    ir_report(diag, module, func, block, inst, "تعليمة `قارن`: المعامل الثاني بدون نوع.");
                    break;
                }

                // لا نخلط بين مؤشرات بيانات ومؤشرات دوال.
                if (!((a_ptr && b_ptr) || (a_fun && b_fun))) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `قارن`: مقارنة المؤشرات تتطلب نوعين متطابقين (مؤشر/مؤشر أو دالة/دالة).");
                    break;
                }

                if (!ir_types_equal(a->type, b->type)) {
                    ir_report(diag, module, func, block, inst, "تعليمة `قارن`: نوعا المعاملين غير متطابقين.");
                }
                if (!(inst->cmp_pred == IR_CMP_EQ || inst->cmp_pred == IR_CMP_NE)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `قارن`: مقارنة المؤشرات/مؤشرات الدوال تدعم فقط (==) أو (!=).");
                }
                break;
            }

            if (!a || !a->type || !(a_int || a_f64)) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن`: المعامل الأول ليس نوعاً عددياً.");
                break;
            }
            if (!b || !b->type || !(b_int || b_f64)) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن`: المعامل الثاني ليس نوعاً عددياً.");
                break;
            }
            if (!ir_types_equal(a->type, b->type)) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن`: نوعا المعاملين غير متطابقين.");
            }

            // قيود المحمول على العشري: لا نسمح بمقارنات unsigned.
            if (a_f64 || b_f64) {
                if (inst->cmp_pred == IR_CMP_UGT || inst->cmp_pred == IR_CMP_ULT ||
                    inst->cmp_pred == IR_CMP_UGE || inst->cmp_pred == IR_CMP_ULE)
                {
                    ir_report(diag, module, func, block, inst, "تعليمة `قارن`: محمول unsigned غير صالح على 'عشري'.");
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // br: no dest + 1 operand block
        // --------------------------------------------------------------------
        case IR_OP_BR: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            if (!ir_value_is_block(inst->operands[0])) {
                ir_report(diag, module, func, block, inst, "تعليمة `قفز` يجب أن تشير إلى كتلة.");
            } else if (!ir_value_is_block_in_func(inst->operands[0], func)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `قفز`: الهدف يشير إلى كتلة خارج الدالة (غير مسموح).");
            }
            break;
        }

        // --------------------------------------------------------------------
        // br_cond: no dest + 3 operands: cond(i1), true(block), false(block)
        // --------------------------------------------------------------------
        case IR_OP_BR_COND: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 3);

            IRValue* cond = inst->operands[0];
            if (!cond || !cond->type || cond->type->kind != IR_TYPE_I1) {
                ir_report(diag, module, func, block, inst, "تعليمة `قفز_شرط`: الشرط يجب أن يكون ص١.");
            }

            if (!ir_value_is_block(inst->operands[1]) || !ir_value_is_block(inst->operands[2])) {
                ir_report(diag, module, func, block, inst, "تعليمة `قفز_شرط`: أهداف القفز يجب أن تكون كتل.");
            } else {
                if (!ir_value_is_block_in_func(inst->operands[1], func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `قفز_شرط`: هدف_صواب يشير إلى كتلة خارج الدالة (غير مسموح).");
                }
                if (!ir_value_is_block_in_func(inst->operands[2], func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `قفز_شرط`: هدف_خطأ يشير إلى كتلة خارج الدالة (غير مسموح).");
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // ret: no dest, 0 أو 1 معامل، النوع يجب أن يطابق نوع الدالة
        // --------------------------------------------------------------------
        case IR_OP_RET: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);

            if (inst->operand_count != 0 && inst->operand_count != 1) {
                ir_report(diag, module, func, block, inst, "تعليمة `رجوع`: عدد معاملات غير صالح.");
                break;
            }

            if (inst->operand_count == 0) {
                if (!func->ret_type || func->ret_type->kind != IR_TYPE_VOID) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `رجوع` بدون قيمة لكن نوع الدالة ليس فراغاً.");
                }
                break;
            }

            IRValue* v = inst->operands[0];
            if (!v || !v->type) {
                ir_report(diag, module, func, block, inst, "تعليمة `رجوع`: قيمة بدون نوع.");
                break;
            }

            if (!func->ret_type) {
                ir_report(diag, module, func, block, inst, "تعليمة `رجوع`: func->ret_type فارغ.");
                break;
            }

            if (!ir_types_equal(func->ret_type, v->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `رجوع`: نوع القيمة لا يطابق نوع الدالة (val=%s, func=%s).",
                          ir_type_to_arabic(v->type),
                          ir_type_to_arabic(func->ret_type));
            }
            break;
        }

        // --------------------------------------------------------------------
        // call:
        // - مباشر: call_target غير فارغ
        // - غير مباشر: call_callee غير فارغ ونوعه IR_TYPE_FUNC
        // call_args حسب call_arg_count
        // --------------------------------------------------------------------
        case IR_OP_CALL: {
            if (inst->call_target && inst->call_callee) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `نداء`: لا يمكن تحديد call_target و call_callee معاً.");
            }
            if ((!inst->call_target || !inst->call_target[0]) && !inst->call_callee) {
                ir_report(diag, module, func, block, inst, "تعليمة `نداء`: الهدف فارغ (لا مباشر ولا غير مباشر).");
            }
            if (inst->call_callee) {
                if (!inst->call_callee->type || inst->call_callee->type->kind != IR_TYPE_FUNC) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: call_callee يجب أن يكون من نوع 'دالة'.");
                }
                if (!ir_value_reg_in_range(inst->call_callee, func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: call_callee سجل خارج نطاق الدالة.");
                }
            }

            if (inst->call_arg_count < 0) {
                ir_report(diag, module, func, block, inst, "تعليمة `نداء`: call_arg_count سالب.");
            }

            if (inst->call_arg_count > 0 && !inst->call_args) {
                ir_report(diag, module, func, block, inst, "تعليمة `نداء`: call_args فارغ رغم وجود معاملات.");
            }

            for (int i = 0; i < inst->call_arg_count; i++) {
                IRValue* a = inst->call_args ? inst->call_args[i] : NULL;
                if (!a) {
                    ir_report(diag, module, func, block, inst, "تعليمة `نداء`: معامل فارغ عند الفهرس %d.", i);
                    continue;
                }
                if (!a->type) {
                    ir_report(diag, module, func, block, inst, "تعليمة `نداء`: معامل بدون نوع عند الفهرس %d.", i);
                }
                if (!ir_value_reg_in_range(a, func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: معامل سجل خارج نطاق الدالة عند الفهرس %d.", i);
                }
            }

            // قواعد dest/ret_type.
            if (inst->dest >= 0) {
                if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: dest موجود لكن نوع الإرجاع فراغ/غير صالح.");
                }
                if (!ir_inst_dest_in_range(inst, func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: سجل الوجهة خارج نطاق الدالة.");
                }
            } else {
                if (inst->type && inst->type->kind != IR_TYPE_VOID) {
                    // ليس خطأ قاتل، لكن يساعد على كشف تناقضات البناء.
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: لا يوجد dest لكن نوع التعليمة ليس فراغاً.");
                }
            }

            // تحقق توقيع الدالة إن أمكن (فقط على مستوى الوحدة).
            if (verify_module_for_calls) {
                if (inst->call_target) {
                    IRFunc* callee = ir_module_find_func((IRModule*)verify_module_for_calls, inst->call_target);
                    if (callee) {
                        if (callee->param_count != inst->call_arg_count) {
                            ir_report(diag, module, func, block, inst,
                                      "تعليمة `نداء`: عدد المعاملات لا يطابق توقيع الدالة (%d vs %d).",
                                      inst->call_arg_count, callee->param_count);
                        } else {
                            for (int i = 0; i < callee->param_count; i++) {
                                IRType* pt = callee->params[i].type;
                                IRValue* av = inst->call_args ? inst->call_args[i] : NULL;
                                if (pt && av && av->type && !ir_types_equal(pt, av->type)) {
                                    ir_report(diag, module, func, block, inst,
                                              "تعليمة `نداء`: نوع المعامل %d لا يطابق (arg=%s, param=%s).",
                                              i,
                                              ir_type_to_arabic(av->type),
                                              ir_type_to_arabic(pt));
                                }
                            }
                        }

                        // نوع الإرجاع.
                        if (inst->dest >= 0) {
                            if (callee->ret_type && inst->type && !ir_types_equal(callee->ret_type, inst->type)) {
                                ir_report(diag, module, func, block, inst,
                                          "تعليمة `نداء`: نوع الإرجاع لا يطابق توقيع الدالة (call=%s, callee=%s).",
                                          ir_type_to_arabic(inst->type),
                                          ir_type_to_arabic(callee->ret_type));
                            }
                        }
                    }
                } else if (inst->call_callee && inst->call_callee->type && inst->call_callee->type->kind == IR_TYPE_FUNC) {
                    IRType* sig = inst->call_callee->type;
                    int pc = sig->data.func.param_count;
                    if (pc != inst->call_arg_count) {
                        ir_report(diag, module, func, block, inst,
                                  "تعليمة `نداء`: عدد المعاملات لا يطابق توقيع الهدف (%d vs %d).",
                                  inst->call_arg_count, pc);
                    } else {
                        for (int i = 0; i < pc; i++) {
                            IRType* pt = sig->data.func.params ? sig->data.func.params[i] : NULL;
                            IRValue* av = inst->call_args ? inst->call_args[i] : NULL;
                            if (pt && av && av->type && !ir_types_equal(pt, av->type)) {
                                ir_report(diag, module, func, block, inst,
                                          "تعليمة `نداء`: نوع المعامل %d لا يطابق (arg=%s, param=%s).",
                                          i,
                                          ir_type_to_arabic(av->type),
                                          ir_type_to_arabic(pt));
                            }
                        }
                    }

                    if (inst->dest >= 0) {
                        IRType* rt = sig->data.func.ret;
                        if (rt && inst->type && !ir_types_equal(rt, inst->type)) {
                            ir_report(diag, module, func, block, inst,
                                      "تعليمة `نداء`: نوع الإرجاع لا يطابق توقيع الهدف (call=%s, sig=%s).",
                                      ir_type_to_arabic(inst->type),
                                      ir_type_to_arabic(rt));
                        }
                    }
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // phi
        // --------------------------------------------------------------------
        case IR_OP_PHI:
            ir_verify_phi(diag, module, func, block, inst);
            break;

        // --------------------------------------------------------------------
        // cast: dest + 1 operand، النوعان غير void
        // --------------------------------------------------------------------
        case IR_OP_CAST: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `تحويل`: نوع الهدف غير صالح.");
            }

            IRValue* v = inst->operands[0];
            if (v && v->type && v->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `تحويل`: نوع المصدر فراغ غير صالح.");
            }

            // قواعد تحويل محافظة: نسمح بتحويل:
            // - عدد <-> عدد
            // - مؤشر <-> مؤشر
            // - مؤشر <-> عدد
            // - ص١ -> عدد
            if (inst->type && v && v->type) {
                int from_int = ir_type_is_int(v->type);
                int to_int = ir_type_is_int(inst->type);
                int from_ptr = ir_type_is_ptr(v->type);
                int to_ptr = ir_type_is_ptr(inst->type);

                int from_f64 = (v->type->kind == IR_TYPE_F64);
                int to_f64 = (inst->type->kind == IR_TYPE_F64);

                // سماح: int <-> f64 كتحويل عددي.
                int ok_num_f64 = (from_int && to_f64) || (from_f64 && to_int) || (from_f64 && to_f64);

                if (!((from_int && to_int) ||
                      (from_ptr && to_ptr) ||
                      (from_ptr && to_int) ||
                      (from_int && to_ptr) ||
                      (v->type->kind == IR_TYPE_I1 && to_int) ||
                      ok_num_f64)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `تحويل`: تحويل غير مدعوم/غير واضح (from=%s, to=%s).",
                              ir_type_to_arabic(v->type),
                              ir_type_to_arabic(inst->type));
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // nop: بدون معاملات ولا وجهة
        // --------------------------------------------------------------------
        case IR_OP_NOP:
            if (inst->operand_count != 0) {
                ir_report(diag, module, func, block, inst, "تعليمة NOP يجب ألا تملك معاملات.");
            }
            if (inst->dest >= 0) {
                ir_report(diag, module, func, block, inst, "تعليمة NOP لا يجب أن تملك وجهة.");
            }
            break;

        default:
            ir_report(diag, module, func, block, inst, "Opcode غير معروف/غير مدعوم في المتحقق.");
            break;
    }
}

// ============================================================================
// تحقق دالة (Function verification)
// ============================================================================

static bool ir_verify_func_internal(IRModule* module_for_diag,
                                    IRModule* module_for_calls,
                                    IRFunc* func,
                                    FILE* out) {
    IRVerifyDiag diag = {0};
    diag.out = out ? out : stderr;
    diag.error_cap = IR_VERIFY_MAX_ERRORS;

    if (!func) return false;
    if (func->is_prototype) return true;

    if (!func->entry) {
        ir_report(&diag, module_for_diag, func, NULL, NULL,
                  "الدالة تحتوي جسماً بدون كتلة دخول (entry).");
        return false;
    }

    // تحقق مبكر: لا تسمح بمراجع كتل خارج هذه الدالة (حماية قبل rebuild_preds).
    IRVerifyCommonCtx cctx;
    cctx.diag = &diag;
    cctx.module = module_for_diag;
    cctx.func = func;
    if (!ir_verify_func_block_refs_safe(func, ir_verify_common_report, &cctx))
        return false;

    // إعادة بناء succ/pred لضمان أن pred_count دقيق لفحص `فاي` و CFG.
    ir_func_rebuild_preds(func);

    // تحقق سريع للـ CFG (منهيات/أهداف القفز).
    if (!ir_func_validate_cfg(func)) {
        ir_report(&diag, module_for_diag, func, func->entry, func->entry ? func->entry->last : NULL,
                  "CFG غير صالح (منهيات/أهداف قفز).");
        return false;
    }

    // تحقق المعاملات: توافر params/أنواعها.
    if (func->param_count < 0) {
        ir_report(&diag, module_for_diag, func, func->entry, NULL, "param_count سالب.");
    }
    if (func->param_count > 0 && !func->params) {
        ir_report(&diag, module_for_diag, func, func->entry, NULL, "params فارغ رغم وجود معاملات.");
    }
    for (int i = 0; i < func->param_count; i++) {
        if (!func->params[i].type) {
            ir_report(&diag, module_for_diag, func, func->entry, NULL,
                      "نوع معامل الدالة فارغ عند الفهرس %d.", i);
        }
        if (func->params[i].reg < 0 || func->params[i].reg >= func->next_reg) {
            ir_report(&diag, module_for_diag, func, func->entry, NULL,
                      "سجل معامل الدالة خارج نطاق next_reg عند الفهرس %d.", i);
        }
    }

    if (func->next_reg < 0) {
        ir_report(&diag, module_for_diag, func, func->entry, NULL, "next_reg سالب.");
    }

    // تحقق الكتل.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b) continue;

        ir_verify_block_list(&diag, module_for_diag, func, b);

        int seen_non_phi = 0;
        int seen_terminator = 0;

        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (!inst) continue;

            if (seen_terminator) {
                ir_report(&diag, module_for_diag, func, b, inst,
                          "تعليمة بعد المنهي داخل نفس الكتلة (غير مسموح).");
            }

            if (inst->op != IR_OP_PHI) {
                seen_non_phi = 1;
            }

            ir_verify_inst(&diag, module_for_diag, func, b, inst, seen_non_phi, module_for_calls);

            if (ir_is_terminator_op(inst->op)) {
                seen_terminator = 1;
                // يجب أن يكون المنهي هو آخر تعليمة.
                if (inst != b->last) {
                    ir_report(&diag, module_for_diag, func, b, inst,
                              "المنهي ليس آخر تعليمة في الكتلة.");
                }
            }

            // تحقق نطاق الوجهة أيضاً (مفيد للـ SSA/التمريرات).
            if (!ir_inst_dest_in_range(inst, func)) {
                ir_report(&diag, module_for_diag, func, b, inst, "وجهة خارج نطاق next_reg.");
            }
        }
    }

    return diag.error_count == 0;
}

