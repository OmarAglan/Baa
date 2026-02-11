/**
 * @file ir_mem2reg.c
 * @brief تمريرة ترقية الذاكرة إلى سجلات (Mem2Reg) — إدراج فاي + إعادة تسمية SSA (v0.3.2.5.2).
 * @version 0.3.2.5.2
 *
 * هذه التمريرة تنفّذ Mem2Reg بالطريقة القياسية:
 * - حساب المسيطرات وحدود السيطرة.
 * - إدراج عقد `فاي` عند نقاط الدمج.
 * - إعادة التسمية في SSA لإزالة `حمل/خزن` وتحويلها إلى قيم SSA.
 *
 * قيود السلامة (الصحة أولاً):
 * - نُرقّي فقط `alloca` الذي لا يهرب مؤشّره:
 *   - لا يُمرَّر المؤشر إلى `نداء` ولا يُستخدم داخل `فاي`.
 *   - لا يُخزَّن المؤشر نفسه كقيمة.
 * - كتلة تعريف المؤشر (الكتلة التي تحتوي `حجز`) تسيطر على كل الاستعمالات
 *   دون استثناء.
 * - يوجد خزن تهيئة داخل نفس كتلة `حجز` قبل أي `حمل` لنفس المؤشر.
 *
 * ملاحظة ملكية الذاكرة:
 * - لا نُعيد استعمال نفس مؤشر IRValue بين تعليمات مختلفة لتجنب التحرير المزدوج.
 *   لذلك نقوم بعمل نسخ للقيم عند إعادة الكتابة.
 */

#include "ir_mem2reg.h"
#include "ir_analysis.h"
#include "ir_mutate.h"

#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// دمج IRPass
// -----------------------------------------------------------------------------

IRPass IR_PASS_MEM2REG = {
    .name = "ترقية_الذاكرة_إلى_سجلات",
    .run = ir_mem2reg_run
};

// -----------------------------------------------------------------------------
// مساعدات: بنية المتغير المرقّى + النسخ الآمن
// -----------------------------------------------------------------------------

typedef struct {
    int ptr_reg;
    IRType* pointee;
    IRInst* alloca_inst;
    IRBlock* alloca_block;

    // اسم المتغير المرتبط بهذا alloca (للتتبع بعد Mem2Reg)
    const char* dbg_name;

    IRInst** phi_in_block; // [max_block_id]

    IRValue** stack;
    int stack_size;
    int stack_cap;
} Mem2RegVar;

static int ir_value_is_reg_num(IRValue* v, int reg_num) {
    return v && v->kind == IR_VAL_REG && v->data.reg_num == reg_num;
}

static IRType* ir_alloca_pointee_type(IRInst* alloca_inst) {
    if (!alloca_inst) return NULL;
    if (!alloca_inst->type) return NULL;
    if (alloca_inst->type->kind != IR_TYPE_PTR) return NULL;
    return alloca_inst->type->data.pointee;
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

static int ir_func_max_block_id_local(IRFunc* func) {
    int max_id = -1;
    if (!func) return 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id > max_id) max_id = b->id;
    }

    return max_id + 1;
}

static int ir_block_dominates(IRBlock* dom, IRBlock* node) {
    if (!dom || !node) return 0;
    if (dom == node) return 1;
    if (!node->idom) return 0;

    IRBlock* cur = node;
    while (cur && cur != cur->idom) {
        if (cur == dom) return 1;
        cur = cur->idom;
    }

    return (cur == dom) ? 1 : 0;
}

// -----------------------------------------------------------------------------
// ملاحظة:
// نستخدم مساعدات التعديل المشتركة من ir_mutate.c لضمان:
// - تعيين parent/id للتعليمات المُدرجة
// - تحديث inst_count بشكل موحّد
// - عدم تحرير كائنات IR الفردية (Arena)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// مساعدات: فحص استعمالات المؤشر (عدم الهروب) + تحقق الأنواع
// -----------------------------------------------------------------------------

static int ir_inst_has_ptr_reg_use(IRInst* inst, int ptr_reg) {
    if (!inst) return 0;

    for (int i = 0; i < inst->operand_count; i++) {
        if (ir_value_is_reg_num(inst->operands[i], ptr_reg)) return 1;
    }

    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            if (ir_value_is_reg_num(inst->call_args[i], ptr_reg)) return 1;
        }
    }

    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            if (ir_value_is_reg_num(e->value, ptr_reg)) return 1;
        }
    }

    return 0;
}

static int ir_inst_ptr_use_is_allowed(IRInst* inst, int ptr_reg) {
    if (!inst) return 0;

    for (int i = 0; i < inst->operand_count; i++) {
        if (!ir_value_is_reg_num(inst->operands[i], ptr_reg)) continue;

        if (inst->op == IR_OP_LOAD) {
            return (i == 0);
        }

        if (inst->op == IR_OP_STORE) {
            if (i != 1) return 0;
            if (inst->operand_count >= 1 && ir_value_is_reg_num(inst->operands[0], ptr_reg)) {
                return 0;
            }
            return 1;
        }

        return 0;
    }

    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            if (ir_value_is_reg_num(inst->call_args[i], ptr_reg)) {
                return 0;
            }
        }
    }

    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            if (ir_value_is_reg_num(e->value, ptr_reg)) {
                return 0;
            }
        }
    }

    return 1;
}

static int ir_alloca_has_init_store_in_block(IRBlock* block, IRInst* alloca_inst,
                                             int ptr_reg, IRType* pointee) {
    if (!block || !alloca_inst || !pointee) return 0;

    int seen_store = 0;
    for (IRInst* inst = alloca_inst->next; inst; inst = inst->next) {
        if (inst->op == IR_OP_STORE &&
            inst->operand_count >= 2 &&
            ir_value_is_reg_num(inst->operands[1], ptr_reg)) {

            IRValue* stored_val = inst->operands[0];
            if (!stored_val || !stored_val->type) return 0;
            if (!ir_types_equal(stored_val->type, pointee)) return 0;

            seen_store = 1;
            continue;
        }

        if (inst->op == IR_OP_LOAD &&
            inst->operand_count >= 1 &&
            ir_value_is_reg_num(inst->operands[0], ptr_reg)) {

            if (!seen_store) return 0;
        }
    }

    return seen_store ? 1 : 0;
}

static int ir_mem2reg_can_promote_alloca(IRFunc* func, IRBlock* alloca_block, IRInst* alloca_inst) {
    if (!func || !alloca_block || !alloca_inst) return 0;
    if (alloca_inst->op != IR_OP_ALLOCA) return 0;
    if (alloca_inst->dest < 0) return 0;

    int ptr_reg = alloca_inst->dest;
    IRType* pointee = ir_alloca_pointee_type(alloca_inst);
    if (!pointee) return 0;

    if (!ir_alloca_has_init_store_in_block(alloca_block, alloca_inst, ptr_reg, pointee)) {
        return 0;
    }

    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (!ir_inst_has_ptr_reg_use(inst, ptr_reg)) continue;

            if (!ir_inst_ptr_use_is_allowed(inst, ptr_reg)) return 0;
            if (!ir_block_dominates(alloca_block, b)) return 0;

            if (inst->op == IR_OP_STORE &&
                inst->operand_count >= 2 &&
                ir_value_is_reg_num(inst->operands[1], ptr_reg)) {

                IRValue* stored_val = inst->operands[0];
                if (!stored_val || !stored_val->type) return 0;
                if (!ir_types_equal(stored_val->type, pointee)) return 0;
            }

            if (inst->op == IR_OP_LOAD &&
                inst->operand_count >= 1 &&
                ir_value_is_reg_num(inst->operands[0], ptr_reg)) {

                if (!inst->type || !ir_types_equal(inst->type, pointee)) return 0;
            }
        }
    }

    return 1;
}

// -----------------------------------------------------------------------------
// مكدس إعادة التسمية في SSA
// -----------------------------------------------------------------------------

static void mem2reg_stack_free_to(Mem2RegVar* v, int new_size) {
    if (!v) return;
    if (new_size < 0) new_size = 0;

    while (v->stack_size > new_size) {
        IRValue* top = v->stack[v->stack_size - 1];
        v->stack[v->stack_size - 1] = NULL;
        v->stack_size--;
        if (top) ir_value_free(top);
    }
}

static int mem2reg_stack_push(Mem2RegVar* v, IRValue* val) {
    if (!v || !val) return 0;

    if (v->stack_size >= v->stack_cap) {
        int new_cap = (v->stack_cap == 0) ? 8 : v->stack_cap * 2;
        IRValue** new_arr = (IRValue**)realloc(v->stack, (size_t)new_cap * sizeof(IRValue*));
        if (!new_arr) return 0;
        v->stack = new_arr;
        v->stack_cap = new_cap;
    }

    v->stack[v->stack_size++] = val;
    return 1;
}

static IRValue* mem2reg_stack_top(Mem2RegVar* v) {
    if (!v || v->stack_size <= 0) return NULL;
    return v->stack[v->stack_size - 1];
}

// -----------------------------------------------------------------------------
// إدراج فاي
// -----------------------------------------------------------------------------

static void mem2reg_collect_def_blocks(IRFunc* func, int ptr_reg,
                                       unsigned char* is_def, int max_id,
                                       int** out_ids, int* out_count) {
    if (!func || !is_def || max_id <= 0 || !out_ids || !out_count) return;

    int* ids = (int*)malloc((size_t)max_id * sizeof(int));
    if (!ids) return;
    int count = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id < 0 || b->id >= max_id) continue;

        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->op != IR_OP_STORE) continue;
            if (inst->operand_count < 2) continue;
            if (!ir_value_is_reg_num(inst->operands[1], ptr_reg)) continue;

            if (!is_def[b->id]) {
                is_def[b->id] = 1;
                ids[count++] = b->id;
            }
        }
    }

    *out_ids = ids;
    *out_count = count;
}

static void mem2reg_insert_phis_for_var(IRFunc* func, IRBlock** block_by_id,
                                        Mem2RegVar* v,
                                        unsigned char* is_def, int* def_ids, int def_count,
                                        int max_id) {
    if (!func || !block_by_id || !v || !is_def || !def_ids || def_count <= 0 || max_id <= 0) return;

    unsigned char* in_work = (unsigned char*)calloc((size_t)max_id, sizeof(unsigned char));
    int* work = (int*)malloc((size_t)max_id * sizeof(int));
    if (!in_work || !work) {
        if (in_work) free(in_work);
        if (work) free(work);
        return;
    }

    int head = 0;
    int tail = 0;

    for (int i = 0; i < def_count; i++) {
        int id = def_ids[i];
        if (id < 0 || id >= max_id) continue;
        work[tail++] = id;
        in_work[id] = 1;
    }

    while (head < tail) {
        int x_id = work[head++];
        if (x_id < 0 || x_id >= max_id) continue;

        IRBlock* x = block_by_id[x_id];
        if (!x) continue;

        for (int i = 0; i < x->dom_frontier_count; i++) {
            IRBlock* y = x->dom_frontier[i];
            if (!y) continue;
            if (y->id < 0 || y->id >= max_id) continue;

            if (!v->phi_in_block[y->id]) {
                int dest = ir_func_alloc_reg(func);
                IRInst* phi = ir_inst_phi(v->pointee, dest);
                if (!phi) continue;

                // ربط الفاي بموقع/اسم المتغير الأصلي (إن وُجد).
                if (v->alloca_inst) {
                    ir_inst_set_loc(phi, v->alloca_inst->src_file, v->alloca_inst->src_line, v->alloca_inst->src_col);
                }
                if (v->dbg_name) {
                    ir_inst_set_dbg_name(phi, v->dbg_name);
                }

                ir_block_insert_phi(y, phi);
                v->phi_in_block[y->id] = phi;

                if (!is_def[y->id] && !in_work[y->id]) {
                    work[tail++] = y->id;
                    in_work[y->id] = 1;
                }
            }
        }
    }

    free(in_work);
    free(work);
}

// -----------------------------------------------------------------------------
// إعادة التسمية في SSA
// -----------------------------------------------------------------------------

typedef struct {
    IRFunc* func;
    IRBlock** block_by_id;
    Mem2RegVar* vars;
    int var_count;
    int* ptr_reg_to_var;
    int ptr_map_size;
    int* child_head;
    int* child_next;
    int max_id;
    int changed;
} Mem2RegRenameCtx;

static void mem2reg_rename_block(Mem2RegRenameCtx* ctx, IRBlock* block) {
    if (!ctx || !block) return;
    if (block->id < 0 || block->id >= ctx->max_id) return;

    int* saved = (int*)malloc((size_t)ctx->var_count * sizeof(int));
    if (!saved) return;

    for (int i = 0; i < ctx->var_count; i++) {
        saved[i] = ctx->vars[i].stack_size;
    }

    // فاي في هذه الكتلة تُعتبر تعريفاً للقيمة الحالية
    for (int i = 0; i < ctx->var_count; i++) {
        IRInst* phi = ctx->vars[i].phi_in_block[block->id];
        if (!phi) continue;

        IRValue* phi_val = ir_value_reg(phi->dest, phi->type ? phi->type : ctx->vars[i].pointee);
        if (phi_val) (void)mem2reg_stack_push(&ctx->vars[i], phi_val);
    }

    // إعادة كتابة الحمل/الخزن
    IRInst* inst = block->first;
    while (inst) {
        IRInst* next = inst->next;

        if (inst->op == IR_OP_STORE &&
            inst->operand_count >= 2 &&
            inst->operands[1] &&
            inst->operands[1]->kind == IR_VAL_REG) {

            int ptr_reg = inst->operands[1]->data.reg_num;
            if (ptr_reg >= 0 && ptr_reg < ctx->ptr_map_size) {
                int vi = ctx->ptr_reg_to_var[ptr_reg];
                if (vi >= 0 && vi < ctx->var_count) {
                    Mem2RegVar* v = &ctx->vars[vi];

                    IRValue* stored_val = inst->operands[0];
                    IRValue* clone = ir_value_clone_with_type(stored_val, v->pointee);
                    if (clone) (void)mem2reg_stack_push(v, clone);

                    ir_block_remove_inst(block, inst);
                    ctx->changed = 1;
                    inst = next;
                    continue;
                }
            }
        }

        if (inst->op == IR_OP_LOAD &&
            inst->operand_count >= 1 &&
            inst->operands[0] &&
            inst->operands[0]->kind == IR_VAL_REG) {

            int ptr_reg = inst->operands[0]->data.reg_num;
            if (ptr_reg >= 0 && ptr_reg < ctx->ptr_map_size) {
                int vi = ctx->ptr_reg_to_var[ptr_reg];
                if (vi >= 0 && vi < ctx->var_count) {
                    Mem2RegVar* v = &ctx->vars[vi];

                    IRValue* cur = mem2reg_stack_top(v);
                    if (!cur) {
                        inst = next;
                        continue;
                    }

                    IRValue* src = ir_value_clone_with_type(cur, inst->type ? inst->type : v->pointee);
                    if (!src) {
                        inst = next;
                        continue;
                    }

                    for (int i = 0; i < inst->operand_count; i++) {
                        if (inst->operands[i]) {
                            ir_value_free(inst->operands[i]);
                            inst->operands[i] = NULL;
                        }
                    }

                    inst->op = IR_OP_COPY;
                    inst->operand_count = 1;
                    inst->operands[0] = src;
                    inst->phi_entries = NULL;
                    inst->call_target = NULL;
                    inst->call_args = NULL;
                    inst->call_arg_count = 0;

                     if (v->dbg_name) {
                         ir_inst_set_dbg_name(inst, v->dbg_name);
                     }

                    ctx->changed = 1;
                    inst = next;
                    continue;
                }
            }
        }

        inst = next;
    }

    // تعبئة معاملات فاي في الخلفاء
    for (int s = 0; s < block->succ_count; s++) {
        IRBlock* succ = block->succs[s];
        if (!succ) continue;
        if (succ->id < 0 || succ->id >= ctx->max_id) continue;

        for (int i = 0; i < ctx->var_count; i++) {
            IRInst* phi = ctx->vars[i].phi_in_block[succ->id];
            if (!phi) continue;

            IRValue* cur = mem2reg_stack_top(&ctx->vars[i]);
            if (!cur) {
                IRValue* zero = ir_value_const_int(0, phi->type ? phi->type : ctx->vars[i].pointee);
                if (zero) {
                    ir_inst_phi_add(phi, zero, block);
                    ctx->changed = 1;
                }
                continue;
            }

            IRValue* clone = ir_value_clone_with_type(cur, phi->type ? phi->type : ctx->vars[i].pointee);
            if (!clone) continue;
            ir_inst_phi_add(phi, clone, block);
            ctx->changed = 1;
        }
    }

    // زيارة الأبناء في شجرة المسيطر
    for (int child_id = ctx->child_head[block->id];
         child_id != -1;
         child_id = ctx->child_next[child_id]) {

        if (child_id < 0 || child_id >= ctx->max_id) continue;
        IRBlock* child = ctx->block_by_id[child_id];
        if (child) mem2reg_rename_block(ctx, child);
    }

    // استرجاع المكدسات
    for (int i = 0; i < ctx->var_count; i++) {
        mem2reg_stack_free_to(&ctx->vars[i], saved[i]);
    }

    free(saved);
}

// -----------------------------------------------------------------------------
// النواة: Mem2Reg لكل دالة
// -----------------------------------------------------------------------------

static int ir_mem2reg_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    // حساب dominators/DF + إعادة بناء preds/succs
    ir_func_compute_dominators(func);

    int max_id = ir_func_max_block_id_local(func);
    if (max_id <= 0) return 0;

    IRBlock** block_by_id = (IRBlock**)calloc((size_t)max_id, sizeof(IRBlock*));
    if (!block_by_id) return 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (b->id >= 0 && b->id < max_id) {
            block_by_id[b->id] = b;
        }
    }

    // جمع allocas القابلة للترقية
    Mem2RegVar* vars = NULL;
    int var_count = 0;
    int var_cap = 0;

    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (inst->op != IR_OP_ALLOCA) continue;
            if (inst->dest < 0) continue;
            if (!ir_mem2reg_can_promote_alloca(func, b, inst)) continue;

            IRType* pointee = ir_alloca_pointee_type(inst);
            if (!pointee) continue;

            if (var_count >= var_cap) {
                int new_cap = (var_cap == 0) ? 8 : var_cap * 2;
                Mem2RegVar* new_arr = (Mem2RegVar*)realloc(vars, (size_t)new_cap * sizeof(Mem2RegVar));
                if (!new_arr) break;
                vars = new_arr;
                var_cap = new_cap;
            }

            Mem2RegVar* v = &vars[var_count++];
            memset(v, 0, sizeof(*v));
            v->ptr_reg = inst->dest;
            v->pointee = pointee;
            v->alloca_inst = inst;
            v->alloca_block = b;
            v->dbg_name = inst->dbg_name;
            v->phi_in_block = (IRInst**)calloc((size_t)max_id, sizeof(IRInst*));
        }
    }

    if (var_count == 0) {
        free(block_by_id);
        free(vars);
        return 0;
    }

    // خريطة ptr_reg -> var index (قبل إدراج فاي)
    int ptr_map_size = func->next_reg;
    if (ptr_map_size < 1) ptr_map_size = 1;

    int* ptr_reg_to_var = (int*)malloc((size_t)ptr_map_size * sizeof(int));
    if (!ptr_reg_to_var) {
        for (int i = 0; i < var_count; i++) {
            free(vars[i].phi_in_block);
        }
        free(vars);
        free(block_by_id);
        return 0;
    }
    for (int i = 0; i < ptr_map_size; i++) ptr_reg_to_var[i] = -1;
    for (int i = 0; i < var_count; i++) {
        int r = vars[i].ptr_reg;
        if (r >= 0 && r < ptr_map_size) ptr_reg_to_var[r] = i;
    }

    int changed = 0;

    // إدراج فاي
    for (int i = 0; i < var_count; i++) {
        unsigned char* is_def = (unsigned char*)calloc((size_t)max_id, sizeof(unsigned char));
        if (!is_def) continue;

        int* def_ids = NULL;
        int def_count = 0;
        mem2reg_collect_def_blocks(func, vars[i].ptr_reg, is_def, max_id, &def_ids, &def_count);

        if (def_ids && def_count > 0) {
            mem2reg_insert_phis_for_var(func, block_by_id, &vars[i], is_def, def_ids, def_count, max_id);
            changed = 1;
        }

        free(def_ids);
        free(is_def);
    }

    // بناء شجرة المسيطر: child_next[b->id] و child_head[parent]
    int* child_head = (int*)malloc((size_t)max_id * sizeof(int));
    int* child_next = (int*)malloc((size_t)max_id * sizeof(int));
    if (!child_head || !child_next) {
        if (child_head) free(child_head);
        if (child_next) free(child_next);
        free(ptr_reg_to_var);
        for (int i = 0; i < var_count; i++) {
            free(vars[i].phi_in_block);
            mem2reg_stack_free_to(&vars[i], 0);
            free(vars[i].stack);
        }
        free(vars);
        free(block_by_id);
        return changed;
    }

    for (int i = 0; i < max_id; i++) {
        child_head[i] = -1;
        child_next[i] = -1;
    }

    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b->idom) continue;
        if (b == func->entry) continue;
        if (b->id < 0 || b->id >= max_id) continue;
        if (b->idom->id < 0 || b->idom->id >= max_id) continue;

        int parent = b->idom->id;
        child_next[b->id] = child_head[parent];
        child_head[parent] = b->id;
    }

    // إعادة التسمية في SSA
    Mem2RegRenameCtx rctx = {0};
    rctx.func = func;
    rctx.block_by_id = block_by_id;
    rctx.vars = vars;
    rctx.var_count = var_count;
    rctx.ptr_reg_to_var = ptr_reg_to_var;
    rctx.ptr_map_size = ptr_map_size;
    rctx.child_head = child_head;
    rctx.child_next = child_next;
    rctx.max_id = max_id;
    rctx.changed = 0;

    mem2reg_rename_block(&rctx, func->entry);
    if (rctx.changed) changed = 1;

    // حذف allocas المرقّاة
    for (int i = 0; i < var_count; i++) {
        if (vars[i].alloca_block && vars[i].alloca_inst) {
            ir_block_remove_inst(vars[i].alloca_block, vars[i].alloca_inst);
            changed = 1;
        }
    }

    // Cleanup
    free(child_head);
    free(child_next);
    free(ptr_reg_to_var);

    for (int i = 0; i < var_count; i++) {
        free(vars[i].phi_in_block);
        mem2reg_stack_free_to(&vars[i], 0);
        free(vars[i].stack);
    }
    free(vars);
    free(block_by_id);

    return changed;
}

// -----------------------------------------------------------------------------
// الواجهة العامة
// -----------------------------------------------------------------------------

bool ir_mem2reg_run(IRModule* module) {
    if (!module) return false;

    // ضمان أن أي قيم/فاي جديدة تُخصَّص ضمن ساحة هذه الوحدة.
    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_mem2reg_func(f);
    }

    return changed ? true : false;
}
