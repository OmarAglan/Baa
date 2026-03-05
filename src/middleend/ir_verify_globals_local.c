// ============================================================================
// تحقق Global (على مستوى الوحدة)
// ============================================================================

static void ir_verify_globals(IRVerifyDiag* diag, IRModule* module) {
    if (!diag || !module) return;

    for (IRGlobal* g = module->globals; g; g = g->next) {
        if (!g->name || !g->name[0]) {
            ir_report(diag, module, NULL, NULL, NULL, "متغير عام بدون اسم.");
        }
        if (!g->type) {
            ir_report(diag, module, NULL, NULL, NULL, "متغير عام @%s بدون نوع.",
                      ir_safe_str(g->name));
            continue;
        }
        if (g->type->kind == IR_TYPE_VOID) {
            ir_report(diag, module, NULL, NULL, NULL, "متغير عام @%s من نوع فراغ (غير مسموح).",
                      ir_safe_str(g->name));
        }

        // مصفوفة عامة: تحقق من قائمة التهيئة
        if (g->type->kind == IR_TYPE_ARRAY) {
            IRType* elem_t = g->type->data.array.element;
            int count = g->type->data.array.count;

            if (g->init) {
                ir_report(diag, module, NULL, NULL, NULL,
                          "متغير عام @%s من نوع مصفوفة لا يجب أن يملك init مفرداً.",
                          ir_safe_str(g->name));
            }

            if (count < 0) {
                ir_report(diag, module, NULL, NULL, NULL,
                          "متغير عام @%s لديه حجم مصفوفة غير صالح.",
                          ir_safe_str(g->name));
            }

            if (g->init_elem_count < 0 || (count >= 0 && g->init_elem_count > count)) {
                ir_report(diag, module, NULL, NULL, NULL,
                          "تهيئة مصفوفة عامة @%s بعدد عناصر غير صالح (init=%d, size=%d).",
                          ir_safe_str(g->name), g->init_elem_count, count);
            }

            for (int i = 0; i < g->init_elem_count; i++) {
                IRValue* v = g->init_elems ? g->init_elems[i] : NULL;
                if (!v) {
                    ir_report(diag, module, NULL, NULL, NULL,
                              "تهيئة مصفوفة عامة @%s تحتوي على عنصر NULL عند %d.",
                              ir_safe_str(g->name), i);
                    continue;
                }
                if (elem_t && v->type && !ir_types_equal(elem_t, v->type)) {
                    ir_report(diag, module, NULL, NULL, NULL,
                              "تهيئة مصفوفة عامة @%s بعنصر نوعه لا يطابق (elem=%s, init=%s).",
                              ir_safe_str(g->name),
                              ir_type_to_arabic(elem_t),
                              ir_type_to_arabic(v->type));
                }

                // قيود: إن كان العنصر عدداً صحيحاً، يجب أن يكون ثابتاً.
                if (elem_t && (elem_t->kind == IR_TYPE_I1 || elem_t->kind == IR_TYPE_I8 ||
                              elem_t->kind == IR_TYPE_I16 || elem_t->kind == IR_TYPE_I32 ||
                              elem_t->kind == IR_TYPE_I64 ||
                              elem_t->kind == IR_TYPE_U8 || elem_t->kind == IR_TYPE_U16 ||
                              elem_t->kind == IR_TYPE_U32 || elem_t->kind == IR_TYPE_U64))
                {
                    if (v->kind != IR_VAL_CONST_INT) {
                        ir_report(diag, module, NULL, NULL, NULL,
                                  "تهيئة مصفوفة عامة @%s يجب أن تكون ثوابت عددية.",
                                  ir_safe_str(g->name));
                    }
                }
            }

            continue;
        }

        if (g->init && g->init->type && !ir_types_equal(g->type, g->init->type)) {
            // ملاحظة: في IR الحالي، global->type هو "نوع القيمة" وليس نوع المؤشر.
            ir_report(diag, module, NULL, NULL, NULL,
                      "تهيئة متغير عام @%s بنوع لا يطابق (init=%s, global=%s).",
                      ir_safe_str(g->name),
                      ir_type_to_arabic(g->init->type),
                      ir_type_to_arabic(g->type));
        }
    }
}

// ============================================================================
// الواجهات العامة
// ============================================================================

bool ir_func_verify_ir(IRFunc* func, FILE* out) {
    // لا يوجد سياق وحدة هنا → لا نتحقق من تواقيع النداءات عبر الوحدة.
    return ir_verify_func_internal(NULL, NULL, func, out);
}

bool ir_module_verify_ir(IRModule* module, FILE* out) {
    IRVerifyDiag diag = {0};
    diag.out = out ? out : stderr;
    diag.error_cap = IR_VERIFY_MAX_ERRORS;

    if (!module) return false;

    // تحقق globals أولاً.
    ir_verify_globals(&diag, module);

    int ok = (diag.error_count == 0) ? 1 : 0;

    // تحقق الدوال، مع تمكين فحص تواقيع `نداء` عندما تكون الدالة الهدف داخل الوحدة.
    for (IRFunc* f = module->funcs; f; f = f->next) {
        if (!ir_verify_func_internal(module, module, f, diag.out)) {
            ok = 0;
        }
    }

    return ok ? true : false;
}
