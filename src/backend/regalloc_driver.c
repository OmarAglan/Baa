// ============================================================================
// تشغيل تخصيص السجلات لدالة واحدة
// ============================================================================

static bool regalloc_func_ex(MachineFunc *func, const BaaCallingConv* cc)
{
    if (!func || func->is_prototype)
        return true;
    if (func->block_count == 0)
        return true;

    // إنشاء السياق
    RegAllocCtx *ctx = regalloc_ctx_new(func);
    if (!ctx)
        return false;

    ctx->cc = cc;

    // 1. ترقيم التعليمات
    regalloc_number_insts(ctx);

    // 2. حساب def/use
    regalloc_compute_def_use(ctx);

    // 3. حساب الحيوية (live-in / live-out)
    regalloc_compute_liveness(ctx);

    // 4. بناء فترات الحيوية
    regalloc_build_intervals(ctx);

    // 5. المسح الخطي
    regalloc_linear_scan(ctx);

    // 6. إدراج كود التسريب
    regalloc_insert_spill_code(ctx);

    // 7. إعادة كتابة المعاملات
    regalloc_rewrite(ctx);

    // تحرير السياق
    regalloc_ctx_free(ctx);

    return true;
}

bool regalloc_func(MachineFunc *func)
{
    return regalloc_func_ex(func, baa_target_builtin_windows_x86_64()->cc);
}

// ============================================================================
// تشغيل تخصيص السجلات على وحدة كاملة
// ============================================================================

bool regalloc_run_ex(MachineModule *module, const BaaTarget* target)
{
    if (!module)
        return false;

    const BaaCallingConv* cc = regalloc_cc_or_default(target);

    for (MachineFunc *func = module->funcs; func; func = func->next)
    {
        if (!regalloc_func_ex(func, cc))
        {
            return false;
        }
    }

    return true;
}

bool regalloc_run(MachineModule *module)
{
    return regalloc_run_ex(module, baa_target_builtin_windows_x86_64());
}

// ============================================================================
// طباعة فترات الحيوية (Debug Printing)
// ============================================================================

void regalloc_print_intervals(RegAllocCtx *ctx, FILE *out)
{
    if (!ctx || !out)
        return;

    fprintf(out, "# فترات الحيوية (Live Intervals):\n");
    for (int i = 0; i < ctx->interval_count; i++)
    {
        LiveInterval *li = &ctx->intervals[i];
        fprintf(out, "#   vreg %d: [%d, %d]", li->vreg, li->start, li->end);
        if (li->spilled)
        {
            fprintf(out, " → spilled [rbp%+d]", li->spill_offset);
        }
        else if (li->phys_reg != PHYS_NONE)
        {
            fprintf(out, " → %s", phys_reg_name(li->phys_reg));
        }
        else
        {
            fprintf(out, " → unassigned");
        }
        fprintf(out, "\n");
    }
}

void regalloc_print_allocation(RegAllocCtx *ctx, FILE *out)
{
    if (!ctx || !out)
        return;

    fprintf(out, "# نتائج تخصيص السجلات (Register Allocation Results):\n");
    fprintf(out, "#   عدد السجلات الافتراضية: %d\n", ctx->max_vreg);
    fprintf(out, "#   عدد الفترات: %d\n", ctx->interval_count);
    fprintf(out, "#   عدد المسرّبة: %d\n", ctx->spill_count);
    fprintf(out, "#   حجم المكدس: %d\n", ctx->func->stack_size);

    for (int i = 0; i < ctx->max_vreg; i++)
    {
        if (ctx->vreg_to_phys[i] != PHYS_NONE)
        {
            fprintf(out, "#   v%d → %s\n", i, phys_reg_name(ctx->vreg_to_phys[i]));
        }
        else if (ctx->vreg_spilled[i])
        {
            fprintf(out, "#   v%d → spilled [rbp%+d]\n", i, ctx->vreg_spill_offset[i]);
        }
    }

    // طباعة السجلات المحفوظة المستخدمة
    fprintf(out, "#   السجلات المحفوظة المستخدمة:");
    bool first = true;
    for (int i = 0; i < PHYS_REG_COUNT; i++)
    {
        if (ctx->callee_saved_used[i])
        {
            fprintf(out, "%s %s", first ? "" : ",", phys_reg_name(i));
            first = false;
        }
    }
    fprintf(out, "%s\n", first ? " (لا شيء)" : "");
}
