/**
 * @brief خفض عملية القفز غير المشروط.
 *
 * النمط: قفز target
 * →  jmp label
 */
static void isel_lower_br(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    MachineOperand target = isel_lower_value(ctx, inst->operands[0]);
    isel_emit(ctx, MACH_JMP, target, mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية القفز المشروط.
 *
 * النمط: قفز_شرط cond, if_true, if_false
 * →  test cond, cond    (أو cmp cond, 0)
 *    jne  if_true
 *    jmp  if_false
 */
static void isel_lower_br_cond(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 3)
        return;

    MachineOperand cond = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand if_true = isel_lower_value(ctx, inst->operands[1]);
    MachineOperand if_false = isel_lower_value(ctx, inst->operands[2]);

    // إذا كان الشرط قيمة فورية، ننقله لسجل
    if (cond.kind == MACH_OP_IMM)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, cond, mach_op_none());
        cond = tmp_op;
    }

    // فحص الشرط
    isel_emit(ctx, MACH_TEST, mach_op_none(), cond, cond);

    // قفز إلى الكتلة الصحيحة
    isel_emit(ctx, MACH_JNE, if_true, mach_op_none(), mach_op_none());
    isel_emit(ctx, MACH_JMP, if_false, mach_op_none(), mach_op_none());
}

/**
 * @brief خفض عملية الرجوع.
 *
 * النمط: رجوع value
 * →  mov vreg_ret, value   (إذا كانت هناك قيمة)
 *    ret
 */
static void isel_lower_ret(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    const BaaCallingConv *cc = isel_cc_or_default(ctx);

    if (inst->operand_count > 0 && inst->operands[0])
    {
        IRType *rt = inst->operands[0] ? inst->operands[0]->type : NULL;
        MachineOperand val = isel_lower_value(ctx, inst->operands[0]);

        if (rt && rt->kind == IR_TYPE_F64)
        {
            // القيمة المرجعة العشرية → XMM0
            val = isel_materialize_imm_to_gpr64(ctx, val);
            MachineOperand xmm0 = mach_op_xmm(0);
            isel_emit_comment(ctx, MACH_MOV, xmm0, val, mach_op_none(),
                              "// قيمة الإرجاع → XMM0");
        }
        else
        {
            // نضع القيمة المرجعة في سجل خاص (سيصبح RAX في تخصيص السجلات)
            MachineOperand ret_reg = mach_op_vreg(isel_abi_ret_vreg(cc), isel_type_bits(rt));
            isel_emit_comment(ctx, MACH_MOV, ret_reg, val, mach_op_none(),
                              "// قيمة الإرجاع → RAX");
        }
    }

    isel_emit(ctx, MACH_RET, mach_op_none(), mach_op_none(), mach_op_none());
}

typedef struct
{
    int fixed_vreg;
    int bits;
    IRValue *addr;
} InlineAsmOutputBinding;

