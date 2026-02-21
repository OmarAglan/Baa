/**
 * @file ir_sccp.c
 * @brief تمريرة نشر الثوابت المتناثر (SCCP) — v0.3.2.8.6.
 *
 * SCCP يقوم بدمج:
 * - تحليل وصول الكتل (Reachability)
 * - ونشر الثوابت عبر SSA
 *
 * ثم يُبسّط:
 * - `قفز_شرط` عندما يصبح الشرط ثابتاً
 * - ويستبدل استعمالات سجلات ثابتة بقيم ثابتة
 *
 * ملاحظة:
 * - التمريرة حالياً تركز على ثوابت الأعداد الصحيحة فقط.
 */

#include "ir_sccp.h"

#include "ir_analysis.h"
#include "ir_defuse.h"
#include "ir_mutate.h"

#include <stdint.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_SCCP = {
    .name = "نشر_الثوابت_المتناثر",
    .run = ir_sccp_run,
};

// -----------------------------------------------------------------------------
// Lattice
// -----------------------------------------------------------------------------

typedef enum {
    SCCP_UNDEF = 0,
    SCCP_CONST = 1,
    SCCP_OVER  = 2,
} SCCPKind;

typedef struct {
    SCCPKind kind;
    int64_t c;
} SCCPVal;

static SCCPVal sccp_join(SCCPVal a, SCCPVal b)
{
    if (a.kind == SCCP_OVER || b.kind == SCCP_OVER)
        return (SCCPVal){SCCP_OVER, 0};

    if (a.kind == SCCP_UNDEF)
        return b;
    if (b.kind == SCCP_UNDEF)
        return a;

    if (a.kind == SCCP_CONST && b.kind == SCCP_CONST)
    {
        if (a.c == b.c)
            return a;
        return (SCCPVal){SCCP_OVER, 0};
    }

    return (SCCPVal){SCCP_OVER, 0};
}

static int sccp_update(SCCPVal* dst, SCCPVal v)
{
    if (!dst) return 0;
    SCCPVal old = *dst;
    SCCPVal joined = sccp_join(old, v);
    if (joined.kind != old.kind || (joined.kind == SCCP_CONST && joined.c != old.c))
    {
        *dst = joined;
        return 1;
    }
    return 0;
}

static int sccp_type_is_unsigned(IRType* t)
{
    if (!t) return 0;
    return t->kind == IR_TYPE_U8 || t->kind == IR_TYPE_U16 || t->kind == IR_TYPE_U32 || t->kind == IR_TYPE_U64;
}

static int sccp_type_is_intlike(IRType* t)
{
    if (!t) return 0;
    switch (t->kind)
    {
        case IR_TYPE_I1:
        case IR_TYPE_I8:
        case IR_TYPE_I16:
        case IR_TYPE_I32:
        case IR_TYPE_I64:
        case IR_TYPE_U8:
        case IR_TYPE_U16:
        case IR_TYPE_U32:
        case IR_TYPE_U64:
        case IR_TYPE_CHAR:
            return 1;
        default:
            return 0;
    }
}

static int64_t sccp_trunc_to_type(int64_t v, IRType* t)
{
    if (!t) return v;
    if (t->kind == IR_TYPE_I1) return v ? 1 : 0;

    int bits = ir_type_bits(t);
    if (bits <= 0 || bits >= 64) return v;

    uint64_t mask = (1ULL << (unsigned)bits) - 1ULL;
    uint64_t u = ((uint64_t)v) & mask;

    if (sccp_type_is_unsigned(t))
        return (int64_t)u;

    uint64_t sign = 1ULL << (unsigned)(bits - 1);
    if (u & sign) u |= ~mask;
    return (int64_t)u;
}

static SCCPVal sccp_val_of_value(IRValue* v, SCCPVal* regs, int max_reg)
{
    if (!v) return (SCCPVal){SCCP_UNDEF, 0};

    if (v->kind == IR_VAL_CONST_INT)
        return (SCCPVal){SCCP_CONST, sccp_trunc_to_type(v->data.const_int, v->type)};

    if (v->kind == IR_VAL_REG)
    {
        int r = v->data.reg_num;
        if (r >= 0 && r < max_reg)
            return regs[r];
    }

    return (SCCPVal){SCCP_OVER, 0};
}

// -----------------------------------------------------------------------------
// Feasible edges
// -----------------------------------------------------------------------------

typedef struct {
    unsigned char* reachable;     // [max_id]
    unsigned char* fe0;           // [max_id] feasible succ0
    unsigned char* fe1;           // [max_id] feasible succ1
    int max_id;
} SCCPEdges;

static void sccp_set_block_reachable(SCCPEdges* e, IRBlock* b, int* changed)
{
    if (!e || !b || b->id < 0 || b->id >= e->max_id) return;
    if (!e->reachable[b->id])
    {
        e->reachable[b->id] = 1;
        if (changed) *changed = 1;
    }
}

static void sccp_mark_term_edges(IRInst* term, SCCPVal* regs, int max_reg, SCCPEdges* edges, int* changed)
{
    if (!term || !edges) return;

    if (term->op == IR_OP_BR)
    {
        if (term->operand_count < 1) return;
        if (term->operands[0] && term->operands[0]->kind == IR_VAL_BLOCK)
        {
            IRBlock* t = term->operands[0]->data.block;
            sccp_set_block_reachable(edges, t, changed);
        }
        return;
    }

    if (term->op == IR_OP_BR_COND)
    {
        if (term->operand_count < 3) return;
        SCCPVal c = sccp_val_of_value(term->operands[0], regs, max_reg);

        IRBlock* bt = (term->operands[1] && term->operands[1]->kind == IR_VAL_BLOCK) ? term->operands[1]->data.block : NULL;
        IRBlock* bf = (term->operands[2] && term->operands[2]->kind == IR_VAL_BLOCK) ? term->operands[2]->data.block : NULL;

        if (c.kind == SCCP_CONST)
        {
            if (c.c)
                sccp_set_block_reachable(edges, bt, changed);
            else
                sccp_set_block_reachable(edges, bf, changed);
        }
        else
        {
            sccp_set_block_reachable(edges, bt, changed);
            sccp_set_block_reachable(edges, bf, changed);
        }
    }
}

static int sccp_pred_edge_feasible_to(IRBlock* pred, IRBlock* succ, const SCCPEdges* edges)
{
    if (!pred || !succ || !edges) return 0;
    if (pred->id < 0 || pred->id >= edges->max_id) return 0;

    // إذا لم تُبنَ succs بعد: اعتبرها غير معروفة.
    if (pred->succ_count <= 0) return 0;

    if (pred->succ_count >= 1 && pred->succs[0] == succ)
        return edges->fe0[pred->id] ? 1 : 0;
    if (pred->succ_count >= 2 && pred->succs[1] == succ)
        return edges->fe1[pred->id] ? 1 : 0;

    return 0;
}

// -----------------------------------------------------------------------------
// Eval
// -----------------------------------------------------------------------------

static int sccp_try_fold_arith(IROp op, IRType* type, int64_t lhs, int64_t rhs, int64_t* out)
{
    if (!out) return 0;

    uint64_t ul = (uint64_t)lhs;
    uint64_t ur = (uint64_t)rhs;

    bool is_unsigned = sccp_type_is_unsigned(type);

    switch (op)
    {
        case IR_OP_ADD: *out = (int64_t)(ul + ur); return 1;
        case IR_OP_SUB: *out = (int64_t)(ul - ur); return 1;
        case IR_OP_MUL: *out = (int64_t)(ul * ur); return 1;
        case IR_OP_NEG: *out = (int64_t)(-(uint64_t)lhs); return 1;

        case IR_OP_DIV:
            if (rhs == 0) return 0;
            if (is_unsigned)
            {
                *out = (int64_t)(ul / ur);
                return 1;
            }
            if (lhs == INT64_MIN && rhs == -1) { *out = INT64_MIN; return 1; }
            *out = lhs / rhs;
            return 1;

        case IR_OP_MOD:
            if (rhs == 0) return 0;
            if (is_unsigned)
            {
                *out = (int64_t)(ul % ur);
                return 1;
            }
            if (lhs == INT64_MIN && rhs == -1) { *out = 0; return 1; }
            *out = lhs % rhs;
            return 1;

        default:
            return 0;
    }
}

static SCCPVal sccp_eval_inst(IRInst* inst, SCCPVal* regs, int max_reg, const SCCPEdges* edges)
{
    if (!inst || inst->dest < 0) return (SCCPVal){SCCP_UNDEF, 0};

    switch (inst->op)
    {
        case IR_OP_COPY:
            if (inst->operand_count >= 1)
                return sccp_val_of_value(inst->operands[0], regs, max_reg);
            return (SCCPVal){SCCP_UNDEF, 0};

        case IR_OP_PHI:
        {
            SCCPVal acc = (SCCPVal){SCCP_UNDEF, 0};
            if (!inst->parent) return (SCCPVal){SCCP_OVER, 0};

            for (IRPhiEntry* e = inst->phi_entries; e; e = e->next)
            {
                if (!e->block || !e->value) continue;
                if (!edges) continue;
                if (!sccp_pred_edge_feasible_to(e->block, inst->parent, edges))
                    continue;

                SCCPVal v = sccp_val_of_value(e->value, regs, max_reg);
                acc = sccp_join(acc, v);
                if (acc.kind == SCCP_OVER) break;
            }

            return acc;
        }

        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
        {
            if (inst->operand_count < 2) return (SCCPVal){SCCP_OVER, 0};
            if (!sccp_type_is_intlike(inst->type)) return (SCCPVal){SCCP_OVER, 0};
            SCCPVal a = sccp_val_of_value(inst->operands[0], regs, max_reg);
            SCCPVal b = sccp_val_of_value(inst->operands[1], regs, max_reg);
            if (a.kind == SCCP_OVER || b.kind == SCCP_OVER) return (SCCPVal){SCCP_OVER, 0};
            if (a.kind != SCCP_CONST || b.kind != SCCP_CONST) return (SCCPVal){SCCP_UNDEF, 0};

            int64_t out = 0;
            if (!sccp_try_fold_arith(inst->op, inst->type, a.c, b.c, &out)) return (SCCPVal){SCCP_OVER, 0};
            out = sccp_trunc_to_type(out, inst->type);
            return (SCCPVal){SCCP_CONST, out};
        }

        case IR_OP_NEG:
        {
            if (inst->operand_count < 1) return (SCCPVal){SCCP_OVER, 0};
            if (!sccp_type_is_intlike(inst->type)) return (SCCPVal){SCCP_OVER, 0};
            SCCPVal a = sccp_val_of_value(inst->operands[0], regs, max_reg);
            if (a.kind == SCCP_OVER) return (SCCPVal){SCCP_OVER, 0};
            if (a.kind != SCCP_CONST) return (SCCPVal){SCCP_UNDEF, 0};
            int64_t out = 0;
            if (!sccp_try_fold_arith(IR_OP_NEG, inst->type, a.c, 0, &out)) return (SCCPVal){SCCP_OVER, 0};
            out = sccp_trunc_to_type(out, inst->type);
            return (SCCPVal){SCCP_CONST, out};
        }

        case IR_OP_AND:
        case IR_OP_OR:
        {
            if (inst->operand_count < 2) return (SCCPVal){SCCP_OVER, 0};
            if (!sccp_type_is_intlike(inst->type)) return (SCCPVal){SCCP_OVER, 0};
            SCCPVal a = sccp_val_of_value(inst->operands[0], regs, max_reg);
            SCCPVal b = sccp_val_of_value(inst->operands[1], regs, max_reg);
            if (a.kind == SCCP_OVER || b.kind == SCCP_OVER) return (SCCPVal){SCCP_OVER, 0};
            if (a.kind != SCCP_CONST || b.kind != SCCP_CONST) return (SCCPVal){SCCP_UNDEF, 0};
            int64_t out = (inst->op == IR_OP_AND) ? (a.c & b.c) : (a.c | b.c);
            out = sccp_trunc_to_type(out, inst->type);
            return (SCCPVal){SCCP_CONST, out};
        }

        case IR_OP_NOT:
        {
            // في IR الحالي، NOT يستخدم كـ "نفي منطقي" لـ i1.
            if (inst->operand_count < 1) return (SCCPVal){SCCP_OVER, 0};
            if (!sccp_type_is_intlike(inst->type)) return (SCCPVal){SCCP_OVER, 0};
            SCCPVal a = sccp_val_of_value(inst->operands[0], regs, max_reg);
            if (a.kind == SCCP_OVER) return (SCCPVal){SCCP_OVER, 0};
            if (a.kind != SCCP_CONST) return (SCCPVal){SCCP_UNDEF, 0};
            int64_t out = a.c ? 0 : 1;
            out = sccp_trunc_to_type(out, inst->type);
            return (SCCPVal){SCCP_CONST, out};
        }

        case IR_OP_CAST:
        {
            if (inst->operand_count < 1) return (SCCPVal){SCCP_OVER, 0};
            // لا نقيّم تحويلات العشري/العدد (float<->int) هنا لتجنب دلالات غير صحيحة.
            if (inst->type && inst->type->kind == IR_TYPE_F64) return (SCCPVal){SCCP_OVER, 0};
            SCCPVal a = sccp_val_of_value(inst->operands[0], regs, max_reg);
            if (a.kind == SCCP_OVER) return (SCCPVal){SCCP_OVER, 0};
            if (a.kind != SCCP_CONST) return (SCCPVal){SCCP_UNDEF, 0};
            if (inst->operands[0] && inst->operands[0]->type && inst->operands[0]->type->kind == IR_TYPE_F64)
                return (SCCPVal){SCCP_OVER, 0};
            int64_t out = sccp_trunc_to_type(a.c, inst->type);
            return (SCCPVal){SCCP_CONST, out};
        }

        case IR_OP_CMP:
        {
            if (inst->operand_count < 2) return (SCCPVal){SCCP_OVER, 0};
            SCCPVal a = sccp_val_of_value(inst->operands[0], regs, max_reg);
            SCCPVal b = sccp_val_of_value(inst->operands[1], regs, max_reg);
            if (a.kind == SCCP_OVER || b.kind == SCCP_OVER) return (SCCPVal){SCCP_OVER, 0};
            if (a.kind != SCCP_CONST || b.kind != SCCP_CONST) return (SCCPVal){SCCP_UNDEF, 0};

            if (inst->operands[0] && inst->operands[0]->type && inst->operands[0]->type->kind == IR_TYPE_F64)
                return (SCCPVal){SCCP_OVER, 0};

            int64_t res = 0;
            uint64_t ua = (uint64_t)a.c;
            uint64_t ub = (uint64_t)b.c;
            switch (inst->cmp_pred)
            {
                case IR_CMP_EQ: res = (a.c == b.c); break;
                case IR_CMP_NE: res = (a.c != b.c); break;
                case IR_CMP_GT: res = (a.c >  b.c); break;
                case IR_CMP_LT: res = (a.c <  b.c); break;
                case IR_CMP_GE: res = (a.c >= b.c); break;
                case IR_CMP_LE: res = (a.c <= b.c); break;
                case IR_CMP_UGT: res = (ua >  ub); break;
                case IR_CMP_ULT: res = (ua <  ub); break;
                case IR_CMP_UGE: res = (ua >= ub); break;
                case IR_CMP_ULE: res = (ua <= ub); break;
                default: return (SCCPVal){SCCP_OVER, 0};
            }

            return (SCCPVal){SCCP_CONST, res ? 1 : 0};
        }

        // بقية التعليمات: اعتبرها غير قابلة للتحليل (overdefined) إذا كانت تنتج قيمة.
        default:
            return (SCCPVal){SCCP_OVER, 0};
    }
}

static int sccp_simplify_brcond(IRFunc* func, IRBlock* b, SCCPVal* regs, int max_reg)
{
    if (!func || !b || !b->last) return 0;
    IRInst* term = b->last;
    if (term->op != IR_OP_BR_COND) return 0;
    if (term->operand_count < 3) return 0;

    SCCPVal c = sccp_val_of_value(term->operands[0], regs, max_reg);
    if (c.kind != SCCP_CONST) return 0;

    IRBlock* bt = (term->operands[1] && term->operands[1]->kind == IR_VAL_BLOCK) ? term->operands[1]->data.block : NULL;
    IRBlock* bf = (term->operands[2] && term->operands[2]->kind == IR_VAL_BLOCK) ? term->operands[2]->data.block : NULL;
    IRBlock* dst = c.c ? bt : bf;
    if (!dst) return 0;

    IRInst* br = ir_inst_br(dst);
    if (!br) return 0;

    if (term->src_file && term->src_line > 0)
        ir_inst_set_loc(br, term->src_file, term->src_line, term->src_col);
    if (term->dbg_name)
        ir_inst_set_dbg_name(br, term->dbg_name);

    ir_block_remove_inst(b, term);
    ir_block_append(b, br);
    return 1;
}

static int ir_sccp_func(IRFunc* func)
{
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    ir_func_rebuild_preds(func);

    int max_reg = func->next_reg;

    int max_id = 0;
    for (IRBlock* b = func->blocks; b; b = b->next)
        if (b->id + 1 > max_id) max_id = b->id + 1;

    SCCPVal* regs = NULL;
    if (max_reg > 0)
    {
        regs = (SCCPVal*)calloc((size_t)max_reg, sizeof(SCCPVal));
        if (!regs) return 0;
    }

    // البارامترات تعتبر overdefined (قيم غير معروفة).
    if (regs)
    {
        for (int i = 0; i < func->param_count; i++)
        {
            int r = func->params[i].reg;
            if (r >= 0 && r < max_reg)
                regs[r].kind = SCCP_OVER;
        }
    }

    SCCPEdges edges = {0};
    edges.max_id = max_id;
    edges.reachable = (unsigned char*)calloc((size_t)max_id, 1);
    edges.fe0 = (unsigned char*)calloc((size_t)max_id, 1);
    edges.fe1 = (unsigned char*)calloc((size_t)max_id, 1);
    if (!edges.reachable || !edges.fe0 || !edges.fe1)
    {
        free(edges.reachable);
        free(edges.fe0);
        free(edges.fe1);
        free(regs);
        return 0;
    }

    // entry reachable
    edges.reachable[func->entry->id] = 1;

    int changed_any = 0;

    // حلقة تثبيت بسيطة (monotone) مع حد.
    for (int iter = 0; iter < 64; iter++)
    {
        int changed = 0;

        for (IRBlock* b = func->blocks; b; b = b->next)
        {
            if (!b) continue;
            if (b->id < 0 || b->id >= max_id) continue;
            if (!edges.reachable[b->id]) continue;

            // افتراض: إن لم يكن المنهي ثابتاً، كل الحواف ممكنة.
            edges.fe0[b->id] = 0;
            edges.fe1[b->id] = 0;

            for (IRInst* inst = b->first; inst; inst = inst->next)
            {
                if (!inst) continue;

                if (inst->op == IR_OP_BR)
                {
                    edges.fe0[b->id] = 1;
                    sccp_mark_term_edges(inst, regs, max_reg, &edges, &changed);
                    break;
                }
                if (inst->op == IR_OP_BR_COND)
                {
                    SCCPVal c = sccp_val_of_value(inst->operands[0], regs, max_reg);
                    if (c.kind == SCCP_CONST)
                    {
                        if (c.c) edges.fe0[b->id] = 1; else edges.fe1[b->id] = 1;
                    }
                    else
                    {
                        edges.fe0[b->id] = 1;
                        edges.fe1[b->id] = 1;
                    }
                    sccp_mark_term_edges(inst, regs, max_reg, &edges, &changed);
                    break;
                }
                if (inst->op == IR_OP_RET)
                {
                    break;
                }

                if (inst->dest >= 0 && inst->dest < max_reg)
                {
                    SCCPVal v = sccp_eval_inst(inst, regs, max_reg, &edges);
                    changed |= sccp_update(&regs[inst->dest], v);
                }
            }
        }

        if (!changed)
            break;
        changed_any = 1;
    }

    // تطبيق: استبدال استعمالات السجلات الثابتة
    int applied = 0;
    IRDefUse* du = ir_func_get_defuse(func, true);
    if (du)
    {
        for (int r = 0; r < max_reg && r < du->max_reg; r++)
        {
            if (regs[r].kind != SCCP_CONST) continue;

            for (IRUse* u = du->uses_by_reg[r]; u; u = u->next)
            {
                if (!u->slot || !*u->slot) continue;

                IRValue* cur = *u->slot;
                if (!cur || cur->kind != IR_VAL_REG) continue;
                if (cur->data.reg_num != r) continue;

                IRType* t = cur->type;
                if (!t) t = IR_TYPE_I64_T;

                *u->slot = ir_value_const_int(sccp_trunc_to_type(regs[r].c, t), t);
                applied = 1;
            }
        }
    }

    // تبسيط br_cond
    for (IRBlock* b = func->blocks; b; b = b->next)
    {
        if (!b) continue;
        applied |= sccp_simplify_brcond(func, b, regs, max_reg);
    }

    if (applied)
    {
        ir_func_rebuild_preds(func);
        ir_func_invalidate_defuse(func);
    }

    free(edges.reachable);
    free(edges.fe0);
    free(edges.fe1);
    free(regs);

    return (changed_any || applied) ? 1 : 0;
}

bool ir_sccp_run(IRModule* module)
{
    if (!module) return false;
    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next)
        changed |= ir_sccp_func(f);

    return changed ? true : false;
}
