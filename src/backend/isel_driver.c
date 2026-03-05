static MachineBlock *isel_lower_block(ISelCtx *ctx, IRBlock *ir_block)
{
    if (!ir_block)
        return NULL;

    MachineBlock *mblock = mach_block_new(ir_block->label, ir_block->id);
    if (!mblock)
        return NULL;

    ctx->ir_block = ir_block;
    ctx->mblock = mblock;

    // إصدار تسمية الكتلة
    MachineInst *label = mach_inst_new(MACH_LABEL, mach_op_label(ir_block->id),
                                       mach_op_none(), mach_op_none());
    if (label)
    {
        label->comment = ir_block->label;
        mach_block_append(mblock, label);
    }

    // خفض كل تعليمة في الكتلة
    IRInst *inst = ir_block->first;
    while (inst)
    {
        ctx->ir_inst = inst;

        // نمط النداء_الذيلي: CALL ثم RET مباشرة.
        if (inst->op == IR_OP_CALL)
        {
            IRInst *next = inst->next;
            if (next && isel_is_tailcall_pair(ctx, inst, next))
            {
                if (isel_lower_tailcall(ctx, inst))
                {
                    inst = next->next; // تخطَّ RET
                    continue;
                }
            }
        }

        isel_lower_inst(ctx, inst);
        inst = inst->next;
    }

    ctx->ir_inst = NULL;

    // نسخ معلومات الخلفاء
    mblock->succ_count = ir_block->succ_count;
    // (سيتم ربط الخلفاء بالكتل الآلية لاحقاً)

    return mblock;
}

// ============================================================================
// خفض دالة IR
// ============================================================================

/**
 * @brief خفض دالة IR إلى دالة آلية.
 */
static MachineFunc *isel_lower_func(ISelCtx *ctx, IRFunc *ir_func)
{
    if (!ir_func)
        return NULL;

    MachineFunc *mfunc = mach_func_new(ir_func->name);
    if (!mfunc)
        return NULL;

    mfunc->is_prototype = ir_func->is_prototype;
    mfunc->param_count = ir_func->param_count;

    // نبدأ عداد السجلات الافتراضية من حيث توقف IR
    mfunc->next_vreg = ir_func->next_reg;

    ctx->ir_func = ir_func;
    ctx->mfunc = mfunc;

    // تخطي النماذج الأولية
    if (ir_func->is_prototype)
        return mfunc;

    // تحليل الحلقات لاستخدامه في تحسينات داخل isel.
    ctx->loop_info = ir_loop_analyze_func(ir_func);

    // خفض كل كتلة
    for (IRBlock *block = ir_func->blocks; block; block = block->next)
    {
        MachineBlock *mblock = isel_lower_block(ctx, block);
        if (mblock)
        {
            mach_func_add_block(mfunc, mblock);
        }
    }

    if (ctx->loop_info)
    {
        ir_loop_info_free(ctx->loop_info);
        ctx->loop_info = NULL;
    }

    // ──────────────────────────────────────────────────────────────────────
    // ربط CFG: خلفاء الكتل الآلية (MachineBlock.succs)
    //
    // ملاحظة مهمة (v0.3.2.6.4): تحليل الحيوية في regalloc يعتمد على succs[]
    // لحساب live_out عبر الحواف، بما فيها حواف الرجوع في الحلقات (back-edges).
    // كان لدينا سابقاً succ_count فقط بدون ربط succs، مما يجعل liveness عبر
    // الحلقات خاطئاً تحت ضغط سجلات عالٍ.
    // ──────────────────────────────────────────────────────────────────────
    {
        int max_id = -1;
        for (IRBlock *b = ir_func->blocks; b; b = b->next)
        {
            if (b->id > max_id)
                max_id = b->id;
        }

        if (max_id >= 0)
        {
            MachineBlock **by_id = (MachineBlock **)calloc((size_t)(max_id + 1), sizeof(MachineBlock *));
            if (by_id)
            {
                for (MachineBlock *mb = mfunc->blocks; mb; mb = mb->next)
                {
                    if (mb->id >= 0 && mb->id <= max_id)
                        by_id[mb->id] = mb;
                }

                for (IRBlock *ib = ir_func->blocks; ib; ib = ib->next)
                {
                    if (ib->id < 0 || ib->id > max_id)
                        continue;
                    MachineBlock *mb = by_id[ib->id];
                    if (!mb)
                        continue;

                    mb->succ_count = 0;
                    mb->succs[0] = NULL;
                    mb->succs[1] = NULL;

                    for (int s = 0; s < ib->succ_count && s < 2; s++)
                    {
                        IRBlock *succ = ib->succs[s];
                        if (!succ)
                            continue;
                        if (succ->id < 0 || succ->id > max_id)
                            continue;
                        MachineBlock *msucc = by_id[succ->id];
                        if (!msucc)
                            continue;
                        mb->succs[mb->succ_count++] = msucc;
                    }
                }

                free(by_id);
            }
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // نسخ معاملات الدالة من سجلات ABI إلى سجلات المعاملات الافتراضية
    // ──────────────────────────────────────────────────────────────────────
    if (ir_func->param_count > 0 && mfunc->entry)
    {
        // نُنشئ تعليمات النسخ ونحشرها في بداية كتلة الدخول
        const BaaCallingConv *cc = isel_cc_or_default(ctx);
        bool is_win = (ctx && ctx->target && ctx->target->kind == BAA_TARGET_X86_64_WINDOWS);
        int gpr_max = (cc && cc->int_arg_reg_count > 0) ? cc->int_arg_reg_count : 4;
        int xmm_max = is_win ? 4 : 8;

        typedef enum { IN_GPR, IN_XMM, IN_STACK } InLocKind;
        typedef struct { InLocKind kind; int idx; } InLoc;

        InLoc *locs = (InLoc *)calloc((size_t)ir_func->param_count, sizeof(InLoc));
        if (!locs)
            return mfunc;

        int gpr_i = 0;
        int xmm_i = 0;
        int stack_i = 0;

        for (int i = 0; i < ir_func->param_count; i++)
        {
            IRType *pt = ir_func->params ? ir_func->params[i].type : NULL;
            bool is_f64 = (pt && pt->kind == IR_TYPE_F64);

            if (is_win)
            {
                if (i < 4)
                {
                    locs[i] = (InLoc){is_f64 ? IN_XMM : IN_GPR, i};
                }
                else
                {
                    locs[i] = (InLoc){IN_STACK, stack_i++};
                }
            }
            else
            {
                if (is_f64)
                {
                    if (xmm_i < xmm_max)
                        locs[i] = (InLoc){IN_XMM, xmm_i++};
                    else
                        locs[i] = (InLoc){IN_STACK, stack_i++};
                }
                else
                {
                    if (gpr_i < gpr_max)
                        locs[i] = (InLoc){IN_GPR, gpr_i++};
                    else
                        locs[i] = (InLoc){IN_STACK, stack_i++};
                }
            }
        }

        // نبني سلسلة التعليمات المؤقتة
        MachineInst *param_first = NULL;
        MachineInst *param_last = NULL;

        for (int i = 0; i < ir_func->param_count; i++)
        {
            MachineInst *mi = NULL;
            IRType *pt = ir_func->params ? ir_func->params[i].type : NULL;
            MachineOperand param_dst = mach_op_vreg(ir_func->params[i].reg, 64);

            if (locs[i].kind == IN_GPR)
            {
                MachineOperand abi_reg = mach_op_vreg(isel_abi_arg_vreg(cc, locs[i].idx), 64);
                mi = mach_inst_new(MACH_MOV, param_dst, abi_reg, mach_op_none());
            }
            else if (locs[i].kind == IN_XMM)
            {
                (void)pt;
                MachineOperand xmm = mach_op_xmm(locs[i].idx);
                mi = mach_inst_new(MACH_MOV, param_dst, xmm, mach_op_none());
            }
            else
            {
                // معاملات المكدس: تُقرأ من مساحة المستدعي فوق return address.
                // ملاحظة: على Windows توجد shadow space (32) بين home slots ومعاملات المكدس.
                int shadow = (cc) ? cc->shadow_space_bytes : 0;
                int32_t off = (int32_t)(16 + shadow + locs[i].idx * 8);
                MachineOperand mem = mach_op_mem(-1, off, 64);
                mi = mach_inst_new(MACH_LOAD, param_dst, mem, mach_op_none());
            }

            if (!mi)
                continue;

            if (!param_first)
            {
                param_first = mi;
                param_last = mi;
            }
            else
            {
                param_last->next = mi;
                mi->prev = param_last;
                param_last = mi;
            }
        }

        // حشر السلسلة في بداية كتلة الدخول
        if (param_first && mfunc->entry->first)
        {
            param_last->next = mfunc->entry->first;
            mfunc->entry->first->prev = param_last;
            mfunc->entry->first = param_first;
        }
        else if (param_first)
        {
            mfunc->entry->first = param_first;
            mfunc->entry->last = param_last;
        }

        free(locs);
    }

    return mfunc;
}

// ============================================================================
// نقطة الدخول الرئيسية لاختيار التعليمات
// ============================================================================

MachineModule *isel_run_ex(IRModule *ir_module, bool enable_tco, const BaaTarget* target)
{
    if (!ir_module)
        return NULL;

    MachineModule *mmod = mach_module_new(ir_module->name);
    if (!mmod)
        return NULL;

    // نسخ مراجع المتغيرات العامة وجدول النصوص
    mmod->globals = ir_module->globals;
    mmod->global_count = ir_module->global_count;
    mmod->strings = ir_module->strings;
    mmod->string_count = ir_module->string_count;
    mmod->baa_strings = ir_module->baa_strings;
    mmod->baa_string_count = ir_module->baa_string_count;

    ISelCtx ctx = {0};
    ctx.mmod = mmod;
    ctx.ir_module = ir_module;
    ctx.enable_tco = enable_tco;
    ctx.target = target ? target : baa_target_builtin_windows_x86_64();

    // خفض كل دالة
    for (IRFunc *func = ir_module->funcs; func; func = func->next)
    {
        MachineFunc *mfunc = isel_lower_func(&ctx, func);
        if (mfunc)
        {
            mach_module_add_func(mmod, mfunc);
        }
    }

    if (ctx.had_error)
    {
        mach_module_free(mmod);
        return NULL;
    }

    return mmod;
}

MachineModule *isel_run(IRModule *ir_module)
{
    return isel_run_ex(ir_module, false, baa_target_builtin_windows_x86_64());
}

// ============================================================================
// طباعة تمثيل الآلة (للتنقيح)
// ============================================================================

const char *mach_op_to_string(MachineOp op)
{
    switch (op)
    {
    case MACH_ADD:
        return "add";
    case MACH_SUB:
        return "sub";
    case MACH_IMUL:
        return "imul";
    case MACH_SHL:
        return "shl";
    case MACH_SHR:
        return "shr";
    case MACH_SAR:
        return "sar";
    case MACH_IDIV:
        return "idiv";
    case MACH_DIV:
        return "div";
    case MACH_NEG:
        return "neg";
    case MACH_CQO:
        return "cqo";
    case MACH_ADDSD:
        return "addsd";
    case MACH_SUBSD:
        return "subsd";
    case MACH_MULSD:
        return "mulsd";
    case MACH_DIVSD:
        return "divsd";
    case MACH_UCOMISD:
        return "ucomisd";
    case MACH_XORPD:
        return "xorpd";
    case MACH_CVTSI2SD:
        return "cvtsi2sd";
    case MACH_CVTTSD2SI:
        return "cvttsd2si";
    case MACH_MOV:
        return "mov";
    case MACH_LEA:
        return "lea";
    case MACH_LOAD:
        return "load";
    case MACH_STORE:
        return "store";
    case MACH_CMP:
        return "cmp";
    case MACH_TEST:
        return "test";
    case MACH_SETE:
        return "sete";
    case MACH_SETNE:
        return "setne";
    case MACH_SETG:
        return "setg";
    case MACH_SETL:
        return "setl";
    case MACH_SETGE:
        return "setge";
    case MACH_SETLE:
        return "setle";
    case MACH_SETA:
        return "seta";
    case MACH_SETB:
        return "setb";
    case MACH_SETAE:
        return "setae";
    case MACH_SETBE:
        return "setbe";
    case MACH_SETP:
        return "setp";
    case MACH_SETNP:
        return "setnp";
    case MACH_MOVZX:
        return "movzx";
    case MACH_MOVSX:
        return "movsx";
    case MACH_AND:
        return "and";
    case MACH_OR:
        return "or";
    case MACH_NOT:
        return "not";
    case MACH_XOR:
        return "xor";
    case MACH_JMP:
        return "jmp";
    case MACH_JE:
        return "je";
    case MACH_JNE:
        return "jne";
    case MACH_CALL:
        return "call";
    case MACH_TAILJMP:
        return "tailjmp";
    case MACH_RET:
        return "ret";
    case MACH_PUSH:
        return "push";
    case MACH_POP:
        return "pop";
    case MACH_NOP:
        return "nop";
    case MACH_LABEL:
        return "label";
    case MACH_COMMENT:
        return "comment";
    case MACH_INLINE_ASM:
        return "inline_asm";
    default:
        return "???";
    }
}

void mach_operand_print(MachineOperand *op, FILE *out)
{
    if (!op || !out)
        return;

    switch (op->kind)
    {
    case MACH_OP_NONE:
        break;
    case MACH_OP_VREG:
        if (op->data.vreg == -2)
        {
            fprintf(out, "%%ret");
        }
        else if (op->data.vreg == -6)
        {
            fprintf(out, "%%rcx");
        }
        else if (op->data.vreg <= -10 && op->data.vreg >= -13)
        {
            // سجلات معاملات ABI
            const char *abi_regs[] = {"%%rcx", "%%rdx", "%%r8", "%%r9"};
            int idx = -(op->data.vreg + 10);
            fprintf(out, "%s", abi_regs[idx]);
        }
        else
        {
            fprintf(out, "%%v%d", op->data.vreg);
        }
        break;
    case MACH_OP_IMM:
        fprintf(out, "$%lld", (long long)op->data.imm);
        break;
    case MACH_OP_MEM:
        if (op->data.mem.base_vreg == -1)
        {
            fprintf(out, "%d(%%rbp)", op->data.mem.offset);
        }
        else
        {
            if (op->data.mem.offset != 0)
                fprintf(out, "%d(%%v%d)", op->data.mem.offset, op->data.mem.base_vreg);
            else
                fprintf(out, "(%%v%d)", op->data.mem.base_vreg);
        }
        break;
    case MACH_OP_LABEL:
        fprintf(out, ".L%d", op->data.label_id);
        break;
    case MACH_OP_GLOBAL:
        fprintf(out, "%s", op->data.name ? op->data.name : "???");
        break;
    case MACH_OP_FUNC:
        fprintf(out, "@%s", op->data.name ? op->data.name : "???");
        break;
    case MACH_OP_XMM:
        fprintf(out, "%%xmm%d", op->data.xmm);
        break;
    }
}

void mach_inst_print(MachineInst *inst, FILE *out)
{
    if (!inst || !out)
        return;

    if (inst->op == MACH_LABEL)
    {
        // طباعة تسمية
        fprintf(out, ".L%d:", inst->dst.data.label_id);
        if (inst->comment)
            fprintf(out, "  # %s", inst->comment);
        fprintf(out, "\n");
        return;
    }

    if (inst->op == MACH_COMMENT)
    {
        if (inst->comment)
            fprintf(out, "    # %s\n", inst->comment);
        return;
    }

    if (inst->op == MACH_INLINE_ASM)
    {
        if (inst->comment)
            fprintf(out, "    %s\n", inst->comment);
        return;
    }

    fprintf(out, "    %-8s", mach_op_to_string(inst->op));

    // طباعة المعاملات
    int has_dst = (inst->dst.kind != MACH_OP_NONE);
    int has_src1 = (inst->src1.kind != MACH_OP_NONE);
    int has_src2 = (inst->src2.kind != MACH_OP_NONE);

    if (has_dst)
    {
        fprintf(out, " ");
        mach_operand_print(&inst->dst, out);
    }
    if (has_src1)
    {
        fprintf(out, "%s", has_dst ? ", " : " ");
        mach_operand_print(&inst->src1, out);
    }
    if (has_src2)
    {
        fprintf(out, ", ");
        mach_operand_print(&inst->src2, out);
    }

    if (inst->comment)
        fprintf(out, "  # %s", inst->comment);
    fprintf(out, "\n");
}

void mach_block_print(MachineBlock *block, FILE *out)
{
    if (!block || !out)
        return;

    for (MachineInst *inst = block->first; inst; inst = inst->next)
    {
        mach_inst_print(inst, out);
    }
}

void mach_func_print(MachineFunc *func, FILE *out)
{
    if (!func || !out)
        return;

    if (func->is_prototype)
    {
        fprintf(out, "# prototype: %s\n", func->name ? func->name : "???");
        return;
    }

    fprintf(out, "# function: %s (stack=%d, vregs=%d)\n",
            func->name ? func->name : "???",
            func->stack_size, func->next_vreg);

    for (MachineBlock *block = func->blocks; block; block = block->next)
    {
        mach_block_print(block, out);
    }
    fprintf(out, "\n");
}

void mach_module_print(MachineModule *module, FILE *out)
{
    if (!module || !out)
        return;

    fprintf(out, "# ============================================\n");
    fprintf(out, "# Machine IR: %s\n", module->name ? module->name : "???");
    fprintf(out, "# Functions: %d\n", module->func_count);
    fprintf(out, "# ============================================\n\n");

    for (MachineFunc *func = module->funcs; func; func = func->next)
    {
        mach_func_print(func, out);
    }
}

