static void isel_lower_alloca(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    const IRDataLayout *dl = (ctx && ctx->target) ? ctx->target->data_layout : NULL;

    // تعليمة alloca تنتج مؤشراً إلى نوع pointee، لذلك حجم التخصيص يعتمد على pointee وليس على حجم المؤشر.
    IRType *pointee = NULL;
    if (inst->type && inst->type->kind == IR_TYPE_PTR)
        pointee = inst->type->data.pointee;
    if (!pointee)
        pointee = IR_TYPE_I64_T;

    int alloc_size = ir_type_store_size(dl, pointee);
    int align = ir_type_alignment(dl, pointee);

    // تبسيط: نضمن 8 بايت كحد أدنى للتوافق مع معظم أنماط الحمل/الخزن الحالية.
    if (alloc_size < 8) alloc_size = 8;
    if (align < 8) align = 8;

    // محاذاة تصاعدية: offset النهائي يجب أن يحقق محاذاة النوع.
    int s = ctx->mfunc->stack_size + alloc_size;
    int rem = (align > 0) ? (s % align) : 0;
    if (rem != 0) s += (align - rem);
    ctx->mfunc->stack_size = s;

    MachineOperand dst = mach_op_vreg(inst->dest, 64); // المؤشر دائماً 64 بت
    MachineOperand mem = mach_op_mem(-1, -(int32_t)ctx->mfunc->stack_size, 64);
    // base_vreg = -1 يعني RBP (سيُحل في مرحلة إصدار الكود)

    MachineInst *mi = isel_emit(ctx, MACH_LEA, dst, mem, mach_op_none());
    if (mi)
    {
        mi->ir_reg = inst->dest;
        mi->comment = "// حجز مكان في المكدس";
    }
}

/**
 * @brief خفض عملية التحميل (load).
 *
 * النمط: %dst = حمل type, ptr
 * →  mov vreg_dst, [ptr]
 */
static void isel_lower_load(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 1)
        return;

    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);
    MachineOperand ptr = isel_lower_value(ctx, inst->operands[0]);

    // إذا كان المعامل سجلاً، نولد load من الذاكرة
    if (ptr.kind == MACH_OP_VREG)
    {
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        MachineInst *mi = isel_emit(ctx, MACH_LOAD, dst, mem, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
    else if (ptr.kind == MACH_OP_GLOBAL)
    {
        // تحميل من متغير عام
        MachineInst *mi = isel_emit(ctx, MACH_LOAD, dst, ptr, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
    else
    {
        // حالة احتياطية: نقل مباشر
        MachineInst *mi = isel_emit(ctx, MACH_MOV, dst, ptr, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
}

/**
 * @brief خفض عملية التخزين (store).
 *
 * النمط: خزن value, ptr
 * →  mov [ptr], value
 */
static void isel_lower_store(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    MachineOperand val = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand ptr = isel_lower_value(ctx, inst->operands[1]);
    int bits = 64;
    if (inst->operands[0] && inst->operands[0]->type)
        bits = isel_type_bits(inst->operands[0]->type);

    // إذا كانت القيمة فورية، يمكن لـ x86-64 تخزين imm32 مباشرة في الذاكرة.
    // أما imm64 الكامل فلا يمكن تخزينه مباشرة إلى الذاكرة: نحتاج سجل وسيط.
    MachineOperand store_val = val;
    if (val.kind == MACH_OP_IMM && ptr.kind == MACH_OP_VREG)
    {
        bool fits_imm32 = (val.data.imm >= INT32_MIN && val.data.imm <= INT32_MAX);
        if (!(bits == 64 && !fits_imm32))
        {
            MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
            isel_emit(ctx, MACH_STORE, mem, store_val, mach_op_none());
            return;
        }

        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, 64);
        isel_emit(ctx, MACH_MOV, tmp_op, val, mach_op_none());
        store_val = tmp_op;
    }

    if (ptr.kind == MACH_OP_VREG)
    {
        MachineOperand mem = mach_op_mem(ptr.data.vreg, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, store_val, mach_op_none());
    }
    else if (ptr.kind == MACH_OP_GLOBAL)
    {
        isel_emit(ctx, MACH_STORE, ptr, store_val, mach_op_none());
    }
    else
    {
        // حالة احتياطية
        isel_emit(ctx, MACH_STORE, ptr, store_val, mach_op_none());
    }
}

/**
 * @brief خفض عملية المقارنة.
 *
 * النمط: %dst = قارن pred type lhs, rhs
 * →  cmp lhs, rhs
 *    setCC tmp8
 *    movzx vreg_dst, tmp8
 *
 * تحسين: إذا كان المعامل الأيمن قيمة فورية، نستخدمها مباشرة مع cmp.
 */
static void isel_lower_cmp(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || inst->operand_count < 2)
        return;

    // عشري: ucomisd + setcc
    if (inst->operands[0] && inst->operands[0]->type && inst->operands[0]->type->kind == IR_TYPE_F64)
    {
        isel_lower_fcmp(ctx, inst);
        return;
    }

    int bits = isel_type_bits(inst->operands[0] ? inst->operands[0]->type : NULL);
    MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
    MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);

    // إذا كان الطرف الأيسر قيمة فورية، ننقله لسجل مؤقت
    // (cmp لا يقبل فوري كمعامل أول)
    if (lhs.kind == MACH_OP_IMM)
    {
        int tmp = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp_op = mach_op_vreg(tmp, bits);
        isel_emit(ctx, MACH_MOV, tmp_op, lhs, mach_op_none());
        lhs = tmp_op;
    }

    // تنفيذ المقارنة
    isel_emit(ctx, MACH_CMP, mach_op_none(), lhs, rhs);

    // اختيار تعليمة setCC المناسبة
    MachineOp setcc;
    switch (inst->cmp_pred)
    {
    case IR_CMP_EQ:
        setcc = MACH_SETE;
        break;
    case IR_CMP_NE:
        setcc = MACH_SETNE;
        break;
    case IR_CMP_GT:
        setcc = MACH_SETG;
        break;
    case IR_CMP_LT:
        setcc = MACH_SETL;
        break;
    case IR_CMP_GE:
        setcc = MACH_SETGE;
        break;
    case IR_CMP_LE:
        setcc = MACH_SETLE;
        break;
    case IR_CMP_UGT:
        setcc = MACH_SETA;
        break;
    case IR_CMP_ULT:
        setcc = MACH_SETB;
        break;
    case IR_CMP_UGE:
        setcc = MACH_SETAE;
        break;
    case IR_CMP_ULE:
        setcc = MACH_SETBE;
        break;
    default:
        setcc = MACH_SETE;
        break;
    }

    // setCC يكتب بايت واحد
    MachineOperand dst8 = mach_op_vreg(inst->dest, 8);
    isel_emit(ctx, setcc, dst8, mach_op_none(), mach_op_none());

    // توسيع بالأصفار إلى حجم كامل
    MachineOperand dst64 = mach_op_vreg(inst->dest, 64);
    MachineInst *mi = isel_emit(ctx, MACH_MOVZX, dst64, dst8, mach_op_none());
    if (mi)
        mi->ir_reg = inst->dest;
}

/**
 * @brief خفض عمليات منطقية (AND, OR, NOT).
 */
static void isel_lower_logical(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    // العمليات البتية تستخدم عرض النوع الناتج.
    int bits = isel_type_bits(inst->type);
    MachineOperand dst = mach_op_vreg(inst->dest, bits);

    if (inst->op == IR_OP_NOT)
    {
        // نفي: not dst
        if (inst->operand_count < 1)
            return;
        MachineOperand src = isel_lower_value(ctx, inst->operands[0]);
        // ضمان ٦٤ بت للمعامل (بعد التوسيع بالأصفار من المقارنات)
        if (src.kind == MACH_OP_VREG && src.size_bits < bits)
            src.size_bits = bits;
        isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
        MachineInst *mi = isel_emit(ctx, MACH_NOT, dst, dst, mach_op_none());
        if (mi)
            mi->ir_reg = inst->dest;
    }
    else
    {
        // AND أو OR
        if (inst->operand_count < 2)
            return;
        MachineOperand lhs = isel_lower_value(ctx, inst->operands[0]);
        MachineOperand rhs = isel_lower_value(ctx, inst->operands[1]);
        // ضمان عرض النوع للمعاملات.
        if (lhs.kind == MACH_OP_VREG && lhs.size_bits < bits)
            lhs.size_bits = bits;
        if (rhs.kind == MACH_OP_VREG && rhs.size_bits < bits)
            rhs.size_bits = bits;
        MachineOp mop = MACH_OR;
        if (inst->op == IR_OP_AND) mop = MACH_AND;
        else if (inst->op == IR_OP_XOR) mop = MACH_XOR;
        isel_emit(ctx, MACH_MOV, dst, lhs, mach_op_none());
        MachineInst *mi = isel_emit(ctx, mop, dst, dst, rhs);
        if (mi)
            mi->ir_reg = inst->dest;
    }
}

