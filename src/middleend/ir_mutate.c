/**
 * @file ir_mutate.c
 * @brief تنفيذ مساعدات تعديل IR.
 */

#include "ir_mutate.h"

#include "ir_defuse.h"

#include <stdlib.h>

void ir_block_insert_before(IRBlock* block, IRInst* before, IRInst* inst) {
    if (!block || !inst) return;

    if (block->parent) {
        ir_func_invalidate_defuse(block->parent);
    }

    // تعيين الأب + رقم تعليمة ثابت عند أول إدراج.
    inst->parent = block;
    if (inst->id < 0 && block->parent && block->parent->next_inst_id >= 0) {
        inst->id = block->parent->next_inst_id++;
    }

    if (!before) {
        // إدراج في نهاية الكتلة
        ir_block_append(block, inst);
        return;
    }

    inst->next = before;
    inst->prev = before->prev;

    if (before->prev) before->prev->next = inst;
    else block->first = inst;

    before->prev = inst;
    block->inst_count++;
}

void ir_block_remove_inst(IRBlock* block, IRInst* inst) {
    if (!block || !inst) return;

    if (block->parent) {
        ir_func_invalidate_defuse(block->parent);
    }

    if (inst->prev) inst->prev->next = inst->next;
    else block->first = inst->next;

    if (inst->next) inst->next->prev = inst->prev;
    else block->last = inst->prev;

    inst->prev = NULL;
    inst->next = NULL;
    inst->parent = NULL;

    if (block->inst_count > 0) block->inst_count--;
}

void ir_block_insert_phi(IRBlock* block, IRInst* phi) {
    if (!block || !phi) return;

    IRInst* pos = block->first;
    while (pos && pos->op == IR_OP_PHI) {
        pos = pos->next;
    }
    ir_block_insert_before(block, pos, phi);
}

void ir_block_free_analysis_caches(IRBlock* block) {
    if (!block) return;

    if (block->preds) {
        free(block->preds);
        block->preds = NULL;
    }
    block->pred_count = 0;
    block->pred_capacity = 0;

    if (block->dom_frontier) {
        free(block->dom_frontier);
        block->dom_frontier = NULL;
    }
    block->dom_frontier_count = 0;

    block->idom = NULL;
}
