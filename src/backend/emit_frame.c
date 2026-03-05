// ============================================================================
// إصدار مقدمة وخاتمة الدالة (Function Prologue/Epilogue)
// ============================================================================

/**
 * @brief إصدار مقدمة الدالة (Prologue).
 *
 * تشمل:
 * 1. حفظ مؤشر الإطار القديم (push %rbp)
 * 2. إعداد مؤشر الإطار الجديد (mov %rsp, %rbp)
 * 3. حجز مساحة المكدس المحلي (sub $N, %rsp)
 * 4. حفظ السجلات المحفوظة (callee-saved) المستخدمة
 *
 * @param func الدالة.
 * @param out ملف الخرج.
 * @param callee_saved_used مصفوفة السجلات المحفوظة المستخدمة.
 * @param callee_count عدد السجلات المحفوظة لحفظها.
 */
static void emit_prologue(MachineFunc* func, FILE* out,
                           PhysReg* callee_regs, int callee_count) {
    emit_comment(out, "بداية prologue");
    // حفظ إطار المكدس
    fprintf(out, "    push %%rbp\n");
    fprintf(out, "    mov %%rsp, %%rbp\n");

    // حساب حجم المكدس الإجمالي:
    // حجم محلي (func->stack_size) + shadow space (حسب الهدف) + حفظ callee-saved
    // مع محاذاة 16 بايت
    int local_size = func->stack_size;
    int callee_save_size = callee_count * 8;

    // إجمالي الحجم المطلوب (بعد push rbp)
    // المكدس بعد push rbp: RSP ناقص 8 (لـ rbp المحفوظ)
    int shadow = emit_shadow_bytes();
    int canary_size = emit_stack_protector_size(func);
    int total_frame = local_size + shadow + canary_size + callee_save_size;

    // محاذاة إلى قيمة الهدف (عادة 16 بايت)
    // بعد push rbp، RSP = aligned - 8
    // نحتاج sub ليجعل RSP محاذى عند نقاط الاستدعاء.
    int align = emit_stack_align_bytes();
    if (align > 0 && (total_frame % align) != 0) {
        total_frame = ((total_frame / align) + 1) * align;
    }

    if (total_frame > 0) {
        fprintf(out, "    sub $%d, %%rsp\n", total_frame);
        emit_comment(out, "حجز إطار مكدس بحجم %d بايت", total_frame);
    }

    // تهيئة كناري حماية المكدس (إن كان مفعلاً)
    emit_stack_protector_prologue(func, out);

    // حفظ السجلات المحفوظة (callee-saved)
    for (int i = 0; i < callee_count; i++) {
        int save_offset = -(local_size + shadow + canary_size + (i + 1) * 8);
        fprintf(out, "    mov %s, %d(%%rbp)\n",
                reg64_names[callee_regs[i]], save_offset);
        emit_comment(out, "حفظ سجل callee-saved: %s", reg64_names[callee_regs[i]]);
    }
}

/**
 * @brief إصدار خاتمة الدالة (Epilogue).
 *
 * تشمل:
 * 1. استعادة السجلات المحفوظة (callee-saved)
 * 2. استعادة مؤشر الإطار (leave)
 * 3. الرجوع (ret)
 *
 * ملاحظة: يتم إصدار الخاتمة قبل كل تعليمة ret في الدالة.
 */
static void emit_epilogue(MachineFunc* func, FILE* out,
                           PhysReg* callee_regs, int callee_count) {
    emit_comment(out, "بداية epilogue");
    int local_size = func->stack_size;
    int shadow = emit_shadow_bytes();
    int canary_size = emit_stack_protector_size(func);
    // تحقق كناري حماية المكدس قبل تفكيك الإطار
    emit_stack_protector_epilogue(func, out, g_emit_sp_seq++);

    // استعادة السجلات المحفوظة (callee-saved) بترتيب عكسي
    for (int i = callee_count - 1; i >= 0; i--) {
        int save_offset = -(local_size + shadow + canary_size + (i + 1) * 8);
        fprintf(out, "    mov %d(%%rbp), %s\n",
                save_offset, reg64_names[callee_regs[i]]);
        emit_comment(out, "استرجاع سجل callee-saved: %s", reg64_names[callee_regs[i]]);
    }

    // استعادة المكدس والرجوع
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");
    emit_comment(out, "نهاية epilogue");
}

/**
 * @brief إصدار نداء_ذيلي (Tail Call) كقفز بعد تفكيك إطار الدالة.
 *
 * الفكرة:
 * - نفك إطار الدالة الحالية (استعادة callee-saved ثم leave)
 * - نقل/تحضير المعاملات يتم مسبقاً في ISel
 * - نقفز إلى الدالة الهدف بدون push عنوان رجوع جديد
 *
 * بهذه الطريقة، عندما تُنفّذ الدالة الهدف `ret` ستعود مباشرةً
 * إلى مستدعي الدالة الحالية (إعادة استخدام إطار المكدس).
 */
static void emit_tailjmp(MachineFunc* func, MachineInst* inst, FILE* out,
                         PhysReg* callee_regs, int callee_count) {
    if (!func || !inst || !out) return;

    int local_size = func->stack_size;
    int shadow = emit_shadow_bytes();
    int canary_size = emit_stack_protector_size(func);
    // تحقق كناري حماية المكدس قبل الخروج من الدالة (حتى في النداء_الذيلي)
    emit_stack_protector_epilogue(func, out, g_emit_sp_seq++);

    // استعادة السجلات المحفوظة (callee-saved) بترتيب عكسي
    for (int i = callee_count - 1; i >= 0; i--) {
        int save_offset = -(local_size + shadow + canary_size + (i + 1) * 8);
        fprintf(out, "    mov %d(%%rbp), %s\n",
                save_offset, reg64_names[callee_regs[i]]);
    }

    // تفكيك إطار الدالة: بعد leave يصبح RSP على عنوان الرجوع
    fprintf(out, "    leave\n");

    fprintf(out, "    jmp ");
    emit_operand(&inst->src1, out);
    fprintf(out, "\n");
}

// ============================================================================
// إصدار التعليمات (Instruction Emission)
// ============================================================================

/**
 * @brief تحديد لاحقة الحجم لتعليمة بناءً على معاملاتها.
 */
static char infer_suffix(MachineInst* inst) {
    // نستخدم أول معامل يحدد الحجم (dst ثم src1 ثم src2)
    if (inst->dst.size_bits > 0) return size_suffix(inst->dst.size_bits);
    if (inst->src1.size_bits > 0) return size_suffix(inst->src1.size_bits);
    if (inst->src2.size_bits > 0) return size_suffix(inst->src2.size_bits);
    return 'q'; // افتراضي 64-بت
}

