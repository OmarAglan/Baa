static void isel_inline_asm_error(ISelCtx *ctx, IRInst *inst, const char *message)
{
    if (ctx) ctx->had_error = true;
    if (!message) message = "خطأ غير معروف في خفض 'مجمع'.";

    if (inst && inst->src_file)
    {
        fprintf(stderr, "خطأ (ISel): %s:%d:%d: %s\n",
                inst->src_file,
                inst->src_line > 0 ? inst->src_line : 1,
                inst->src_col > 0 ? inst->src_col : 1,
                message);
    }
    else
    {
        fprintf(stderr, "خطأ (ISel): %s\n", message);
    }
}

static bool isel_inline_asm_parse_count(IRValue *v, int *out_value)
{
    if (!out_value) return false;
    *out_value = 0;
    if (!v || v->kind != IR_VAL_CONST_INT) return false;
    if (v->data.const_int < 0 || v->data.const_int > INT32_MAX) return false;
    *out_value = (int)v->data.const_int;
    return true;
}

static bool isel_inline_asm_parse_fixed_vreg(const char *constraint, bool is_output, int *out_vreg)
{
    if (!constraint || !out_vreg) return false;
    *out_vreg = 0;

    const char *p = constraint;
    if (is_output)
    {
        if (p[0] != '=') return false;
        p++;
    }
    else if (p[0] == '=')
    {
        return false;
    }

    if (p[0] == '\0' || p[1] != '\0') return false;

    switch (p[0])
    {
    case 'a':
        *out_vreg = -2; // RAX
        return true;
    case 'd':
        *out_vreg = -5; // RDX
        return true;
    case 'c':
        *out_vreg = -6; // RCX
        return true;
    default:
        return false;
    }
}

static int isel_inline_asm_output_bits(IRValue *addr)
{
    if (!addr || !addr->type || addr->type->kind != IR_TYPE_PTR || !addr->type->data.pointee)
        return 64;
    return isel_type_bits(addr->type->data.pointee);
}

static void isel_inline_asm_store_output(ISelCtx *ctx, IRInst *inst,
                                         IRValue *addr_val, MachineOperand src, int bits)
{
    if (!ctx || !addr_val) return;
    if (bits <= 0) bits = 64;
    src.size_bits = bits;

    MachineOperand addr = isel_lower_value(ctx, addr_val);
    if (addr.kind == MACH_OP_VREG)
    {
        MachineOperand mem = mach_op_mem(addr.data.vreg, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
        return;
    }
    if (addr.kind == MACH_OP_GLOBAL)
    {
        addr.size_bits = bits;
        isel_emit(ctx, MACH_STORE, addr, src, mach_op_none());
        return;
    }
    if (addr.kind == MACH_OP_MEM)
    {
        int tmp_v = mach_func_alloc_vreg(ctx->mfunc);
        MachineOperand tmp = mach_op_vreg(tmp_v, 64);
        isel_emit(ctx, MACH_MOV, tmp, addr, mach_op_none());
        MachineOperand mem = mach_op_mem(tmp_v, 0, bits);
        isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
        return;
    }

    isel_inline_asm_error(ctx, inst, "تعذر خفض عنوان خرج 'مجمع'.");
}

static bool isel_lower_inline_asm_call(ISelCtx *ctx, IRInst *inst)
{
    if (!inst || !inst->call_target) return false;
    if (strcmp(inst->call_target, ISEL_INLINE_ASM_PSEUDO_CALL) != 0) return false;

    if (!inst->call_args || inst->call_arg_count < 3)
    {
        isel_inline_asm_error(ctx, inst, "بيانات 'مجمع' داخل IR غير مكتملة.");
        return true;
    }

    int template_count = 0;
    int output_count = 0;
    int input_count = 0;
    if (!isel_inline_asm_parse_count(inst->call_args[0], &template_count) ||
        !isel_inline_asm_parse_count(inst->call_args[1], &output_count) ||
        !isel_inline_asm_parse_count(inst->call_args[2], &input_count))
    {
        isel_inline_asm_error(ctx, inst, "ترويسة بيانات 'مجمع' غير صالحة.");
        return true;
    }

    int expected = 3 + template_count + (output_count * 2) + (input_count * 2);
    if (inst->call_arg_count != expected)
    {
        isel_inline_asm_error(ctx, inst, "عدد معاملات 'مجمع' في IR غير متطابق.");
        return true;
    }

    InlineAsmOutputBinding *outputs = NULL;
    if (output_count > 0)
    {
        outputs = (InlineAsmOutputBinding *)calloc((size_t)output_count, sizeof(InlineAsmOutputBinding));
        if (!outputs)
        {
            isel_inline_asm_error(ctx, inst, "نفدت الذاكرة أثناء خفض مخارج 'مجمع'.");
            return true;
        }
    }

    int out_idx = 0;
    int idx = 3 + template_count;
    for (int i = 0; i < output_count; i++)
    {
        IRValue *constraint_val = inst->call_args[idx++];
        IRValue *addr_val = inst->call_args[idx++];

        if (!constraint_val || constraint_val->kind != IR_VAL_CONST_STR || !constraint_val->data.const_str.data)
        {
            isel_inline_asm_error(ctx, inst, "قيد خرج 'مجمع' يجب أن يكون نصاً ثابتاً.");
            free(outputs);
            return true;
        }

        int fixed_vreg = 0;
        if (!isel_inline_asm_parse_fixed_vreg(constraint_val->data.const_str.data, true, &fixed_vreg))
        {
            isel_inline_asm_error(ctx, inst, "قيد خرج غير مدعوم في خفض 'مجمع'.");
            free(outputs);
            return true;
        }

        outputs[out_idx].fixed_vreg = fixed_vreg;
        outputs[out_idx].bits = isel_inline_asm_output_bits(addr_val);
        outputs[out_idx].addr = addr_val;
        out_idx++;
    }

    for (int i = 0; i < input_count; i++)
    {
        IRValue *constraint_val = inst->call_args[idx++];
        IRValue *value_val = inst->call_args[idx++];

        if (!constraint_val || constraint_val->kind != IR_VAL_CONST_STR || !constraint_val->data.const_str.data)
        {
            isel_inline_asm_error(ctx, inst, "قيد إدخال 'مجمع' يجب أن يكون نصاً ثابتاً.");
            free(outputs);
            return true;
        }

        int fixed_vreg = 0;
        if (!isel_inline_asm_parse_fixed_vreg(constraint_val->data.const_str.data, false, &fixed_vreg))
        {
            isel_inline_asm_error(ctx, inst, "قيد إدخال غير مدعوم في خفض 'مجمع'.");
            free(outputs);
            return true;
        }

        MachineOperand src = isel_lower_value(ctx, value_val);
        int bits = isel_type_bits(value_val ? value_val->type : NULL);
        if (bits <= 0) bits = 64;
        MachineOperand dst = mach_op_vreg(fixed_vreg, bits);
        isel_emit(ctx, MACH_MOV, dst, src, mach_op_none());
    }

    for (int i = 0; i < template_count; i++)
    {
        IRValue *tv = inst->call_args[3 + i];
        if (!tv || tv->kind != IR_VAL_CONST_STR || !tv->data.const_str.data)
        {
            isel_inline_asm_error(ctx, inst, "سطر 'مجمع' في IR يجب أن يكون نصاً ثابتاً.");
            free(outputs);
            return true;
        }

        MachineInst *mi = isel_emit(ctx, MACH_INLINE_ASM, mach_op_none(), mach_op_none(), mach_op_none());
        if (mi) mi->comment = tv->data.const_str.data;
    }

    for (int i = 0; i < output_count; i++)
    {
        MachineOperand src = mach_op_vreg(outputs[i].fixed_vreg, outputs[i].bits);
        isel_inline_asm_store_output(ctx, inst, outputs[i].addr, src, outputs[i].bits);
    }

    free(outputs);
    return true;
}

/**
 * @brief خفض عملية الاستدعاء.
 *
 * اتفاقية Windows x64 ABI:
 * - المعاملات الأربعة الأولى: RCX, RDX, R8, R9
 * - المعاملات الإضافية: على المكدس
 * - القيمة المرجعة: RAX
 * - Shadow space: 32 بايت
 *
 * النمط: %dst = نداء ret_type @target(args...)
 * →  mov arg_reg1, arg1     (أول 4 معاملات في سجلات)
 *    push argN              (المعاملات الإضافية)
 *    call target
 *    mov vreg_dst, ret_reg  (إذا كانت هناك قيمة مرجعة)
 */
static void isel_lower_call(ISelCtx *ctx, IRInst *inst)
{
    if (!inst)
        return;

    if (isel_lower_inline_asm_call(ctx, inst))
        return;

    const BaaCallingConv *cc = isel_cc_or_default(ctx);
    bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);

    int gpr_max = (cc && cc->int_arg_reg_count > 0) ? cc->int_arg_reg_count : 4;
    int xmm_max = is_win ? 4 : 8;

    typedef enum { ARG_GPR, ARG_XMM, ARG_STACK } ArgLocKind;
    typedef struct { ArgLocKind kind; int idx; } ArgLoc;

    int n = inst->call_arg_count;
    ArgLoc *locs = NULL;
    if (n > 0)
    {
        locs = (ArgLoc *)calloc((size_t)n, sizeof(ArgLoc));
        if (!locs) return;
    }

    int gpr_i = 0;
    int xmm_i = 0;
    int stack_i = 0;

    for (int i = 0; i < n; i++)
    {
        IRType *at = (inst->call_args && inst->call_args[i]) ? inst->call_args[i]->type : NULL;
        bool is_f64 = (at && at->kind == IR_TYPE_F64);

        if (is_win)
        {
            if (i < 4)
            {
                if (is_f64)
                    locs[i] = (ArgLoc){ARG_XMM, i};
                else
                    locs[i] = (ArgLoc){ARG_GPR, i};
            }
            else
            {
                locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
        }
        else
        {
            if (is_f64)
            {
                if (xmm_i < xmm_max)
                    locs[i] = (ArgLoc){ARG_XMM, xmm_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
            else
            {
                if (gpr_i < gpr_max)
                    locs[i] = (ArgLoc){ARG_GPR, gpr_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
        }
    }

    int shadow = (cc) ? cc->shadow_space_bytes : 0;
    int align = (cc && cc->stack_align_bytes > 0) ? cc->stack_align_bytes : 16;

    int call_frame = shadow + stack_i * 8;
    int call_frame_aligned = call_frame;
    if (align > 0 && call_frame_aligned % align != 0)
        call_frame_aligned = ((call_frame_aligned / align) + 1) * align;

    if (call_frame_aligned > 0)
    {
        MachineOperand sp = mach_op_vreg(isel_abi_sp_vreg(), 64);
        MachineOperand imm = mach_op_imm(call_frame_aligned, 64);
        isel_emit_comment(ctx, MACH_SUB, sp, sp, imm, "// حجز إطار نداء");
    }

    // 1) معاملات السجلات
    for (int i = 0; i < n; i++)
    {
        MachineOperand arg = isel_lower_value(ctx, inst->call_args[i]);
        IRType *at = (inst->call_args && inst->call_args[i]) ? inst->call_args[i]->type : NULL;
        bool is_f64 = (at && at->kind == IR_TYPE_F64);

        if (locs[i].kind == ARG_GPR)
        {
            int bits = at ? isel_type_bits(at) : 64;
            MachineOperand param_reg = mach_op_vreg(isel_abi_arg_vreg(cc, locs[i].idx), bits);
            isel_emit(ctx, MACH_MOV, param_reg, arg, mach_op_none());
        }
        else if (locs[i].kind == ARG_XMM)
        {
            (void)is_f64;
            arg = isel_materialize_imm_to_gpr64(ctx, arg);
            MachineOperand xmm = mach_op_xmm(locs[i].idx);
            isel_emit(ctx, MACH_MOV, xmm, arg, mach_op_none());
        }
    }

    // 2) Windows: home/shadow space لمعاملات السجلات (بما فيها XMM)
    if (cc && cc->home_reg_args_on_call && shadow >= 32)
    {
        int n_home = (n < 4) ? n : 4;
        for (int i = 0; i < n_home; i++)
        {
            MachineOperand mem = mach_op_mem(isel_abi_sp_vreg(), (int32_t)(i * 8), 64);
            if (locs[i].kind == ARG_XMM)
            {
                MachineOperand src = mach_op_xmm(i);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
            else
            {
                MachineOperand src = mach_op_vreg(isel_abi_arg_vreg(cc, i), 64);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
        }
    }

    // 3) معاملات المكدس
    for (int i = 0; i < n; i++)
    {
        if (locs[i].kind != ARG_STACK)
            continue;

        int32_t off = (int32_t)(shadow + locs[i].idx * 8);
        MachineOperand mem = mach_op_mem(isel_abi_sp_vreg(), off, 64);

        MachineOperand arg = isel_lower_value(ctx, inst->call_args[i]);
        arg = isel_materialize_imm_to_gpr64(ctx, arg);

        if (arg.kind == MACH_OP_MEM || arg.kind == MACH_OP_GLOBAL)
        {
            int tmp_v = mach_func_alloc_vreg(ctx->mfunc);
            MachineOperand tmp = mach_op_vreg(tmp_v, 64);
            isel_emit(ctx, MACH_MOV, tmp, arg, mach_op_none());
            arg = tmp;
        }

        isel_emit(ctx, MACH_STORE, mem, arg, mach_op_none());
    }

    // استدعاء
    MachineOperand target = mach_op_none();
    if (inst->call_target) {
        target = mach_op_func(inst->call_target);
    } else if (inst->call_callee) {
        target = isel_lower_value(ctx, inst->call_callee);
        target = isel_materialize_imm_to_gpr64(ctx, target);
    } else {
        // احتياطي: لا هدف.
        target = mach_op_func("???");
    }
    MachineInst *calli = isel_emit(ctx, MACH_CALL, mach_op_none(), target, mach_op_none());
    if (calli)
    {
        // SysV varargs rule: عدد سجلات XMM المستخدمة.
        calli->sysv_al = is_win ? -1 : xmm_i;
    }

    // تحرير إطار النداء
    if (call_frame_aligned > 0)
    {
        MachineOperand sp = mach_op_vreg(isel_abi_sp_vreg(), 64);
        MachineOperand imm = mach_op_imm(call_frame_aligned, 64);
        isel_emit_comment(ctx, MACH_ADD, sp, sp, imm, "// تحرير إطار نداء");
    }

    // نقل القيمة المرجعة
    if (inst->dest >= 0 && inst->type && inst->type->kind != IR_TYPE_VOID)
    {
        if (inst->type->kind == IR_TYPE_F64)
        {
            MachineOperand dst = mach_op_vreg(inst->dest, 64);
            MachineOperand xmm0 = mach_op_xmm(0);
            MachineInst *mi = isel_emit_comment(ctx, MACH_MOV, dst, xmm0, mach_op_none(),
                                                "// القيمة المرجعة من XMM0");
            if (mi) mi->ir_reg = inst->dest;
        }
        else
        {
            int bits = isel_type_bits(inst->type);
            MachineOperand dst = mach_op_vreg(inst->dest, bits);
            MachineOperand ret_reg = mach_op_vreg(isel_abi_ret_vreg(cc), bits);
            MachineInst *mi = isel_emit_comment(ctx, MACH_MOV, dst, ret_reg, mach_op_none(),
                                                "// القيمة المرجعة من RAX");
            if (mi) mi->ir_reg = inst->dest;
        }
    }

    free(locs);
}

// -----------------------------------------------------------------------------
// تحسين النداء_الذيلي (Tail Call Optimization) — v0.3.2.7.3
// -----------------------------------------------------------------------------

static int isel_is_tailcall_pair(ISelCtx *ctx, IRInst *call, IRInst *ret)
{
    if (!ctx || !ctx->enable_tco)
        return 0;
    if (!call || !ret)
        return 0;
    if (call->op != IR_OP_CALL)
        return 0;
    if (ret->op != IR_OP_RET)
        return 0;
    if (!call->call_target)
        return 0;

    // النداء_الذيلي مع معاملات مكدس قد يكون ممكناً، لكن سيتم التحقق منه في مرحلة الخفض.

    // 1) رجوع فراغ: يجب أن يكون النداء أيضاً بلا قيمة.
    if (ret->operand_count == 0 || !ret->operands[0])
    {
        if (call->dest >= 0 && call->type && call->type->kind != IR_TYPE_VOID)
            return 0;
        return 1;
    }

    // 2) رجوع بقيمة: يجب أن يرجع نفس سجل نتيجة النداء.
    if (call->dest < 0)
        return 0;
    if (ret->operand_count < 1 || !ret->operands[0])
        return 0;
    if (ret->operands[0]->kind != IR_VAL_REG)
        return 0;
    if (ret->operands[0]->data.reg_num != call->dest)
        return 0;

    return 1;
}

static bool isel_lower_tailcall(ISelCtx *ctx, IRInst *call)
{
    if (!ctx || !call)
        return false;

    const BaaCallingConv *cc = isel_cc_or_default(ctx);
    bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
    int gpr_max = (cc && cc->int_arg_reg_count > 0) ? cc->int_arg_reg_count : 4;
    int xmm_max = is_win ? 4 : 8;

    typedef enum { ARG_GPR, ARG_XMM, ARG_STACK } ArgLocKind;
    typedef struct { ArgLocKind kind; int idx; } ArgLoc;

    int n = call->call_arg_count;
    ArgLoc *locs = NULL;
    if (n > 0)
    {
        locs = (ArgLoc *)calloc((size_t)n, sizeof(ArgLoc));
        if (!locs) return false;
    }

    int gpr_i = 0;
    int xmm_i = 0;
    int stack_i = 0;

    for (int i = 0; i < n; i++)
    {
        IRType *at = (call->call_args && call->call_args[i]) ? call->call_args[i]->type : NULL;
        bool is_f64 = (at && at->kind == IR_TYPE_F64);

        if (is_win)
        {
            if (i < 4)
                locs[i] = (ArgLoc){is_f64 ? ARG_XMM : ARG_GPR, i};
            else
                locs[i] = (ArgLoc){ARG_STACK, stack_i++};
        }
        else
        {
            if (is_f64)
            {
                if (xmm_i < xmm_max)
                    locs[i] = (ArgLoc){ARG_XMM, xmm_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
            else
            {
                if (gpr_i < gpr_max)
                    locs[i] = (ArgLoc){ARG_GPR, gpr_i++};
                else
                    locs[i] = (ArgLoc){ARG_STACK, stack_i++};
            }
        }
    }

    int out_stack = stack_i;
    int in_stack = 0;
    if (ctx->ir_func && ctx->ir_func->param_count > 0)
    {
        int in_gpr = 0;
        int in_xmm = 0;
        int in_stack_i = 0;

        for (int i = 0; i < ctx->ir_func->param_count; i++)
        {
            IRType *pt = ctx->ir_func->params ? ctx->ir_func->params[i].type : NULL;
            bool is_f64 = (pt && pt->kind == IR_TYPE_F64);

            if (is_win)
            {
                if (i >= 4) in_stack_i++;
            }
            else
            {
                if (is_f64)
                {
                    if (in_xmm < xmm_max) in_xmm++;
                    else in_stack_i++;
                }
                else
                {
                    if (in_gpr < gpr_max) in_gpr++;
                    else in_stack_i++;
                }
            }
        }

        in_stack = in_stack_i;
    }

    // لا نُطبّق TCO إذا كانت معاملات المكدس المطلوبة أكبر مما وفره المستدعي لنا.
    if (out_stack > in_stack)
    {
        free(locs);
        return false;
    }

    // تحضير معاملات ABI (نفس مسار CALL لكن بدون call/ret/mov من RAX).
    for (int i = 0; i < n; i++)
    {
        MachineOperand arg = isel_lower_value(ctx, call->call_args[i]);
        IRType *at = (call->call_args && call->call_args[i]) ? call->call_args[i]->type : NULL;

        if (locs[i].kind == ARG_GPR)
        {
            int bits = at ? isel_type_bits(at) : 64;
            MachineOperand param_reg = mach_op_vreg(isel_abi_arg_vreg(cc, locs[i].idx), bits);
            isel_emit(ctx, MACH_MOV, param_reg, arg, mach_op_none());
        }
        else if (locs[i].kind == ARG_XMM)
        {
            arg = isel_materialize_imm_to_gpr64(ctx, arg);
            MachineOperand xmm = mach_op_xmm(locs[i].idx);
            isel_emit(ctx, MACH_MOV, xmm, arg, mach_op_none());
        }
    }

    int shadow = (cc) ? cc->shadow_space_bytes : 0;

    // Windows: home/shadow space لمعاملات السجلات قبل leave.
    // بعد leave تصبح هذه الخانات عند 8(%rsp).
    if (cc && cc->home_reg_args_on_call && shadow >= 32)
    {
        int n_home = (n < 4) ? n : 4;
        for (int i = 0; i < n_home; i++)
        {
            MachineOperand mem = mach_op_mem(-1, (int32_t)(16 + i * 8), 64);
            if (locs[i].kind == ARG_XMM)
            {
                MachineOperand src = mach_op_xmm(locs[i].idx);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
            else
            {
                MachineOperand src = mach_op_vreg(isel_abi_arg_vreg(cc, i), 64);
                isel_emit(ctx, MACH_STORE, mem, src, mach_op_none());
            }
        }
    }

    // معاملات المكدس للنداء_الذيلي: نكتبها في منطقة معاملات الدالة الحالية فوق return address.
    // هذا آمن فقط إذا كانت دالتنا نفسها تتلقى نفس/أكثر عدد معاملات مكدس.
    if (out_stack > 0)
    {
        for (int i = 0; i < n; i++)
        {
            if (locs[i].kind != ARG_STACK)
                continue;

            int32_t off = (int32_t)(16 + shadow + locs[i].idx * 8);
            MachineOperand mem = mach_op_mem(-1, off, 64); // base -1 => RBP

            MachineOperand arg = isel_lower_value(ctx, call->call_args[i]);
            arg = isel_materialize_imm_to_gpr64(ctx, arg);

            if (arg.kind == MACH_OP_MEM || arg.kind == MACH_OP_GLOBAL)
            {
                int tmp_v = mach_func_alloc_vreg(ctx->mfunc);
                MachineOperand tmp = mach_op_vreg(tmp_v, 64);
                isel_emit(ctx, MACH_MOV, tmp, arg, mach_op_none());
                arg = tmp;
            }

            isel_emit(ctx, MACH_STORE, mem, arg, mach_op_none());
        }
    }

    // SysV varargs rule: AL = عدد سجلات XMM المستخدمة.
    if (cc && cc->sysv_set_al_zero_on_call)
    {
        MachineOperand eax = mach_op_vreg(isel_abi_ret_vreg(cc), 32);
        MachineOperand imm = mach_op_imm((int64_t)xmm_i, 32);
        isel_emit(ctx, MACH_MOV, eax, imm, mach_op_none());
    }

    MachineOperand target = mach_op_func(call->call_target);
    isel_emit_comment(ctx, MACH_TAILJMP, mach_op_none(), target, mach_op_none(),
                      "// نداء_ذيلي: قفز إلى الدالة الهدف");

    free(locs);
    return true;
}

/**
 * @brief خفض عملية النسخ (copy).
 *
 * النمط: %dst = نسخ type src
 * →  mov vreg_dst, src
 */
