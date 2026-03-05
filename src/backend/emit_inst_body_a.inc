        // تسمية كتلة (Block Label)
        // ================================================================
        case MACH_LABEL:
            emit_comment(out, "بداية كتلة: %d", inst->dst.data.label_id);
            fprintf(out, ".LBB_%d_%d:\n", g_emit_current_func_uid, inst->dst.data.label_id);
            break;

        // ================================================================
        // تعليق (Comment)
        // ================================================================
        case MACH_COMMENT:
            if (inst->comment) {
                fprintf(out, "    # %s\n", inst->comment);
            }
            break;

        // ================================================================
        // لا عملية (NOP)
        // ================================================================
        case MACH_NOP:
            // تجاهل تعليمات NOP (عناصر نائبة لـ phi وغيرها)
            break;

        // ================================================================
        // عمليات نقل البيانات (Data Movement)
        // ================================================================
        case MACH_MOV:
            // AT&T: mov src, dst
            if (inst->dst.kind == MACH_OP_NONE) break;
            if (inst->src1.kind == MACH_OP_NONE) break;

            {
                int bits = inst->dst.size_bits;
                if (bits <= 0) bits = inst->src1.size_bits;
                if (bits <= 0) bits = 64;

                // دعم imm64 الحقيقي: movabs للتحميل، وسجل وسيط عند التخزين للذاكرة.
                if (inst->src1.kind == MACH_OP_IMM && bits == 64 && !imm_fits_imm32(inst->src1.data.imm)) {
                    MachineOperand imm = inst->src1;
                    imm.size_bits = 64;

                    if (inst->dst.kind == MACH_OP_VREG) {
                        fprintf(out, "    movabsq ");
                        emit_operand(&imm, out);
                        fprintf(out, ", ");
                        emit_operand(&inst->dst, out);
                        fprintf(out, "\n");
                        break;
                    }

                    if (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL) {
                        MachineOperand tmp = emit_scratch_r11(64);

                        fprintf(out, "    movabsq ");
                        emit_operand(&imm, out);
                        fprintf(out, ", ");
                        emit_operand(&tmp, out);
                        fprintf(out, "\n");

                        fprintf(out, "    movq ");
                        emit_operand(&tmp, out);
                        fprintf(out, ", ");
                        emit_operand(&inst->dst, out);
                        fprintf(out, "\n");
                        break;
                    }
                }
            }

            // معالجة حالة ذاكرة ← ذاكرة (غير صالحة مباشرة في x86-64)
            // نستخدم %rax كسجل مؤقت لأنه محجوز عادةً ولا نخصصه للفترات العادية.
            if (inst->dst.kind == MACH_OP_MEM && inst->src1.kind == MACH_OP_MEM) {
                int bits = inst->dst.size_bits;
                if (bits <= 0) bits = inst->src1.size_bits;
                if (bits <= 0) bits = 64;

                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    mov%c ", infer_suffix(inst));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
                break;
            }

            // التحقق من عدم وجود mov بين نفس السجل
            if (inst->dst.kind == MACH_OP_VREG &&
                inst->src1.kind == MACH_OP_VREG &&
                inst->dst.data.vreg == inst->src1.data.vreg) {
                // mov %reg, %reg - لا حاجة لإصداره
                break;
            }

            fprintf(out, "    mov%c ", infer_suffix(inst));
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات حسابية (Arithmetic)
        // ================================================================
        case MACH_ADD:
            // AT&T: add src, dst (dst = dst + src)
            // src2 هو المعامل الفعلي للإضافة
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    addq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    add%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_SUB:
            if (inst->src2.kind == MACH_OP_IMM && inst->dst.size_bits == 64 && !imm_fits_imm32(inst->src2.data.imm)) {
                MachineOperand imm = inst->src2;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);
                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    subq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            } else {
                fprintf(out, "    sub%c ", infer_suffix(inst));
                emit_operand(&inst->src2, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            break;

        case MACH_IMUL:
            fprintf(out, "    imul%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SHL:
            // AT&T: shl $imm, dst
            fprintf(out, "    shl%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SHR:
            // AT&T: shr $imm|%cl, dst
            fprintf(out, "    shr%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SAR:
            // AT&T: sar $imm|%cl, dst
            fprintf(out, "    sar%c ", infer_suffix(inst));
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_NEG:
            fprintf(out, "    neg%c ", infer_suffix(inst));
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات القسمة (Division)
        // ================================================================
        case MACH_CQO:
            fprintf(out, "    cqo\n");
            break;

        case MACH_IDIV:
            // AT&T: idiv src (يقسم RDX:RAX على src)
            fprintf(out, "    idivq ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_DIV:
            // AT&T: div src (يقسم RDX:RAX على src بدون إشارة)
            fprintf(out, "    divq ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // عمليات العشري (SSE2 f64)
        // ================================================================
        case MACH_ADDSD:
            fprintf(out, "    addsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_SUBSD:
            fprintf(out, "    subsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_MULSD:
            fprintf(out, "    mulsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_DIVSD:
            fprintf(out, "    divsd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_UCOMISD:
            // AT&T: ucomisd src, dst
            fprintf(out, "    ucomisd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->src1, out);
            fprintf(out, "\n");
            break;

        case MACH_XORPD:
            fprintf(out, "    xorpd ");
            emit_operand(&inst->src2, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_CVTSI2SD:
            // AT&T: cvtsi2sd src(int), dst(xmm)
            fprintf(out, "    cvtsi2sd ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        case MACH_CVTTSD2SI:
            // AT&T: cvttsd2si src(xmm), dst(int)
            fprintf(out, "    cvttsd2si ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // تحميل العنوان الفعّال (LEA)
        // ================================================================
        case MACH_LEA:
            // AT&T: lea src, dst  (dst يجب أن يكون سجلاً)
            // إذا كانت الوجهة ذاكرة بسبب التسريب، نستخدم %rax مؤقتاً ثم نخزن.
            if (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL) {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = 64;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    leaq ");
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    movq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
                break;
            }

            fprintf(out, "    leaq ");
            emit_operand(&inst->src1, out);
            fprintf(out, ", ");
            emit_operand(&inst->dst, out);
            fprintf(out, "\n");
            break;

        // ================================================================
        // تحميل من الذاكرة (LOAD)
        // ================================================================
        case MACH_LOAD:
        {
            // LOAD هو mov من مصدر إلى وجهة. قد تتحول الوجهة إلى ذاكرة بعد التسريب.
            bool src_mem = (inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL);
            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);

            int bits = inst->dst.size_bits;
            if (bits <= 0) bits = inst->src1.size_bits;
            if (bits <= 0) bits = 64;

            if (src_mem && dst_mem)
            {
                // تجنب mem->mem: استخدم %rax كمؤقت (RAX غير مخصص لفترات عادية).
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            else
            {
                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
        }
            break;

        // ================================================================
        // تخزين في الذاكرة (STORE)
        // ================================================================
        case MACH_STORE:
        {
            // STORE هو mov src1 -> dst. بعد التسريب قد يصبح src1 ذاكرة.
            bool src_mem = (inst->src1.kind == MACH_OP_MEM || inst->src1.kind == MACH_OP_GLOBAL);
            bool dst_mem = (inst->dst.kind == MACH_OP_MEM || inst->dst.kind == MACH_OP_GLOBAL);

            int bits = inst->src1.size_bits;
            if (bits <= 0) bits = inst->dst.size_bits;
            if (bits <= 0) bits = 64;

            if (dst_mem && inst->src1.kind == MACH_OP_IMM && bits == 64 && !imm_fits_imm32(inst->src1.data.imm)) {
                MachineOperand imm = inst->src1;
                imm.size_bits = 64;
                MachineOperand tmp = emit_scratch_r11(64);

                fprintf(out, "    movabsq ");
                emit_operand(&imm, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    movq ");
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
                break;
            }

            if (src_mem && dst_mem)
            {
                MachineOperand tmp = {0};
                tmp.kind = MACH_OP_VREG;
                tmp.size_bits = bits;
                tmp.data.vreg = PHYS_RAX;

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&tmp, out);
                fprintf(out, "\n");

                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&tmp, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
            else
            {
                fprintf(out, "    mov%c ", size_suffix(bits));
                emit_operand(&inst->src1, out);
                fprintf(out, ", ");
                emit_operand(&inst->dst, out);
                fprintf(out, "\n");
            }
        }
            break;

        // ================================================================
        // عمليات المقارنة (Comparison)
        // ================================================================
