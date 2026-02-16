/**
 * @file ir_licm.c
 * @brief تنفيذ حركة التعليمات غير المتغيرة داخل الحلقة (LICM).
 *
 * ملاحظات مهمة:
 * - هذه النسخة تُطبّق LICM بشكل محافظ جداً:
 *   - ننقل فقط تعليمات "نقية" غير قابلة للمصائد.
 *   - نعتمد على تحليل الحلقات الطبيعية عبر `ir_loop_analyze_func()`.
 */

#include "ir_licm.h"

#include "ir_defuse.h"
#include "ir_loop.h"
#include "ir_mutate.h"

#include <stdlib.h>

IRPass IR_PASS_LICM = {
    .name = "LICM",
    .run = ir_licm_run
};

static int ir_licm_is_safe_op(IROp op) {
    // تعليمات آمنة للتنفيذ في preheader حتى لو لم تُنفذ الحلقة (لا آثار جانبية ولا مصائد).
    // نتجنب: div/mod/call/load/store/alloca...
    switch (op) {
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_NEG:
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_NOT:
        case IR_OP_CMP:
        case IR_OP_CAST:
        case IR_OP_COPY:
            return 1;
        default:
            return 0;
    }
}

static int ir_licm_value_is_invariant(IRValue* v, const IRLoop* loop, const IRDefUse* du) {
    if (!v) return 0;
    if (!loop || !du) return 0;

    if (v->kind == IR_VAL_CONST_INT || v->kind == IR_VAL_CONST_STR) {
        return 1;
    }

    if (v->kind == IR_VAL_GLOBAL || v->kind == IR_VAL_FUNC) {
        return 1;
    }

    if (v->kind != IR_VAL_REG) {
        // قيم الكتل/void غير مناسبة هنا.
        return 0;
    }

    int r = v->data.reg_num;
    if (r < 0 || r >= du->max_reg) return 0;

    if (du->def_is_param && du->def_is_param[r]) {
        return 1;
    }

    IRInst* def = du->def_inst_by_reg ? du->def_inst_by_reg[r] : NULL;
    if (!def || !def->parent) {
        return 0;
    }

    // إن كان تعريف السجل خارج الحلقة، فهو ثابت بالنسبة للحلقة.
    return ir_loop_contains((IRLoop*)loop, def->parent) ? 0 : 1;
}

static int ir_licm_inst_is_invariant(IRInst* inst, const IRLoop* loop, const IRDefUse* du) {
    if (!inst || !loop || !du) return 0;
    if (!ir_licm_is_safe_op(inst->op)) return 0;
    if (inst->dest < 0) return 0;

    // لا ننقل فاي أو المنهيات.
    if (inst->op == IR_OP_PHI || inst->op == IR_OP_BR || inst->op == IR_OP_BR_COND || inst->op == IR_OP_RET) {
        return 0;
    }

    // تحقق المعاملات.
    for (int i = 0; i < inst->operand_count; i++) {
        if (!ir_licm_value_is_invariant(inst->operands[i], loop, du)) {
            return 0;
        }
    }

    // معاملات النداء ليست هنا لأننا لا ننقل CALL.
    return 1;
}

static int ir_licm_loop(IRFunc* func, IRLoop* loop) {
    if (!func || !loop) return 0;

    IRBlock* preheader = ir_loop_preheader(loop);
    if (!preheader) {
        return 0;
    }
    if (!preheader->last) {
        return 0;
    }

    // Def-Use محلي: لا نستخدم كاش func->def_use لتجنب إعادة البناء أثناء النقل.
    IRDefUse* du = ir_defuse_build(func);
    if (!du) return 0;

    int changed = 0;
    int progress = 1;

    while (progress) {
        progress = 0;

        // امسح جميع كتل الحلقة وابحث عن تعليمات يمكن نقلها.
        int nblocks = ir_loop_block_count(loop);
        for (int bi = 0; bi < nblocks; bi++) {
            IRBlock* b = ir_loop_block_at(loop, bi);
            if (!b) continue;

            // لا ننقل أي شيء من رأس الحلقة قبل فاي.
            IRInst* inst = b->first;
            while (inst) {
                IRInst* next = inst->next;

                if (inst->op == IR_OP_PHI) {
                    inst = next;
                    continue;
                }

                // لا ننقل المنهي.
                if (inst == b->last) {
                    inst = next;
                    continue;
                }

                if (ir_licm_inst_is_invariant(inst, loop, du)) {
                    // انقل إلى preheader قبل المنهي.
                    ir_block_remove_inst(b, inst);
                    ir_block_insert_before(preheader, preheader->last, inst);
                    progress = 1;
                    changed = 1;
                }

                inst = next;
            }
        }
    }

    ir_defuse_free(du);
    return changed;
}

static int ir_licm_func(IRFunc* func) {
    if (!func) return 0;
    if (func->is_prototype) return 0;
    if (!func->entry) return 0;

    IRLoopInfo* info = ir_loop_analyze_func(func);
    if (!info) return 0;

    int changed = 0;
    int n = ir_loop_info_count(info);
    for (int i = 0; i < n; i++) {
        IRLoop* loop = ir_loop_info_get(info, i);
        if (!loop) continue;
        changed |= ir_licm_loop(func, loop);
    }

    ir_loop_info_free(info);
    return changed;
}

bool ir_licm_run(IRModule* module) {
    if (!module) return false;

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_licm_func(f);
    }

    return changed ? true : false;
}
