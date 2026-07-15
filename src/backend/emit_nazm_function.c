/**
 * @file emit_nazm_function.c
 * @brief Private function-body writer included by emit_nazm.c.
 *
 * This implementation fragment is intentionally not a standalone translation
 * unit. It shares the validated operand and source-map helpers owned by the
 * canonical Nazm emitter.
 */

static void nazm_write_function(FILE *out,
                                const MachineFunc *func,
                                const BaaTarget *target,
                                unsigned function_id,
                                NazmSourceMapWriter *map)
{
    bool is_arabic_entry = strcmp(func->name, "الرئيسية") == 0;
    PhysReg callee_regs[PHYS_REG_COUNT];
    int callee_count = machine_func_collect_callee_saved(
        func, target, callee_regs, PHYS_REG_COUNT);
    if (callee_count < 0) callee_count = 0;
    fputs("\n.عام ", out);
    fputs(func->name, out);
    fputc('\n', out);
    map->generated_line += 2;
    fputs(func->name, out);
    fputs(":\n", out);
    fputs("    ادفع مؤشر_القاعدة\n", out);
    fputs("    انقل مؤشر_القاعدة، مؤشر_المكدس\n", out);
    map->generated_line += 3;

    int frame_size = nazm_frame_size(func, target, callee_count);
    if (frame_size > 0)
    {
        fputs("    اطرح مؤشر_المكدس، ", out);
        nazm_write_unsigned(out, (uint64_t)frame_size);
        fputc('\n', out);
        map->generated_line += 1;
    }

    for (int i = 0; i < callee_count; ++i)
    {
        int offset = -(func->stack_size + target->cc->shadow_space_bytes + (i + 1) * 8);
        fputs("    انقل [مؤشر_القاعدة", out);
        nazm_write_signed(out, offset);
        fputs("]، ", out);
        fputs(nazm_register_name(callee_regs[i], 64), out);
        fputc('\n', out);
        map->generated_line += 1;
    }

    bool has_return = false;
    for (const MachineBlock *block = func->blocks; block; block = block->next)
    {
        for (const MachineInst *inst = block->first; inst; inst = inst->next)
        {
            map->generated_line += nazm_write_source_span(out, inst);
            unsigned generated_start = map->generated_line + 1;
            unsigned emitted_lines = 0;
            switch (inst->op)
            {
                case MACH_LABEL:
                    nazm_write_local_label(out, function_id, inst->dst.data.label_id);
                    fputs(":\n", out);
                    emitted_lines = 1;
                    break;

                case MACH_MOV:
                case MACH_LOAD:
                case MACH_STORE:
                    emitted_lines = nazm_write_move(
                        out, &inst->dst, &inst->src1);
                    break;

                case MACH_LEA:
                    emitted_lines = nazm_write_lea(
                        out, &inst->dst, &inst->src1);
                    break;

                case MACH_ADD:
                    emitted_lines = nazm_write_binary(
                        out, "أضف", &inst->dst, &inst->src2);
                    break;

                case MACH_SUB:
                    emitted_lines = nazm_write_binary(
                        out, "اطرح", &inst->dst, &inst->src2);
                    break;

                case MACH_AND:
                    emitted_lines = nazm_write_binary(
                        out, "و_بتيا", &inst->dst, &inst->src2);
                    break;

                case MACH_OR:
                    emitted_lines = nazm_write_binary(
                        out, "أو_بتيا", &inst->dst, &inst->src2);
                    break;

                case MACH_XOR:
                    emitted_lines = nazm_write_binary(
                        out, "خالف_بتيا", &inst->dst, &inst->src2);
                    break;

                case MACH_CMP:
                    emitted_lines = nazm_write_comparison(
                        out, "قارن", &inst->src1, &inst->src2, true);
                    break;

                case MACH_TEST:
                    emitted_lines = nazm_write_comparison(
                        out, "اختبر_البتات", &inst->src1, &inst->src2, false);
                    break;

                case MACH_IMUL:
                    if (inst->src2.kind == MACH_OP_IMM)
                    {
                        fputs("    اضرب_موقع ", out);
                        nazm_write_operand(out, &inst->dst);
                        fputs("، ", out);
                        nazm_write_operand(out, &inst->dst);
                        fputs("، ", out);
                        nazm_write_operand(out, &inst->src2);
                        fputc('\n', out);
                        emitted_lines = 1;
                    }
                    else
                    {
                        emitted_lines = nazm_write_binary(
                            out, "اضرب_موقع", &inst->dst, &inst->src2);
                    }
                    break;

                case MACH_SHL:
                    emitted_lines = nazm_write_shift(
                        out, "ازح_يسارا", &inst->dst, &inst->src2);
                    break;

                case MACH_SHR:
                    emitted_lines = nazm_write_shift(
                        out, "ازح_منطقيا_يمينا", &inst->dst, &inst->src2);
                    break;

                case MACH_SAR:
                    emitted_lines = nazm_write_shift(
                        out, "ازح_حسابيا_يمينا", &inst->dst, &inst->src2);
                    break;

                case MACH_NEG:
                case MACH_NOT:
                    fputs(inst->op == MACH_NEG
                              ? "    اعكس_الإشارة "
                              : "    اعكس_البتات ", out);
                    nazm_write_operand(out, &inst->dst);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_IDIV:
                case MACH_DIV:
                    fputs(inst->op == MACH_IDIV
                              ? "    اقسم_موقع "
                              : "    اقسم_غير_موقع ", out);
                    nazm_write_operand(out, &inst->src1);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_CQO:
                    fputs("    وسع_إشارة_القسمة\n", out);
                    emitted_lines = 1;
                    break;

                case MACH_JMP:
                    fputs("    اقفز ", out);
                    nazm_write_local_label(out, function_id, inst->dst.data.label_id);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_JE:
                case MACH_JNE:
                    fputs(inst->op == MACH_JE
                              ? "    اقفز_مساو "
                              : "    اقفز_غير_مساو ", out);
                    nazm_write_local_label(out, function_id, inst->dst.data.label_id);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_SETE: case MACH_SETNE:
                case MACH_SETG: case MACH_SETL:
                case MACH_SETGE: case MACH_SETLE:
                case MACH_SETA: case MACH_SETB:
                case MACH_SETAE: case MACH_SETBE:
                case MACH_SETP: case MACH_SETNP:
                    fputs("    ", out);
                    fputs(nazm_setcc_mnemonic(inst->op), out);
                    fputc(' ', out);
                    nazm_write_operand(out, &inst->dst);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_MOVZX:
                case MACH_MOVSX:
                    emitted_lines = nazm_write_extension(
                        out,
                        inst->op == MACH_MOVZX ? "وسع_بصفر" : "وسع_بإشارة",
                        &inst->dst,
                        &inst->src1);
                    break;

                case MACH_CALL:
                    fputs("    ناد ", out);
                    if (inst->src1.kind == MACH_OP_FUNC)
                        nazm_write_symbol(out, inst->src1.data.name);
                    else
                        nazm_write_operand(out, &inst->src1);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_PUSH:
                    fputs("    ادفع ", out);
                    nazm_write_operand(out, &inst->src1);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_POP:
                    fputs("    اسحب ", out);
                    nazm_write_operand(out, &inst->dst);
                    fputc('\n', out);
                    emitted_lines = 1;
                    break;

                case MACH_RET:
                    emitted_lines = nazm_write_epilogue(
                        out, is_arabic_entry, func, target,
                        callee_regs, callee_count);
                    has_return = true;
                    break;

                case MACH_NOP:
                case MACH_COMMENT:
                    break;

                default:
                    break;
            }
            map->generated_line += emitted_lines;
            if (emitted_lines > 0)
                nazm_source_map_entry(map,
                                      generated_start,
                                      map->generated_line,
                                      inst);
        }
    }

    if (!has_return)
    {
        if (strcmp(func->name, "الرئيسية") == 0)
        {
            fputs("    انقل سجل_المركم، ٠\n", out);
            map->generated_line += 1;
        }
        map->generated_line += nazm_write_epilogue(
            out, is_arabic_entry, func, target, callee_regs, callee_count);
    }
}
