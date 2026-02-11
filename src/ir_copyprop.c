/**
 * @file ir_copyprop.c
 * @brief IR copy propagation pass (نشر_النسخ).
 * @version 0.3.1.4
 *
 * هدف التمريرة:
 * - اكتشاف تعليمات النسخ في IR:  %مX = نسخ <type> <value>
 * - نشر قيمة المصدر إلى جميع استعمالات %مX
 * - حذف تعليمات النسخ بعد أن تصبح غير لازمة
 *
 * ملاحظات تصميمية:
 * - التمريرة "محلية للدالة" لأن السجلات الافتراضية في هذا IR محلية لكل دالة.
 * - لا نشارك نفس مؤشر IRValue بين أكثر من موضع لتجنب double-free:
 *   نستبدل الاستعمالات عبر إنشاء IRValue جديدة (clone) ثم تحرير القديمة.
 */

#include "ir_copyprop.h"

#include "ir_defuse.h"
#include "ir_mutate.h"

#include <stdlib.h>
#include <string.h>

#define COPYPROP_MAX_CHAIN_DEPTH 64

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_COPYPROP = {
    .name = "نشر_النسخ",
    .run = ir_copyprop_run
};

// -----------------------------------------------------------------------------
// Helpers: value classification / cloning
// -----------------------------------------------------------------------------

static IRValue* ir_value_clone_with_type(IRValue* v, IRType* override_type) {
    if (!v) return NULL;

    IRType* t = override_type ? override_type : v->type;

    switch (v->kind) {
        case IR_VAL_CONST_INT:
            return ir_value_const_int(v->data.const_int, t);

        case IR_VAL_CONST_STR:
            // Keep same string-id and content; constructor will strdup() the data.
            return ir_value_const_str(v->data.const_str.data, v->data.const_str.id);

        case IR_VAL_REG:
            return ir_value_reg(v->data.reg_num, t);

        case IR_VAL_GLOBAL:
            // Note: ir_value_global() expects the pointee type and will build a ptr type.
            // If v->type is already a pointer, pass the pointee when possible.
            if (v->type && v->type->kind == IR_TYPE_PTR) {
                return ir_value_global(v->data.global_name, v->type->data.pointee);
            }
            return ir_value_global(v->data.global_name, v->type);

        case IR_VAL_FUNC:
            return ir_value_func_ref(v->data.global_name, t);

        case IR_VAL_BLOCK:
            // Blocks are control-flow labels; copying them is nonsensical for this pass.
            return ir_value_block(v->data.block);

        case IR_VAL_NONE:
        default:
            return NULL;
    }
}

static IRValue* ir_canonicalize_value(IRValue* v, IRValue** aliases, int max_reg, int depth) {
    if (!v || !aliases) return v;
    if (depth >= COPYPROP_MAX_CHAIN_DEPTH) return v;

    if (v->kind != IR_VAL_REG) return v;

    int r = v->data.reg_num;
    if (r < 0 || r >= max_reg) return v;

    IRValue* a = aliases[r];
    if (!a) return v;

    return ir_canonicalize_value(a, aliases, max_reg, depth + 1);
}

static int ir_try_replace_value_slot(IRValue** slot, IRValue** aliases, int max_reg) {
    if (!slot || !*slot || !aliases) return 0;

    IRValue* cur = *slot;
    if (cur->kind != IR_VAL_REG) return 0;

    int r = cur->data.reg_num;
    if (r < 0 || r >= max_reg) return 0;

    IRValue* repl = aliases[r];
    if (!repl) return 0;

    // Clone replacement; preserve the use-site type when available.
    IRValue* new_val = ir_value_clone_with_type(repl, cur->type ? cur->type : repl->type);
    if (!new_val) return 0;

    ir_value_free(cur);
    *slot = new_val;
    return 1;
}

// -----------------------------------------------------------------------------
// ملاحظة:
// حذف التعليمات يتم عبر ir_block_remove_inst() من ir_mutate.c (unlink فقط).
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Core: per-function copy propagation
// -----------------------------------------------------------------------------

static int ir_copyprop_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    int max_reg = func->next_reg;
    if (max_reg <= 0) return 0;

    IRValue** aliases = (IRValue**)calloc((size_t)max_reg, sizeof(IRValue*));
    IRType** alias_types = (IRType**)calloc((size_t)max_reg, sizeof(IRType*));
    if (!aliases || !alias_types) {
        if (aliases) free(aliases);
        if (alias_types) free(alias_types);
        return 0;
    }

    int changed = 0;

    // 1) Collect raw alias mapping for IR_OP_COPY destinations.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->op != IR_OP_COPY) continue;
            if (inst->dest < 0 || inst->dest >= max_reg) continue;
            if (inst->operand_count < 1) continue;

            IRValue* src = inst->operands[0];
            if (!src) continue;

            IRType* dest_type = inst->type ? inst->type : src->type;
            if (!dest_type || !src->type) continue;

            // Be conservative: only propagate if types match.
            if (!ir_types_equal(dest_type, src->type)) continue;

            // If multiple definitions appear (shouldn't happen in SSA), keep the first safely.
            if (aliases[inst->dest]) continue;

            IRValue* src_canon = ir_canonicalize_value(src, aliases, max_reg, 0);
            IRValue* clone = ir_value_clone_with_type(src_canon, dest_type);
            if (!clone) continue;

            aliases[inst->dest] = clone;
            alias_types[inst->dest] = dest_type;
        }
    }

    // 2) Canonicalize alias map (resolve chains like %م2 -> %م1 -> %م0).
    for (int r = 0; r < max_reg; r++) {
        if (!aliases[r]) continue;

        IRValue* canon = ir_canonicalize_value(aliases[r], aliases, max_reg, 0);
        if (canon == aliases[r]) continue;

        IRType* t = alias_types[r] ? alias_types[r] : aliases[r]->type;
        IRValue* new_clone = ir_value_clone_with_type(canon, t);
        if (!new_clone) continue;

        ir_value_free(aliases[r]);
        aliases[r] = new_clone;
    }

    // 3) Replace all uses in the function.
    // نستخدم Def-Use لتجنب مسح الدالة بالكامل إن أمكن.
    IRDefUse* du = ir_func_get_defuse(func, true);
    if (du) {
        for (int r = 0; r < max_reg && r < du->max_reg; r++) {
            if (!aliases[r]) continue;

            for (IRUse* u = du->uses_by_reg[r]; u; u = u->next) {
                if (!u->slot || !*u->slot) continue;

                IRValue* cur = *u->slot;
                if (!cur || cur->kind != IR_VAL_REG) continue;
                if (cur->data.reg_num != r) continue;

                IRValue* repl = aliases[r];
                IRValue* new_val = ir_value_clone_with_type(repl, cur->type ? cur->type : repl->type);
                if (!new_val) continue;

                *u->slot = new_val;
                changed = 1;
            }
        }
    } else {
        // احتياطي: مسح كامل.
        for (IRBlock* b = func->blocks; b; b = b->next) {
            for (IRInst* inst = b->first; inst; inst = inst->next) {
                for (int i = 0; i < inst->operand_count; i++) {
                    changed |= ir_try_replace_value_slot(&inst->operands[i], aliases, max_reg);
                }

                if (inst->op == IR_OP_CALL && inst->call_args) {
                    for (int i = 0; i < inst->call_arg_count; i++) {
                        changed |= ir_try_replace_value_slot(&inst->call_args[i], aliases, max_reg);
                    }
                }

                if (inst->op == IR_OP_PHI) {
                    for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                        changed |= ir_try_replace_value_slot(&e->value, aliases, max_reg);
                    }
                }
            }
        }
    }

    // 4) Remove copy instructions whose destination has an alias mapping.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        IRInst* inst = b->first;
        while (inst) {
            IRInst* next = inst->next;

            if (inst->op == IR_OP_COPY &&
                inst->dest >= 0 && inst->dest < max_reg &&
                aliases[inst->dest] != NULL) {

                ir_block_remove_inst(b, inst);
                changed = 1;
            }

            inst = next;
        }
    }

    // 5) Cleanup alias map.
    for (int r = 0; r < max_reg; r++) {
        if (aliases[r]) {
            ir_value_free(aliases[r]);
            aliases[r] = NULL;
        }
    }

    free(aliases);
    free(alias_types);

    if (changed) {
        // الاستبدالات تمت عبر تعديل مؤشرات الخانات مباشرة؛ نبطل Def-Use لضمان إعادة بناء لاحقة صحيحة.
        ir_func_invalidate_defuse(func);
    }

    return changed;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool ir_copyprop_run(IRModule* module) {
    if (!module) return false;

    // ضمان أن أي قيم جديدة تُخصَّص ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_copyprop_func(f);
    }

    return changed ? true : false;
}
