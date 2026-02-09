/**
 * @file ir_mem2reg.c
 * @brief تمريرة ترقية الذاكرة إلى سجلات (Mem2Reg) — خط أساس (v0.3.2.5.1).
 * @version 0.3.2.5.1
 *
 * هذه التمريرة هي خطوة أولى بسيطة لبناء SSA الحقيقي لاحقاً.
 *
 * القيود (مقصودة لتبسيط السلامة):
 * - نُرقّي فقط `حجز` (alloca) الذي:
 *   1) كل استعمالاته داخل نفس الكتلة الأساسية.
 *   2) لا يهرب المؤشر: لا يُمرَّر إلى `نداء` ولا يُستخدم كقيمة داخل `فاي`
 *      ولا يظهر كقيمة مخزّنة (أي لا يُخزَّن المؤشر نفسه).
 *   3) كل `حمل` يأتي بعد `خزن` سابق داخل نفس الكتلة (منع القراءة غير المهيّأة).
 *
 * التحويل:
 * - حذف كل تعليمات `خزن` المرتبطة بهذا المؤشر.
 * - تحويل كل `حمل` إلى `نسخ` من آخر قيمة تم تخزينها.
 * - حذف `حجز` نفسه بعد إزالة الاستعمالات.
 *
 * ملاحظة ملكية الذاكرة:
 * - لا نُعيد استعمال نفس مؤشر IRValue بين تعليمات مختلفة لتجنب double-free.
 *   لذلك نقوم بعمل clone للقيم عند إعادة الكتابة.
 */

#include "ir_mem2reg.h"

#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_MEM2REG = {
    .name = "ترقية_الذاكرة_إلى_سجلات",
    .run = ir_mem2reg_run
};

// -----------------------------------------------------------------------------
// Helpers: تصنيف القيم + النسخ الآمن
// -----------------------------------------------------------------------------

static int ir_value_is_reg(IRValue* v) {
    return v && v->kind == IR_VAL_REG;
}

static int ir_value_is_reg_num(IRValue* v, int reg_num) {
    return ir_value_is_reg(v) && v->data.reg_num == reg_num;
}

static IRValue* ir_value_clone_with_type(IRValue* v, IRType* override_type) {
    if (!v) return NULL;

    IRType* t = override_type ? override_type : v->type;

    switch (v->kind) {
        case IR_VAL_CONST_INT:
            return ir_value_const_int(v->data.const_int, t);

        case IR_VAL_CONST_STR:
            return ir_value_const_str(v->data.const_str.data, v->data.const_str.id);

        case IR_VAL_REG:
            return ir_value_reg(v->data.reg_num, t);

        case IR_VAL_GLOBAL:
            // ir_value_global() يبني نوع مؤشر من نوع pointee
            if (v->type && v->type->kind == IR_TYPE_PTR) {
                return ir_value_global(v->data.global_name, v->type->data.pointee);
            }
            return ir_value_global(v->data.global_name, v->type);

        case IR_VAL_FUNC:
            return ir_value_func_ref(v->data.global_name, t);

        case IR_VAL_BLOCK:
            return ir_value_block(v->data.block);

        case IR_VAL_NONE:
        default:
            return NULL;
    }
}

// -----------------------------------------------------------------------------
// Helpers: التعامل مع قائمة التعليمات في الكتل
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
// Helpers: فحص استعمالات المؤشر (عدم الهروب) + التحقق من نفس الكتلة
// -----------------------------------------------------------------------------

static int ir_inst_ptr_use_is_allowed(IRInst* inst, int ptr_reg) {
    if (!inst) return 0;

    // 1) استعمالات operands العادية
    for (int i = 0; i < inst->operand_count; i++) {
        if (!ir_value_is_reg_num(inst->operands[i], ptr_reg)) continue;

        // مسموح فقط في:
        // - حمل: operand[0] = ptr
        // - خزن: operand[1] = ptr  (وليس operand[0])
        if (inst->op == IR_OP_LOAD) {
            return (i == 0);
        }

        if (inst->op == IR_OP_STORE) {
            // operand[1] هو المؤشر، operand[0] هي القيمة المخزّنة
            if (i != 1) return 0;

            // لا نسمح بتخزين المؤشر نفسه (escape)
            if (inst->operand_count >= 1 && ir_value_is_reg_num(inst->operands[0], ptr_reg)) {
                return 0;
            }

            return 1;
        }

        // أي استعمال آخر يعتبر هروباً/غير مسموح.
        return 0;
    }

    // 2) استعمالات معاملات النداء
    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            if (ir_value_is_reg_num(inst->call_args[i], ptr_reg)) {
                return 0;
            }
        }
    }

    // 3) استعمالات فاي
    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            if (ir_value_is_reg_num(e->value, ptr_reg)) {
                return 0;
            }
        }
    }

    return 1;
}

static int ir_block_contains_reg_use_outside_load_store(IRBlock* block, int ptr_reg) {
    if (!block) return 0;

    for (IRInst* inst = block->first; inst; inst = inst->next) {
        if (!ir_inst_ptr_use_is_allowed(inst, ptr_reg)) {
            // نحتاج لمعرفة هل يوجد استعمال فعلي للمؤشر داخل هذه التعليمة.
            // إذا التعليمة لا تحتوي المؤشر أصلاً، فـ ir_inst_ptr_use_is_allowed ستعيد 1.
            // إذن الوصول هنا يعني أن المؤشر استُعمل بشكل غير مسموح.
            return 1;
        }
    }

    return 0;
}

static int ir_func_reg_used_outside_block(IRFunc* func, IRBlock* def_block, int ptr_reg) {
    if (!func || !def_block) return 1;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b == def_block) continue;

        for (IRInst* inst = b->first; inst; inst = inst->next) {
            // operands
            for (int i = 0; i < inst->operand_count; i++) {
                if (ir_value_is_reg_num(inst->operands[i], ptr_reg)) {
                    return 1;
                }
            }

            // call args
            if (inst->op == IR_OP_CALL && inst->call_args) {
                for (int i = 0; i < inst->call_arg_count; i++) {
                    if (ir_value_is_reg_num(inst->call_args[i], ptr_reg)) {
                        return 1;
                    }
                }
            }

            // phi entries
            if (inst->op == IR_OP_PHI) {
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                    if (ir_value_is_reg_num(e->value, ptr_reg)) {
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

static IRType* ir_alloca_pointee_type(IRInst* alloca_inst) {
    if (!alloca_inst) return NULL;
    if (!alloca_inst->type) return NULL;
    if (alloca_inst->type->kind != IR_TYPE_PTR) return NULL;
    return alloca_inst->type->data.pointee;
}

// -----------------------------------------------------------------------------
// Core: ترقية alloca واحدة داخل كتلة واحدة
// -----------------------------------------------------------------------------

static int ir_mem2reg_can_promote_alloca(IRFunc* func, IRBlock* block, IRInst* alloca_inst) {
    if (!func || !block || !alloca_inst) return 0;
    if (alloca_inst->op != IR_OP_ALLOCA) return 0;
    if (alloca_inst->dest < 0) return 0;

    int ptr_reg = alloca_inst->dest;

    // شرط: الاستعمالات يجب أن تكون داخل نفس الكتلة فقط
    if (ir_func_reg_used_outside_block(func, block, ptr_reg)) {
        return 0;
    }

    // شرط: لا استعمالات غير مسموحة داخل نفس الكتلة
    if (ir_block_contains_reg_use_outside_load_store(block, ptr_reg)) {
        return 0;
    }

    // شرط: كل حمل يأتي بعد خزن سابق + تحقق من الأنواع
    IRType* pointee = ir_alloca_pointee_type(alloca_inst);
    if (!pointee) return 0;

    int seen_store = 0;

    for (IRInst* inst = alloca_inst->next; inst; inst = inst->next) {
        if (inst->op == IR_OP_STORE) {
            if (inst->operand_count < 2) continue;
            if (!ir_value_is_reg_num(inst->operands[1], ptr_reg)) continue;

            IRValue* stored_val = inst->operands[0];
            if (!stored_val || !stored_val->type) return 0;

            // يجب أن يكون نوع القيمة المخزنة مطابقاً لنوع المتغير
            if (!ir_types_equal(stored_val->type, pointee)) return 0;

            seen_store = 1;
            continue;
        }

        if (inst->op == IR_OP_LOAD) {
            if (inst->operand_count < 1) continue;
            if (!ir_value_is_reg_num(inst->operands[0], ptr_reg)) continue;

            // لا نسمح بقراءة قبل أي كتابة
            if (!seen_store) return 0;

            // يجب أن يطابق نوع الحمل نوع المتغير
            if (!inst->type || !ir_types_equal(inst->type, pointee)) return 0;

            continue;
        }
    }

    // بدون أي استعمالات لا داعي للترقية
    if (!seen_store) {
        return 0;
    }

    return 1;
}

static int ir_mem2reg_promote_alloca(IRBlock* block, IRInst* alloca_inst) {
    if (!block || !alloca_inst) return 0;
    if (alloca_inst->op != IR_OP_ALLOCA) return 0;
    if (alloca_inst->dest < 0) return 0;

    int ptr_reg = alloca_inst->dest;

    // آخر قيمة تم تخزينها (مملوكة للتمريره)
    IRValue* last_stored = NULL;

    // 1) إعادة كتابة الكتلة: حذف الخزن + تحويل الحمل إلى نسخ
    IRInst* inst = alloca_inst->next;
    while (inst) {
        IRInst* next = inst->next;

        if (inst->op == IR_OP_STORE &&
            inst->operand_count >= 2 &&
            ir_value_is_reg_num(inst->operands[1], ptr_reg)) {

            // تحديث آخر قيمة مخزّنة
            IRValue* stored_val = inst->operands[0];

            IRValue* clone = ir_value_clone_with_type(stored_val, stored_val ? stored_val->type : NULL);
            if (!clone) {
                if (last_stored) ir_value_free(last_stored);
                return 0;
            }

            if (last_stored) {
                ir_value_free(last_stored);
            }
            last_stored = clone;

            // حذف تعليمة الخزن
            ir_block_remove_inst(block, inst);
            inst = next;
            continue;
        }

        if (inst->op == IR_OP_LOAD &&
            inst->operand_count >= 1 &&
            ir_value_is_reg_num(inst->operands[0], ptr_reg)) {

            if (!last_stored) {
                // هذا لا يجب أن يحدث إذا كان التحقق صحيحاً
                return 0;
            }

            // تحويل الحمل إلى نسخ:
            // %dest = نسخ <type> <last_stored>
            IRValue* src = ir_value_clone_with_type(last_stored, inst->type);
            if (!src) {
                if (last_stored) ir_value_free(last_stored);
                return 0;
            }

            // تحرير المعامل القديم (المؤشر)
            for (int i = 0; i < inst->operand_count; i++) {
                if (inst->operands[i]) {
                    ir_value_free(inst->operands[i]);
                    inst->operands[i] = NULL;
                }
            }

            inst->op = IR_OP_COPY;
            inst->operand_count = 1;
            inst->operands[0] = src;

            // حقول أخرى غير مستخدمة هنا (phi/call)
            inst->phi_entries = NULL;
            inst->call_target = NULL;
            inst->call_args = NULL;
            inst->call_arg_count = 0;

            inst = next;
            continue;
        }

        inst = next;
    }

    // 2) حذف alloca نفسه (بعد إزالة الاستعمالات)
    ir_block_remove_inst(block, alloca_inst);

    if (last_stored) ir_value_free(last_stored);
    return 1;
}

static int ir_mem2reg_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;

    int changed = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        IRInst* inst = b->first;
        while (inst) {
            IRInst* next = inst->next;

            if (inst->op == IR_OP_ALLOCA && inst->dest >= 0) {
                if (ir_mem2reg_can_promote_alloca(func, b, inst)) {
                    if (ir_mem2reg_promote_alloca(b, inst)) {
                        changed = 1;
                    }
                }
            }

            inst = next;
        }
    }

    return changed;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool ir_mem2reg_run(IRModule* module) {
    if (!module) return false;

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_mem2reg_func(f);
    }

    return changed ? true : false;
}