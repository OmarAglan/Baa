/**
 * @file ir_constfold.c
 * @brief IR constant folding pass (طي_الثوابت).
 * @version 0.3.1.2
 *
 * Folds:
 * - Arithmetic: جمع/طرح/ضرب/قسم/باقي when both operands are immediate constants
 * - Comparisons: قارن <pred> when both operands are immediate constants
 *
 * Implementation notes:
 * - This IR currently models constants via IR_VAL_CONST_INT only (int64_t payload).
 * - We replace uses of a folded destination register with a new immediate constant IRValue.
 * - Then we delete the folded instruction from its basic block.
 */

#include "ir_constfold.h"

#include <stdint.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_CONSTFOLD = {
    .name = "طي_الثوابت",
    .run = ir_constfold_run
};

// -----------------------------------------------------------------------------
// Helpers: integer semantics
// -----------------------------------------------------------------------------

static int is_int_type(IRType* t) {
    if (!t) return 0;
    return t->kind == IR_TYPE_I1 ||
           t->kind == IR_TYPE_I8 ||
           t->kind == IR_TYPE_I16 ||
           t->kind == IR_TYPE_I32 ||
           t->kind == IR_TYPE_I64;
}

static int64_t trunc_sext_to_type(int64_t v, IRType* t) {
    if (!t) return v;
    if (!is_int_type(t)) return v;

    // `ص١` is a boolean-like type in Baa IR and is treated as 0/1.
    // Do NOT sign-extend i1 (otherwise 1 becomes -1).
    if (t->kind == IR_TYPE_I1) {
        return v ? 1 : 0;
    }

    int bits = ir_type_bits(t);
    if (bits <= 0 || bits >= 64) return v;

    // Mask to width, then sign-extend.
    uint64_t mask = (1ULL << (unsigned)bits) - 1ULL;
    uint64_t u = ((uint64_t)v) & mask;

    uint64_t sign_bit = 1ULL << (unsigned)(bits - 1);
    if (u & sign_bit) {
        u |= ~mask;
    }

    return (int64_t)u;
}

static int is_const_int(IRValue* v) {
    return v && v->kind == IR_VAL_CONST_INT;
}

static int is_reg(IRValue* v) {
    return v && v->kind == IR_VAL_REG;
}

static IRValue* new_const_for_type(int64_t v, IRType* t) {
    // Preserve IR's type (esp. i1 for comparisons)
    int64_t folded = trunc_sext_to_type(v, t);
    return ir_value_const_int(folded, t);
}

// -----------------------------------------------------------------------------
// Helpers: instruction list manipulation
// -----------------------------------------------------------------------------

static void ir_block_remove_inst(IRBlock* block, IRInst* inst) {
    if (!block || !inst) return;

    if (inst->prev) inst->prev->next = inst->next;
    else block->first = inst->next;

    if (inst->next) inst->next->prev = inst->prev;
    else block->last = inst->prev;

    inst->prev = NULL;
    inst->next = NULL;

    if (block->inst_count > 0) block->inst_count--;
    ir_inst_free(inst);
}

// -----------------------------------------------------------------------------
// Helpers: replace register uses
// -----------------------------------------------------------------------------

static int ir_value_replace_reg_use(IRValue** slot, int reg_num, IRType* folded_type, int64_t folded_value) {
    if (!slot || !*slot) return 0;

    IRValue* v = *slot;
    if (!is_reg(v) || v->data.reg_num != reg_num) return 0;

    // Replace this IRValue with a new constant.
    ir_value_free(v);
    *slot = new_const_for_type(folded_value, folded_type);
    return 1;
}

static int ir_inst_replace_reg_uses(IRInst* inst, int reg_num, IRType* folded_type, int64_t folded_value) {
    if (!inst) return 0;

    int changed = 0;

    // Normal operands
    for (int i = 0; i < inst->operand_count; i++) {
        changed |= ir_value_replace_reg_use(&inst->operands[i], reg_num, folded_type, folded_value);
    }

    // Call args
    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            changed |= ir_value_replace_reg_use(&inst->call_args[i], reg_num, folded_type, folded_value);
        }
    }

    // Phi entries
    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            if (e->value && is_reg(e->value) && e->value->data.reg_num == reg_num) {
                ir_value_free(e->value);
                e->value = new_const_for_type(folded_value, folded_type);
                changed = 1;
            }
        }
    }

    return changed;
}

static int ir_block_replace_reg_uses(IRBlock* block, int reg_num, IRType* folded_type, int64_t folded_value) {
    if (!block) return 0;

    int changed = 0;
    for (IRInst* inst = block->first; inst; inst = inst->next) {
        changed |= ir_inst_replace_reg_uses(inst, reg_num, folded_type, folded_value);
    }
    return changed;
}

static int ir_func_replace_reg_uses(IRFunc* func, int reg_num, IRType* folded_type, int64_t folded_value) {
    if (!func) return 0;

    int changed = 0;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        changed |= ir_block_replace_reg_uses(b, reg_num, folded_type, folded_value);
    }
    return changed;
}

// -----------------------------------------------------------------------------
// Folding logic
// -----------------------------------------------------------------------------

static int try_fold_arith(IROp op, IRType* type, int64_t lhs, int64_t rhs, int64_t* out) {
    if (!out) return 0;

    // For now, use signed 64-bit arithmetic then truncate/sign-extend to result type.
    // Avoid folding division/mod by zero.
    switch (op) {
        case IR_OP_ADD: *out = lhs + rhs; return 1;
        case IR_OP_SUB: *out = lhs - rhs; return 1;
        case IR_OP_MUL: *out = lhs * rhs; return 1;
        case IR_OP_DIV:
            if (rhs == 0) return 0;
            *out = lhs / rhs;
            return 1;
        case IR_OP_MOD:
            if (rhs == 0) return 0;
            *out = lhs % rhs;
            return 1;
        default:
            (void)type;
            return 0;
    }
}

static int eval_cmp(IRCmpPred pred, int64_t lhs, int64_t rhs) {
    switch (pred) {
        case IR_CMP_EQ: return lhs == rhs;
        case IR_CMP_NE: return lhs != rhs;
        case IR_CMP_GT: return lhs > rhs;
        case IR_CMP_LT: return lhs < rhs;
        case IR_CMP_GE: return lhs >= rhs;
        case IR_CMP_LE: return lhs <= rhs;
        default:        return 0;
    }
}

static int ir_try_fold_inst(IRInst* inst, int* out_reg, IRType** out_type, int64_t* out_value) {
    if (!inst || inst->dest < 0) return 0;
    if (!out_reg || !out_type || !out_value) return 0;

    // Only fold integer-producing ops.
    switch (inst->op) {
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD: {
            if (inst->operand_count < 2) return 0;
            IRValue* a = inst->operands[0];
            IRValue* b = inst->operands[1];
            if (!is_const_int(a) || !is_const_int(b)) return 0;

            IRType* result_type = inst->type ? inst->type : IR_TYPE_I64_T;
            if (!is_int_type(result_type)) return 0;

            int64_t v = 0;
            if (!try_fold_arith(inst->op, result_type, a->data.const_int, b->data.const_int, &v)) return 0;

            v = trunc_sext_to_type(v, result_type);

            *out_reg = inst->dest;
            *out_type = result_type;
            *out_value = v;
            return 1;
        }

        case IR_OP_CMP: {
            if (inst->operand_count < 2) return 0;
            IRValue* a = inst->operands[0];
            IRValue* b = inst->operands[1];
            if (!is_const_int(a) || !is_const_int(b)) return 0;

            int cmp = eval_cmp(inst->cmp_pred, a->data.const_int, b->data.const_int);

            *out_reg = inst->dest;
            *out_type = IR_TYPE_I1_T;
            *out_value = (int64_t)(cmp ? 1 : 0);
            return 1;
        }

        default:
            return 0;
    }
}

static int ir_constfold_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;

    int changed = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        IRInst* inst = b->first;
        while (inst) {
            IRInst* next = inst->next;

            int folded_reg = -1;
            IRType* folded_type = NULL;
            int64_t folded_value = 0;

            if (ir_try_fold_inst(inst, &folded_reg, &folded_type, &folded_value)) {
                // Replace uses across the whole module? We'll do function-local replacement here
                // (module-wide wrapper will call per-function, but replacing within the function is enough
                // because virtual regs are function-scoped in this IR).
                changed |= ir_func_replace_reg_uses(func, folded_reg, folded_type, folded_value);

                // Remove the folded instruction itself.
                ir_block_remove_inst(b, inst);
                changed = 1;
            }

            inst = next;
        }
    }

    return changed;
}

bool ir_constfold_run(IRModule* module) {
    if (!module) return false;

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_constfold_func(f);
    }

    // Note: CFG edges are still valid for constant folding, but if later passes rely on rebuilt
    // predecessor lists after instruction deletions, they should call:
    //   ir_module_rebuild_preds(module)
    //
    // We keep constfold side-effect minimal here.

    return changed ? true : false;
}
