/**
 * @file ir_clone.c
 * @brief تنفيذ استنساخ دوال/كتل IR بعمق.
 */

#include "ir_clone.h"

#include "ir_analysis.h"

#include <stdlib.h>
#include <string.h>

static IRValue* ir_clone_value(IRValue* v, IRBlock** block_map, int map_cap) {
    if (!v) return NULL;

    switch (v->kind) {
        case IR_VAL_CONST_INT:
            return ir_value_const_int(v->data.const_int, v->type);

        case IR_VAL_CONST_STR:
            return ir_value_const_str(v->data.const_str.data, v->data.const_str.id);

        case IR_VAL_REG:
            return ir_value_reg(v->data.reg_num, v->type);

        case IR_VAL_GLOBAL:
            // نحافظ على النوع كما هو.
            return ir_value_global(v->data.global_name,
                                   (v->type && v->type->kind == IR_TYPE_PTR) ? v->type->data.pointee : v->type);

        case IR_VAL_FUNC:
            return ir_value_func_ref(v->data.global_name, v->type);

        case IR_VAL_BLOCK: {
            IRBlock* b = v->data.block;
            if (!b) return ir_value_block(NULL);
            if (b->id >= 0 && b->id < map_cap && block_map) {
                return ir_value_block(block_map[b->id]);
            }
            return ir_value_block(NULL);
        }

        case IR_VAL_NONE:
        default:
            return NULL;
    }
}

static void ir_clone_inst_common(IRInst* dst, IRInst* src) {
    if (!dst || !src) return;

    dst->cmp_pred = src->cmp_pred;
    dst->src_file = src->src_file;
    dst->src_line = src->src_line;
    dst->src_col = src->src_col;
    dst->id = src->id;
}

IRFunc* ir_func_clone(IRModule* module, IRFunc* src, const char* new_name) {
    if (!module || !src) return NULL;

    ir_module_set_current(module);

    IRFunc* f = ir_func_new(new_name ? new_name : src->name, src->ret_type);
    if (!f) return NULL;

    f->is_prototype = src->is_prototype;
    f->next_reg = 0;
    f->next_block_id = 0;
    f->next_inst_id = 0;

    // نسخ المعاملات بنفس الترتيب.
    for (int i = 0; i < src->param_count; i++) {
        ir_func_add_param(f, src->params[i].name, src->params[i].type);
    }

    // ضبط العدّادات كما في المصدر.
    f->next_reg = src->next_reg;
    f->next_block_id = src->next_block_id;
    f->next_inst_id = src->next_inst_id;

    // خريطة الكتل: id -> new block.
    int max_id = -1;
    for (IRBlock* b = src->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }
    int map_cap = max_id + 1;
    if (map_cap < 0) map_cap = 0;

    IRBlock** block_map = NULL;
    if (map_cap > 0) {
        block_map = (IRBlock**)calloc((size_t)map_cap, sizeof(IRBlock*));
        if (!block_map) return NULL;
    }

    // 1) إنشاء كتل جديدة بنفس label/id.
    for (IRBlock* b = src->blocks; b; b = b->next) {
        IRBlock* nb = ir_block_new(b->label, b->id);
        if (!nb) {
            free(block_map);
            return NULL;
        }
        ir_func_add_block(f, nb);
        if (b->id >= 0 && b->id < map_cap) {
            block_map[b->id] = nb;
        }
    }

    // 2) نسخ التعليمات لكل كتلة.
    IRBlock* sb = src->blocks;
    IRBlock* db = f->blocks;
    while (sb && db) {
        for (IRInst* inst = sb->first; inst; inst = inst->next) {
            IRInst* ni = ir_inst_new(inst->op, inst->type, inst->dest);
            if (!ni) {
                free(block_map);
                return NULL;
            }

            ir_clone_inst_common(ni, inst);

            // operands
            ni->operand_count = 0;
            for (int k = 0; k < inst->operand_count; k++) {
                IRValue* ov = inst->operands[k];
                IRValue* cv = ir_clone_value(ov, block_map, map_cap);
                if (cv) {
                    ir_inst_add_operand(ni, cv);
                }
            }

            // call
            if (inst->op == IR_OP_CALL) {
                ni->call_arg_count = inst->call_arg_count;
                ni->call_target = inst->call_target ? ir_arena_strdup(&module->arena, inst->call_target) : NULL;

                if (inst->call_arg_count > 0 && inst->call_args) {
                    ni->call_args = (IRValue**)ir_arena_alloc(&module->arena,
                                                             (size_t)inst->call_arg_count * sizeof(IRValue*),
                                                             _Alignof(IRValue*));
                    if (ni->call_args) {
                        for (int k = 0; k < inst->call_arg_count; k++) {
                            ni->call_args[k] = ir_clone_value(inst->call_args[k], block_map, map_cap);
                        }
                    } else {
                        ni->call_arg_count = 0;
                    }
                }
            }

            // phi entries
            if (inst->op == IR_OP_PHI) {
                // للحفاظ على نفس الترتيب، نجمع أولاً ثم نضيف بالعكس.
                int n = 0;
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) n++;

                IRPhiEntry** entries = NULL;
                if (n > 0) {
                    entries = (IRPhiEntry**)malloc((size_t)n * sizeof(IRPhiEntry*));
                }
                int i = 0;
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
                    if (entries) entries[i++] = e;
                }

                for (int j = n - 1; j >= 0; j--) {
                    IRPhiEntry* e = entries ? entries[j] : NULL;
                    if (!e) continue;
                    IRBlock* ob = e->block;
                    IRBlock* nb = NULL;
                    if (ob && ob->id >= 0 && ob->id < map_cap && block_map) {
                        nb = block_map[ob->id];
                    }
                    IRValue* cv = ir_clone_value(e->value, block_map, map_cap);
                    if (cv && nb) {
                        ir_inst_phi_add(ni, cv, nb);
                    }
                }

                if (entries) free(entries);
            }

            ir_block_append(db, ni);
        }

        sb = sb->next;
        db = db->next;
    }

    // إعادة بناء CFG edges.
    ir_func_rebuild_preds(f);

    // أضفها للوحدة.
    ir_module_add_func(module, f);

    if (block_map) free(block_map);
    return f;
}
