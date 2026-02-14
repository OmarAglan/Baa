/**
 * @file ir_constfold.c
 * @brief مرور طي الثوابت (طي_الثوابت) — v0.3.2.6.6.
 * @version 0.3.2.6.6
 *
 * يطوي:
 * - الحساب: جمع/طرح/ضرب/قسم/باقي عندما يكون كلا المعاملين ثوابت فورية.
 * - المقارنات: قارن <محمول> عندما يكون كلا المعاملين ثوابت فورية.
 *
 * دلالات الحساب (v0.3.2.6.6):
 * ─────────────────────────────
 * • الطفحان: التفاف مكمل الاثنين (two's-complement wrap).
 * • القسمة/الباقي: اقتطاع نحو الصفر (truncation toward zero).
 *   - INT64_MIN / -1 = INT64_MIN (التفاف آمن، ليس سلوكاً غير محدد).
 *   - INT64_MIN % -1 = 0.
 * • المقارنات: دائماً بإشارة (signed) للأنواع الصحيحة.
 * • ص١ (i1): أي قيمة غير صفرية → 1، صفر → 0.
 *
 * ملاحظات التنفيذ:
 * - الـ IR حالياً يمثل الثوابت بـ IR_VAL_CONST_INT فقط (حمولة int64_t).
 * - نستبدل استعمالات سجل الوجهة المطوي بقيمة ثابتة جديدة.
 * - ثم نحذف التعليمة المطوية من كتلتها الأساسية.
 */

#include "ir_constfold.h"

#include "ir_defuse.h"
#include "ir_mutate.h"

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
// مساعدات: دلالات الأعداد الصحيحة (Integer Semantics)
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
// ملاحظة:
// حذف التعليمات يتم عبر ir_block_remove_inst() من ir_mutate.c (unlink فقط).
// -----------------------------------------------------------------------------

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

static int ir_func_replace_reg_uses_defuse(IRDefUse* du,
                                          int reg_num, IRType* folded_type, int64_t folded_value) {
    if (!du) return 0;
    if (reg_num < 0 || reg_num >= du->max_reg) return 0;

    int changed = 0;
    for (IRUse* u = du->uses_by_reg[reg_num]; u; u = u->next) {
        if (!u->slot || !*u->slot) continue;

        IRValue* cur = *u->slot;
        if (!cur || cur->kind != IR_VAL_REG) continue;
        if (cur->data.reg_num != reg_num) continue;

        IRType* t = cur->type ? cur->type : folded_type;
        *u->slot = new_const_for_type(folded_value, t);
        changed = 1;
    }

    return changed;
}

// -----------------------------------------------------------------------------
// Folding logic
// -----------------------------------------------------------------------------

/**
 * @brief طي عملية حسابية ثنائية على ثوابت.
 *
 * دلالات الحساب (v0.3.2.6.6):
 * • الطفحان: التفاف مكمل الاثنين — العمليات تُنفَّذ على uint64_t ثم
 *   يُعاد تفسيرها كـ int64_t (سلوك محدد في C).
 * • القسمة/الباقي على صفر: لا يُطوى (يُرجع 0).
 * • حالة خاصة: INT64_MIN / -1 يُرجع INT64_MIN (التفاف آمن).
 * • حالة خاصة: INT64_MIN % -1 يُرجع 0.
 *
 * @param op كود العملية (جمع/طرح/ضرب/قسم/باقي).
 * @param type نوع النتيجة (يُستعمل لاحقاً بعد الاقتطاع).
 * @param lhs المعامل الأيسر.
 * @param rhs المعامل الأيمن.
 * @param[out] out مؤشر لتخزين النتيجة.
 * @return 1 عند نجاح الطي، 0 عند الفشل.
 */
static int try_fold_arith(IROp op, IRType* type, int64_t lhs, int64_t rhs, int64_t* out) {
    if (!out) return 0;

    // حساب بمكمل الاثنين: نُحوّل إلى uint64_t لتجنب السلوك غير المحدد
    // في C عند طفحان الأعداد المُعلَّمة (signed overflow).
    uint64_t ul = (uint64_t)lhs;
    uint64_t ur = (uint64_t)rhs;

    switch (op) {
        case IR_OP_ADD:
            *out = (int64_t)(ul + ur);
            return 1;
        case IR_OP_SUB:
            *out = (int64_t)(ul - ur);
            return 1;
        case IR_OP_MUL:
            *out = (int64_t)(ul * ur);
            return 1;
        case IR_OP_DIV:
            if (rhs == 0) return 0;
            // حالة خاصة: INT64_MIN / -1 — في C هذا سلوك غير محدد.
            // في Baa IR: نتيجة الالتفاف = INT64_MIN.
            if (lhs == INT64_MIN && rhs == -1) {
                *out = INT64_MIN;
                return 1;
            }
            *out = lhs / rhs;   // اقتطاع نحو الصفر (C11 §6.5.5)
            return 1;
        case IR_OP_MOD:
            if (rhs == 0) return 0;
            // حالة خاصة: INT64_MIN % -1 — في C هذا سلوك غير محدد.
            // في Baa IR: النتيجة = 0 (لأن INT64_MIN / -1 = INT64_MIN بالالتفاف).
            if (lhs == INT64_MIN && rhs == -1) {
                *out = 0;
                return 1;
            }
            *out = lhs % rhs;   // اقتطاع نحو الصفر
            return 1;
        default:
            (void)type;
            return 0;
    }
}

/**
 * @brief تقييم مقارنة ثوابت.
 *
 * المقارنات دائماً بإشارة (signed) — هذا عقد الـ IR (v0.3.2.6.6).
 * لا توجد مقارنات بدون إشارة في IR باء حالياً.
 *
 * @param pred المحمول (يساوي/لا_يساوي/أكبر/أصغر/أكبر_أو_يساوي/أصغر_أو_يساوي).
 * @param lhs المعامل الأيسر.
 * @param rhs المعامل الأيمن.
 * @return 1 إذا تحققت المقارنة، 0 غير ذلك.
 */
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

    // بناء Def-Use مرة واحدة لتسريع استبدال الاستعمالات.
    IRDefUse* du = ir_func_get_defuse(func, true);

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
                if (du) {
                    changed |= ir_func_replace_reg_uses_defuse(du, folded_reg, folded_type, folded_value);
                } else {
                    changed |= ir_func_replace_reg_uses(func, folded_reg, folded_type, folded_value);
                }

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

    // ضمان أن أي قيم/أنواع جديدة تُخصَّص ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);

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
