// ============================================================================
// إصدار الدوال والوحدة (Function/Module Emission)
// ============================================================================

/**
 * @brief جمع السجلات المحفوظة (callee-saved) المستخدمة في الدالة.
 *
 * يمشي على جميع تعليمات الدالة ويكشف أي سجلات callee-saved
 * استُخدمت كوجهة (dst) حتى نحفظها في المقدمة ونستعيدها في الخاتمة.
 */
static int collect_callee_saved(MachineFunc* func, PhysReg* regs_out) {
    // السجلات المحفوظة التي قد نحتاج لحفظها
    // RBX, RSI, RDI, R12-R15 (نرشحها حسب الهدف؛ لا نحفظ RBP لأنه يُحفظ في المقدمة)
    static const PhysReg candidates[] = {
        PHYS_RBX, PHYS_RSI, PHYS_RDI,
        PHYS_R12, PHYS_R13, PHYS_R14, PHYS_R15
    };
    static const int candidate_count = 7;

    bool used[PHYS_REG_COUNT] = {false};

    // المشي على جميع التعليمات
    for (MachineBlock* block = func->blocks; block; block = block->next) {
        for (MachineInst* inst = block->first; inst; inst = inst->next) {
            // التحقق من الوجهة
            if (inst->dst.kind == MACH_OP_VREG) {
                int r = inst->dst.data.vreg;
                if (r >= 0 && r < PHYS_REG_COUNT) used[r] = true;
            }
            // التحقق من المصادر
            if (inst->src1.kind == MACH_OP_VREG) {
                int r = inst->src1.data.vreg;
                if (r >= 0 && r < PHYS_REG_COUNT) used[r] = true;
            }
            if (inst->src2.kind == MACH_OP_VREG) {
                int r = inst->src2.data.vreg;
                if (r >= 0 && r < PHYS_REG_COUNT) used[r] = true;
            }
        }
    }

    int count = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (used[candidates[i]] && emit_reg_is_callee_saved(candidates[i])) {
            regs_out[count++] = candidates[i];
        }
    }

    return count;
}

bool emit_func(MachineFunc* func, FILE* out) {
    if (!func || !out) return false;

    // تخطي النماذج الأولية
    if (func->is_prototype) return true;

    // تعيين بادئة فريدة لتسميات الكتل داخل هذه الدالة
    g_emit_current_func_uid = g_emit_next_func_uid++;
    g_emit_sp_seq = 0;

    // تحديد اسم الدالة (تحويل الرئيسية → main)
    const char* func_name = func->name;
    bool is_main = false;
    if (func_name && strcmp(func_name, "الرئيسية") == 0) {
        func_name = "main";
        is_main = true;
    }

    // إصدار تعريف الرمز العام
    fprintf(out, "\n.globl %s\n", func_name);
    fprintf(out, "%s:\n", func_name);
    emit_comment(out, "دالة: %s", func_name);

    // جمع السجلات المحفوظة المستخدمة
    PhysReg callee_regs[16];
    int callee_count = collect_callee_saved(func, callee_regs);

    // إصدار المقدمة
    emit_prologue(func, out, callee_regs, callee_count);

    // إصدار تعليمات الجسم
    for (MachineBlock* block = func->blocks; block; block = block->next) {
        for (MachineInst* inst = block->first; inst; inst = inst->next) {
            if (inst->op != MACH_LABEL && inst->op != MACH_COMMENT) {
                emit_debug_loc(out, inst);
            }
            if (inst->op == MACH_RET) {
                // عند الرجوع: إصدار قيمة الإرجاع الافتراضية إذا كانت main
                // ثم إصدار الخاتمة
                emit_epilogue(func, out, callee_regs, callee_count);
            } else if (inst->op == MACH_TAILJMP) {
                // نداء_ذيلي: تفكيك الإطار ثم قفز إلى الهدف
                emit_tailjmp(func, inst, out, callee_regs, callee_count);
            } else {
                emit_inst(inst, func, out);
            }
        }
    }

    // إذا لم تنتهِ الدالة بـ ret صريح، نضيف خاتمة افتراضية
    // (للسلامة: إرجاع 0 من main)
    {
        bool has_ret = false;
        for (MachineBlock* block = func->blocks; block; block = block->next) {
            for (MachineInst* inst = block->first; inst; inst = inst->next) {
                if (inst->op == MACH_RET || inst->op == MACH_TAILJMP) has_ret = true;
            }
        }
        if (!has_ret) {
            if (is_main) {
                fprintf(out, "    mov $0, %%rax\n");
            }
            emit_epilogue(func, out, callee_regs, callee_count);
        }
    }

    return true;
}

// ============================================================================
// إصدار وحدة كاملة (Module Emission)
// ============================================================================

bool emit_module_ex2(MachineModule* module, FILE* out, bool debug_info,
                     const BaaTarget* target, BaaCodegenOptions opts) {
    if (!module || !out) return false;

    g_emit_target = target ? target : baa_target_builtin_windows_x86_64();
    g_emit_cc = (g_emit_target && g_emit_target->cc) ? g_emit_target->cc : baa_target_builtin_windows_x86_64()->cc;
    g_emit_opts = opts;

    g_emit_debug_info = debug_info ? true : false;
    g_emit_asm_comments = opts.asm_comments ? true : false;
    emit_debug_reset();

    // إعادة تعيين بادئات الدوال لضمان حتمية أسماء التسميات داخل كل ملف .s
    g_emit_next_func_uid = 0;
    g_emit_current_func_uid = 0;

    // 1. قسم البيانات الثابتة (صيغ الطباعة)
    emit_rdata_section(out);

    // 2. قسم البيانات العامة (المتغيرات)
    emit_data_section(module, out);

    // 3. قسم النصوص (الدوال)
    fprintf(out, "\n.text\n");

    for (MachineFunc* func = module->funcs; func; func = func->next) {
        if (!emit_func(func, out)) {
            return false;
        }
    }

    // 4. جدول النصوص
    emit_string_table(module, out);
    emit_baa_string_table(module, out);

    // 5. وسم "لا مكدس تنفيذي" على ELF لتفادي تحذيرات ld
    if (g_emit_target && g_emit_target->obj_format == BAA_OBJFORMAT_ELF)
    {
        fprintf(out, "\n.section .note.GNU-stack,\"\",@progbits\n");
    }

    emit_debug_reset();

    return true;
}

bool emit_module_ex(MachineModule* module, FILE* out, bool debug_info, const BaaTarget* target) {
    return emit_module_ex2(module, out, debug_info, target, baa_codegen_options_default());
}

bool emit_module(MachineModule* module, FILE* out, bool debug_info) {
    return emit_module_ex(module, out, debug_info, baa_target_builtin_windows_x86_64());
}
