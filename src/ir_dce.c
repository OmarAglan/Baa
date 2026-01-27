/**
 * @file ir_dce.c
 * @brief IR dead code elimination pass (حذف_الميت).
 * @version 0.3.1.3
 *
 * Removes:
 *  1) Unreachable blocks (no path from entry)
 *  2) Dead SSA instructions (dest never used) for instructions that have no side effects
 *
 * Conservatism:
 *  - Calls are considered side-effecting (never removed) even if their result is unused.
 *  - Stores and terminators are always considered live.
 */

#include "ir_dce.h"
#include "ir_analysis.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    IRBlock* block;
    IRInst* inst;
} DCEWorkItem;

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_DCE = {
    .name = "حذف_الميت",
    .run = ir_dce_run
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static int ir_is_terminator_op(IROp op) {
    return op == IR_OP_BR || op == IR_OP_BR_COND || op == IR_OP_RET;
}

static int ir_inst_has_side_effects(IRInst* inst) {
    if (!inst) return 1;

    switch (inst->op) {
        case IR_OP_STORE:
        case IR_OP_CALL:
            return 1;

        case IR_OP_BR:
        case IR_OP_BR_COND:
        case IR_OP_RET:
            return 1;

        default:
            return 0;
    }
}

static int ir_inst_is_removable_dead(IRInst* inst, const int* uses, int max_reg) {
    if (!inst || !uses) return 0;
    if (inst->dest < 0) return 0; // No produced value
    if (inst->dest >= max_reg) return 0;
    if (uses[inst->dest] != 0) return 0;
    if (ir_inst_has_side_effects(inst)) return 0;
    return 1;
}

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

static int ir_func_max_block_id(IRFunc* func) {
    if (!func) return 0;
    int max_id = -1;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }
    return max_id + 1;
}

static void ir_mark_reachable(IRBlock* entry, unsigned char* reachable, int max_id) {
    if (!entry || !reachable || max_id <= 0) return;
    if (entry->id < 0 || entry->id >= max_id) return;

    IRBlock** stack = (IRBlock**)malloc((size_t)max_id * sizeof(IRBlock*));
    if (!stack) return;

    int sp = 0;
    stack[sp++] = entry;
    reachable[entry->id] = 1;

    while (sp > 0) {
        IRBlock* b = stack[--sp];
        if (!b) continue;

        for (int i = 0; i < b->succ_count; i++) {
            IRBlock* s = b->succs[i];
            if (!s) continue;
            if (s->id < 0 || s->id >= max_id) continue;

            if (!reachable[s->id]) {
                reachable[s->id] = 1;
                stack[sp++] = s;
            }
        }
    }

    free(stack);
}

static void ir_phi_prune_unreachable_entries_in_block(IRBlock* block, const unsigned char* reachable, int max_id) {
    if (!block || !reachable || max_id <= 0) return;

    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op != IR_OP_PHI) continue;

        IRPhiEntry** link = &inst->phi_entries;
        while (*link) {
            IRPhiEntry* e = *link;

            int keep = 1;
            if (!e->block) {
                keep = 0;
            } else if (e->block->id < 0 || e->block->id >= max_id) {
                keep = 0;
            } else if (!reachable[e->block->id]) {
                keep = 0;
            }

            if (keep) {
                link = &e->next;
                continue;
            }

            *link = e->next;
            if (e->value) ir_value_free(e->value);
            free(e);
        }
    }
}

static int ir_func_remove_unreachable_blocks(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    // Ensure CFG edges reflect terminators.
    ir_func_rebuild_preds(func);

    int max_id = ir_func_max_block_id(func);
    if (max_id <= 0) return 0;

    unsigned char* reachable = (unsigned char*)calloc((size_t)max_id, sizeof(unsigned char));
    if (!reachable) return 0;

    ir_mark_reachable(func->entry, reachable, max_id);

    // Prune phi entries referencing unreachable predecessors to avoid dangling pointers.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id >= 0 && b->id < max_id && reachable[b->id]) {
            ir_phi_prune_unreachable_entries_in_block(b, reachable, max_id);
        }
    }

    int changed = 0;

    // Remove unreachable blocks from the linked list.
    IRBlock** link = &func->blocks;
    while (*link) {
        IRBlock* b = *link;

        int is_reachable = 0;
        if (b == func->entry) {
            is_reachable = 1;
        } else if (b->id >= 0 && b->id < max_id && reachable[b->id]) {
            is_reachable = 1;
        }

        if (is_reachable) {
            link = &b->next;
            continue;
        }

        // Unlink and free.
        *link = b->next;
        b->next = NULL;

        if (func->block_count > 0) func->block_count--;
        ir_block_free(b);

        changed = 1;
    }

    // Entry should still be in the list; if the list is empty, clear entry.
    if (!func->blocks) {
        func->entry = NULL;
    }

    if (changed) {
        // Rebuild CFG edges after block removals.
        ir_func_rebuild_preds(func);
    }

    free(reachable);
    return changed;
}

static void ir_count_use_from_value(IRValue* v, int* uses, int max_reg) {
    if (!v || !uses) return;
    if (v->kind != IR_VAL_REG) return;

    int r = v->data.reg_num;
    if (r < 0 || r >= max_reg) return;
    uses[r]++;
}

static void ir_inst_count_uses(IRInst* inst, int* uses, int max_reg) {
    if (!inst || !uses) return;

    for (int i = 0; i < inst->operand_count; i++) {
        ir_count_use_from_value(inst->operands[i], uses, max_reg);
    }

    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            ir_count_use_from_value(inst->call_args[i], uses, max_reg);
        }
    }

    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            ir_count_use_from_value(e->value, uses, max_reg);
        }
    }
}

static void ir_inst_decrement_operand_uses(IRInst* inst, int* uses, int max_reg) {
    if (!inst || !uses) return;

    for (int i = 0; i < inst->operand_count; i++) {
        IRValue* v = inst->operands[i];
        if (v && v->kind == IR_VAL_REG) {
            int r = v->data.reg_num;
            if (r >= 0 && r < max_reg && uses[r] > 0) uses[r]--;
        }
    }

    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            IRValue* v = inst->call_args[i];
            if (v && v->kind == IR_VAL_REG) {
                int r = v->data.reg_num;
                if (r >= 0 && r < max_reg && uses[r] > 0) uses[r]--;
            }
        }
    }

    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            IRValue* v = e->value;
            if (v && v->kind == IR_VAL_REG) {
                int r = v->data.reg_num;
                if (r >= 0 && r < max_reg && uses[r] > 0) uses[r]--;
            }
        }
    }
}

static int ir_func_dce_instructions(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    int max_reg = func->next_reg;
    if (max_reg <= 0) return 0;

    int* uses = (int*)calloc((size_t)max_reg, sizeof(int));
    IRInst** def_inst = (IRInst**)calloc((size_t)max_reg, sizeof(IRInst*));
    IRBlock** def_block = (IRBlock**)calloc((size_t)max_reg, sizeof(IRBlock*));
    unsigned char* queued = (unsigned char*)calloc((size_t)max_reg, sizeof(unsigned char));

    if (!uses || !def_inst || !def_block || !queued) {
        if (uses) free(uses);
        if (def_inst) free(def_inst);
        if (def_block) free(def_block);
        if (queued) free(queued);
        return 0;
    }

    // 1) Build use counts + def maps
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            ir_inst_count_uses(inst, uses, max_reg);

            if (inst->dest >= 0 && inst->dest < max_reg) {
                // SSA should define each reg once; keep the first definition.
                if (!def_inst[inst->dest]) {
                    def_inst[inst->dest] = inst;
                    def_block[inst->dest] = b;
                }
            }
        }
    }

    // 2) Seed worklist with dead removable instructions.
    DCEWorkItem* work = NULL;
    int work_count = 0;
    int work_cap = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->dest < 0 || inst->dest >= max_reg) continue;
            if (queued[inst->dest]) continue;

            if (ir_inst_is_removable_dead(inst, uses, max_reg)) {
                if (work_count >= work_cap) {
                    int new_cap = (work_cap == 0) ? 32 : work_cap * 2;
                    DCEWorkItem* new_work = (DCEWorkItem*)realloc(work, (size_t)new_cap * sizeof(DCEWorkItem));
                    if (!new_work) break;
                    work = new_work;
                    work_cap = new_cap;
                }

                work[work_count].block = b;
                work[work_count].inst = inst;
                work_count++;

                queued[inst->dest] = 1;
            }
        }
    }

    int changed = 0;

    // 3) Process worklist
    while (work_count > 0) {
        DCEWorkItem item = work[--work_count];
        IRInst* inst = item.inst;
        IRBlock* b = item.block;

        if (!inst || !b) continue;
        if (inst->dest < 0 || inst->dest >= max_reg) continue;

        int d = inst->dest;

        // If the def map no longer points at this inst, it was removed already.
        if (def_inst[d] != inst) continue;

        // Re-check liveness conditions (could have become used since we queued it).
        if (!ir_inst_is_removable_dead(inst, uses, max_reg)) continue;

        // Decrement operand uses first (so cascading DCE can trigger).
        ir_inst_decrement_operand_uses(inst, uses, max_reg);

        // Remove this instruction.
        ir_block_remove_inst(b, inst);
        def_inst[d] = NULL;
        def_block[d] = NULL;

        changed = 1;

        // Any operand reg that just became unused might now be removable.
        // We conservatively scan all regs whose use count is 0? Too expensive.
        // Instead, we re-check immediate dependencies by scanning operands again:
        // NOTE: We already decremented uses; now we must discover which regs hit 0.
        // We do this by scanning all regs in the function is expensive; instead,
        // we re-scan the just-removed instruction's operands AGAIN is impossible since it is freed.
        //
        // Therefore, we trigger cascades by checking definitions for regs with uses==0 during decrement.
        // We implement this by re-walking the function's defs for regs with uses==0 would be expensive.
        //
        // Pragmatic compromise: perform a second linear sweep after the worklist empties.
        // This is still DCE-correct and keeps implementation simple and safe.
    }

    // 4) Second sweep iteration until fixed point (cheap enough for v0.3.x)
    // This handles cascades without complex bookkeeping.
    int progress = 1;
    while (progress) {
        progress = 0;

        // Recompute uses/defs fresh each iteration (safe, deterministic).
        memset(uses, 0, (size_t)max_reg * sizeof(int));
        memset(def_inst, 0, (size_t)max_reg * sizeof(IRInst*));
        memset(def_block, 0, (size_t)max_reg * sizeof(IRBlock*));

        for (IRBlock* bb = func->blocks; bb; bb = bb->next) {
            for (IRInst* i = bb->first; i; i = i->next) {
                ir_inst_count_uses(i, uses, max_reg);
                if (i->dest >= 0 && i->dest < max_reg && !def_inst[i->dest]) {
                    def_inst[i->dest] = i;
                    def_block[i->dest] = bb;
                }
            }
        }

        for (IRBlock* bb = func->blocks; bb; bb = bb->next) {
            IRInst* cur = bb->first;
            while (cur) {
                IRInst* next = cur->next;

                if (ir_inst_is_removable_dead(cur, uses, max_reg)) {
                    ir_block_remove_inst(bb, cur);
                    progress = 1;
                    changed = 1;
                }

                cur = next;
            }
        }
    }

    free(work);
    free(uses);
    free(def_inst);
    free(def_block);
    free(queued);

    return changed;
}

bool ir_dce_run(IRModule* module) {
    if (!module) return false;

    int changed = 0;

    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_func_remove_unreachable_blocks(f);
        changed |= ir_func_dce_instructions(f);
    }

    return changed ? true : false;
}