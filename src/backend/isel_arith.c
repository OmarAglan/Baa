/**
 * @brief خفض عملية حسابية ثنائية (جمع، طرح، ضرب).
 *
 * النمط: %dst = op type lhs, rhs
 * →  mov vreg_dst, lhs
 *    op  vreg_dst, rhs
 *
 * تحسين: إذا كان أحد المعاملين قيمة فورية وكان نفس سجل الوجهة
 * يمكننا استخدامها مباشرة.
 */
static void isel_lower_binop(ISelCtx *ctx, IRInst *inst, MachineOp mop)
{
    if (!inst || inst->operand_count < 2)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // نقل القيمة اليسرى إلى سجل الوجهة
    isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());

    // تنفيذ العملية
    MachineInst *mi = isel_emit(ctx, mop, dst, dst, rhs);
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية القسمة والباقي.
 *
 * القسمة في x86-64 تستخدم idiv:
 *   cqo          ; توسيع الإشارة RAX → RDX:RAX
 *   idiv divisor ; RAX = حاصل القسمة، RDX = الباقي
 *
 * نمثل ذلك بسجلات افتراضية ونترك تخصيص السجلات لاحقاً.
 *
 * النمط: %dst = قسم type lhs, rhs
 * →  mov vreg_dst, lhs
 *    cqo
 *    idiv rhs
 *    (الناتج في vreg_dst لحاصل القسمة أو vreg مؤقت للباقي)
 */
static void isel_lower_div(ISelCtx *ctx, IRInst *inst, bool is_mod)
{
    if (!inst || inst->operand_count < 2)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    IRType *lhs_t = (inst->operands[0] ? inst->operands[0]->type : inst->type);
    IRType *rhs_t = (inst->operands[1] ? inst->operands[1]->type : inst->type);

    MachineOperand lhs0 = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs0 = isel_lower_value(ctx, inst->operands[1]);

    // نُجري القسمة دائماً بعرض 64-بت (idivq/divq) لتبسيط الدعم.
    // ثم نقتطع النتيجة عند النسخ إلى الوجهة إذا كانت أصغر (32-بت مثلاً).
    MachineOperand lhs = isel_extend_to_gpr64(ctx, lhs0, lhs_t);
    MachineOperand divisor = isel_extend_to_gpr64(ctx, rhs0, rhs_t);

    // القسمة تتطلب RAX:RDX ضمنياً
    // نستخدم vreg -2 (RAX) لنقل المقسوم ثم نسخ النتيجة
    MachineOperand rax = mach_op_vreg(-2, 64);
    MachineOperand rdx = mach_op_vreg(-5, 64);

    // نقل المقسوم إلى RAX
    isel_emit_comment(ctx, MACH_MOV, rax, lhs, mach_op_none(),
                      is_mod ? "// باقي: تحضير المقسوم في RAX" : "// قسمة: تحضير المقسوم في RAX");

    bool is_unsigned = false;
    if (inst->type)
    {
        switch (inst->type->kind)
        {
        case IR_TYPE_U8:
        case IR_TYPE_U16:
        case IR_TYPE_U32:
        case IR_TYPE_U64:
            is_unsigned = true;
            break;
        default:
            break;
        }
    }

    if (is_unsigned)
    {
        // div: RDX:RAX يجب أن يكون بدون إشارة → صفّر RDX.
        isel_emit_comment(ctx, MACH_XOR, rdx, rdx, rdx,
                          "// قسمة بدون إشارة: تصفير RDX");
        isel_emit_comment(ctx, MACH_DIV, rax, divisor, mach_op_none(),
                          is_mod ? "// باقي القسمة في RDX (unsigned)" : "// حاصل القسمة في RAX (unsigned)");
    }
    else
    {
        // توسيع الإشارة RAX → RDX:RAX
        isel_emit(ctx, MACH_CQO, mach_op_none(), rax, mach_op_none());

        // القسمة: idivq divisor → حاصل القسمة في RAX، الباقي في RDX
        isel_emit_comment(ctx, MACH_IDIV, rax, divisor, mach_op_none(),
                          is_mod ? "// باقي القسمة في RDX" : "// حاصل القسمة في RAX");
    }

    // نسخ النتيجة إلى سجل الوجهة
    // - القسمة: النتيجة في RAX
    // - الباقي: النتيجة في RDX
    MachineOperand result = is_mod ? mach_op_vreg(-5, bits) : mach_op_vreg(-2, bits);
    MachineInst *mi = isel_emit_comment(ctx, MACH_MOV, dst, result, mach_op_none(),
                                         is_mod ? "// نسخ نتيجة الباقي" : "// نسخ نتيجة القسمة");
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عمليات الإزاحة (shl/shr/sar).
 *
 * قيود x86-64:
 * - عدد الإزاحة إما قيمة فورية 8-بت أو السجل CL.
 */
static void isel_lower_shift(ISelCtx *ctx, IRInst *inst, MachineOp mop)
{
    if (!inst || inst->operand_count < 2)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // نقل القيمة المزاحة إلى الوجهة
    isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());

    MachineOperand sh = rhs;
    if (rhs.kind == MACH_OP_IMM)
    {
        // x86 يستخدم imm8 لعدد الإزاحة
        sh = rhs;
        sh.size_bits = 8;
    }
    else
    {
        // عدد الإزاحة غير الفوري يجب أن يكون في CL (RCX منخفض 8-بت)
        // vreg -6 محجوز لمطابقة RCX في مرحلة regalloc.
        MachineOperand rcx = mach_op_vreg(-6, 64);
        MachineOperand rhs64 = rhs;
        if (rhs64.kind == MACH_OP_VREG || rhs64.kind == MACH_OP_MEM) {
            rhs64.size_bits = 64;
        }
        isel_emit(ctx, MACH_MOV, rcx, rhs64, mach_op_none());
        sh = mach_op_vreg(-6, 8); // %cl
    }

    MachineInst *mi = isel_emit(ctx, mop, dst, dst, sh);
    if (mi)
        mi->ir_reg = inst->dest;
}

static int isel_irtype_is_f64(IRType *t)
{
    return t && t->kind == IR_TYPE_F64;
}

static int isel_irtype_is_signed_int(IRType *t)
{
    if (!t) return 0;
    return t->kind == IR_TYPE_I8 || t->kind == IR_TYPE_I16 || t->kind == IR_TYPE_I32 || t->kind == IR_TYPE_I64 ||
           t->kind == IR_TYPE_CHAR || t->kind == IR_TYPE_I1;
}

static int isel_irtype_is_unsigned_int(IRType *t)
{
    if (!t) return 0;
    return t->kind == IR_TYPE_U8 || t->kind == IR_TYPE_U16 || t->kind == IR_TYPE_U32 || t->kind == IR_TYPE_U64;
}

static MachineOperand isel_materialize_imm_to_gpr64(ISelCtx *ctx, MachineOperand op)
{
    if (!ctx || !ctx->mfunc)
        return op;
    if (op.kind != MACH_OP_IMM)
        return op;

    int tmp = mach_func_alloc_vreg(ctx->mfunc);
    MachineOperand tmp_op = mach_op_vreg(tmp, 64);
    MachineOperand imm = op;
    imm.size_bits = 64;
    isel_emit(ctx, MACH_MOV, tmp_op, imm, mach_op_none());
    return tmp_op;
}

static MachineOperand isel_extend_to_gpr64(ISelCtx *ctx, MachineOperand src, IRType *src_type)
{
    if (!ctx || !ctx->mfunc)
        return src;

    if (src.kind == MACH_OP_IMM)
        return isel_materialize_imm_to_gpr64(ctx, src);

    int sb = src.size_bits;
    if (sb <= 0) sb = 64;
    if (sb >= 64)
    {
        if (src.kind == MACH_OP_VREG) src.size_bits = 64;
        return src;
    }

    int tmp = mach_func_alloc_vreg(ctx->mfunc);
    MachineOperand dst = mach_op_vreg(tmp, 64);
    MachineOp mop = isel_irtype_is_signed_int(src_type) ? MACH_MOVSX : MACH_MOVZX;

    // src يحتفظ بحجمه الأصلي.
    isel_emit(ctx, mop, dst, src, mach_op_none());
    return dst;
}

static void isel_lower_fbinop(ISelCtx *ctx, IRInst *inst, MachineOp mop)
{
    if (!inst || inst->operand_count < 2)
        return;

    MachineOperand dst_gpr = mach_op_vreg(inst->dest, 64);
    MachineOperand lhs0 = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs0 = isel_lower_value(ctx, inst->operands[1]);

    lhs0 = isel_materialize_imm_to_gpr64(ctx, lhs0);
    rhs0 = isel_materialize_imm_to_gpr64(ctx, rhs0);

    MachineOperand xmm0 = mach_op_xmm(0);
    MachineOperand xmm1 = mach_op_xmm(1);

    isel_emit(ctx, MACH_MOV, xmm0, lhs0, mach_op_none());
    isel_emit(ctx, MACH_MOV, xmm1, rhs0, mach_op_none());

    isel_emit(ctx, mop, xmm0, xmm0, xmm1);

    MachineInst *mi = isel_emit(ctx, MACH_MOV, dst_gpr, xmm0, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

static void isel_lower_fneg(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    MachineOperand dst_gpr = mach_op_vreg(inst->dest, 64);
    MachineOperand src0 = isel_lower_value(ctx, inst->operands[0]);
    src0 = isel_materialize_imm_to_gpr64(ctx, src0);

    MachineOperand xmm0 = mach_op_xmm(0);
    MachineOperand xmm1 = mach_op_xmm(1);

    isel_emit(ctx, MACH_MOV, xmm0, src0, mach_op_none());

    // signmask = 0x8000..00
    int tmp = mach_func_alloc_vreg(ctx->mfunc);
    MachineOperand mask_gpr = mach_op_vreg(tmp, 64);
    isel_emit(ctx, MACH_MOV, mask_gpr, mach_op_imm((int64_t)0x8000000000000000ULL, 64), mach_op_none());
    isel_emit(ctx, MACH_MOV, xmm1, mask_gpr, mach_op_none());

    isel_emit(ctx, MACH_XORPD, xmm0, xmm0, xmm1);

    MachineInst *mi = isel_emit(ctx, MACH_MOV, dst_gpr, xmm0, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

static void isel_lower_fcmp(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    MachineOperand lhs0 = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs0 = isel_lower_value(ctx, inst->operands[1]);
    lhs0 = isel_materialize_imm_to_gpr64(ctx, lhs0);
    rhs0 = isel_materialize_imm_to_gpr64(ctx, rhs0);

    MachineOperand xmm0 = mach_op_xmm(0);
    MachineOperand xmm1 = mach_op_xmm(1);
    isel_emit(ctx, MACH_MOV, xmm0, lhs0, mach_op_none());
    isel_emit(ctx, MACH_MOV, xmm1, rhs0, mach_op_none());

    // ucomisd xmm1, xmm0
    isel_emit(ctx, MACH_UCOMISD, mach_op_none(), xmm0, xmm1);

    MachineOperand dst8 = mach_op_vreg(inst->dest, 8);

    // اختيار setcc؛ مع معالجة unordered عند الحاجة.
    switch (inst->cmp_pred)
    {
    case IR_CMP_EQ:
        isel_emit(ctx, MACH_SETE, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETNP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_AND, dst8, dst8, t8);
        }
        break;

    case IR_CMP_NE:
        isel_emit(ctx, MACH_SETNE, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_OR, dst8, dst8, t8);
        }
        break;

    case IR_CMP_LT:
        isel_emit(ctx, MACH_SETB, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETNP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_AND, dst8, dst8, t8);
        }
        break;

    case IR_CMP_LE:
        isel_emit(ctx, MACH_SETBE, dst8, mach_op_none(), mach_op_none());
        {
            int t = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand t8 = mach_op_vreg(t, 8);
            isel_emit(ctx, MACH_SETNP, t8, mach_op_none(), mach_op_none());
            isel_emit(ctx, MACH_AND, dst8, dst8, t8);
        }
        break;

    case IR_CMP_GT:
        isel_emit(ctx, MACH_SETA, dst8, mach_op_none(), mach_op_none());
        break;

    case IR_CMP_GE:
        isel_emit(ctx, MACH_SETAE, dst8, mach_op_none(), mach_op_none());
        break;

    default:
        isel_emit(ctx, MACH_SETE, dst8, mach_op_none(), mach_op_none());
        break;
    }

    MachineOperand dst64 = mach_op_vreg(inst->dest, 64);
    MachineInst *mi = isel_emit(ctx, MACH_MOVZX, dst64, dst8, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية السالب (negation).
 *
 * النمط: %dst = سالب type operand
 * →  mov vreg_dst, operand
 *    neg vreg_dst
 */
static void isel_lower_neg(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand src = isel_lower_value(ctx, inst->operands[0]);

    isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    MachineInst *mi = isel_emit(ctx, MACH_NEG, dst, dst, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عملية تخصيص المكدس (alloca).
 *
 * النمط: %dst = حجز type
 * →  lea vreg_dst, [rbp - offset]
 *
 * نحسب الإزاحة بناءً على حجم النوع ونضيفها لحجم المكدس.
 */
