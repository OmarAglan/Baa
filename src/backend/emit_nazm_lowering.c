/**
 * @file emit_nazm_lowering.c
 * @brief Private spill-safe operand lowering included by emit_nazm.c.
 *
 * This implementation fragment is intentionally not a standalone translation
 * unit. It lowers validated operations that Nazm cannot encode directly while
 * preserving the reserved scratch-register contracts.
 */

static void nazm_write_any_operand(FILE *out, const MachineOperand *operand)
{
    if (operand->kind == MACH_OP_MEM)
        nazm_write_memory_operand(out, operand);
    else if (operand->kind == MACH_OP_GLOBAL)
        nazm_write_symbolic_memory_operand(out, operand->data.name);
    else
        nazm_write_operand(out, operand);
}

static unsigned nazm_write_move(FILE *out,
                                const MachineOperand *dst,
                                const MachineOperand *src)
{
    if (nazm_operand_is_memory(dst) && src->kind == MACH_OP_IMM)
    {
        MachineOperand scratch = {0};
        scratch.kind = MACH_OP_VREG;
        scratch.size_bits = nazm_operand_bits(dst);
        scratch.data.vreg = dst->kind == MACH_OP_MEM &&
                            dst->data.mem.base_vreg == PHYS_R11
            ? PHYS_RAX
            : PHYS_R11;
        fputs("    انقل ", out);
        nazm_write_operand(out, &scratch);
        fputs("، ", out);
        nazm_write_operand(out, src);
        fputs("\n    انقل ", out);
        nazm_write_any_operand(out, dst);
        fputs("، ", out);
        nazm_write_operand(out, &scratch);
        fputc('\n', out);
        return 2;
    }

    if (nazm_operand_is_memory(dst) && nazm_operand_is_memory(src))
    {
        MachineOperand scratch = {0};
        scratch.kind = MACH_OP_VREG;
        scratch.size_bits = nazm_operand_bits(dst);
        scratch.data.vreg = PHYS_RAX;
        fputs("    انقل ", out);
        nazm_write_operand(out, &scratch);
        fputs("، ", out);
        nazm_write_any_operand(out, src);
        fputs("\n    انقل ", out);
        nazm_write_any_operand(out, dst);
        fputs("، ", out);
        nazm_write_operand(out, &scratch);
        fputc('\n', out);
        return 2;
    }

    fputs("    انقل ", out);
    nazm_write_any_operand(out, dst);
    fputs("، ", out);
    nazm_write_any_operand(out, src);
    fputc('\n', out);
    return 1;
}

static MachineOperand nazm_scratch_operand(PhysReg reg, int bits)
{
    MachineOperand scratch = {0};
    scratch.kind = MACH_OP_VREG;
    scratch.size_bits = bits > 0 ? bits : 64;
    scratch.data.vreg = reg;
    return scratch;
}

static bool nazm_operand_uses_register(const MachineOperand *operand,
                                       PhysReg reg)
{
    if (!operand) return false;
    if (operand->kind == MACH_OP_VREG) return operand->data.vreg == reg;
    if (operand->kind == MACH_OP_MEM)
        return operand->data.mem.base_vreg == reg;
    return false;
}

static unsigned nazm_write_lea(FILE *out,
                               const MachineOperand *dst,
                               const MachineOperand *src)
{
    MachineOperand scratch = nazm_scratch_operand(PHYS_R11, 64);
    const MachineOperand *actual_dst = dst;
    unsigned lines = 0;
    if (dst->kind == MACH_OP_MEM)
        actual_dst = &scratch;

    if (src->kind == MACH_OP_FUNC || src->kind == MACH_OP_GLOBAL)
    {
        fputs("    احسب_عنوان ", out);
        nazm_write_operand(out, actual_dst);
        fputs("، ", out);
        nazm_write_symbolic_memory_operand(out, src->data.name);
    }
    else
    {
        fputs("    احسب_عنوان ", out);
        nazm_write_operand(out, actual_dst);
        fputs("، ", out);
        nazm_write_memory_operand(out, src);
    }
    fputc('\n', out);
    lines += 1;

    if (dst->kind == MACH_OP_MEM)
        lines += nazm_write_move(out, dst, &scratch);
    return lines;
}

static unsigned nazm_write_binary(FILE *out,
                                  const char *mnemonic,
                                  const MachineOperand *dst,
                                  const MachineOperand *src)
{
    int bits = nazm_operand_bits(dst);
    MachineOperand materialized_source = nazm_scratch_operand(
        dst->kind == MACH_OP_MEM || nazm_operand_uses_register(dst, PHYS_R11)
            ? PHYS_RAX
            : PHYS_R11,
        bits);
    const MachineOperand *initial_source = src;
    unsigned prefix_lines = 0;
    if (src->kind == MACH_OP_IMM && bits == 64 &&
        (src->data.imm < INT32_MIN || src->data.imm > INT32_MAX))
    {
        prefix_lines += nazm_write_move(out, &materialized_source, src);
        initial_source = &materialized_source;
    }

    if (dst->kind == MACH_OP_MEM)
    {
        MachineOperand accumulator = nazm_scratch_operand(PHYS_R11, bits);
        MachineOperand preserved_source = nazm_scratch_operand(PHYS_RAX, bits);
        const MachineOperand *actual_source = initial_source;
        unsigned lines = prefix_lines;
        if (nazm_operand_uses_register(initial_source, PHYS_R11))
        {
            lines += nazm_write_move(out, &preserved_source, initial_source);
            actual_source = &preserved_source;
        }
        lines += nazm_write_move(out, &accumulator, dst);
        fputs("    ", out);
        fputs(mnemonic, out);
        fputc(' ', out);
        nazm_write_operand(out, &accumulator);
        fputs("، ", out);
        nazm_write_any_operand(out, actual_source);
        fputc('\n', out);
        lines += 1;
        lines += nazm_write_move(out, dst, &accumulator);
        return lines;
    }

    fputs("    ", out);
    fputs(mnemonic, out);
    fputc(' ', out);
    nazm_write_any_operand(out, dst);
    fputs("، ", out);
    nazm_write_any_operand(out, initial_source);
    fputc('\n', out);
    return prefix_lines + 1;
}

static unsigned nazm_write_comparison(FILE *out,
                                      const char *mnemonic,
                                      const MachineOperand *left,
                                      const MachineOperand *right,
                                      bool memory_rhs_supported)
{
    int bits = nazm_operand_bits(left);
    MachineOperand left_scratch = nazm_scratch_operand(PHYS_R11, bits);
    MachineOperand right_scratch = nazm_scratch_operand(PHYS_RAX, bits);
    const MachineOperand *actual_left = left;
    const MachineOperand *actual_right = right;
    unsigned lines = 0;
    if (right->kind == MACH_OP_IMM && bits == 64 &&
        (right->data.imm < INT32_MIN || right->data.imm > INT32_MAX))
    {
        lines += nazm_write_move(out, &right_scratch, right);
        actual_right = &right_scratch;
    }
    bool materialize_right = right->kind == MACH_OP_MEM &&
        (!memory_rhs_supported ||
         (left->kind == MACH_OP_MEM &&
          nazm_operand_uses_register(right, PHYS_R11)));

    if (materialize_right && left->kind == MACH_OP_MEM &&
        nazm_operand_uses_register(right, PHYS_R11))
    {
        lines += nazm_write_move(out, &right_scratch, right);
        actual_right = &right_scratch;
    }
    if (left->kind == MACH_OP_MEM)
    {
        lines += nazm_write_move(out, &left_scratch, left);
        actual_left = &left_scratch;
    }
    if (materialize_right && actual_right == right)
    {
        lines += nazm_write_move(out, &right_scratch, right);
        actual_right = &right_scratch;
    }

    fputs("    ", out);
    fputs(mnemonic, out);
    fputc(' ', out);
    nazm_write_operand(out, actual_left);
    fputs("، ", out);
    nazm_write_any_operand(out, actual_right);
    fputc('\n', out);
    return lines + 1;
}

static unsigned nazm_write_extension(FILE *out,
                                     const char *mnemonic,
                                     const MachineOperand *dst,
                                     const MachineOperand *src)
{
    int dst_bits = nazm_operand_bits(dst);
    int src_bits = nazm_operand_bits(src);
    MachineOperand source_scratch = nazm_scratch_operand(PHYS_R11, src_bits);
    MachineOperand destination_scratch = nazm_scratch_operand(PHYS_RAX, dst_bits);
    const MachineOperand *actual_src = src;
    const MachineOperand *actual_dst = dst;
    unsigned lines = 0;

    if (src->kind == MACH_OP_MEM)
    {
        lines += nazm_write_move(out, &source_scratch, src);
        actual_src = &source_scratch;
    }
    if (dst->kind == MACH_OP_MEM)
        actual_dst = &destination_scratch;

    fputs("    ", out);
    fputs(mnemonic, out);
    fputc(' ', out);
    nazm_write_operand(out, actual_dst);
    fputs("، ", out);
    nazm_write_operand(out, actual_src);
    fputc('\n', out);
    lines += 1;

    if (dst->kind == MACH_OP_MEM)
        lines += nazm_write_move(out, dst, &destination_scratch);
    return lines;
}

static unsigned nazm_write_shift(FILE *out,
                                 const char *mnemonic,
                                 const MachineOperand *dst,
                                 const MachineOperand *amount)
{
    if (dst->kind != MACH_OP_MEM)
        return nazm_write_binary(out, mnemonic, dst, amount);

    MachineOperand scratch = nazm_scratch_operand(
        PHYS_R11, nazm_operand_bits(dst));
    unsigned lines = nazm_write_move(out, &scratch, dst);
    lines += nazm_write_binary(out, mnemonic, &scratch, amount);
    lines += nazm_write_move(out, dst, &scratch);
    return lines;
}
