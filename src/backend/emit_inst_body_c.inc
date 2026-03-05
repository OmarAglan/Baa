        case MACH_NOT:
            fprintf(out, "    not%c ", infer_suffix(inst));
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_XOR:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    xorq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    xor%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        // ================================================================
        // عمليات التحكم (Control Flow)
        // ================================================================
        case MACH_JMP:
            fprintf(out, "    jmp ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_JE:
            fprintf(out, "    je ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_JNE:
            fprintf(out, "    jne ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_INLINE_ASM:
            if (inst->comment && inst->comment[0]) {
                fprintf(out, "    %s\n", inst->comment);
            }
            break;

        // ================================================================
        // استدعاء دالة (CALL)
        // ================================================================
        case MACH_CALL:
        {
            const BaaCallingConv* cc = emit_cc_or_default();

            // SystemV AMD64 varargs rule: AL = number of XMM regs used (here 0).
            // (نطبقها بشكل محافظ على كل النداءات على ELF)
            if (cc && cc->sysv_set_al_zero_on_call)
            {
                int al = inst->sysv_al;
                if (al < 0) al = 0;
                if (al == 0) {
                    fprintf(out, "    xorl %%eax, %%eax\n");
                } else {
                    fprintf(out, "    movl $%d, %%eax\n", al);
                }
            }

            // ملاحظة (v0.3.2.8.5): إدارة إطار النداء (shadow/stack args) أصبحت في ISel.
            fprintf(out, "    call ");
            // النداء غير المباشر في AT&T يتطلب نجمة: call *%reg أو call *mem.
            if (inst->src1.kind != MACH_OP_FUNC) {
                fputc('*', out);
            }
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
        }
            break;

        // ================================================================
        // رجوع (RET) - يتم إصدار الخاتمة عند هذه النقطة
        // ================================================================
        case MACH_RET:
            // الخاتمة تُصدر في emit_func() عند الرجوع
            // هنا نضع علامة فقط
            fprintf(out, "    # ret placeholder\n");
            break;

        // ================================================================
        // عمليات المكدس (Stack)
        // ================================================================
        case MACH_PUSH:
            fprintf(out, "    pushq ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_POP:
            fprintf(out, "    popq ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        default:
            fprintf(out, "    # عملية غير معالجة: %s\n", mach_op_to_string(inst->op));
            break;
