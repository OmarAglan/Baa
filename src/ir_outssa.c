/**
 * @file ir_outssa.c
 * @brief تمريرة الخروج من SSA — إزالة عقد فاي قبل الخلفية.
 * @version 0.3.2.5.2
 *
 * الفكرة:
 * - لكل كتلة تحتوي على تعليمات `فاي`:
 *   - لكل سابق P إلى هذه الكتلة B:
 *     - نبني قائمة نسخ: %وجهة_فاي = القيمة_الداخلة_من_هذا_السابق
 *     - إذا كانت الحافة P->B حرجة (P يملك أكثر من تابع)، نقسم الحافة:
 *       P -> E -> B
 *       ونضع النسخ في E قبل القفز إلى B.
 *     - غير ذلك، نُدرج النسخ في P قبل تعليمة الإنهاء.
 * - بعد إدراج النسخ على كل الحواف، نحذف تعليمات `فاي` من B.
 *
 * ملاحظة:
 * - هذه التمريرة تُخرجنا من SSA: قد يصبح للسجل أكثر من تعريف عبر كتل مختلفة.
 */

#include "ir_outssa.h"

#include "ir_analysis.h"
#include "ir_mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// ملاحظة:
// نستخدم مساعدات التعديل المشتركة من ir_mutate.c لضمان parent/id للتعليمات.
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

static IRValue* ir_value_clone_with_type_local(IRValue* v, IRType* override_type) {
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
// مساعدات: تعديل أهداف القفز (تقسيم الحافة)
// -----------------------------------------------------------------------------

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
        return;
    }
}

// -----------------------------------------------------------------------------
// النواة: إخراج فاي على حافة واحدة مع نسخ متوازية
// -----------------------------------------------------------------------------

typedef struct {
    int dest_reg;
    IRType* type;
    IRValue* src;   // قد يُستبدل بمؤقت أثناء حل الدورات

    // معلومات ديبغ من عقدة فاي الأصلية
    const char* dbg_name;
    const char* src_file;
    int src_line;
    int src_col;
} EdgeCopy;

static int edge_copy_dest_is_used_as_source(const EdgeCopy* copies, const unsigned char* done,
                                            int count, int dest_reg) {
    if (!copies || !done || count <= 0) return 0;

    for (int i = 0; i < count; i++) {
        if (done[i]) continue;
        IRValue* src = copies[i].src;
        if (!src) continue;
        if (src->kind != IR_VAL_REG) continue;
        if (src->data.reg_num == dest_reg) return 1;
    }

    return 0;
}

static void ir_emit_edge_copy(IRBlock* insert_block, IRInst* before,
                              int dest_reg, IRType* type, IRValue* src,
                              const char* dbg_name,
                              const char* src_file, int src_line, int src_col) {
    if (!insert_block || !type || !src) return;

    IRValue* clone = ir_value_clone_with_type_local(src, type);
    if (!clone) return;

    IRInst* copy = ir_inst_unary(IR_OP_COPY, type, dest_reg, clone);
    if (!copy) {
        ir_value_free(clone);
        return;
    }

    if (src_file && src_line > 0) {
        ir_inst_set_loc(copy, src_file, src_line, src_col);
    }
    if (dbg_name) {
        ir_inst_set_dbg_name(copy, dbg_name);
    }

    if (before) {
        ir_block_insert_before(insert_block, before, copy);
    } else {
        ir_block_append(insert_block, copy);
    }
}

static void ir_emit_parallel_copies(IRFunc* func, IRBlock* insert_block, IRInst* before,
                                    EdgeCopy* copies, int copy_count) {
    if (!func || !insert_block || !copies || copy_count <= 0) return;

    unsigned char* done = (unsigned char*)calloc((size_t)copy_count, sizeof(unsigned char));
    if (!done) return;

    // مؤقتات تم إنشاؤها لكسر الدورات
    IRValue** temps = NULL;
    int temp_count = 0;
    int temp_cap = 0;

    int remaining = copy_count;
    while (remaining > 0) {
        int progress = 0;

        // 1) النسخ الآمنة: يمكن إصدار النسخ إذا لم يكن سجل الوجهة مطلوباً كمصدر لاحقاً
        for (int i = 0; i < copy_count; i++) {
            if (done[i]) continue;

            IRValue* src = copies[i].src;
            if (!src) {
                done[i] = 1;
                remaining--;
                progress = 1;
                continue;
            }

            if (!edge_copy_dest_is_used_as_source(copies, done, copy_count, copies[i].dest_reg)) {
                ir_emit_edge_copy(insert_block, before,
                                  copies[i].dest_reg, copies[i].type, src,
                                  copies[i].dbg_name,
                                  copies[i].src_file, copies[i].src_line, copies[i].src_col);
                done[i] = 1;
                remaining--;
                progress = 1;
            }
        }

        if (progress) continue;

        // 2) دورة: نكسرها بمؤقت
        int pick = -1;
        for (int i = 0; i < copy_count; i++) {
            if (done[i]) continue;
            pick = i;
            break;
        }

        if (pick < 0) break;

        int cycle_dest = copies[pick].dest_reg;
        IRType* t = copies[pick].type ? copies[pick].type : IR_TYPE_I64_T;

        int temp_reg = ir_func_alloc_reg(func);

        // مؤقت = نسخ <type> %cycle_dest  (حفظ القيمة القديمة قبل الكتابة فوقها)
        IRValue* cycle_dest_val = ir_value_reg(cycle_dest, t);
        if (!cycle_dest_val) break;

        ir_emit_edge_copy(insert_block, before,
                          temp_reg, t, cycle_dest_val,
                          copies[pick].dbg_name,
                          copies[pick].src_file, copies[pick].src_line, copies[pick].src_col);
        ir_value_free(cycle_dest_val);

        IRValue* temp_val = ir_value_reg(temp_reg, t);
        if (!temp_val) break;

        // سجل المؤقت ليتم تحريره لاحقاً
        if (temp_count >= temp_cap) {
            int new_cap = (temp_cap == 0) ? 4 : temp_cap * 2;
            IRValue** new_arr = (IRValue**)realloc(temps, (size_t)new_cap * sizeof(IRValue*));
            if (!new_arr) {
                ir_value_free(temp_val);
                break;
            }
            temps = new_arr;
            temp_cap = new_cap;
        }
        temps[temp_count++] = temp_val;

        // استبدال كل استعمالات سجل الوجهة في المصادر بمؤقت
        for (int i = 0; i < copy_count; i++) {
            if (done[i]) continue;
            if (!copies[i].src) continue;
            if (copies[i].src->kind != IR_VAL_REG) continue;
            if (copies[i].src->data.reg_num != cycle_dest) continue;
            copies[i].src = temp_val;
        }
    }

    for (int i = 0; i < temp_count; i++) {
        if (temps[i]) ir_value_free(temps[i]);
    }
    free(temps);
    free(done);
}

// -----------------------------------------------------------------------------
// النواة: الخروج من SSA لكل دالة
// -----------------------------------------------------------------------------

static IRValue* ir_phi_incoming_value(IRInst* phi, IRBlock* pred, IRType* phi_type) {
    if (!phi || phi->op != IR_OP_PHI || !pred) return NULL;

    for (IRPhiEntry* e = phi->phi_entries; e; e = e->next) {
        if (e->block == pred) {
            return e->value;
        }
    }

    // غياب المدخل يعني IR غير مكتمل؛ نعود بصفر كقيمة افتراضية لتجنب الانهيار
    if (phi_type) return ir_value_const_int(0, phi_type);
    return ir_value_const_int(0, IR_TYPE_I64_T);
}

static IRBlock* ir_outssa_split_edge(IRFunc* func, IRBlock* pred, IRBlock* succ) {
    if (!func || !pred || !succ) return NULL;

    char label[128];
    snprintf(label, sizeof(label), "كتلة_فاي_حافة_%d_%d", pred->id, succ->id);

    IRBlock* split = ir_func_new_block(func, label);
    if (!split) return NULL;

    // تعديل هدف القفز في تعليمة الإنهاء للسابق
    IRInst* term = pred->last;
    if (!term || !ir_is_terminator(term)) return split;
    ir_term_replace_target(term, succ, split);

    // تقسيم الحافة: ... ثم قفز إلى التابع (سنضيف النسخ قبل ذلك)
    IRInst* br = ir_inst_br(succ);
    if (br) {
        ir_block_append(split, br);
    }

    return split;
}

static int ir_outssa_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    // تأكد من صحة الحواف قبل البدء
    ir_func_rebuild_preds(func);

    int changed = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b->first) continue;
        if (b->first->op != IR_OP_PHI) continue;

        // اجمع كل فاي في بداية الكتلة
        IRInst** phis = NULL;
        int phi_count = 0;
        int phi_cap = 0;

        for (IRInst* inst = b->first; inst && inst->op == IR_OP_PHI; inst = inst->next) {
            if (phi_count >= phi_cap) {
                int new_cap = (phi_cap == 0) ? 4 : phi_cap * 2;
                IRInst** new_arr = (IRInst**)realloc(phis, (size_t)new_cap * sizeof(IRInst*));
                if (!new_arr) break;
                phis = new_arr;
                phi_cap = new_cap;
            }
            phis[phi_count++] = inst;
        }

        if (phi_count == 0) {
            free(phis);
            continue;
        }

        // لكل سابق: أنشئ نسخ الحافة
        for (int p = 0; p < b->pred_count; p++) {
            IRBlock* pred = b->preds[p];
            if (!pred) continue;
            if (!pred->last || !ir_is_terminator(pred->last)) continue;

            EdgeCopy* copies = (EdgeCopy*)calloc((size_t)phi_count, sizeof(EdgeCopy));
            if (!copies) continue;

            for (int i = 0; i < phi_count; i++) {
                IRInst* phi = phis[i];
                IRType* t = phi->type ? phi->type : IR_TYPE_I64_T;

                copies[i].dest_reg = phi->dest;
                copies[i].type = t;
                copies[i].src = ir_phi_incoming_value(phi, pred, t);

                // حافظ على معلومات الديبغ من الفاي.
                copies[i].dbg_name = phi->dbg_name;
                copies[i].src_file = phi->src_file;
                copies[i].src_line = phi->src_line;
                copies[i].src_col = phi->src_col;
            }

            // اختيار مكان الإدراج
            IRBlock* insert_block = pred;
            IRInst* before = pred->last;

            // إذا كان للسابق أكثر من تابع، نحتاج تقسيم الحافة
            if (pred->succ_count != 1) {
                insert_block = ir_outssa_split_edge(func, pred, b);
                if (insert_block) {
                    // في كتلة التقسيم، نُدرج النسخ قبل تعليمة الإنهاء الخاصة بها
                    before = insert_block->last;
                }
            }

            if (insert_block) {
                // في كتلة التقسيم الجديدة قد لا يكون هناك تعليمة إنهاء بعد (في حال فشل الإضافة)
                if (before && !ir_is_terminator(before)) {
                    before = NULL;
                }

                // إذا كانت كتلة تقسيم، نريد إدراج النسخ قبل القفز إلى b
                ir_emit_parallel_copies(func, insert_block, before, copies, phi_count);
                changed = 1;
            }

            // إذا كنا في كتلة تقسيم ولم يكن لديها تعليمة إنهاء (لسبب ما)، أضف قفزاً
            if (insert_block && insert_block != pred && !insert_block->last) {
                IRInst* br = ir_inst_br(b);
                if (br) ir_block_append(insert_block, br);
            }

            free(copies);
        }

        // حذف كل فاي من بداية الكتلة
        IRInst* inst = b->first;
        while (inst && inst->op == IR_OP_PHI) {
            IRInst* next = inst->next;
            ir_block_remove_inst(b, inst);
            inst = next;
            changed = 1;
        }

        free(phis);
    }

    if (changed) {
        ir_func_rebuild_preds(func);
    }

    return changed;
}

// -----------------------------------------------------------------------------
// الواجهة العامة
// -----------------------------------------------------------------------------

bool ir_outssa_run(IRModule* module) {
    if (!module) return false;

    // ضمان أن أي نسخ/قيم جديدة تُخصَّص ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_outssa_func(f);
    }

    return changed ? true : false;
}
