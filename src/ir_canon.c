/**
 * @file ir_canon.c
 * @brief تنفيذ تمريرة توحيد/تطبيع الـ IR (Canonicalization Pass) (v0.3.2.6.5).
 *
 * مبادئ التوحيد الحالية:
 * 1) العمليات التبادلية (commutative): جمع/ضرب/و/أو
 *    - ضع الثابت على اليمين إن أمكن.
 *    - في حال كان الطرفان سجلات: اجعل (reg الأصغر) على اليسار لضمان شكل ثابت.
 *
 * 2) المقارنات (`قارن`):
 *    - إذا كان الطرف الأيسر ثابتاً والطرف الأيمن غير ثابت، نقلب المقارنة:
 *      قارن أكبر a, b  ==  قارن أصغر b, a
 *      قارن أكبر_أو_يساوي a, b  ==  قارن أصغر_أو_يساوي b, a
 *      ...إلخ (مع map للـ pred)
 *
 * ملاحظة:
 * - لا نحاول اختزال المقارنات إلى شكل واحد مثل "lt" دائماً؛ فقط نزيل اختلافات شائعة.
 * - لا نغير types أو نضيف تعليمات جديدة؛ فقط نعيد ترتيب المعاملات/المعاملات المنطقية.
 */

#include "ir_canon.h"

#include "ir_defuse.h"

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_CANON = {
    .name = "توحيد_الـIR",
    .run = ir_canon_run
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static int ir_value_is_const_int_local(IRValue* v) {
    return v && v->kind == IR_VAL_CONST_INT;
}

static int ir_value_is_reg_local(IRValue* v) {
    return v && v->kind == IR_VAL_REG;
}

static int ir_op_is_commutative(IROp op) {
    switch (op) {
        case IR_OP_ADD:
        case IR_OP_MUL:
        case IR_OP_AND:
        case IR_OP_OR:
            return 1;
        default:
            return 0;
    }
}

static int ir_cmp_is_swappable(IRCmpPred p) {
    // كل مقارناتنا متناظرة عند قلب الطرفين مع تغيير pred
    switch (p) {
        case IR_CMP_EQ:
        case IR_CMP_NE:
        case IR_CMP_GT:
        case IR_CMP_LT:
        case IR_CMP_GE:
        case IR_CMP_LE:
            return 1;
        default:
            return 0;
    }
}

static IRCmpPred ir_cmp_swap_pred(IRCmpPred p) {
    switch (p) {
        case IR_CMP_EQ: return IR_CMP_EQ;
        case IR_CMP_NE: return IR_CMP_NE;
        case IR_CMP_GT: return IR_CMP_LT;
        case IR_CMP_LT: return IR_CMP_GT;
        case IR_CMP_GE: return IR_CMP_LE;
        case IR_CMP_LE: return IR_CMP_GE;
        default:        return p;
    }
}

static void ir_swap_values(IRValue** a, IRValue** b) {
    IRValue* tmp = *a;
    *a = *b;
    *b = tmp;
}

static int ir_canon_commutative_two_operand(IRInst* inst) {
    if (!inst) return 0;
    if (inst->operand_count < 2) return 0;

    IRValue* a = inst->operands[0];
    IRValue* b = inst->operands[1];

    if (!a || !b) return 0;

    // 1) إن كان اليسار ثابتاً واليمين ليس ثابتاً: بدّل
    if (ir_value_is_const_int_local(a) && !ir_value_is_const_int_local(b)) {
        ir_swap_values(&inst->operands[0], &inst->operands[1]);
        return 1;
    }

    // 2) إن كان الطرفان سجلات: اجعل الأصغر على اليسار لضبط ترتيب ثابت
    if (ir_value_is_reg_local(a) && ir_value_is_reg_local(b)) {
        int ra = a->data.reg_num;
        int rb = b->data.reg_num;
        if (rb < ra) {
            ir_swap_values(&inst->operands[0], &inst->operands[1]);
            return 1;
        }
    }

    return 0;
}

static int ir_canon_cmp(IRInst* inst) {
    if (!inst || inst->op != IR_OP_CMP) return 0;
    if (inst->operand_count < 2) return 0;

    IRValue* a = inst->operands[0];
    IRValue* b = inst->operands[1];
    if (!a || !b) return 0;

    // إن كان الطرف الأيسر ثابتاً واليمين ليس ثابتاً: نقلب
    if (ir_value_is_const_int_local(a) && !ir_value_is_const_int_local(b) && ir_cmp_is_swappable(inst->cmp_pred)) {
        ir_swap_values(&inst->operands[0], &inst->operands[1]);
        inst->cmp_pred = ir_cmp_swap_pred(inst->cmp_pred);
        return 1;
    }

    // إن كان الطرفان سجلات: ترتيب ثابت أيضاً
    if (ir_value_is_reg_local(a) && ir_value_is_reg_local(b)) {
        int ra = a->data.reg_num;
        int rb = b->data.reg_num;
        if (rb < ra && ir_cmp_is_swappable(inst->cmp_pred)) {
            ir_swap_values(&inst->operands[0], &inst->operands[1]);
            inst->cmp_pred = ir_cmp_swap_pred(inst->cmp_pred);
            return 1;
        }
    }

    return 0;
}

static int ir_canon_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    int changed = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (!inst) continue;

            if (ir_op_is_commutative(inst->op)) {
                changed |= ir_canon_commutative_two_operand(inst);
                continue;
            }

            if (inst->op == IR_OP_CMP) {
                changed |= ir_canon_cmp(inst);
                continue;
            }
        }
    }

    if (changed) {
        // التمريرة تُغيّر مؤشرات داخل IRValue / ترتيبها → أبطل Def-Use.
        ir_func_invalidate_defuse(func);
    }

    return changed;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool ir_canon_run(IRModule* module) {
    if (!module) return false;

    // ضمان تخصيصات IR داخل هذه الوحدة إن احتجنا مستقبلاً (حتى لو لم نخصّص هنا).
    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_canon_func(f);
    }

    return changed ? true : false;
}