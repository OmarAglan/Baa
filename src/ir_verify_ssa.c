/**
 * @file ir_verify_ssa.c
 * @brief التحقق من صحة SSA للـ IR قبل مرحلة الخروج من SSA.
 * @version 0.3.2.5.3
 *
 * قواعد التحقق الأساسية:
 * 1) تعريف واحد لكل سجل افتراضي (بما في ذلك سجلات معاملات الدالة).
 * 2) كل استعمال لِـ %م<n> يجب أن يكون مُسيطَراً عليه بواسطة تعريفه.
 * 3) `فاي` يجب أن يحتوي على مدخل واحد لكل سابق، دون تكرار أو سوابق غير صحيحة.
 */

#include "ir_verify_ssa.h"

#include "ir_analysis.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// ثوابت وتشخيص (Diagnostics)
// ============================================================================

#define SSA_VERIFY_MAX_ERRORS 20

typedef struct {
    FILE* out;
    int error_count;
    int error_cap;
} SSAVerifyDiag;

static void ssa_print_loc(FILE* out, const IRInst* inst) {
    if (!out || !inst) return;
    if (!inst->src_file || inst->src_line <= 0) return;

    if (inst->src_col > 0) {
        fprintf(out, " (الموقع: %s:%d:%d)", inst->src_file, inst->src_line, inst->src_col);
    } else {
        fprintf(out, " (الموقع: %s:%d)", inst->src_file, inst->src_line);
    }
}

static const char* ssa_safe_str(const char* s) {
    return s ? s : "<غير_معروف>";
}

static int ir_func_param_index_for_reg(const IRFunc* func, int reg_num) {
    if (!func) return -1;
    for (int i = 0; i < func->param_count; i++) {
        if (func->params[i].reg == reg_num) return i;
    }
    return -1;
}

static void ssa_format_reg(char* buf, size_t cap, const IRFunc* func, int reg_num) {
    if (!buf || cap == 0) return;

    char num_buf[64];
    int param_index = ir_func_param_index_for_reg(func, reg_num);
    if (param_index >= 0) {
        snprintf(buf, cap, "%%معامل%s", int_to_arabic_numerals(param_index, num_buf));
        return;
    }

    snprintf(buf, cap, "%%م%s", int_to_arabic_numerals(reg_num, num_buf));
}

static void ssa_report(SSAVerifyDiag* diag,
                       const IRFunc* func,
                       const IRBlock* block,
                       const IRInst* inst,
                       const char* fmt,
                       ...) {
    if (!diag) return;
    if (!diag->out) diag->out = stderr;

    if (diag->error_count >= diag->error_cap) return;

    fprintf(diag->out, "خطأ SSA: @%s | %s",
            ssa_safe_str(func ? func->name : NULL),
            ssa_safe_str(block ? block->label : NULL));

    if (inst) {
        fprintf(diag->out, " | %s", ir_op_to_arabic(inst->op));
    }

    fprintf(diag->out, ": ");

    va_list args;
    va_start(args, fmt);
    vfprintf(diag->out, fmt, args);
    va_end(args);

    if (inst) {
        ssa_print_loc(diag->out, inst);
    }

    fputc('\n', diag->out);

    diag->error_count++;
    if (diag->error_count == diag->error_cap) {
        fprintf(diag->out, "ملاحظة: تم الوصول إلى حد الأخطاء (%d). سيتم إخفاء الباقي.\n",
                diag->error_cap);
    }
}

static void* ssa_safe_calloc(size_t n, size_t size) {
    if (n == 0 || size == 0) return NULL;
    return calloc(n, size);
}

// ============================================================================
// مساعدات عامة (Helpers)
// ============================================================================

static int ir_func_max_block_id_local(const IRFunc* func) {
    int max_id = -1;
    if (!func) return 0;

    for (const IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }

    return max_id + 1;
}

static int ir_block_dominates_local(const IRFunc* func, const IRBlock* a, const IRBlock* b) {
    if (!func || !a || !b) return 0;
    if (a == b) return 1;

    int max_steps = func->block_count + 4;
    const IRBlock* cur = b;
    while (cur && max_steps-- > 0) {
        if (cur == a) return 1;
        if (!cur->idom) return 0;
        if (cur->idom == cur) break;
        cur = cur->idom;
    }
    return 0;
}

static int ir_block_pred_index(const IRBlock* block, const IRBlock* pred) {
    if (!block || !pred) return -1;
    for (int i = 0; i < block->pred_count; i++) {
        if (block->preds[i] == pred) return i;
    }
    return -1;
}

static void ssa_compute_reachable(const IRFunc* func, int max_block_id, unsigned char* reachable) {
    if (!func || !reachable) return;
    if (!func->entry) return;
    if (max_block_id <= 0) return;

    IRBlock** stack = (IRBlock**)malloc((size_t)max_block_id * sizeof(IRBlock*));
    if (!stack) return;

    int top = 0;
    if (func->entry->id >= 0 && func->entry->id < max_block_id) {
        reachable[func->entry->id] = 1;
    }
    stack[top++] = func->entry;

    while (top > 0) {
        IRBlock* b = stack[--top];
        if (!b) continue;
        if (b->id < 0 || b->id >= max_block_id) continue;

        for (int i = 0; i < b->succ_count; i++) {
            IRBlock* s = b->succs[i];
            if (!s) continue;
            if (s->id < 0 || s->id >= max_block_id) continue;
            if (!reachable[s->id]) {
                reachable[s->id] = 1;
                if (top < max_block_id) {
                    stack[top++] = s;
                }
            }
        }
    }

    free(stack);
}

static int ssa_scan_max_reg(const IRFunc* func) {
    int max_reg = -1;
    if (!func) return -1;

    for (int i = 0; i < func->param_count; i++) {
        if (func->params[i].reg > max_reg) max_reg = func->params[i].reg;
    }

    for (const IRBlock* b = func->blocks; b; b = b->next) {
        for (const IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->dest > max_reg) max_reg = inst->dest;

            for (int o = 0; o < inst->operand_count; o++) {
                IRValue* v = inst->operands[o];
                if (!v) continue;
                if (v->kind == IR_VAL_REG && v->data.reg_num > max_reg) {
                    max_reg = v->data.reg_num;
                }
            }

            if (inst->op == IR_OP_CALL) {
                for (int a = 0; a < inst->call_arg_count; a++) {
                    IRValue* v = inst->call_args ? inst->call_args[a] : NULL;
                    if (!v) continue;
                    if (v->kind == IR_VAL_REG && v->data.reg_num > max_reg) {
                        max_reg = v->data.reg_num;
                    }
                }
            }

            if (inst->op == IR_OP_PHI) {
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                    IRValue* v = e->value;
                    if (!v) continue;
                    if (v->kind == IR_VAL_REG && v->data.reg_num > max_reg) {
                        max_reg = v->data.reg_num;
                    }
                }
            }
        }
    }

    return max_reg;
}

// ============================================================================
// التحقق الأساسي (Core Verification)
// ============================================================================

static void ssa_check_reg_use_dom(SSAVerifyDiag* diag,
                                 const IRFunc* func,
                                 const IRBlock* use_block,
                                 const IRInst* use_inst,
                                 int use_inst_index,
                                 int reg_num,
                                 IRBlock** def_block_by_reg,
                                 int* def_inst_index_by_reg,
                                 unsigned char* reachable) {
    if (!diag || !func || !use_block || !def_block_by_reg || !def_inst_index_by_reg || !reachable) return;
    if (reg_num < 0) return;

    IRBlock* def_block = def_block_by_reg[reg_num];
    if (!def_block) {
        char reg_buf[128];
        ssa_format_reg(reg_buf, sizeof(reg_buf), func, reg_num);
        ssa_report(diag, func, use_block, use_inst, "استعمال سجل غير معرّف: %s", reg_buf);
        return;
    }

    if (use_block->id >= 0 && reachable[use_block->id]) {
        if (!ir_block_dominates_local(func, def_block, use_block)) {
            char reg_buf[128];
            ssa_format_reg(reg_buf, sizeof(reg_buf), func, reg_num);
            ssa_report(diag, func, use_block, use_inst,
                       "تعريف السجل لا يسيطر على الاستعمال: %s", reg_buf);
            return;
        }

        if (def_block == use_block) {
            int def_index = def_inst_index_by_reg[reg_num];
            if (def_index >= 0 && def_index >= use_inst_index) {
                char reg_buf[128];
                ssa_format_reg(reg_buf, sizeof(reg_buf), func, reg_num);
                ssa_report(diag, func, use_block, use_inst,
                           "استعمال السجل قبل تعريفه داخل نفس الكتلة: %s", reg_buf);
                return;
            }
        }
    }
}

static void ssa_check_phi(SSAVerifyDiag* diag,
                          const IRFunc* func,
                          const IRBlock* block,
                          const IRInst* phi,
                          IRBlock** def_block_by_reg,
                          int* def_inst_index_by_reg,
                          unsigned char* reachable) {
    if (!diag || !func || !block || !phi) return;
    if (phi->op != IR_OP_PHI) return;

    if (block->id >= 0 && !reachable[block->id]) {
        return;
    }

    int pred_count = block->pred_count;
    int* seen = (int*)ssa_safe_calloc((size_t)pred_count, sizeof(int));
    if (pred_count > 0 && !seen) {
        ssa_report(diag, func, block, phi, "فشل تخصيص ذاكرة للتحقق من `فاي`.");
        return;
    }

    int entry_count = 0;
    for (IRPhiEntry* e = phi->phi_entries; e; e = e->next) {
        entry_count++;

        if (!e->block || !e->value) {
            ssa_report(diag, func, block, phi, "مدخل `فاي` غير صالح (كتلة/قيمة فارغة).");
            continue;
        }

        int idx = ir_block_pred_index(block, e->block);
        if (idx < 0) {
            ssa_report(diag, func, block, phi, "مدخل `فاي` يشير إلى كتلة ليست سابقاً: %s",
                       ssa_safe_str(e->block->label));
            continue;
        }

        if (seen && seen[idx]) {
            ssa_report(diag, func, block, phi, "مدخل `فاي` مكرر لنفس السابق: %s",
                       ssa_safe_str(e->block->label));
            continue;
        }
        if (seen) seen[idx] = 1;

        if (e->value->kind == IR_VAL_REG) {
            int reg_num = e->value->data.reg_num;
            if (reg_num >= 0) {
                if (!def_block_by_reg || !def_inst_index_by_reg) {
                    ssa_report(diag, func, block, phi, "بيانات تعريف السجلات غير متاحة للتحقق.");
                } else {
                    IRBlock* def_block = def_block_by_reg[reg_num];
                    if (!def_block) {
                        char reg_buf[128];
                        ssa_format_reg(reg_buf, sizeof(reg_buf), func, reg_num);
                        ssa_report(diag, func, block, phi, "قيمة `فاي` تستخدم سجل غير معرّف: %s", reg_buf);
                    } else if (!ir_block_dominates_local(func, def_block, e->block)) {
                        char reg_buf[128];
                        ssa_format_reg(reg_buf, sizeof(reg_buf), func, reg_num);
                        ssa_report(diag, func, block, phi,
                                   "قيمة `فاي` لا يسيطر تعريفها على الحافة (السابق %s): %s",
                                   ssa_safe_str(e->block->label), reg_buf);
                    }
                }
            }
        }
    }

    if (pred_count == 0 && entry_count > 0) {
        ssa_report(diag, func, block, phi, "تعليمة `فاي` داخل كتلة بلا سوابق.");
    }

    for (int i = 0; i < pred_count; i++) {
        if (seen && !seen[i]) {
            IRBlock* p = block->preds ? block->preds[i] : NULL;
            ssa_report(diag, func, block, phi, "مدخل `فاي` مفقود للسابق: %s",
                       ssa_safe_str(p ? p->label : NULL));
        }
    }

    if (seen) free(seen);
}

bool ir_func_verify_ssa(IRFunc* func, FILE* out) {
    SSAVerifyDiag diag = {0};
    diag.out = out ? out : stderr;
    diag.error_cap = SSA_VERIFY_MAX_ERRORS;

    if (!func) return false;
    if (func->is_prototype) return true;

    if (!func->entry) {
        ssa_report(&diag, func, NULL, NULL, "الدالة تحتوي جسماً بدون كتلة دخول (entry).");
        return false;
    }

    if (!ir_func_validate_cfg(func)) {
        ssa_report(&diag, func, func->entry, func->entry ? func->entry->last : NULL,
                   "CFG غير صالح؛ لا يمكن التحقق من SSA.");
        return false;
    }

    ir_func_compute_dominators(func);

    int max_block_id = ir_func_max_block_id_local(func);
    if (max_block_id <= 0) {
        ssa_report(&diag, func, func->entry, NULL, "فشل حساب معرفات الكتل.");
        return false;
    }

    unsigned char* reachable = (unsigned char*)ssa_safe_calloc((size_t)max_block_id, sizeof(unsigned char));
    if (!reachable) {
        ssa_report(&diag, func, func->entry, NULL, "فشل تخصيص ذاكرة لقائمة الوصول (reachable).");
        return false;
    }

    ssa_compute_reachable(func, max_block_id, reachable);

    int max_reg = ssa_scan_max_reg(func);
    if (max_reg < 0) {
        free(reachable);
        return true;
    }

    size_t reg_count = (size_t)max_reg + 1;

    IRBlock** def_block_by_reg = (IRBlock**)ssa_safe_calloc(reg_count, sizeof(IRBlock*));
    IRInst** def_inst_by_reg = (IRInst**)ssa_safe_calloc(reg_count, sizeof(IRInst*));
    int* def_inst_index_by_reg = (int*)ssa_safe_calloc(reg_count, sizeof(int));
    int* def_is_param = (int*)ssa_safe_calloc(reg_count, sizeof(int));

    if (!def_block_by_reg || !def_inst_by_reg || !def_inst_index_by_reg || !def_is_param) {
        ssa_report(&diag, func, func->entry, NULL, "فشل تخصيص ذاكرة لبيانات تعريف السجلات.");
        free(reachable);
        if (def_block_by_reg) free(def_block_by_reg);
        if (def_inst_by_reg) free(def_inst_by_reg);
        if (def_inst_index_by_reg) free(def_inst_index_by_reg);
        if (def_is_param) free(def_is_param);
        return false;
    }

    for (size_t i = 0; i < reg_count; i++) {
        def_inst_index_by_reg[i] = -2;
    }

    // ------------------------------------------------------------------------
    // 1) إدراج تعريفات المعاملات كتعريفات أولية
    // ------------------------------------------------------------------------
    for (int i = 0; i < func->param_count; i++) {
        int r = func->params[i].reg;
        if (r < 0 || (size_t)r >= reg_count) continue;
        def_block_by_reg[r] = func->entry;
        def_inst_by_reg[r] = NULL;
        def_inst_index_by_reg[r] = -1;
        def_is_param[r] = 1;
    }

    // ------------------------------------------------------------------------
    // 2) جمع التعريفات: تعريف واحد لكل سجل
    // ------------------------------------------------------------------------
    for (IRBlock* b = func->blocks; b; b = b->next) {
        int inst_index = 0;
        for (IRInst* inst = b->first; inst; inst = inst->next, inst_index++) {
            if (inst->dest < 0) continue;
            int r = inst->dest;
            if (r < 0 || (size_t)r >= reg_count) continue;

            if (def_block_by_reg[r]) {
                char reg_buf[128];
                ssa_format_reg(reg_buf, sizeof(reg_buf), func, r);
                if (def_is_param[r]) {
                    ssa_report(&diag, func, b, inst, "إعادة تعريف سجل معامل (غير مسموح): %s", reg_buf);
                } else {
                    ssa_report(&diag, func, b, inst, "تعريف مكرر للسجل: %s", reg_buf);
                }
                continue;
            }

            def_block_by_reg[r] = b;
            def_inst_by_reg[r] = inst;
            def_inst_index_by_reg[r] = inst_index;
        }
    }

    // ------------------------------------------------------------------------
    // 3) التحقق من الاستعمالات + سيطرة التعريف + `فاي`
    // ------------------------------------------------------------------------
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id < 0 || b->id >= max_block_id) continue;

        int non_phi_seen = 0;
        int inst_index = 0;

        for (IRInst* inst = b->first; inst; inst = inst->next, inst_index++) {
            if (inst->op == IR_OP_PHI) {
                if (non_phi_seen) {
                    ssa_report(&diag, func, b, inst, "تعليمة `فاي` ليست في بداية الكتلة.");
                }
                ssa_check_phi(&diag, func, b, inst, def_block_by_reg, def_inst_index_by_reg, reachable);
                continue;
            }

            non_phi_seen = 1;

            if (b->id >= 0 && !reachable[b->id]) {
                continue;
            }

            for (int o = 0; o < inst->operand_count; o++) {
                IRValue* v = inst->operands[o];
                if (!v) continue;
                if (v->kind != IR_VAL_REG) continue;
                int reg_num = v->data.reg_num;
                if (reg_num < 0 || (size_t)reg_num >= reg_count) continue;

                ssa_check_reg_use_dom(&diag, func, b, inst, inst_index,
                                      reg_num, def_block_by_reg, def_inst_index_by_reg, reachable);
            }

            if (inst->op == IR_OP_CALL) {
                for (int a = 0; a < inst->call_arg_count; a++) {
                    IRValue* v = inst->call_args ? inst->call_args[a] : NULL;
                    if (!v) continue;
                    if (v->kind != IR_VAL_REG) continue;
                    int reg_num = v->data.reg_num;
                    if (reg_num < 0 || (size_t)reg_num >= reg_count) continue;

                    ssa_check_reg_use_dom(&diag, func, b, inst, inst_index,
                                          reg_num, def_block_by_reg, def_inst_index_by_reg, reachable);
                }
            }
        }
    }

    free(reachable);
    free(def_block_by_reg);
    free(def_inst_by_reg);
    free(def_inst_index_by_reg);
    free(def_is_param);

    return diag.error_count == 0;
}

bool ir_module_verify_ssa(IRModule* module, FILE* out) {
    if (!module) return false;

    // المتحقق قد ينشئ بنى مساعدة؛ اجعل السياق مضبوطاً.
    ir_module_set_current(module);

    int ok = 1;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        if (!ir_func_verify_ssa(f, out)) {
            ok = 0;
        }
    }
    return ok ? true : false;
}
