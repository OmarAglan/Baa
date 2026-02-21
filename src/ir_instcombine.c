/**
 * @file ir_instcombine.c
 * @brief تمريرة دمج/تبسيط التعليمات (InstCombine) — v0.3.2.8.6.
 *
 * أهداف التمريرة:
 * - تبسيطات محلية سريعة (قبل/بين تمريرات أخرى).
 * - تحويل أنماط مثل x+0, x*1, cmp x,x إلى نسخ/ثوابت.
 *
 * ملاحظة مهمة:
 * - هذه التمريرة لا تحذف تعريفات SSA مباشرة. عند النجاح نعيد كتابة التعليمة
 *   إلى `نسخ` (IR_OP_COPY) كي تبقى SSA صالحة، ثم تتكفل CopyProp/DCE بالتنظيف.
 */

#include "ir_instcombine.h"

#include "ir_defuse.h"

#include <stdint.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_INSTCOMBINE = {
    .name = "دمج_التعليمات",
    .run = ir_instcombine_run,
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static int ir_value_is_const_int_local(IRValue* v, int64_t* out)
{
    if (!v || v->kind != IR_VAL_CONST_INT)
        return 0;
    if (out)
        *out = v->data.const_int;
    return 1;
}

static int64_t ic_trunc_to_type(int64_t v, IRType* t)
{
    if (!t)
        return v;

    if (t->kind == IR_TYPE_I1)
        return v ? 1 : 0;

    int bits = ir_type_bits(t);
    if (bits <= 0 || bits >= 64)
        return v;

    uint64_t mask = (1ULL << (unsigned)bits) - 1ULL;
    uint64_t u = ((uint64_t)v) & mask;

    if (t->kind == IR_TYPE_U8 || t->kind == IR_TYPE_U16 || t->kind == IR_TYPE_U32 || t->kind == IR_TYPE_U64)
        return (int64_t)u;

    uint64_t sign_bit = 1ULL << (unsigned)(bits - 1);
    if (u & sign_bit)
        u |= ~mask;

    return (int64_t)u;
}

static IRValue* ic_new_const_for_type(int64_t v, IRType* t)
{
    return ir_value_const_int(ic_trunc_to_type(v, t), t);
}

static IRValue* ic_clone_value_with_type(IRValue* v, IRType* override_type)
{
    if (!v) return NULL;

    IRType* t = override_type ? override_type : v->type;

    switch (v->kind)
    {
        case IR_VAL_CONST_INT:
            return ir_value_const_int(v->data.const_int, t);

        case IR_VAL_CONST_STR:
            return ir_value_const_str(v->data.const_str.data, v->data.const_str.id);

        case IR_VAL_BAA_STR:
            return ir_value_baa_str(v->data.const_str.data, v->data.const_str.id);

        case IR_VAL_REG:
            return ir_value_reg(v->data.reg_num, t);

        case IR_VAL_GLOBAL:
            if (v->type && v->type->kind == IR_TYPE_PTR)
                return ir_value_global(v->data.global_name, v->type->data.pointee);
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

static void ic_rewrite_to_copy(IRInst* inst, IRValue* src)
{
    if (!inst || !src)
        return;

    for (int i = 0; i < inst->operand_count; i++)
    {
        if (inst->operands[i])
        {
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
}

static int ic_simplify_phi(IRInst* inst)
{
    if (!inst || inst->op != IR_OP_PHI)
        return 0;

    IRPhiEntry* e = inst->phi_entries;
    while (e && (!e->value || !e->block))
        e = e->next;
    if (!e || !e->value)
        return 0;

    IRValueKind k = e->value->kind;
    int64_t ci = 0;
    int rr = -1;
    const char* gn = NULL;

    if (k == IR_VAL_CONST_INT)
        ci = e->value->data.const_int;
    else if (k == IR_VAL_REG)
        rr = e->value->data.reg_num;
    else if (k == IR_VAL_GLOBAL)
        gn = e->value->data.global_name;
    else
        return 0;

    for (IRPhiEntry* it = e->next; it; it = it->next)
    {
        if (!it->value || !it->block)
            continue;

        if (it->value->kind != k)
            return 0;

        if (k == IR_VAL_CONST_INT && it->value->data.const_int != ci)
            return 0;
        if (k == IR_VAL_REG && it->value->data.reg_num != rr)
            return 0;
        if (k == IR_VAL_GLOBAL && it->value->data.global_name != gn)
            return 0;
    }

    IRValue* repl = NULL;
    if (k == IR_VAL_CONST_INT)
        repl = ic_new_const_for_type(ci, inst->type);
    else if (k == IR_VAL_REG)
        repl = ir_value_reg(rr, inst->type);
    else if (k == IR_VAL_GLOBAL)
    {
        if (e->value->type && e->value->type->kind == IR_TYPE_PTR)
            repl = ir_value_global(gn, e->value->type->data.pointee);
        else
            repl = ir_value_global(gn, e->value->type);
    }

    if (!repl)
        return 0;

    ic_rewrite_to_copy(inst, repl);
    return 1;
}

static int ic_simplify_arith(IRInst* inst)
{
    if (!inst || inst->operand_count < 1)
        return 0;

    // تجنب تبسيطات العشري (NaN/-0 قد تكسر بعض أنماط x+0/x*0).
    if (inst->type && inst->type->kind == IR_TYPE_F64)
        return 0;

    IRValue* a = inst->operand_count >= 1 ? inst->operands[0] : NULL;
    IRValue* b = inst->operand_count >= 2 ? inst->operands[1] : NULL;

    int64_t ca = 0, cb = 0;

    switch (inst->op)
    {
        case IR_OP_ADD:
            if (b && ir_value_is_const_int_local(b, &cb) && cb == 0)
            {
                IRValue* src = ic_clone_value_with_type(a, inst->type);
                if (!src) return 0;
                ic_rewrite_to_copy(inst, src);
                return 1;
            }
            return 0;

        case IR_OP_SUB:
            if (b && ir_value_is_const_int_local(b, &cb) && cb == 0)
            {
                IRValue* src = ic_clone_value_with_type(a, inst->type);
                if (!src) return 0;
                ic_rewrite_to_copy(inst, src);
                return 1;
            }
            return 0;

        case IR_OP_MUL:
            if (b && ir_value_is_const_int_local(b, &cb) && cb == 1)
            {
                IRValue* src = ic_clone_value_with_type(a, inst->type);
                if (!src) return 0;
                ic_rewrite_to_copy(inst, src);
                return 1;
            }
            if (b && ir_value_is_const_int_local(b, &cb) && cb == 0)
            {
                IRValue* c0 = ic_new_const_for_type(0, inst->type);
                if (!c0) return 0;
                ic_rewrite_to_copy(inst, c0);
                return 1;
            }
            return 0;

        case IR_OP_AND:
            if (b && ir_value_is_const_int_local(b, &cb) && cb == 0)
            {
                IRValue* c0 = ic_new_const_for_type(0, inst->type);
                if (!c0) return 0;
                ic_rewrite_to_copy(inst, c0);
                return 1;
            }
            return 0;

        case IR_OP_OR:
            if (b && ir_value_is_const_int_local(b, &cb) && cb == 0)
            {
                IRValue* src = ic_clone_value_with_type(a, inst->type);
                if (!src) return 0;
                ic_rewrite_to_copy(inst, src);
                return 1;
            }
            return 0;

        case IR_OP_NEG:
            if (a && ir_value_is_const_int_local(a, &ca))
            {
                IRValue* c = ic_new_const_for_type(-ca, inst->type);
                if (!c) return 0;
                ic_rewrite_to_copy(inst, c);
                return 1;
            }
            return 0;

        case IR_OP_NOT:
            if (a && inst->type && inst->type->kind == IR_TYPE_I1 && ir_value_is_const_int_local(a, &ca))
            {
                IRValue* c = ic_new_const_for_type(ca ? 0 : 1, inst->type);
                if (!c) return 0;
                ic_rewrite_to_copy(inst, c);
                return 1;
            }
            return 0;

        case IR_OP_CMP:
            if (!a || !b) return 0;
            if (a->kind != b->kind) return 0;

            if (a->kind == IR_VAL_REG && a->data.reg_num == b->data.reg_num)
            {
                int64_t res = 0;
                switch (inst->cmp_pred)
                {
                    case IR_CMP_EQ: res = 1; break;
                    case IR_CMP_NE: res = 0; break;
                    case IR_CMP_GT: res = 0; break;
                    case IR_CMP_LT: res = 0; break;
                    case IR_CMP_GE: res = 1; break;
                    case IR_CMP_LE: res = 1; break;
                    case IR_CMP_UGT: res = 0; break;
                    case IR_CMP_ULT: res = 0; break;
                    case IR_CMP_UGE: res = 1; break;
                    case IR_CMP_ULE: res = 1; break;
                    default: return 0;
                }

                IRValue* c = ic_new_const_for_type(res, inst->type);
                if (!c) return 0;
                ic_rewrite_to_copy(inst, c);
                return 1;
            }

            if (a->kind == IR_VAL_CONST_INT && a->data.const_int == b->data.const_int)
            {
                int64_t res = 0;
                switch (inst->cmp_pred)
                {
                    case IR_CMP_EQ: res = 1; break;
                    case IR_CMP_NE: res = 0; break;
                    case IR_CMP_GT: res = 0; break;
                    case IR_CMP_LT: res = 0; break;
                    case IR_CMP_GE: res = 1; break;
                    case IR_CMP_LE: res = 1; break;
                    case IR_CMP_UGT: res = 0; break;
                    case IR_CMP_ULT: res = 0; break;
                    case IR_CMP_UGE: res = 1; break;
                    case IR_CMP_ULE: res = 1; break;
                    default: return 0;
                }

                IRValue* c = ic_new_const_for_type(res, inst->type);
                if (!c) return 0;
                ic_rewrite_to_copy(inst, c);
                return 1;
            }

            return 0;

        default:
            return 0;
    }
}

static int ir_instcombine_func(IRFunc* func)
{
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;

    // بناء Def-Use للأنماط التي تتطلب فحص التعريفات (نحافظ على واجهة موحدة).
    (void)ir_func_get_defuse(func, true);

    int changed = 0;

    for (IRBlock* b = func->blocks; b; b = b->next)
    {
        for (IRInst* inst = b->first; inst; inst = inst->next)
        {
            if (!inst) continue;

            if (inst->op == IR_OP_PHI)
            {
                changed |= ic_simplify_phi(inst);
                continue;
            }

            changed |= ic_simplify_arith(inst);
        }
    }

    if (changed)
        ir_func_invalidate_defuse(func);

    return changed;
}

bool ir_instcombine_run(IRModule* module)
{
    if (!module) return false;

    ir_module_set_current(module);

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next)
        changed |= ir_instcombine_func(f);

    return changed ? true : false;
}
