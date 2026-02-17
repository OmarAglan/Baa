/**
 * @file ir_gvn.c
 * @brief تمريرة ترقيم القيم العالمية (GVN) — v0.3.2.8.6.
 *
 * هدف التمريرة:
 * - توحيد التعابير النقية عبر نطاق السيطرة (dominator tree)
 * - الاستفادة من ترقيم القيم لتجاوز اختلافات أرقام السجلات الناتجة عن النسخ
 */

#include "ir_gvn.h"

#include "ir_analysis.h"
#include "ir_defuse.h"
#include "ir_mutate.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_GVN = {
    .name = "ترقيم_القيم",
    .run = ir_gvn_run,
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static int ir_op_is_gvn_eligible(IROp op)
{
    switch (op)
    {
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
        case IR_OP_NEG:
        case IR_OP_CMP:
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_NOT:
        case IR_OP_CAST:
        case IR_OP_COPY:
            return 1;
        default:
            return 0;
    }
}

// -----------------------------------------------------------------------------
// Constant interning -> value numbers
// -----------------------------------------------------------------------------

typedef struct ConstVN {
    int64_t v;
    IRTypeKind t;
    int vn;
    struct ConstVN* next;
} ConstVN;

static unsigned int const_hash(int64_t v, IRTypeKind t)
{
    uint64_t u = (uint64_t)v;
    unsigned int h = (unsigned int)(u ^ (u >> 32));
    h ^= (unsigned int)t * 17u;
    return h;
}

// -----------------------------------------------------------------------------
// Expression table (scoped)
// -----------------------------------------------------------------------------

typedef struct ExprEntry {
    IROp op;
    IRCmpPred pred;
    IRTypeKind type_kind;
    int operand_count;
    int vn[4];
    int reg; // representative result reg
    struct ExprEntry* next;
} ExprEntry;

#define GVN_HASH_SIZE 512

static unsigned int expr_hash(IROp op, IRCmpPred pred, IRTypeKind tk, const int* vn, int n)
{
    unsigned int h = (unsigned int)op * 31u;
    h ^= (unsigned int)pred * 17u;
    h ^= (unsigned int)tk * 13u;
    for (int i = 0; i < n; i++)
        h = h * 37u + (unsigned int)vn[i];
    return h % GVN_HASH_SIZE;
}

static int expr_match(const ExprEntry* e, const ExprEntry* q)
{
    if (e->op != q->op) return 0;
    if (e->op == IR_OP_CMP && e->pred != q->pred) return 0;
    if (e->type_kind != q->type_kind) return 0;
    if (e->operand_count != q->operand_count) return 0;
    for (int i = 0; i < e->operand_count; i++)
        if (e->vn[i] != q->vn[i]) return 0;
    return 1;
}

static int gvn_replace_reg_uses(IRDefUse* du, int old_reg, int new_reg)
{
    if (!du) return 0;
    if (old_reg < 0 || old_reg >= du->max_reg) return 0;

    int changed = 0;
    for (IRUse* u = du->uses_by_reg[old_reg]; u; u = u->next)
    {
        if (!u->slot || !*u->slot) continue;
        IRValue* cur = *u->slot;
        if (!cur || cur->kind != IR_VAL_REG) continue;
        if (cur->data.reg_num != old_reg) continue;
        cur->data.reg_num = new_reg;
        changed = 1;
    }
    return changed;
}

// -----------------------------------------------------------------------------
// Dominator tree children list
// -----------------------------------------------------------------------------

static int ir_func_max_block_id_local(IRFunc* func)
{
    int max_id = -1;
    if (!func) return 0;
    for (IRBlock* b = func->blocks; b; b = b->next)
        if (b->id > max_id) max_id = b->id;
    return max_id + 1;
}

typedef struct {
    IRFunc* func;
    IRDefUse* du;

    int max_reg;
    int* vn_of_reg;
    int next_vn;

    ConstVN** const_table;

    ExprEntry** expr_table;
    ExprEntry** expr_stack;
    int expr_sp;
    int expr_cap;

    IRBlock** block_by_id;
    int* child_head;
    int* child_next;
    int max_id;

    int changed;
} GVNContext;

static int gvn_get_const_vn(GVNContext* ctx, int64_t v, IRType* t)
{
    if (!ctx) return 0;
    IRTypeKind tk = t ? t->kind : IR_TYPE_I64;

    unsigned int h = const_hash(v, tk) % 256u;
    for (ConstVN* e = ctx->const_table[h]; e; e = e->next)
    {
        if (e->v == v && e->t == tk) return e->vn;
    }

    ConstVN* ne = (ConstVN*)malloc(sizeof(ConstVN));
    if (!ne) return 0;
    ne->v = v;
    ne->t = tk;
    ne->vn = ctx->next_vn++;
    ne->next = ctx->const_table[h];
    ctx->const_table[h] = ne;
    return ne->vn;
}

static int gvn_value_vn(GVNContext* ctx, IRValue* v)
{
    if (!ctx || !v) return 0;

    if (v->kind == IR_VAL_CONST_INT)
        return gvn_get_const_vn(ctx, v->data.const_int, v->type);

    if (v->kind == IR_VAL_REG)
    {
        int r = v->data.reg_num;
        if (r >= 0 && r < ctx->max_reg)
        {
            int vn = ctx->vn_of_reg[r];
            if (vn == 0)
            {
                // يجب ألا يحدث لغير PHI؛ لكن نضمن عدم الانهيار.
                vn = ctx->next_vn++;
                ctx->vn_of_reg[r] = vn;
            }
            return vn;
        }
    }

    // بقية القيم: اعتبرها فريدة.
    return ctx->next_vn++;
}

static void gvn_expr_stack_push(GVNContext* ctx, ExprEntry* e)
{
    if (!ctx || !e) return;
    if (ctx->expr_sp >= ctx->expr_cap)
    {
        int new_cap = ctx->expr_cap ? ctx->expr_cap * 2 : 256;
        ExprEntry** n = (ExprEntry**)realloc(ctx->expr_stack, (size_t)new_cap * sizeof(ExprEntry*));
        if (!n) return;
        ctx->expr_stack = n;
        ctx->expr_cap = new_cap;
    }
    ctx->expr_stack[ctx->expr_sp++] = e;
}

static void gvn_expr_pop_to(GVNContext* ctx, int mark)
{
    if (!ctx) return;
    while (ctx->expr_sp > mark)
    {
        ExprEntry* e = ctx->expr_stack[--ctx->expr_sp];
        if (!e) continue;
        unsigned int h = expr_hash(e->op, e->pred, e->type_kind, e->vn, e->operand_count);
        if (ctx->expr_table[h] == e)
        {
            ctx->expr_table[h] = e->next;
        }
        else
        {
            // احتياطي: ابحث في السلسلة
            ExprEntry** link = &ctx->expr_table[h];
            while (*link)
            {
                if (*link == e)
                {
                    *link = e->next;
                    break;
                }
                link = &(*link)->next;
            }
        }
        free(e);
    }
}

static void gvn_visit_block(GVNContext* ctx, IRBlock* b)
{
    if (!ctx || !b) return;

    int mark = ctx->expr_sp;

    for (IRInst* inst = b->first; inst; )
    {
        IRInst* next = inst->next;

        if (inst->dest >= 0 && inst->dest < ctx->max_reg)
        {
            if (inst->op == IR_OP_COPY && inst->operand_count >= 1)
            {
                ctx->vn_of_reg[inst->dest] = gvn_value_vn(ctx, inst->operands[0]);
                inst = next;
                continue;
            }

            if (inst->op == IR_OP_PHI)
            {
                int vn = 0;
                int first = 1;
                for (IRPhiEntry* e = inst->phi_entries; e; e = e->next)
                {
                    if (!e->value || !e->block) continue;
                    int evn = gvn_value_vn(ctx, e->value);
                    if (first)
                    {
                        vn = evn;
                        first = 0;
                    }
                    else if (vn != evn)
                    {
                        vn = 0;
                        break;
                    }
                }
                if (vn == 0) vn = ctx->next_vn++;
                ctx->vn_of_reg[inst->dest] = vn;
                inst = next;
                continue;
            }

            if (ir_op_is_gvn_eligible(inst->op))
            {
                ExprEntry q = {0};
                q.op = inst->op;
                q.pred = inst->cmp_pred;
                q.type_kind = inst->type ? inst->type->kind : IR_TYPE_I64;
                q.operand_count = inst->operand_count;

                if (q.operand_count > 4) q.operand_count = 4;
                for (int i = 0; i < q.operand_count; i++)
                    q.vn[i] = gvn_value_vn(ctx, inst->operands[i]);

                unsigned int h = expr_hash(q.op, q.pred, q.type_kind, q.vn, q.operand_count);
                int found_reg = -1;
                for (ExprEntry* e = ctx->expr_table[h]; e; e = e->next)
                {
                    if (expr_match(e, &q))
                    {
                        found_reg = e->reg;
                        break;
                    }
                }

                if (found_reg >= 0 && found_reg != inst->dest)
                {
                    // استبدل كل استعمالات dest بنتيجة سابقة.
                    (void)gvn_replace_reg_uses(ctx->du, inst->dest, found_reg);
                    ctx->vn_of_reg[inst->dest] = ctx->vn_of_reg[found_reg];

                    ir_block_remove_inst(b, inst);
                    ctx->changed = 1;
                    inst = next;
                    continue;
                }

                // لم نجد: أدخل التعبير.
                ExprEntry* ne = (ExprEntry*)malloc(sizeof(ExprEntry));
                if (ne)
                {
                    *ne = q;
                    ne->reg = inst->dest;
                    ne->next = ctx->expr_table[h];
                    ctx->expr_table[h] = ne;
                    gvn_expr_stack_push(ctx, ne);
                }

                // أعطِ vn لهذه النتيجة
                ctx->vn_of_reg[inst->dest] = ctx->next_vn++;

                inst = next;
                continue;
            }

            // غير مؤهل
            ctx->vn_of_reg[inst->dest] = ctx->next_vn++;
        }

        inst = next;
    }

    // أبناء dom-tree
    if (b->id >= 0 && b->id < ctx->max_id)
    {
        for (int cid = ctx->child_head[b->id]; cid != -1; cid = ctx->child_next[cid])
        {
            IRBlock* child = ctx->block_by_id[cid];
            if (child)
                gvn_visit_block(ctx, child);
        }
    }

    gvn_expr_pop_to(ctx, mark);
}

static void gvn_free_tables(GVNContext* ctx)
{
    if (!ctx) return;
    if (ctx->const_table)
    {
        for (int i = 0; i < 256; i++)
        {
            ConstVN* e = ctx->const_table[i];
            while (e)
            {
                ConstVN* n = e->next;
                free(e);
                e = n;
            }
        }
        free(ctx->const_table);
    }

    if (ctx->expr_table)
    {
        // قد تكون هناك بقايا إن فشل pop بسبب alloc؛ نحررها بأمان.
        for (int i = 0; i < GVN_HASH_SIZE; i++)
        {
            ExprEntry* e = ctx->expr_table[i];
            while (e)
            {
                ExprEntry* n = e->next;
                free(e);
                e = n;
            }
        }
        free(ctx->expr_table);
    }

    free(ctx->expr_stack);
}

static int ir_gvn_func(IRFunc* func)
{
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    ir_func_rebuild_preds(func);
    ir_func_compute_dominators(func);

    int max_id = ir_func_max_block_id_local(func);
    if (max_id <= 0) return 0;

    int max_reg = func->next_reg;
    if (max_reg <= 0) return 0;

    GVNContext ctx = {0};
    ctx.func = func;
    ctx.du = ir_func_get_defuse(func, true);
    ctx.max_reg = max_reg;
    ctx.vn_of_reg = (int*)calloc((size_t)max_reg, sizeof(int));
    ctx.next_vn = 1;
    ctx.max_id = max_id;

    ctx.const_table = (ConstVN**)calloc(256, sizeof(ConstVN*));
    ctx.expr_table = (ExprEntry**)calloc(GVN_HASH_SIZE, sizeof(ExprEntry*));

    ctx.block_by_id = (IRBlock**)calloc((size_t)max_id, sizeof(IRBlock*));
    ctx.child_head = (int*)malloc((size_t)max_id * sizeof(int));
    ctx.child_next = (int*)malloc((size_t)max_id * sizeof(int));
    if (!ctx.vn_of_reg || !ctx.const_table || !ctx.expr_table || !ctx.block_by_id || !ctx.child_head || !ctx.child_next)
    {
        free(ctx.vn_of_reg);
        free(ctx.block_by_id);
        free(ctx.child_head);
        free(ctx.child_next);
        gvn_free_tables(&ctx);
        return 0;
    }

    for (int i = 0; i < max_id; i++)
    {
        ctx.child_head[i] = -1;
        ctx.child_next[i] = -1;
    }

    for (IRBlock* b = func->blocks; b; b = b->next)
    {
        if (b->id >= 0 && b->id < max_id)
            ctx.block_by_id[b->id] = b;
    }

    // بناء قائمة الأبناء من idom
    for (IRBlock* b = func->blocks; b; b = b->next)
    {
        if (!b || b == func->entry) continue;
        if (!b->idom) continue;
        int pid = b->idom->id;
        if (pid < 0 || pid >= max_id) continue;
        if (b->id < 0 || b->id >= max_id) continue;
        ctx.child_next[b->id] = ctx.child_head[pid];
        ctx.child_head[pid] = b->id;
    }

    // ترقيم barameters
    for (int i = 0; i < func->param_count; i++)
    {
        int r = func->params[i].reg;
        if (r >= 0 && r < max_reg)
            ctx.vn_of_reg[r] = ctx.next_vn++;
    }

    gvn_visit_block(&ctx, func->entry);

    int changed = ctx.changed;
    if (changed)
        ir_func_invalidate_defuse(func);

    free(ctx.vn_of_reg);
    free(ctx.block_by_id);
    free(ctx.child_head);
    free(ctx.child_next);
    gvn_free_tables(&ctx);

    return changed;
}

bool ir_gvn_run(IRModule* module)
{
    if (!module) return false;
    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next)
        changed |= ir_gvn_func(f);

    return changed ? true : false;
}
