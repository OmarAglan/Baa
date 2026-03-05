        case MACH_CMP:
            // AT&T: cmp src2, src1 (يقارن src1 مع src2)
            // cmp لا يدعم ذاكرة-إلى-ذاكرة. إذا كان الطرفان ذاكرة/رمزاً عالمياً، استخدم %rax مؤقتاً.
            if (inst->src2.kind == MACH_OP_IMM && inst->src1.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);

                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    cmpq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->src1, out);
                fprintf(out, "\n");
                break;
            }

            if ((inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL) &&
                (inst->src2.kind == MACH_OP_MEM || inst->src2.kind == MACH_OP_GLOBAL)) {
                int bits = inst->src1.size_bits;
                if (bits <= 0) bits = inst->src2.size_bits;
                if (bits <= 0) bits = 64;

                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    cmp%c ", infer_suffix(inst));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->src1, out);
                fprintf(out, "\n");
                break;
            }

            fprintf(out, "    cmp%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_TEST:
            // AT&T: test src2, src1
            // test لا يدعم ذاكرة-إلى-ذاكرة.
            if ((inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL) &&
                (inst->src2.kind == MACH_OP_MEM || inst->src2.kind == MACH_OP_GLOBAL)) {
                int bits = inst->src1.size_bits;
                if (bits <= 0) bits = inst->src2.size_bits;
                if (bits <= 0) bits = 64;

                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    test%c ", infer_suffix(inst));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->src1, out);
                fprintf(out, "\n");
                break;
            }

            fprintf(out, "    test%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // تعليمات SETcc (تعيين بايت حسب الشرط)
        // ================================================================
        case MACH_SETE:
            fprintf(out, "    sete ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETNE:
            fprintf(out, "    setne ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETG:
            fprintf(out, "    setg ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETL:
            fprintf(out, "    setl ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETGE:
            fprintf(out, "    setge ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETLE:
            fprintf(out, "    setle ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETA:
            fprintf(out, "    seta ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETB:
            fprintf(out, "    setb ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETAE:
            fprintf(out, "    setae ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETBE:
            fprintf(out, "    setbe ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETP:
            fprintf(out, "    setp ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SETNP:
            fprintf(out, "    setnp ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // توسيع بالأصفار (MOVZX)
        // ================================================================
        case MACH_MOVZX:
        {
            int sb = inst->src1.size_bits;
            int db = inst->dst.size_bits;
            if (sb <= 0) sb = 8;
            if (sb == 1) sb = 8;
            if (db <= 0) db = 64;

            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);

            // إذا كانت الوجهة ذاكرة، لا يمكن لـ MOVZX الكتابة إليها مباشرة.
            // نُوسّع إلى سجل مؤقت ثم نخزّن.
            if (dst_mem)
            {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = db;
                tmp.data.vreg = PHYS_RAX;

                // استعمل المسار العادي لكن بوجهة tmp.
                MachineOperand saved_dst = inst->dst;
                MachineOperand dst2 = tmp;
                MachineOperand src2 = inst->src1;
                src2.size_bits = sb;
                dst2.size_bits = db;

                // 32->64: movl إلى %eax يصفّر %rax.
                if (sb == 32 && db == 64)
                {
                    MachineOperand s32 = src2;
                    MachineOperand d32 = dst2;
                    s32.size_bits = 32;
                    d32.size_bits = 32;
                    fprintf(out, "    movl ");
                    emit_operand(&s32, out);
                    fprintf(out, ", ");
                    emit_operand(&d32, out);
                    fprintf(out, "\n");
                }
                else
                {
                    const char* mnem = NULL;
                    if (sb == 8 && db == 16) mnem = "movzbw";
                    else if (sb == 8 && db == 32) mnem = "movzbl";
                    else if (sb == 8 && db == 64) mnem = "movzbq";
                    else if (sb == 16 && db == 32) mnem = "movzwl";
                    else if (sb == 16 && db == 64) mnem = "movzwq";
                    else if (sb == 32 && db == 32) mnem = "movl";
                    else if (sb == 16 && db == 16) mnem = "movw";
                    else if (sb == 8 && db == 8) mnem = "movb";

                    if (!mnem)
                    {
                        fprintf(out, "    mov%c ", size_suffix(db));
                        MachineOperand ss = inst->src1;
                        MachineOperand dd = dst2;
                        ss.size_bits = db;
                        dd.size_bits = db;
                        emit_operand(&ss, out);
                        fprintf(out, ", ");
                        emit_operand(&dd, out);
                        fprintf(out, "\n");
                    }
                    else
                    {
                        fprintf(out, "    %s ", mnem);
                        emit_operand(&src2, out);
                        fprintf(out, ", ");
                        emit_operand(&dst2, out);
                        fprintf(out, "\n");
                    }
                }

                // store tmp -> dst
                fprintf(out, "    mov%c ", size_suffix(db));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&saved_dst, out);
                fprintf(out, "\n");
                break;
            }

            // 32->64: movl يصفّر تلقائياً عند الكتابة إلى سجل 32 بت.
            if (sb == 32 && db == 64)
            {
                MachineOperand src = inst->src1;
                MachineOperand dst = inst->dst;
                src.size_bits = 32;
                dst.size_bits = 32;
                fprintf(out, "    movl ");
                emit_operand(&src, out);
                fprintf(out, ", ");
                emit_operand(&dst, out);
                fprintf(out, "\n");
                break;
            }

            const char* mnem = NULL;
            if (sb == 8 && db == 16) mnem = "movzbw";
            else if (sb == 8 && db == 32) mnem = "movzbl";
            else if (sb == 8 && db == 64) mnem = "movzbq";
            else if (sb == 16 && db == 32) mnem = "movzwl";
            else if (sb == 16 && db == 64) mnem = "movzwq";
            else if (sb == 32 && db == 32) mnem = "movl";
            else if (sb == 16 && db == 16) mnem = "movw";
            else if (sb == 8 && db == 8) mnem = "movb";

            if (!mnem)
            {
                // احتياطي: استخدم mov حسب حجم الوجهة.
                fprintf(out, "    mov%c ", size_suffix(db));
                MachineOperand src = inst->src1;
                MachineOperand dst = inst->dst;
                src.size_bits = db;
                dst.size_bits = db;
                emit_operand(&src, out);
                fprintf(out, ", ");
                emit_operand(&dst, out);
                fprintf(out, "\n");
                break;
            }

            MachineOperand src = inst->src1;
            MachineOperand dst = inst->dst;
            src.size_bits = sb;
            dst.size_bits = db;

            fprintf(out, "    %s ", mnem);
            emit_operand(&src, out);
            fprintf(out, ", ");
            emit_operand(&dst, out);
            fprintf(out, "\n");
        }
            break;

        // ================================================================
        // توسيع بالإشارة (MOVSX)
        // ================================================================
        case MACH_MOVSX:
        {
            int sb = inst->src1.size_bits;
            int db = inst->dst.size_bits;
            if (sb <= 0) sb = 8;
            if (sb == 1) sb = 8;
            if (db <= 0) db = 64;

            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);
            if (dst_mem)
            {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = db;
                tmp.data.vreg = PHYS_RAX;

                MachineOperand saved_dst = inst->dst;
                MachineOperand dst2 = tmp;
                MachineOperand src2 = inst->src1;
                src2.size_bits = sb;
                dst2.size_bits = db;

                const char* mnem = NULL;
                if (sb == 8 && db == 16) mnem = "movsbw";
                else if (sb == 8 && db == 32) mnem = "movsbl";
                else if (sb == 8 && db == 64) mnem = "movsbq";
                else if (sb == 16 && db == 32) mnem = "movswl";
                else if (sb == 16 && db == 64) mnem = "movswq";
                else if (sb == 32 && db == 64) mnem = "movslq";
                else if (sb == 32 && db == 32) mnem = "movl";
                else if (sb == 16 && db == 16) mnem = "movw";
                else if (sb == 8 && db == 8) mnem = "movb";

                if (!mnem)
                {
                    fprintf(out, "    mov%c ", size_suffix(db));
                    MachineOperand ss = inst->src1;
                    MachineOperand dd = dst2;
                    ss.size_bits = db;
                    dd.size_bits = db;
                    emit_operand(&ss, out);
                    fprintf(out, ", ");
                    emit_operand(&dd, out);
                    fprintf(out, "\n");
                }
                else
                {
                    fprintf(out, "    %s ", mnem);
                    emit_operand(&src2, out);
                    fprintf(out, ", ");
                    emit_operand(&dst2, out);
                    fprintf(out, "\n");
                }

                fprintf(out, "    mov%c ", size_suffix(db));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&saved_dst, out);
                fprintf(out, "\n");
                break;
            }

            const char* mnem = NULL;
            if (sb == 8 && db == 16) mnem = "movsbw";
            else if (sb == 8 && db == 32) mnem = "movsbl";
            else if (sb == 8 && db == 64) mnem = "movsbq";
            else if (sb == 16 && db == 32) mnem = "movswl";
            else if (sb == 16 && db == 64) mnem = "movswq";
            else if (sb == 32 && db == 64) mnem = "movslq";
            else if (sb == 32 && db == 32) mnem = "movl";
            else if (sb == 16 && db == 16) mnem = "movw";
            else if (sb == 8 && db == 8) mnem = "movb";

            if (!mnem)
            {
                fprintf(out, "    mov%c ", size_suffix(db));
                MachineOperand src = inst->src1;
                MachineOperand dst = inst->dst;
                src.size_bits = db;
                dst.size_bits = db;
                emit_operand(&src, out);
                fprintf(out, ", ");
                emit_operand(&dst, out);
                fprintf(out, "\n");
                break;
            }

            MachineOperand src = inst->src1;
            MachineOperand dst = inst->dst;
            src.size_bits = sb;
            dst.size_bits = db;

            fprintf(out, "    %s ", mnem);
            emit_operand(&src, out);
            fprintf(out, ", ");
            emit_operand(&dst, out);
            fprintf(out, "\n");
        }
            break;

        // ================================================================
        // عمليات منطقية (Logical)
        // ================================================================
        case MACH_AND:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    andq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    and%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_OR:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    orq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    or%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

