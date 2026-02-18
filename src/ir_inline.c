/**
 * @file ir_inline.c
 * @brief تنفيذ تضمين الدوال (Inlining) — v0.3.2.7.2
 */

#include "ir_inline.h"

#include "ir_analysis.h"
#include "ir_defuse.h"
#include "ir_mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// إعدادات محافظة (Conservative Heuristics)
// ============================================================================

#define IR_INLINE_MAX_INST 24

typedef struct {
    IRFunc* caller;
    IRBlock* call_block;
    IRInst* call_inst;
    IRFunc* callee;
} IRInlineSite;

static int ir_inline_has_phi(IRFunc* f) {
    if (!f) return 0;
    for (IRBlock* b = f->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->op == IR_OP_PHI) return 1;
        }
    }
    return 0;
}

static int ir_inline_count_nonterm_insts(IRFunc* f) {
    if (!f) return 0;
    int count = 0;
    for (IRBlock* b = f->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (!inst) continue;
            if (inst == b->last) continue;
            if (inst->op == IR_OP_BR || inst->op == IR_OP_BR_COND || inst->op == IR_OP_RET) continue;
            count++;
        }
    }
    return count;
}

static int ir_inline_is_recursive(IRFunc* f) {
    if (!f || !f->name) return 0;
    for (IRBlock* b = f->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->op != IR_OP_CALL) continue;
            if (inst->call_target && strcmp(inst->call_target, f->name) == 0) return 1;
        }
    }
    return 0;
}

static int ir_inline_module_has_any_phi(IRModule* m) {
    if (!m) return 0;
    for (IRFunc* f = m->funcs; f; f = f->next) {
        if (f->is_prototype) continue;
        if (ir_inline_has_phi(f)) return 1;
    }
    return 0;
}

static IRFunc* ir_inline_find_callee(IRModule* m, const char* name) {
    if (!m || !name) return NULL;
    return ir_module_find_func(m, name);
}

// ============================================================================
// قيمة/سجل: خرائط التضمين (Value/Reg Mapping)
// ============================================================================

static int ir_inline_max_reg_for_func(IRFunc* f) {
    if (!f) return 0;
    int max = f->next_reg;
    for (int i = 0; i < f->param_count; i++) {
        if (f->params[i].reg + 1 > max) max = f->params[i].reg + 1;
    }
    for (IRBlock* b = f->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->dest >= 0 && inst->dest + 1 > max) max = inst->dest + 1;
            for (int k = 0; k < inst->operand_count; k++) {
                IRValue* v = inst->operands[k];
                if (v && v->kind == IR_VAL_REG && v->data.reg_num + 1 > max) {
                    max = v->data.reg_num + 1;
                }
            }
            if (inst->op == IR_OP_CALL && inst->call_args) {
                for (int k = 0; k < inst->call_arg_count; k++) {
                    IRValue* v = inst->call_args[k];
                    if (v && v->kind == IR_VAL_REG && v->data.reg_num + 1 > max) {
                        max = v->data.reg_num + 1;
                    }
                }
            }
        }
    }
    return max;
}

static int ir_inline_map_reg(int* reg_map, int map_len, int old_reg) {
    if (!reg_map || map_len <= 0 || old_reg < 0 || old_reg >= map_len) return -1;
    return reg_map[old_reg];
}

static void ir_inline_set_reg(int* reg_map, int map_len, int old_reg, int new_reg) {
    if (!reg_map || map_len <= 0 || old_reg < 0 || old_reg >= map_len) return;
    reg_map[old_reg] = new_reg;
}

typedef struct {
    IRBlock* from;
    IRBlock* to;
} IRBlockMapEntry;

static IRBlock* ir_inline_map_block(IRBlockMapEntry* bm, int count, IRBlock* from) {
    if (!bm || count <= 0 || !from) return NULL;
    for (int i = 0; i < count; i++) {
        if (bm[i].from == from) return bm[i].to;
    }
    return NULL;
}

static IRValue* ir_inline_clone_value(IRValue* v,
                                      IRBlockMapEntry* bm, int bm_count,
                                      int* reg_map, int reg_map_len,
                                      IRType** old_reg_types, int old_reg_types_len,
                                      IRFunc* caller) {
    if (!v) return NULL;

    switch (v->kind) {
        case IR_VAL_CONST_INT:
        case IR_VAL_CONST_STR:
            return v;
        case IR_VAL_GLOBAL:
        case IR_VAL_FUNC:
            return v;
        case IR_VAL_BLOCK: {
            IRBlock* nb = ir_inline_map_block(bm, bm_count, v->data.block);
            if (!nb) return NULL;
            return ir_value_block(nb);
        }
        case IR_VAL_REG: {
            if (!reg_map || reg_map_len <= 0 || v->data.reg_num < 0 || v->data.reg_num >= reg_map_len) {
                return NULL;
            }

            int nr = reg_map[v->data.reg_num];
            if (nr < 0) {
                // قد يحدث استعمال سجل قبل تعريفه عند استنساخ الكتل بترتيب القائمة.
                // لذلك ننشئ الربط عند أول استعمال لضمان نجاح التضمين.
                if (!caller) return NULL;
                nr = ir_func_alloc_reg(caller);
                reg_map[v->data.reg_num] = nr;
            }

            IRType* t = v->type;
            if (!t && old_reg_types && v->data.reg_num >= 0 && v->data.reg_num < old_reg_types_len) {
                t = old_reg_types[v->data.reg_num];
            }
            if (!t) t = IR_TYPE_I64_T;
            return ir_value_reg(nr, t);
        }
        default:
            return NULL;
    }
}

// ============================================================================
// اختيار مرشح للتضمين (Pick Candidate)
// ============================================================================

typedef struct {
    IRFunc* callee;
    int call_count;
} IRCallCount;

static IRCallCount* ir_inline_build_call_counts(IRModule* m, int* out_count) {
    if (out_count) *out_count = 0;
    if (!m) return NULL;

    int fn_count = 0;
    for (IRFunc* f = m->funcs; f; f = f->next) fn_count++;
    if (fn_count <= 0) return NULL;

    IRCallCount* arr = (IRCallCount*)calloc((size_t)fn_count, sizeof(IRCallCount));
    if (!arr) return NULL;

    int idx = 0;
    for (IRFunc* f = m->funcs; f; f = f->next) {
        arr[idx].callee = f;
        arr[idx].call_count = 0;
        idx++;
    }

    // Scan all calls.
    for (IRFunc* caller = m->funcs; caller; caller = caller->next) {
        if (caller->is_prototype) continue;

        for (IRBlock* b = caller->blocks; b; b = b->next) {
            for (IRInst* inst = b->first; inst; inst = inst->next) {
                if (inst->op != IR_OP_CALL) continue;
                if (!inst->call_target) continue;

                IRFunc* callee = ir_inline_find_callee(m, inst->call_target);
                if (!callee) continue;

                for (int i = 0; i < fn_count; i++) {
                    if (arr[i].callee == callee) {
                        arr[i].call_count++;
                        break;
                    }
                }
            }
        }
    }

    if (out_count) *out_count = fn_count;
    return arr;
}

static int ir_inline_is_eligible(IRCallCount* counts, int count_len,
                                IRFunc* callee, IRFunc* caller) {
    if (!callee || !caller) return 0;
    if (callee->is_prototype) return 0;
    if (!callee->entry) return 0;
    if (callee == caller) return 0;
    if (!callee->name) return 0;

    if (ir_inline_has_phi(callee)) return 0;
    if (ir_inline_is_recursive(callee)) return 0;
    if (ir_inline_count_nonterm_insts(callee) > IR_INLINE_MAX_INST) return 0;

    // must be exactly one call site
    int cc = 0;
    for (int i = 0; i < count_len; i++) {
        if (counts[i].callee == callee) {
            cc = counts[i].call_count;
            break;
        }
    }
    if (cc != 1) return 0;

    return 1;
}

static int ir_inline_find_unique_call_site(IRModule* m, IRFunc* callee, IRInlineSite* out_site) {
    if (!m || !callee || !out_site) return 0;
    memset(out_site, 0, sizeof(*out_site));

    for (IRFunc* caller = m->funcs; caller; caller = caller->next) {
        if (caller->is_prototype) continue;

        for (IRBlock* b = caller->blocks; b; b = b->next) {
            for (IRInst* inst = b->first; inst; inst = inst->next) {
                if (inst->op != IR_OP_CALL) continue;
                if (!inst->call_target) continue;
                if (strcmp(inst->call_target, callee->name) != 0) continue;

                out_site->caller = caller;
                out_site->call_block = b;
                out_site->call_inst = inst;
                out_site->callee = callee;
                return 1;
            }
        }
    }

    return 0;
}

// ============================================================================
// تنفيذ التضمين (Inline Expansion)
// ============================================================================

static void ir_inline_move_after(IRBlock* from, IRInst* start, IRBlock* to) {
    if (!from || !to || !start) return;
    IRInst* inst = start;
    while (inst) {
        IRInst* next = inst->next;
        ir_block_remove_inst(from, inst);
        ir_block_append(to, inst);
        inst = next;
    }
}

static IRInst* ir_inline_emit_copy(IRBlock* b, IRType* t, int dest, IRValue* src) {
    if (!b || !t || dest < 0 || !src) return NULL;
    IRInst* c = ir_inst_new(IR_OP_COPY, t, dest);
    if (!c) return NULL;
    ir_inst_add_operand(c, src);
    ir_block_append(b, c);
    return c;
}

static int ir_inline_clone_block_body(IRFunc* caller,
                                     IRBlock* new_block,
                                     IRBlockMapEntry* bm, int bm_count,
                                     int* reg_map, int reg_map_len,
                                     IRType** old_reg_types, int old_reg_types_len,
                                     IRBlock* cont,
                                     int ret_slot_reg,
                                     IRType* ret_type,
                                     IRBlock* old_block) {
    if (!caller || !new_block || !bm || bm_count <= 0 || !reg_map || reg_map_len <= 0 ||
        !cont || !old_block) {
        return 0;
    }

    for (IRInst* inst = old_block->first; inst; inst = inst->next) {
        if (inst->op == IR_OP_PHI) {
            return 0;
        }

        if (inst->op == IR_OP_RET) {
            // استبدال الرجوع بقفز إلى cont (+ تخزين القيمة إن لزم).
                if (ret_type && ret_type->kind != IR_TYPE_VOID) {
                    if (inst->operand_count < 1 || !inst->operands[0]) return 0;
                    IRValue* rv = ir_inline_clone_value(inst->operands[0], bm, bm_count,
                                                        reg_map, reg_map_len,
                                                        old_reg_types, old_reg_types_len,
                                                        caller);
                    if (!rv) return 0;

                IRType* ret_ptr_type = ir_type_ptr(ret_type);
                if (!ret_ptr_type) return 0;
                IRValue* ptrv = ir_value_reg(ret_slot_reg, ret_ptr_type);
                IRInst* st = ir_inst_store(rv, ptrv);
                if (!st) return 0;
                ir_inst_set_loc(st, inst->src_file, inst->src_line, inst->src_col);
                ir_inst_set_dbg_name(st, inst->dbg_name);
                ir_block_append(new_block, st);
            }

            IRInst* br = ir_inst_br(cont);
            if (!br) return 0;
            ir_inst_set_loc(br, inst->src_file, inst->src_line, inst->src_col);
            ir_block_append(new_block, br);
            continue;
        }

        if (inst->op == IR_OP_BR) {
            if (inst->operand_count < 1 || !inst->operands[0] || inst->operands[0]->kind != IR_VAL_BLOCK) return 0;
            IRBlock* tgt_old = inst->operands[0]->data.block;
            IRBlock* tgt_new = ir_inline_map_block(bm, bm_count, tgt_old);
            if (!tgt_new) return 0;
            IRInst* br = ir_inst_br(tgt_new);
            if (!br) return 0;
            ir_inst_set_loc(br, inst->src_file, inst->src_line, inst->src_col);
            ir_block_append(new_block, br);
            continue;
        }

        if (inst->op == IR_OP_BR_COND) {
            if (inst->operand_count < 3) return 0;
            IRValue* cond = ir_inline_clone_value(inst->operands[0], bm, bm_count,
                                                 reg_map, reg_map_len,
                                                 old_reg_types, old_reg_types_len,
                                                 caller);
            if (!cond) return 0;
            IRBlock* t_old = inst->operands[1] ? inst->operands[1]->data.block : NULL;
            IRBlock* f_old = inst->operands[2] ? inst->operands[2]->data.block : NULL;
            IRBlock* t_new = ir_inline_map_block(bm, bm_count, t_old);
            IRBlock* f_new = ir_inline_map_block(bm, bm_count, f_old);
            if (!t_new || !f_new) return 0;
            IRInst* brc = ir_inst_br_cond(cond, t_new, f_new);
            if (!brc) return 0;
            ir_inst_set_loc(brc, inst->src_file, inst->src_line, inst->src_col);
            ir_block_append(new_block, brc);
            continue;
        }

        // تعليمات عادية / نداء.
        int new_dest = -1;
        if (inst->dest >= 0) {
            new_dest = ir_inline_map_reg(reg_map, reg_map_len, inst->dest);
            if (new_dest < 0) {
                new_dest = ir_func_alloc_reg(caller);
                ir_inline_set_reg(reg_map, reg_map_len, inst->dest, new_dest);
            }
        }

        IRInst* clone = NULL;
        if (inst->op == IR_OP_CALL) {
            // استنساخ CALL مع استنساخ الوسائط.
            IRValue* args_local[64];
            IRValue** args = NULL;
            int ac = inst->call_arg_count;
            if (ac < 0) ac = 0;
            if (ac > 64) return 0;

            for (int i = 0; i < ac; i++) {
                args_local[i] = ir_inline_clone_value(inst->call_args[i], bm, bm_count,
                                                      reg_map, reg_map_len,
                                                      old_reg_types, old_reg_types_len,
                                                      caller);
                if (!args_local[i]) return 0;
            }
            if (ac > 0) {
                args = args_local;
            }

            clone = ir_inst_call(inst->call_target, inst->type, new_dest, args, ac);
        } else {
            clone = ir_inst_new(inst->op, inst->type, new_dest);
            if (clone) {
                clone->cmp_pred = inst->cmp_pred;
                for (int k = 0; k < inst->operand_count; k++) {
                    IRValue* ov = inst->operands[k];
                    IRValue* nv = ir_inline_clone_value(ov, bm, bm_count,
                                                        reg_map, reg_map_len,
                                                        old_reg_types, old_reg_types_len,
                                                        caller);
                    if (!nv) return 0;
                    ir_inst_add_operand(clone, nv);
                }
            }
        }

        if (!clone) return 0;

        ir_inst_set_loc(clone, inst->src_file, inst->src_line, inst->src_col);
        ir_inst_set_dbg_name(clone, inst->dbg_name);
        ir_block_append(new_block, clone);
    }

    return 1;
}

static int ir_inline_expand_site(IRInlineSite* site) {
    if (!site || !site->caller || !site->call_block || !site->call_inst || !site->callee) return 0;

    IRFunc* caller = site->caller;
    IRBlock* call_bb = site->call_block;
    IRInst* call = site->call_inst;
    IRFunc* callee = site->callee;

    // تحقق بدائي.
    if (call->op != IR_OP_CALL || !call->call_target) return 0;
    if (callee->param_count != call->call_arg_count) return 0;

    // إنشاء كتلة استمرار (continuation) ونقل ما بعد call إليها.
    // ملاحظة: يجب أن يكون اسم الكتلة فريداً لأن الطباعة/التصحيح يعتمد على labels.
    int cont_id = ir_func_alloc_block_id(caller);
    char cont_label[64];
    snprintf(cont_label, sizeof(cont_label), "inline_cont_%d", cont_id);
    IRBlock* cont = ir_block_new(cont_label, cont_id);
    if (!cont) return 0;
    ir_func_add_block(caller, cont);

    if (call->next) {
        ir_inline_move_after(call_bb, call->next, cont);
    }

    // إزالة call من مكانه.
    ir_block_remove_inst(call_bb, call);

    // تجهيز خرائط السجلات.
    int reg_map_len = ir_inline_max_reg_for_func(callee);
    if (reg_map_len <= 0) reg_map_len = 1;
    int* reg_map = (int*)malloc((size_t)reg_map_len * sizeof(int));
    if (!reg_map) return 0;
    for (int i = 0; i < reg_map_len; i++) reg_map[i] = -1;

    // جدول أنواع سجلات callee للاستدلال عندما تكون IRValue.type = NULL.
    IRType** old_reg_types = (IRType**)calloc((size_t)reg_map_len, sizeof(IRType*));
    if (!old_reg_types) { free(reg_map); return 0; }
    for (int i = 0; i < callee->param_count; i++) {
        int r = callee->params[i].reg;
        if (r >= 0 && r < reg_map_len) {
            old_reg_types[r] = callee->params[i].type;
        }
    }
    for (IRBlock* b = callee->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->dest >= 0 && inst->dest < reg_map_len) {
                if (!old_reg_types[inst->dest]) {
                    old_reg_types[inst->dest] = inst->type;
                }
            }
        }
    }

    // تجهيز معامل/نسخ معاملات الدالة (param regs).
    for (int i = 0; i < callee->param_count; i++) {
        int old_pr = callee->params[i].reg;
        IRType* pt = callee->params[i].type;
        int new_pr = ir_func_alloc_reg(caller);
        ir_inline_set_reg(reg_map, reg_map_len, old_pr, new_pr);

        IRValue* arg = call->call_args[i];
        if (!arg) { free(reg_map); return 0; }
        (void)ir_inline_emit_copy(call_bb, pt ? pt : IR_TYPE_I64_T, new_pr, arg);
    }

    // معالجة قيمة الإرجاع.
    int ret_slot_reg = -1;
    IRType* ret_type = callee->ret_type;
    int call_dest = call->dest;
    if (ret_type && ret_type->kind != IR_TYPE_VOID) {
        ret_slot_reg = ir_func_alloc_reg(caller);
        IRInst* alloca_ret = ir_inst_alloca(ret_type, ret_slot_reg);
        if (!alloca_ret) { free(reg_map); return 0; }
        ir_block_append(call_bb, alloca_ret);
    }

    // إنشاء كتل جديدة لكل كتلة في callee.
    int callee_blocks = 0;
    for (IRBlock* b = callee->blocks; b; b = b->next) callee_blocks++;
    if (callee_blocks <= 0) { free(reg_map); return 0; }

    IRBlockMapEntry* bm = (IRBlockMapEntry*)calloc((size_t)callee_blocks, sizeof(IRBlockMapEntry));
    if (!bm) { free(reg_map); return 0; }

    int bi = 0;
    for (IRBlock* b = callee->blocks; b; b = b->next) {
        int id = ir_func_alloc_block_id(caller);
        char label[128];
        // اجعل label فريداً حتى لو تم تضمين نفس الدالة أكثر من مرة.
        snprintf(label, sizeof(label), "inl_%s_%d_%d",
                 callee->name ? callee->name : "fn",
                 b->id,
                 id);
        IRBlock* nb = ir_block_new(label, id);
        if (!nb) { free(bm); free(reg_map); return 0; }
        ir_func_add_block(caller, nb);
        bm[bi].from = b;
        bm[bi].to = nb;
        bi++;
    }

    IRBlock* inl_entry = ir_inline_map_block(bm, callee_blocks, callee->entry);
    if (!inl_entry) { free(bm); free(reg_map); return 0; }

    // استبدال مسار التحكم: call_bb ينتهي بقفز إلى inl_entry.
    IRInst* br_into = ir_inst_br(inl_entry);
    if (!br_into) { free(bm); free(reg_map); return 0; }
    ir_block_append(call_bb, br_into);

    // استنساخ تعليمات كل كتلة.
    for (int i = 0; i < callee_blocks; i++) {
        IRBlock* oldb = bm[i].from;
        IRBlock* newb = bm[i].to;
        if (!ir_inline_clone_block_body(caller, newb, bm, callee_blocks,
                                        reg_map, reg_map_len,
                                        old_reg_types, reg_map_len,
                                        cont,
                                        ret_slot_reg, ret_type, oldb)) {
            free(bm);
            free(old_reg_types);
            free(reg_map);
            return 0;
        }
    }

    // تحميل قيمة الإرجاع في cont لتعريف call_dest.
    if (ret_type && ret_type->kind != IR_TYPE_VOID) {
        if (call_dest < 0) {
            free(bm);
            free(reg_map);
            return 0;
        }

        IRType* ret_ptr_type = ir_type_ptr(ret_type);
        if (!ret_ptr_type) {
            free(bm);
            free(old_reg_types);
            free(reg_map);
            return 0;
        }
        IRValue* ptrv = ir_value_reg(ret_slot_reg, ret_ptr_type);
        IRInst* ld = ir_inst_load(ret_type, call_dest, ptrv);
        if (!ld) {
            free(bm);
            free(old_reg_types);
            free(reg_map);
            return 0;
        }
        ir_block_insert_before(cont, cont->first, ld);
    }

    // تحديث CFG/Preds + إبطال التحليلات.
    ir_func_rebuild_preds(caller);
    ir_func_invalidate_defuse(caller);

    free(bm);
    free(old_reg_types);
    free(reg_map);
    return 1;
}

bool ir_inline_run(IRModule* module) {
    if (!module) return false;

    // ضمان تهيئة سياق الساحة قبل أي إنشاء لأنواع/قيم جديدة داخل التضمين.
    ir_module_set_current(module);

    // التضمين يُنفذ قبل Mem2Reg. إذا كانت هناك فاي، نتجاوز لتجنب إعادة التضمين.
    if (ir_inline_module_has_any_phi(module)) {
        return false;
    }

    int changed = 0;
    int progress = 1;
    int guard = 0;

    while (progress && guard++ < 64) {
        progress = 0;

        int count_len = 0;
        IRCallCount* counts = ir_inline_build_call_counts(module, &count_len);
        if (!counts) break;

        IRInlineSite site = {0};
        int found = 0;

        for (int i = 0; i < count_len; i++) {
            IRFunc* callee = counts[i].callee;
            if (!callee) continue;
            if (callee->is_prototype) continue;
            if (counts[i].call_count != 1) continue;

            // اعثر على موقع النداء لمعرفة caller.
            if (!ir_inline_find_unique_call_site(module, callee, &site)) continue;
            if (!site.caller) continue;

            if (!ir_inline_is_eligible(counts, count_len, callee, site.caller)) continue;

            found = 1;
            break;
        }

        free(counts);

        if (!found) break;

        if (ir_inline_expand_site(&site)) {
            progress = 1;
            changed = 1;
        } else {
            // فشل التضمين لموقع مختار: أوقف لتجنب حلقة.
            break;
        }
    }

    return changed ? true : false;
}
