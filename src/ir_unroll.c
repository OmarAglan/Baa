/**
 * @file ir_unroll.c
 * @brief تنفيذ فك الحلقات (Loop Unrolling) بشكل محافظ.
 */

#include "ir_unroll.h"

#include "ir_analysis.h"
#include "ir_loop.h"
#include "ir_mutate.h"
#include "ir_defuse.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// أدوات ثابتات محلية (Local Constant Evaluation)
// ============================================================================

typedef struct {
    unsigned char* known;
    int64_t* value;
    int max_reg;
} IRConstMap;

static IRConstMap ir_constmap_new(int max_reg) {
    IRConstMap m = {0};
    if (max_reg < 0) max_reg = 0;
    m.max_reg = max_reg;
    if (max_reg > 0) {
        m.known = (unsigned char*)calloc((size_t)max_reg, 1);
        m.value = (int64_t*)calloc((size_t)max_reg, sizeof(int64_t));
    }
    return m;
}

static void ir_constmap_free(IRConstMap* m) {
    if (!m) return;
    if (m->known) free(m->known);
    if (m->value) free(m->value);
    m->known = NULL;
    m->value = NULL;
    m->max_reg = 0;
}

static int ir_constmap_get(IRConstMap* m, IRValue* v, int64_t* out) {
    if (!m || !v || !out) return 0;
    if (v->kind == IR_VAL_CONST_INT) {
        *out = v->data.const_int;
        return 1;
    }
    if (v->kind == IR_VAL_REG) {
        int r = v->data.reg_num;
        if (r >= 0 && r < m->max_reg && m->known && m->known[r]) {
            *out = m->value[r];
            return 1;
        }
    }
    return 0;
}

static void ir_constmap_kill(IRConstMap* m, int r) {
    if (!m || !m->known) return;
    if (r < 0 || r >= m->max_reg) return;
    m->known[r] = 0;
}

static void ir_constmap_set(IRConstMap* m, int r, int64_t v) {
    if (!m || !m->known || !m->value) return;
    if (r < 0 || r >= m->max_reg) return;
    m->known[r] = 1;
    m->value[r] = v;
}

static void ir_eval_block_consts(IRBlock* b, IRConstMap* m) {
    if (!b || !m) return;

    for (IRInst* inst = b->first; inst; inst = inst->next) {
        if (inst->dest < 0) continue;
        int d = inst->dest;

        int64_t a = 0;
        int64_t c = 0;

        switch (inst->op) {
            case IR_OP_COPY:
                if (inst->operand_count >= 1 && ir_constmap_get(m, inst->operands[0], &a)) {
                    ir_constmap_set(m, d, a);
                } else {
                    ir_constmap_kill(m, d);
                }
                break;

            case IR_OP_ADD:
                if (inst->operand_count >= 2 &&
                    ir_constmap_get(m, inst->operands[0], &a) &&
                    ir_constmap_get(m, inst->operands[1], &c)) {
                    ir_constmap_set(m, d, a + c);
                } else {
                    ir_constmap_kill(m, d);
                }
                break;

            case IR_OP_SUB:
                if (inst->operand_count >= 2 &&
                    ir_constmap_get(m, inst->operands[0], &a) &&
                    ir_constmap_get(m, inst->operands[1], &c)) {
                    ir_constmap_set(m, d, a - c);
                } else {
                    ir_constmap_kill(m, d);
                }
                break;

            case IR_OP_MUL:
                if (inst->operand_count >= 2 &&
                    ir_constmap_get(m, inst->operands[0], &a) &&
                    ir_constmap_get(m, inst->operands[1], &c)) {
                    ir_constmap_set(m, d, a * c);
                } else {
                    ir_constmap_kill(m, d);
                }
                break;

            case IR_OP_NEG:
                if (inst->operand_count >= 1 && ir_constmap_get(m, inst->operands[0], &a)) {
                    ir_constmap_set(m, d, -a);
                } else {
                    ir_constmap_kill(m, d);
                }
                break;

            case IR_OP_CAST:
                if (inst->operand_count >= 1 && ir_constmap_get(m, inst->operands[0], &a)) {
                    // حالياً: نفترض cast بين أعداد صحيحة لا يغير القيمة (توسيع/تضييق بسيط).
                    ir_constmap_set(m, d, a);
                } else {
                    ir_constmap_kill(m, d);
                }
                break;

            default:
                ir_constmap_kill(m, d);
                break;
        }
    }
}

// ============================================================================
// اكتشاف نمط الحلقة البسيطة (Simple Loop Shape)
// ============================================================================

static IRInst* ir_find_last_def_in_block(IRBlock* b, int reg) {
    if (!b || reg < 0) return NULL;
    IRInst* last = NULL;
    for (IRInst* inst = b->first; inst; inst = inst->next) {
        if (inst == b->last) break;
        if (inst->dest == reg) last = inst;
    }
    return last;
}

static int ir_normalize_cmp(IRInst* cmp, IRCmpPred* out_pred, IRValue** out_lhs, IRValue** out_rhs) {
    if (!cmp || cmp->op != IR_OP_CMP) return 0;
    if (!out_pred || !out_lhs || !out_rhs) return 0;
    if (cmp->operand_count < 2) return 0;

    IRValue* a = cmp->operands[0];
    IRValue* b = cmp->operands[1];
    if (!a || !b) return 0;

    IRCmpPred p = cmp->cmp_pred;

    // إن كان الثابت على اليسار والرج على اليمين، نقلب.
    if (a->kind == IR_VAL_CONST_INT && b->kind == IR_VAL_REG) {
        // swap + pred flip
        IRValue* tmp = a;
        a = b;
        b = tmp;

        switch (p) {
            case IR_CMP_LT: p = IR_CMP_GT; break;
            case IR_CMP_LE: p = IR_CMP_GE; break;
            case IR_CMP_GT: p = IR_CMP_LT; break;
            case IR_CMP_GE: p = IR_CMP_LE; break;
            default: break;
        }
    }

    *out_pred = p;
    *out_lhs = a;
    *out_rhs = b;
    return 1;
}

static int ir_get_trip_count_for_simple_inc1_loop(IRFunc* func,
                                                  IRLoop* loop,
                                                  IRBlock* header,
                                                  IRBlock* preheader,
                                                  int* out_induction_reg,
                                                  int* out_trip) {
    if (out_induction_reg) *out_induction_reg = -1;
    if (out_trip) *out_trip = 0;

    if (!func || !loop || !header || !preheader) return 0;
    if (!header->last || header->last->op != IR_OP_BR_COND) return 0;
    if (header->last->operand_count < 3) return 0;

    // cond reg
    IRValue* cond = header->last->operands[0];
    if (!cond || cond->kind != IR_VAL_REG) return 0;
    int cond_reg = cond->data.reg_num;

    // find defining cmp in header
    IRInst* def = ir_find_last_def_in_block(header, cond_reg);
    if (!def || def->op != IR_OP_CMP) return 0;

    IRCmpPred pred;
    IRValue* lhs;
    IRValue* rhs;
    if (!ir_normalize_cmp(def, &pred, &lhs, &rhs)) return 0;

    // only increasing loops: i < C or i <= C
    if (pred != IR_CMP_LT && pred != IR_CMP_LE) return 0;
    if (!lhs || lhs->kind != IR_VAL_REG) return 0;

    int ind_reg = lhs->data.reg_num;
    if (ind_reg < 0) return 0;

    // bound must be constant (or locally evaluatable constant in header)
    int64_t bound = 0;
    IRConstMap mh = ir_constmap_new(func->next_reg);
    ir_eval_block_consts(header, &mh);
    int bound_ok = ir_constmap_get(&mh, rhs, &bound);
    ir_constmap_free(&mh);
    if (!bound_ok) return 0;

    // init must be constant in preheader
    int64_t init = 0;
    IRConstMap mp = ir_constmap_new(func->next_reg);
    ir_eval_block_consts(preheader, &mp);
    int init_ok = (ind_reg >= 0 && ind_reg < mp.max_reg && mp.known && mp.known[ind_reg]);
    if (init_ok) init = mp.value[ind_reg];
    ir_constmap_free(&mp);
    if (!init_ok) return 0;

    // step must be +1 on backedge (latch)
    IRBlock* latch = NULL;
    for (int bi = 0; bi < ir_loop_block_count(loop); bi++) {
        IRBlock* b = ir_loop_block_at(loop, bi);
        if (!b || b == header) continue;
        if (b->succ_count == 1 && b->succs[0] == header) {
            latch = b;
            break;
        }
    }
    if (!latch || !latch->last || latch->last->op != IR_OP_BR) return 0;

    // find how ind_reg is updated in latch (allow copy chain)
    int cur = ind_reg;
    int step_ok = 0;
    for (int k = 0; k < 4; k++) {
        IRInst* ld = ir_find_last_def_in_block(latch, cur);
        if (!ld) break;

        if (ld->op == IR_OP_ADD && ld->operand_count >= 2) {
            IRValue* a = ld->operands[0];
            IRValue* b = ld->operands[1];
            int64_t imm = 0;

            if (a && a->kind == IR_VAL_REG && a->data.reg_num == ind_reg &&
                b && b->kind == IR_VAL_CONST_INT && b->data.const_int == 1) {
                step_ok = 1;
                break;
            }
            if (b && b->kind == IR_VAL_REG && b->data.reg_num == ind_reg &&
                a && a->kind == IR_VAL_CONST_INT && a->data.const_int == 1) {
                step_ok = 1;
                break;
            }

            (void)imm;
            break;
        }

        if (ld->op == IR_OP_COPY && ld->operand_count >= 1) {
            IRValue* src = ld->operands[0];
            if (src && src->kind == IR_VAL_REG) {
                cur = src->data.reg_num;
                continue;
            }
            break;
        }

        break;
    }
    if (!step_ok) return 0;

    // compute trip count
    int64_t trip64 = 0;
    if (pred == IR_CMP_LT) {
        trip64 = bound - init;
    } else {
        trip64 = bound - init + 1;
    }
    if (trip64 <= 0) return 0;
    if (trip64 > 1000000) return 0;

    if (out_induction_reg) *out_induction_reg = ind_reg;
    if (out_trip) *out_trip = (int)trip64;
    return 1;
}

// ============================================================================
// استنساخ تعليمة IR (Instruction Clone)
// ============================================================================

static IRInst* ir_clone_inst_for_unroll(IRInst* inst) {
    if (!inst) return NULL;

    IRInst* c = ir_inst_new(inst->op, inst->type, inst->dest);
    if (!c) return NULL;

    c->cmp_pred = inst->cmp_pred;
    c->src_file = inst->src_file;
    c->src_line = inst->src_line;
    c->src_col = inst->src_col;
    c->dbg_name = inst->dbg_name;

    // operands
    for (int i = 0; i < inst->operand_count; i++) {
        c->operands[i] = inst->operands[i];
    }
    c->operand_count = inst->operand_count;

    // calls
    if (inst->op == IR_OP_CALL) {
        c->call_target = inst->call_target;
        c->call_arg_count = inst->call_arg_count;
        // مشاركة مصفوفة الوسائط آمنة هنا لأننا لا نُعدّل call_args في هذه التمريرة.
        c->call_args = inst->call_args;
    }

    // phi not expected after outssa; we skip.
    return c;
}

// ============================================================================
// فك الحلقة (Full Unroll)
// ============================================================================

static int ir_unroll_full_simple_loop(IRFunc* func, IRLoop* loop, int max_trip) {
    if (!func || !loop) return 0;

    IRBlock* header = ir_loop_header(loop);
    IRBlock* preheader = ir_loop_preheader(loop);
    if (!header || !preheader) return 0;
    if (!preheader->last || preheader->last->op != IR_OP_BR) return 0;
    if (preheader->last->operand_count < 1) return 0;

    // يجب أن يقفز preheader إلى header.
    IRValue* br_tgt = preheader->last->operands[0];
    if (!br_tgt || br_tgt->kind != IR_VAL_BLOCK || br_tgt->data.block != header) return 0;

    // header يجب أن يكون br_cond إلى داخل/خارج.
    if (!header->last || header->last->op != IR_OP_BR_COND) return 0;
    if (header->succ_count != 2) return 0;

    IRBlock* in_succ = NULL;
    IRBlock* exit_succ = NULL;
    for (int i = 0; i < 2; i++) {
        IRBlock* s = header->succs[i];
        if (!s) continue;
        if (ir_loop_contains(loop, s)) in_succ = s;
        else exit_succ = s;
    }
    if (!in_succ || !exit_succ) return 0;

    int ind_reg = -1;
    int trip = 0;
    if (!ir_get_trip_count_for_simple_inc1_loop(func, loop, header, preheader, &ind_reg, &trip)) {
        return 0;
    }
    (void)ind_reg;

    if (trip <= 0 || trip > max_trip) return 0;

    // بناء مسار الكتل داخل دورة واحدة: in_succ -> ... -> header
    IRBlock* path[8];
    int path_len = 0;
    IRBlock* cur = in_succ;
    int guard = 0;
    while (cur && cur != header) {
        if (path_len >= (int)(sizeof(path) / sizeof(path[0]))) return 0;
        if (!ir_loop_contains(loop, cur)) return 0;
        path[path_len++] = cur;

        // يجب ألا يكون هناك تفرع داخل جسم الحلقة.
        if (!cur->last) return 0;
        if (cur->last->op != IR_OP_BR) return 0;
        if (cur->succ_count != 1) return 0;
        cur = cur->succs[0];
        if (++guard > 16) return 0;
    }
    if (cur != header) return 0;

    // أدخل نسخ التعليمات قبل terminator في preheader.
    IRInst* term = preheader->last;
    for (int t = 0; t < trip; t++) {
        for (int bi = 0; bi < path_len; bi++) {
            IRBlock* b = path[bi];
            for (IRInst* inst = b->first; inst; inst = inst->next) {
                if (inst == b->last) break;
                IRInst* c = ir_clone_inst_for_unroll(inst);
                if (!c) return 0;
                ir_block_insert_before(preheader, term, c);
            }
        }
    }

    // بدّل قفز preheader إلى exit.
    term->operands[0] = ir_value_block(exit_succ);
    ir_func_invalidate_defuse(func);

    // إعادة بناء preds/succs لضمان الاتساق.
    ir_func_rebuild_preds(func);
    return 1;
}

static int ir_unroll_func(IRFunc* func, int max_trip) {
    if (!func) return 0;
    if (func->is_prototype) return 0;
    if (!func->entry) return 0;

    int changed = 0;
    int progress = 1;
    int iters = 0;

    while (progress && iters++ < 8) {
        progress = 0;

        IRLoopInfo* info = ir_loop_analyze_func(func);
        if (!info) break;

        int n = ir_loop_info_count(info);
        for (int i = 0; i < n; i++) {
            IRLoop* loop = ir_loop_info_get(info, i);
            if (!loop) continue;
            if (ir_unroll_full_simple_loop(func, loop, max_trip)) {
                progress = 1;
                changed = 1;
                break; // أعد التحليل لأن CFG تغير.
            }
        }

        ir_loop_info_free(info);
    }

    return changed;
}

bool ir_unroll_run(IRModule* module, int max_trip) {
    if (!module) return false;
    if (max_trip <= 0) return false;

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_unroll_func(f, max_trip);
    }
    return changed ? true : false;
}
