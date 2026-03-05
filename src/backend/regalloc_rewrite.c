// ============================================================================
// تحليل السجلات الخاصة (Special VReg Resolution)
// ============================================================================

/**
 * @brief تحويل السجلات الخاصة (السالبة) إلى سجلات فيزيائية.
 *
 * الاتفاقيات:
 *   vreg -1  → RBP (مؤشر الإطار)
 *   vreg -2  → (افتراضي) سجل الإرجاع (عادة RAX)
 *   vreg -4  → R11 (سجل خدش محجوز لتصحيح تسريب قاعدة الذاكرة)
 *   vreg -5  → RDX (سجل الباقي/المعامل الثاني لبعض التعليمات)
 *   vreg -6  → RCX (عدد الإزاحة المتغير عبر CL)
 *   vreg -10.. → سجلات معاملات ABI حسب الهدف (يُحل عبر calling convention)
 */
static PhysReg resolve_special_vreg(int vreg)
{
    switch (vreg)
    {
    case -1:
        return PHYS_RBP;
    case -3:
        return PHYS_RSP;
    case -4:
        return PHYS_R11;
    case -5:
        return PHYS_RDX;
    case -6:
        return PHYS_RCX;
    default:
        return PHYS_NONE;
    }
}

static PhysReg resolve_special_vreg_with_cc(int vreg, const BaaCallingConv* cc)
{
    PhysReg fixed = resolve_special_vreg(vreg);
    if (fixed != PHYS_NONE)
        return fixed;

    // سجل الإرجاع (عادة vreg -2)
    if (cc && vreg == cc->abi_ret_vreg)
    {
        if (cc->ret_phys_reg >= 0 && cc->ret_phys_reg < PHYS_REG_COUNT)
            return (PhysReg)cc->ret_phys_reg;
    }

    // توافق خلفي: -2 -> RAX
    if (vreg == -2)
        return PHYS_RAX;

    // معاملات ABI: arg i -> (abi_arg_vreg0 - i)
    if (cc)
    {
        int base = cc->abi_arg_vreg0;
        // نطاق حماية بسيط لتفادي أرقام سالبة بعيدة
        if (vreg <= base && vreg >= base - 32)
        {
            int i = base - vreg;
            if (i >= 0 && i < cc->int_arg_reg_count)
            {
                int pr = cc->int_arg_phys_regs[i];
                if (pr >= 0 && pr < PHYS_REG_COUNT)
                    return (PhysReg)pr;
            }
        }
    }
    return PHYS_NONE;
}

// ============================================================================
// إعادة كتابة المعاملات (Operand Rewrite)
// ============================================================================

/**
 * @brief إعادة كتابة معامل واحد بسجل فيزيائي.
 *
 * يحوّل MACH_OP_VREG إلى سجل فيزيائي، أو إذا كان مسرّباً
 * يحوّله إلى معامل ذاكرة [RBP + offset].
 */
static void rewrite_operand(RegAllocCtx *ctx, MachineOperand *op)
{
    if (!op)
        return;

    if (op->kind == MACH_OP_VREG)
    {
        int vreg = op->data.vreg;

        // السجلات الخاصة (السالبة)
        if (vreg < 0)
        {
            PhysReg phys = resolve_special_vreg_with_cc(vreg, ctx ? ctx->cc : NULL);
            if (phys != PHYS_NONE)
            {
                op->data.vreg = phys;
                // نبقي kind كـ MACH_OP_VREG لكن الرقم الآن هو السجل الفيزيائي
                // سيتم تفسيره كسجل فيزيائي في مرحلة إصدار الكود
            }
            return;
        }

        // السجلات العادية
        if (vreg >= 0 && vreg < ctx->max_vreg)
        {
            if (ctx->vreg_spilled[vreg])
            {
                // السجل مسرّب: تحويل إلى معامل ذاكرة
                op->kind = MACH_OP_MEM;
                op->data.mem.base_vreg = PHYS_RBP;
                op->data.mem.offset = ctx->vreg_spill_offset[vreg];
            }
            else if (ctx->vreg_to_phys[vreg] != PHYS_NONE)
            {
                // السجل مخصص: استبدال الرقم
                op->data.vreg = ctx->vreg_to_phys[vreg];
            }
        }
    }
    else if (op->kind == MACH_OP_MEM)
    {
        // إعادة كتابة سجل القاعدة في معاملات الذاكرة
        int base = op->data.mem.base_vreg;
        if (base < 0)
        {
            PhysReg phys = resolve_special_vreg_with_cc(base, ctx ? ctx->cc : NULL);
            if (phys != PHYS_NONE)
            {
                op->data.mem.base_vreg = phys;
            }
        }
        else if (base >= 0 && base < ctx->max_vreg)
        {
            if (ctx->vreg_to_phys[base] != PHYS_NONE)
            {
                op->data.mem.base_vreg = ctx->vreg_to_phys[base];
            }
        }
    }
}

void regalloc_rewrite(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func)
        return;

    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            rewrite_operand(ctx, &inst->dst);
            rewrite_operand(ctx, &inst->src1);
            rewrite_operand(ctx, &inst->src2);
        }
    }
}

// ============================================================================
// إدراج كود التسريب (Spill Code Insertion)
// ============================================================================

static void mach_block_insert_before_local(MachineBlock* block, MachineInst* pos, MachineInst* inst)
{
    if (!block || !pos || !inst) return;

    inst->next = pos;
    inst->prev = pos->prev;

    if (pos->prev) pos->prev->next = inst;
    else block->first = inst;

    pos->prev = inst;
    block->inst_count++;
}

static void regalloc_fix_mem_base_operand(RegAllocCtx* ctx,
                                          MachineBlock* block,
                                          MachineInst* pos,
                                          MachineOperand* op,
                                          int* io_used_scratch)
{
    if (!ctx || !block || !pos || !op) return;
    if (op->kind != MACH_OP_MEM) return;

    int base = op->data.mem.base_vreg;
    if (base < 0) return;
    if (base >= ctx->max_vreg) return;
    if (!ctx->vreg_spilled[base]) return;

    // إذا استخدمنا سجل الخدش مسبقاً في نفس التعليمة، لا يمكننا معالجة قاعدة ثانية.
    if (io_used_scratch && *io_used_scratch) return;

    int32_t spill_off = (int32_t)ctx->vreg_spill_offset[base];

    // reload: %r11 = [rbp + spill_off]
    MachineOperand dst = mach_op_vreg(REGALLOC_VREG_SCRATCH_BASE, 64);
    MachineOperand src = mach_op_mem(-1, spill_off, 64);
    MachineInst* reload = mach_inst_new(MACH_LOAD, dst, src, mach_op_none());
    if (!reload) return;
    reload->comment = "// إعادة تحميل مؤشر مسرّب";

    mach_block_insert_before_local(block, pos, reload);
    op->data.mem.base_vreg = REGALLOC_VREG_SCRATCH_BASE;

    if (io_used_scratch) *io_used_scratch = 1;
}

void regalloc_insert_spill_code(RegAllocCtx *ctx)
{
    if (!ctx || !ctx->func || ctx->spill_count == 0)
        return;

    // للسجلات المسرّبة، نحتاج لإدراج:
    // - store بعد كل تعريف (def)
    // - load قبل كل استخدام (use)
    //
    // ملاحظة: في التنفيذ الحالي، إعادة الكتابة (rewrite) تحوّل
    // معاملات السجلات المسرّبة إلى معاملات ذاكرة مباشرة [RBP + offset].
    // هذا يعمل لمعظم التعليمات في x86-64 لأنها تقبل معامل ذاكرة واحد.
    //
    // الحالات التي تحتاج معالجة خاصة (معاملان ذاكرة) ستُعالج
    // في مرحلة إصدار الكود (v0.3.2.3) بإدراج سجل مؤقت.

    for (MachineBlock *block = ctx->func->blocks; block; block = block->next)
    {
        for (MachineInst *inst = block->first; inst; inst = inst->next)
        {
            int used_scratch = 0;
            regalloc_fix_mem_base_operand(ctx, block, inst, &inst->dst, &used_scratch);
            regalloc_fix_mem_base_operand(ctx, block, inst, &inst->src1, &used_scratch);
            regalloc_fix_mem_base_operand(ctx, block, inst, &inst->src2, &used_scratch);
        }
    }
}

