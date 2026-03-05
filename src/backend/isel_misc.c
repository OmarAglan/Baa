static void isel_lower_copy(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض تعليمة إزاحة_مؤشر (ptr.offset).
 *
 * النمط: %dst = إزاحة_مؤشر ptr<T> base, index
 *
 * حيث index هو فهرس عناصر ويُضرب ضمنياً في حجم T.
 */
static void isel_lower_ptr_offset(ISelCtx *ctx, IRInst *inst)
{
    if (!ctx || !ctx->mfunc || !inst || inst->operand_count < 2)
        return;

    const IRDataLayout *dl = (ctx->target) ? ctx->target->data_layout : NULL;

    IRType *elem_t = NULL;
    if (inst->type && inst->type->kind == IR_TYPE_PTR)
        elem_t = inst->type->data.pointee;

    int elem_size = ir_type_store_size(dl, elem_t ? elem_t : IR_TYPE_I8_T);
    if (elem_size <= 0) elem_size = 1;

    // base: يجب أن يكون عنواناً في سجل
    MachineOperand base = isel_lower_value(ctx, inst->operands[0]);
    if (base.kind == MACH_OP_GLOBAL)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_LEA, tmp_op, base, mach_op_none());
        base = tmp_op;
    }
    else if (base.kind != MACH_OP_VREG)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, base, mach_op_none());
        base = tmp_op;
    }

    // index: قد يكون فوري/سجل/ذاكرة -> نضمن تمثيله كقيمة 64 بت
    MachineOperand idx = isel_lower_value(ctx, inst->operands[1]);

    MachineOperand scaled = idx;

    if (idx.kind == MACH_OP_IMM)
    {
        int64_t v = idx.data.imm;
        int64_t s = v * (int64_t)elem_size;
        scaled = mach_op_imm(s, 64);
    }
    else
    {
        // ننسخ index إلى سجل ثم نُطبّق التحجيم عليه
        if (idx.kind != MACH_OP_VREG)
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t_op = mach_op_vreg(t, 64);
            isel_emit(ctx, MACH_MOV, t_op, idx, mach_op_none());
            idx = t_op;
        }

        if (elem_size != 1)
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t_op = mach_op_vreg(t, 64);
            isel_emit(ctx, MACH_MOV, t_op, idx, mach_op_none());

            int sh = 0;
            if (isel_is_pow2_i64((int64_t)elem_size, &sh) && sh > 0)
            {
                isel_emit(ctx, MACH_SHL, t_op, t_op, mach_op_imm((int64_t)sh, 8));
            }
            else
            {
                isel_emit(ctx, MACH_IMUL, t_op, t_op, mach_op_imm((int64_t)elem_size, 64));
            }

            scaled = t_op;
        }
        else
        {
            scaled = idx;
        }
    }

    MachineOperand dst = mach_op_vreg(inst->dest, 64);
    isel_emit(ctx, MACH_MOV, dst, base, mach_op_none());
    MachineInst *mi = isel_emit(ctx, MACH_ADD, dst, dst, scaled);
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عقدة فاي (phi).
 *
 * عقد فاي تُحل عادةً أثناء خروج SSA أو تخصيص السجلات.
 * حالياً، نولد تعليمات mov مشروحة كتعليقات.
 *
 * ملاحظة: في التنفيذ الكامل، يجب إدراج النسخ في الكتل السابقة.
 */
static void isel_lower_phi(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    // نولد تعليمة NOP كعنصر نائب لعقدة فاي
    // سيتم حل هذا في مرحلة خروج SSA (v0.3.2.2+)
    MachineInst *mi = isel_emit_comment(ctx, MACH_NOP, dst, mach_op_none(), mach_op_none(),
                                        "// فاي - سيُحل في تخصيص السجلات");
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية التحويل (cast).
 *
 * النمط: %dst = تحويل from_type to to_type value
 * →  mov/movzx/movsx vreg_dst, value
 */
static void isel_lower_cast(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    IRType *dst_t = inst->type;
    IRType *src_t = (inst->operands[0]) ? inst->operands[0]->type : NULL;

    // عشري <-> عشري: نسخ بتات.
    if (isel_irtype_is_f64(dst_t) && isel_irtype_is_f64(src_t))
    {
        MachineOperand dst = mach_op_vreg(inst->dest, 64);
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        src = isel_materialize_imm_to_gpr64(ctx, src);
        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
        return;
    }

    // int -> عشري
    if (isel_irtype_is_f64(dst_t) && !isel_irtype_is_f64(src_t))
    {
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        src = isel_extend_to_gpr64(ctx, src, src_t);

        MachineOperand xmm0 = mach_op_xmm(0);
        isel_emit(ctx, MACH_CVTSI2SD, xmm0, src, mach_op_none());

        MachineOperand dst = mach_op_vreg(inst->dest, 64);
        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, xmm0, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
        return;
    }

    // عشري -> int
    if (!isel_irtype_is_f64(dst_t) && isel_irtype_is_f64(src_t))
    {
        int dst_bits = isel_type_bits(dst_t);
        MachineOperand dst = mach_op_vreg(inst->dest, dst_bits);

        MachineOperand src_bits = isel_lower_value(ctx, inst->operands[0]);
        src_bits = isel_materialize_imm_to_gpr64(ctx, src_bits);

        MachineOperand xmm0 = mach_op_xmm(0);
        isel_emit(ctx, MACH_MOV, xmm0, src_bits, mach_op_none());

        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp64 = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_CVTTSD2SI, tmp64, xmm0, mach_op_none());

        // اقتطاع إلى الحجم النهائي إن كان أصغر.
        MachineOperand src = tmp64;
        if (dst_bits > 0 && dst_bits < 64)
            src.size_bits = dst_bits;

        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        if (mi) mi->ir_reg = inst->dest;
        return;
    }

    // تحويلات الأعداد الصحيحة (sign/zero extend + trunc)
    {
        int dst_bits = isel_type_bits(dst_t);
        MachineOperand dst = mach_op_vreg(inst->dest, dst_bits);
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

        int src_bits = src.size_bits;

        if (src_bits < dst_bits && src_bits > 0)
        {
            // توسيع: بالأصفار للأنواع unsigned/i1، وبالإشارة للأنواع signed.
            bool zext = true;
            if (isel_irtype_is_signed_int(src_t))
                zext = false;
            MachineOp mop = zext ? MACH_MOVZX : MACH_MOVSX;
            MachineInst *mi = isel_emit(ctx, mop, dst, src, mach_op_none());
            if (mi) mi->ir_reg = inst->dest;
        }
        else
        {
            // نسخ مباشر (نفس الحجم أو تقليص). عند التقليص، نحدد حجم المصدر.
            if (src_bits > dst_bits && dst_bits > 0)
                src.size_bits = dst_bits;
            MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
            if (mi) mi->ir_reg = inst->dest;
        }
    }
}

// ============================================================================
// خفض تعليمة IR واحدة
// ============================================================================

/**
 * @brief خفض تعليمة IR واحدة إلى تعليمات آلة.
 *
 * هذه هي الدالة المحورية لاختيار التعليمات. تقوم بتوزيع
 * كل عملية IR إلى دالة الخفض المناسبة.
 */
static void isel_lower_inst(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    switch (inst->op)
    {
    // عمليات حسابية
    case IR_OP_ADD:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_ADDSD);
        else isel_lower_binop(ctx, inst, MACH_ADD);
        break;
    case IR_OP_SUB:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_SUBSD);
        else isel_lower_binop(ctx, inst, MACH_SUB);
        break;
    case IR_OP_MUL:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_MULSD);
        else isel_lower_mul_strength_reduce(ctx, inst);
        break;
    case IR_OP_SHL:
        isel_lower_shift(ctx, inst, MACH_SHL);
        break;
    case IR_OP_SHR:
        isel_lower_shift(ctx, inst,
                         isel_irtype_is_unsigned_int(inst->type) ? MACH_SHR : MACH_SAR);
        break;
    case IR_OP_DIV:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fbinop(ctx, inst, MACH_DIVSD);
        else isel_lower_div(ctx, inst, false);
        break;
    case IR_OP_MOD:
        isel_lower_div(ctx, inst, true);
        break;
    case IR_OP_NEG:
        if (isel_irtype_is_f64(inst->type)) isel_lower_fneg(ctx, inst);
        else isel_lower_neg(ctx, inst);
        break;

    // عمليات الذاكرة
    case IR_OP_ALLOCA:
        isel_lower_alloca(ctx, inst);
        break;
    case IR_OP_LOAD:
        isel_lower_load(ctx, inst);
        break;
    case IR_OP_STORE:
        isel_lower_store(ctx, inst);
        break;
    case IR_OP_PTR_OFFSET:
        isel_lower_ptr_offset(ctx, inst);
        break;

    // عملية المقارنة
    case IR_OP_CMP:
        isel_lower_cmp(ctx, inst);
        break;

    // عمليات منطقية
    case IR_OP_AND:
    case IR_OP_OR:
    case IR_OP_XOR:
    case IR_OP_NOT:
        isel_lower_logical(ctx, inst);
        break;

    // عمليات التحكم
    case IR_OP_BR:
        isel_lower_br(ctx, inst);
        break;
    case IR_OP_BR_COND:
        isel_lower_br_cond(ctx, inst);
        break;
    case IR_OP_RET:
        isel_lower_ret(ctx, inst);
        break;
    case IR_OP_CALL:
        isel_lower_call(ctx, inst);
        break;

    // عمليات SSA
    case IR_OP_PHI:
        isel_lower_phi(ctx, inst);
        break;
    case IR_OP_COPY:
        isel_lower_copy(ctx, inst);
        break;

    // تحويل الأنواع
    case IR_OP_CAST:
        isel_lower_cast(ctx, inst);
        break;

    // لا عملية
    case IR_OP_NOP:
        isel_emit(ctx, MACH_NOP, mach_op_none(), mach_op_none(), mach_op_none());
        break;

    default:
        // عملية غير معروفة - نولد تعليق
        isel_emit_comment(ctx, MACH_NOP, mach_op_none(), mach_op_none(), mach_op_none(),
                          "// عملية IR غير مدعومة");
        break;
    }
}

// ============================================================================
// خفض كتلة IR
// ============================================================================

/**
 * @brief خفض كتلة IR إلى كتلة آلية.
 */
