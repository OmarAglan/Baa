/**
 * @file ir_analysis.c
 * @brief IR analysis infrastructure (CFG + dominance) for the Baa compiler.
 * @version 0.3.1.1
 *
 * Implements:
 *  - CFG validation (basic block terminators)
 *  - Predecessor list rebuilding from terminators
 *  - Dominator tree (immediate dominators) + dominance frontier
 */

#include "ir_analysis.h"

#include <stdlib.h>
#include <string.h>

// ============================================================================
// Small helpers
// ============================================================================

static int ir_is_terminator_op(IROp op) {
    return op == IR_OP_BR || op == IR_OP_BR_COND || op == IR_OP_RET;
}

static void ir_clear_preds(IRBlock* block) {
    if (!block) return;

    if (block->preds) {
        free(block->preds);
        block->preds = NULL;
    }
    block->pred_count = 0;
    block->pred_capacity = 0;
}

static void ir_clear_succs(IRBlock* block) {
    if (!block) return;
    block->succs[0] = NULL;
    block->succs[1] = NULL;
    block->succ_count = 0;
}

static void ir_clear_dominance(IRBlock* block) {
    if (!block) return;

    block->idom = NULL;

    if (block->dom_frontier) {
        free(block->dom_frontier);
        block->dom_frontier = NULL;
    }
    block->dom_frontier_count = 0;
}

static IRBlock* ir_block_from_block_value(IRValue* v) {
    if (!v) return NULL;
    if (v->kind != IR_VAL_BLOCK) return NULL;
    return v->data.block;
}

// ============================================================================
// CFG Validation
// ============================================================================

bool ir_func_validate_cfg(IRFunc* func) {
    if (!func) return false;
    if (func->is_prototype) return true;

    if (!func->entry) {
        // Function with body must have entry block.
        return false;
    }

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b->last) {
            // Empty blocks are invalid (no terminator).
            return false;
        }

        if (!ir_is_terminator_op(b->last->op)) {
            return false;
        }

        // Validate terminator operands.
        IRInst* term = b->last;
        switch (term->op) {
            case IR_OP_BR: {
                if (term->operand_count < 1) return false;
                IRBlock* target = ir_block_from_block_value(term->operands[0]);
                if (!target) return false;
                break;
            }

            case IR_OP_BR_COND: {
                if (term->operand_count < 3) return false;

                // Operand 0: condition (should be i1, but allow NULL type for now)
                if (!term->operands[0]) return false;

                IRBlock* if_true = ir_block_from_block_value(term->operands[1]);
                IRBlock* if_false = ir_block_from_block_value(term->operands[2]);
                if (!if_true || !if_false) return false;
                break;
            }

            case IR_OP_RET:
                // RET may have 0 operands (void) or 1 operand.
                if (term->operand_count > 1) return false;
                break;

            default:
                return false;
        }
    }

    return true;
}

bool ir_module_validate_cfg(IRModule* module) {
    if (!module) return false;

    for (IRFunc* f = module->funcs; f; f = f->next) {
        if (!ir_func_validate_cfg(f)) return false;
    }
    return true;
}

// ============================================================================
// Predecessor List Rebuild
// ============================================================================

void ir_func_rebuild_preds(IRFunc* func) {
    if (!func) return;
    if (func->is_prototype) return;

    // 1) Clear edges + dominance caches.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        ir_clear_succs(b);
        ir_clear_preds(b);
        ir_clear_dominance(b);
    }

    // 2) Rebuild succ/pred from terminators.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b->last) continue;

        IRInst* term = b->last;
        if (!ir_is_terminator_op(term->op)) continue;

        if (term->op == IR_OP_BR) {
            IRBlock* target = ir_block_from_block_value(term->operands[0]);
            if (target) {
                ir_block_add_succ(b, target);
            }
            continue;
        }

        if (term->op == IR_OP_BR_COND) {
            IRBlock* if_true = ir_block_from_block_value(term->operands[1]);
            IRBlock* if_false = ir_block_from_block_value(term->operands[2]);

            if (if_true) ir_block_add_succ(b, if_true);
            if (if_false) ir_block_add_succ(b, if_false);
            continue;
        }

        // IR_OP_RET: no successors.
    }

    // Ensure entry's idom can later be set to itself.
    if (func->entry) {
        func->entry->idom = func->entry;
    }
}

void ir_module_rebuild_preds(IRModule* module) {
    if (!module) return;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        ir_func_rebuild_preds(f);
    }
}

// ============================================================================
// Dominator Tree (Cooper-Harvey-Kennedy algorithm)
// ============================================================================

static int ir_func_max_block_id(IRFunc* func) {
    int max_id = -1;
    if (!func) return 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }
    return max_id + 1;
}

static void ir_dfs_postorder(IRBlock* b,
                             int* visited,
                             IRBlock** postorder,
                             int* post_count) {
    if (!b || !visited || !postorder || !post_count) return;
    if (visited[b->id]) return;

    visited[b->id] = 1;

    for (int i = 0; i < b->succ_count; i++) {
        if (b->succs[i]) {
            ir_dfs_postorder(b->succs[i], visited, postorder, post_count);
        }
    }

    postorder[(*post_count)++] = b;
}

// Intersect two idom chains using reverse-postorder numbering.
static IRBlock* ir_intersect_idom(IRBlock* a, IRBlock* b, const int* rpo_num) {
    if (!a || !b) return NULL;

    while (a != b) {
        while (rpo_num[a->id] > rpo_num[b->id]) a = a->idom;
        while (rpo_num[b->id] > rpo_num[a->id]) b = b->idom;

        if (!a || !b) return NULL;
    }

    return a;
}

static void ir_dom_frontier_add(IRBlock* block, IRBlock* frontier, int* cap_by_id) {
    if (!block || !frontier || !cap_by_id) return;

    // Uniqueness check.
    for (int i = 0; i < block->dom_frontier_count; i++) {
        if (block->dom_frontier[i] == frontier) return;
    }

    int id = block->id;
    int cap = cap_by_id[id];

    if (block->dom_frontier_count >= cap) {
        int new_cap = (cap == 0) ? 4 : cap * 2;
        IRBlock** new_arr = (IRBlock**)realloc(block->dom_frontier, (size_t)new_cap * sizeof(IRBlock*));
        if (!new_arr) return;

        block->dom_frontier = new_arr;
        cap_by_id[id] = new_cap;
    }

    block->dom_frontier[block->dom_frontier_count++] = frontier;
}

void ir_func_compute_dominators(IRFunc* func) {
    if (!func) return;
    if (func->is_prototype) return;
    if (!func->entry) return;

    // Ensure CFG edges are consistent with terminators.
    ir_func_rebuild_preds(func);

    int max_id = ir_func_max_block_id(func);
    if (max_id <= 0) return;

    // Build reverse postorder (RPO) over reachable blocks from entry.
    int* visited = (int*)calloc((size_t)max_id, sizeof(int));
    IRBlock** postorder = (IRBlock**)malloc((size_t)max_id * sizeof(IRBlock*));
    if (!visited || !postorder) {
        if (visited) free(visited);
        if (postorder) free(postorder);
        return;
    }

    int post_count = 0;
    ir_dfs_postorder(func->entry, visited, postorder, &post_count);

    // rpo is reverse of postorder.
    IRBlock** rpo = (IRBlock**)malloc((size_t)post_count * sizeof(IRBlock*));
    int* rpo_num = (int*)malloc((size_t)max_id * sizeof(int));
    if (!rpo || !rpo_num) {
        free(visited);
        free(postorder);
        if (rpo) free(rpo);
        if (rpo_num) free(rpo_num);
        return;
    }

    for (int i = 0; i < max_id; i++) rpo_num[i] = -1;

    for (int i = 0; i < post_count; i++) {
        IRBlock* b = postorder[post_count - 1 - i];
        rpo[i] = b;
        if (b) rpo_num[b->id] = i;
    }

    // Initialize idoms:
    // - reachable blocks: NULL (except entry)
    // - entry dominates itself
    for (IRBlock* b = func->blocks; b; b = b->next) {
        b->idom = NULL;
    }
    func->entry->idom = func->entry;

    // Iterative computation
    int changed = 1;
    while (changed) {
        changed = 0;

        for (int i = 1; i < post_count; i++) { // Skip entry at rpo[0]
            IRBlock* b = rpo[i];
            if (!b) continue;

            IRBlock* new_idom = NULL;

            for (int p = 0; p < b->pred_count; p++) {
                IRBlock* pred = b->preds[p];
                if (!pred) continue;

                if (!pred->idom) continue; // Not processed / unreachable

                if (!new_idom) {
                    new_idom = pred;
                } else {
                    new_idom = ir_intersect_idom(pred, new_idom, rpo_num);
                }
            }

            if (new_idom && b->idom != new_idom) {
                b->idom = new_idom;
                changed = 1;
            }
        }
    }

    // Compute dominance frontier.
    // Clear old frontiers and use per-block capacities stored in a side array.
    int* df_cap = (int*)calloc((size_t)max_id, sizeof(int));
    if (!df_cap) {
        free(visited);
        free(postorder);
        free(rpo);
        free(rpo_num);
        return;
    }

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->dom_frontier) {
            free(b->dom_frontier);
            b->dom_frontier = NULL;
        }
        b->dom_frontier_count = 0;
        df_cap[b->id] = 0;
    }

    for (int i = 0; i < post_count; i++) {
        IRBlock* b = rpo[i];
        if (!b) continue;

        if (b->pred_count < 2) continue;

        for (int p = 0; p < b->pred_count; p++) {
            IRBlock* runner = b->preds[p];
            while (runner && runner != b->idom) {
                ir_dom_frontier_add(runner, b, df_cap);

                // Entry has idom = entry; prevent infinite loops.
                if (runner == runner->idom) break;

                runner = runner->idom;
            }
        }
    }

    free(df_cap);
    free(visited);
    free(postorder);
    free(rpo);
    free(rpo_num);
}

void ir_module_compute_dominators(IRModule* module) {
    if (!module) return;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        ir_func_compute_dominators(f);
    }
}