/**
 * @file ir_defuse.c
 * @brief تنفيذ سلاسل التعريف/الاستعمال (Def-Use) لسجلات SSA.
 */

#include "ir_defuse.h"

#include <stdlib.h>
#include <string.h>

static int ir_max_i(int a, int b) {
    return (a > b) ? a : b;
}

static int ir_scan_max_reg(IRFunc* func) {
    if (!func) return 0;

    int max_reg = func->next_reg;

    for (int i = 0; i < func->param_count; i++) {
        max_reg = ir_max_i(max_reg, func->params[i].reg + 1);
    }

    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->dest >= 0) {
                max_reg = ir_max_i(max_reg, inst->dest + 1);
            }

            for (int k = 0; k < inst->operand_count; k++) {
                IRValue* v = inst->operands[k];
                if (v && v->kind == IR_VAL_REG) {
                    max_reg = ir_max_i(max_reg, v->data.reg_num + 1);
                }
            }

            if (inst->op == IR_OP_CALL && inst->call_args) {
                for (int k = 0; k < inst->call_arg_count; k++) {
                    IRValue* v = inst->call_args[k];
                    if (v && v->kind == IR_VAL_REG) {
                        max_reg = ir_max_i(max_reg, v->data.reg_num + 1);
                    }
                }
            }

            if (inst->op == IR_OP_PHI) {
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                    IRValue* v = e->value;
                    if (v && v->kind == IR_VAL_REG) {
                        max_reg = ir_max_i(max_reg, v->data.reg_num + 1);
                    }
                }
            }
        }
    }

    return max_reg;
}

static int ir_count_reg_uses(IRFunc* func) {
    if (!func) return 0;

    int count = 0;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            for (int k = 0; k < inst->operand_count; k++) {
                IRValue* v = inst->operands[k];
                if (v && v->kind == IR_VAL_REG) count++;
            }

            if (inst->op == IR_OP_CALL && inst->call_args) {
                for (int k = 0; k < inst->call_arg_count; k++) {
                    IRValue* v = inst->call_args[k];
                    if (v && v->kind == IR_VAL_REG) count++;
                }
            }

            if (inst->op == IR_OP_PHI) {
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                    IRValue* v = e->value;
                    if (v && v->kind == IR_VAL_REG) count++;
                }
            }
        }
    }

    return count;
}

IRDefUse* ir_defuse_build(IRFunc* func) {
    if (!func) return NULL;

    IRDefUse* du = (IRDefUse*)calloc(1, sizeof(IRDefUse));
    if (!du) return NULL;

    du->built_epoch = func->ir_epoch;
    du->max_reg = ir_scan_max_reg(func);
    if (du->max_reg < 0) du->max_reg = 0;

    du->def_inst_by_reg = (IRInst**)calloc((size_t)du->max_reg, sizeof(IRInst*));
    du->def_is_param = (unsigned char*)calloc((size_t)du->max_reg, sizeof(unsigned char));
    du->uses_by_reg = (IRUse**)calloc((size_t)du->max_reg, sizeof(IRUse*));

    if (!du->def_inst_by_reg || !du->def_is_param || !du->uses_by_reg) {
        ir_defuse_free(du);
        return NULL;
    }

    // تعريفات المعاملات.
    for (int i = 0; i < func->param_count; i++) {
        int r = func->params[i].reg;
        if (r >= 0 && r < du->max_reg) {
            du->def_is_param[r] = 1;
        }
    }

    // تعريفات التعليمات (dest).
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            int d = inst->dest;
            if (d < 0 || d >= du->max_reg) continue;

            if (du->def_inst_by_reg[d] && du->def_inst_by_reg[d] != inst) {
                // تعريف مكرر لنفس السجل (ليس SSA صحيحاً).
                du->has_duplicate_defs = true;
            }
            du->def_inst_by_reg[d] = inst;
        }
    }

    // الاستعمالات.
    du->uses_storage_count = ir_count_reg_uses(func);
    if (du->uses_storage_count < 0) du->uses_storage_count = 0;

    if (du->uses_storage_count > 0) {
        du->uses_storage = (IRUse*)calloc((size_t)du->uses_storage_count, sizeof(IRUse));
        if (!du->uses_storage) {
            ir_defuse_free(du);
            return NULL;
        }
    }

    int idx = 0;
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            for (int k = 0; k < inst->operand_count; k++) {
                IRValue* v = inst->operands[k];
                if (!v || v->kind != IR_VAL_REG) continue;

                int r = v->data.reg_num;
                if (r < 0 || r >= du->max_reg) continue;

                IRUse* u = &du->uses_storage[idx++];
                u->slot = &inst->operands[k];
                u->next = du->uses_by_reg[r];
                du->uses_by_reg[r] = u;
            }

            if (inst->op == IR_OP_CALL && inst->call_args) {
                for (int k = 0; k < inst->call_arg_count; k++) {
                    IRValue* v = inst->call_args[k];
                    if (!v || v->kind != IR_VAL_REG) continue;

                    int r = v->data.reg_num;
                    if (r < 0 || r >= du->max_reg) continue;

                    IRUse* u = &du->uses_storage[idx++];
                    u->slot = &inst->call_args[k];
                    u->next = du->uses_by_reg[r];
                    du->uses_by_reg[r] = u;
                }
            }

            if (inst->op == IR_OP_PHI) {
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                    IRValue* v = e->value;
                    if (!v || v->kind != IR_VAL_REG) continue;

                    int r = v->data.reg_num;
                    if (r < 0 || r >= du->max_reg) continue;

                    IRUse* u = &du->uses_storage[idx++];
                    u->slot = &e->value;
                    u->next = du->uses_by_reg[r];
                    du->uses_by_reg[r] = u;
                }
            }
        }
    }

    // إذا كان هناك عدم تطابق في العدّ، اترك du صالحاً ولكن لا تُعطّل.
    (void)idx;

    return du;
}

void ir_defuse_free(IRDefUse* du) {
    if (!du) return;
    if (du->def_inst_by_reg) free(du->def_inst_by_reg);
    if (du->def_is_param) free(du->def_is_param);
    if (du->uses_by_reg) free(du->uses_by_reg);
    if (du->uses_storage) free(du->uses_storage);
    free(du);
}

void ir_func_invalidate_defuse(IRFunc* func) {
    if (!func) return;
    // لا نُحرِّر هنا لتجنب UAF أثناء التمريرات؛ نكتفي بزيادة epoch.
    func->ir_epoch++;
}

IRDefUse* ir_func_get_defuse(IRFunc* func, bool rebuild) {
    if (!func) return NULL;

    if (!rebuild && func->def_use && func->def_use->built_epoch == func->ir_epoch) {
        return func->def_use;
    }

    if (func->def_use) {
        ir_defuse_free(func->def_use);
        func->def_use = NULL;
    }
    func->def_use = ir_defuse_build(func);
    return func->def_use;
}
