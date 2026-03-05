/**
 * @file ir_cfg_simplify.c
 * @brief تنفيذ تمريرة تبسيط CFG للـ IR (v0.3.2.6.5).
 *
 * ما تقوم به التمريرة:
 * 1) إزالة/تحويل الأفرع الزائدة:
 *    - `قفز_شرط cond, X, X` -> `قفز X`
 *
 * 2) دمج/إزالة الكتل التافهة:
 *    - كتلة B (غير entry) تحتوي فقط على `قفز` إلى S
 *    - ولا تحتوي على أي `فاي`
 *    - نقوم بإعادة توجيه كل سوابق B لتقفز إلى S
 *    - ثم نفصل B من قائمة كتل الدالة (لا تحرير فردي للذاكرة: Arena)
 *
 * 3) أداة تقسيم الحواف الحرجة (محدودة) داخل التمريرة:
 *    - حالياً نستخدمها بشكل داخلي عند الحاجة (وقد نفعلها لاحقاً).
 *
 * ملاحظة:
 * - التمريرة تحافظ على صحة CFG عبر `ir_func_rebuild_preds()` بعد أي تغييرات.
 * - نتجنب إزالة كتل قد تؤثر على `فاي` عبر منع إزالة أي كتلة تحتوي على فاي،
 *   ومنع تجاوز دمج حيث يجب تحديث مداخل فاي (بالسلامة أولاً).
 */

#include "ir_cfg_simplify.h"

#include "ir_analysis.h"
#include "ir_mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_CFG_SIMPLIFY = {
    .name = "تبسيط_CFG",
    .run = ir_cfg_simplify_run
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static int ir_is_terminator(const IRInst* inst) {
    if (!inst) return 0;
    return inst->op == IR_OP_BR || inst->op == IR_OP_BR_COND || inst->op == IR_OP_RET;
}

static IRBlock* ir_block_from_value(IRValue* v) {
    if (!v) return NULL;
    if (v->kind != IR_VAL_BLOCK) return NULL;
    return v->data.block;
}

static int ir_block_has_phi(const IRBlock* block) {
    if (!block) return 0;
    for (const IRInst* inst = block->first; inst; inst = inst->next) {
        if (inst->op == IR_OP_PHI) return 1;
        if (inst->op != IR_OP_PHI) break;
    }
    return 0;
}

static void ir_term_replace_target(IRInst* term, IRBlock* old_target, IRBlock* new_target) {
    if (!term || !old_target || !new_target) return;

    if (term->op == IR_OP_BR) {
        if (term->operand_count < 1) return;
        IRBlock* cur = ir_block_from_value(term->operands[0]);
        if (cur != old_target) return;

        ir_value_free(term->operands[0]);
        term->operands[0] = ir_value_block(new_target);
        return;
    }

    if (term->op == IR_OP_BR_COND) {
        if (term->operand_count < 3) return;

        for (int i = 1; i <= 2; i++) {
            IRBlock* cur = ir_block_from_value(term->operands[i]);
            if (cur != old_target) continue;

            ir_value_free(term->operands[i]);
            term->operands[i] = ir_value_block(new_target);
        }
    }
}

IRBlock* ir_cfg_split_critical_edge(IRFunc* func, IRBlock* pred, IRBlock* succ) {
    if (!func || !pred || !succ) return NULL;
    if (func->is_prototype) return NULL;

    // أعِد بناء preds/succs لضمان الدقة.
    ir_func_rebuild_preds(func);

    int pred_multi = pred->succ_count > 1;
    int succ_multi = succ->pred_count > 1;

    if (!pred_multi || !succ_multi) {
        return succ;
    }

    // تأكد أن الحافة pred->succ موجودة بالفعل.
    int edge_found = 0;
    for (int i = 0; i < pred->succ_count; i++) {
        if (pred->succs[i] == succ) {
            edge_found = 1;
            break;
        }
    }
    if (!edge_found) {
        return NULL;
    }

    char label[128];
    snprintf(label, sizeof(label), "كتلة_تقسيم_حافة_%d_%d", pred->id, succ->id);

    IRBlock* split = ir_func_new_block(func, label);
    if (!split) return NULL;

    // عدّل هدف القفز في المنهي للسابق.
    IRInst* term = pred->last;
    if (!term || !ir_is_terminator(term)) {
        return split;
    }
    ir_term_replace_target(term, succ, split);

    // split: قفز إلى succ
    IRInst* br = ir_inst_br(succ);
    if (br) {
        ir_block_append(split, br);
    }

    // أعد بناء preds بعد التقسيم.
    ir_func_rebuild_preds(func);
    return split;
}

static int ir_cfg_simplify_remove_redundant_brcond(IRFunc* func) {
    if (!func || func->is_prototype) return 0;

    int changed = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b || !b->last) continue;

        IRInst* term = b->last;
        if (term->op != IR_OP_BR_COND) continue;
        if (term->operand_count < 3) continue;

        IRBlock* t = ir_block_from_value(term->operands[1]);
        IRBlock* f = ir_block_from_value(term->operands[2]);
        if (!t || !f) continue;

        if (t != f) continue;

        // استبدال br_cond بـ br
        IRInst* br = ir_inst_br(t);
        if (!br) continue;

        // احتفظ بمعلومات الموقع/الديبغ إن وجدت
        if (term->src_file && term->src_line > 0) {
            ir_inst_set_loc(br, term->src_file, term->src_line, term->src_col);
        }
        if (term->dbg_name) {
            ir_inst_set_dbg_name(br, term->dbg_name);
        }

        // احذف المنهي القديم وأضف الجديد في النهاية
        ir_block_remove_inst(b, term);
        ir_block_append(b, br);

        changed = 1;
    }

    if (changed) {
        ir_func_rebuild_preds(func);
    }

    return changed;
}

static int ir_block_is_trivial_br_only(IRBlock* b, IRBlock** out_target) {
    if (!b || !out_target) return 0;

    if (!b->first || !b->last) return 0;

    // لا نقبل أي `فاي` في الكتلة
    if (ir_block_has_phi(b)) return 0;

    // يجب أن تحتوي تعليمة واحدة فقط (قفز)
    if (b->first != b->last) return 0;
    if (b->last->op != IR_OP_BR) return 0;
    if (b->last->operand_count < 1) return 0;

    IRBlock* target = ir_block_from_value(b->last->operands[0]);
    if (!target) return 0;

    *out_target = target;
    return 1;
}

static int phi_has_entry_for_pred(IRInst* phi, IRBlock* pred) {
    if (!phi || phi->op != IR_OP_PHI || !pred) return 0;
    for (IRPhiEntry* e = phi->phi_entries; e; e = e->next) {
        if (e->block == pred) return 1;
    }
    return 0;
}

static void phi_replace_pred(IRInst* phi, IRBlock* old_pred, IRBlock* new_pred) {
    if (!phi || phi->op != IR_OP_PHI || !old_pred || !new_pred) return;
    for (IRPhiEntry* e = phi->phi_entries; e; e = e->next) {
        if (e->block == old_pred) {
            e->block = new_pred;
        }
    }
}

static int ir_cfg_simplify_remove_trivial_blocks(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    // تأكد من preds/succs حديثة
    ir_func_rebuild_preds(func);

    int changed = 0;

    // نكرر لأن إزالة كتلة قد تخلق فرصاً جديدة
    int progress = 1;
    while (progress) {
        progress = 0;

        IRBlock** link = &func->blocks;
        while (*link) {
            IRBlock* b = *link;
            if (!b) {
                link = &(*link)->next;
                continue;
            }

            // لا تلمس entry
            if (b == func->entry) {
                link = &b->next;
                continue;
            }

            IRBlock* target = NULL;
            if (!ir_block_is_trivial_br_only(b, &target)) {
                link = &b->next;
                continue;
            }

            // لا نحذف self-loop trivial (نادر لكنه قد يكسر تحليلات)
            if (target == b) {
                link = &b->next;
                continue;
            }

            // إن كان التابع يحتوي `فاي`، لا نحذف إلا إذا كانت b تملك سابقاً واحداً
            // ويمكن تحويل مدخلات `فاي` من b إلى هذا السابق دون تكرار.
            IRBlock* sole_pred = NULL;
            if (ir_block_has_phi(target)) {
                if (b->pred_count != 1) {
                    link = &b->next;
                    continue;
                }
                sole_pred = b->preds ? b->preds[0] : NULL;
                if (!sole_pred) {
                    link = &b->next;
                    continue;
                }

                // إذا كان هناك بالفعل مدخل `فاي` للـ sole_pred، لا نحذف لتجنب ازدواجية.
                for (IRInst* inst = target->first; inst && inst->op == IR_OP_PHI; inst = inst->next) {
                    if (phi_has_entry_for_pred(inst, sole_pred)) {
                        sole_pred = NULL;
                        break;
                    }
                }
                if (!sole_pred) {
                    link = &b->next;
                    continue;
                }

                // استبدال مدخلات فاي من b -> sole_pred
                for (IRInst* inst = target->first; inst && inst->op == IR_OP_PHI; inst = inst->next) {
                    phi_replace_pred(inst, b, sole_pred);
                }
            }

            // إعادة توجيه كل سوابق b إلى target
            for (int i = 0; i < b->pred_count; i++) {
                IRBlock* pred = b->preds[i];
                if (!pred || !pred->last || !ir_is_terminator(pred->last)) continue;
                ir_term_replace_target(pred->last, b, target);
            }

            // افصل b من قائمة كتل الدالة
            *link = b->next;
            b->next = NULL;

            if (func->block_count > 0) func->block_count--;
            ir_block_free_analysis_caches(b);

            changed = 1;
            progress = 1;

            // بعد تعديل القفزات، rebuild preds ثم نعيد المسح من البداية
            ir_func_rebuild_preds(func);
            break;
        }
    }

    return changed;
}

static int ir_cfg_simplify_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    int changed = 0;

    changed |= ir_cfg_simplify_remove_redundant_brcond(func);
    changed |= ir_cfg_simplify_remove_trivial_blocks(func);

    if (changed) {
        ir_func_rebuild_preds(func);
    }

    return changed;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool ir_cfg_simplify_run(IRModule* module) {
    if (!module) return false;

    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_cfg_simplify_func(f);
    }

    return changed ? true : false;
}