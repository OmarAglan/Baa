/**
 * @file ir_verify_common.c
 * @brief تنفيذ أدوات مشتركة للتحقق من الـ IR/SSA.
 * @version 0.3.3
 */

#include "ir_verify_common.h"

static int block_is_in_func(const IRBlock* b, const IRFunc* func)
{
    return b && func && b->parent == func;
}

int ir_verify_func_block_refs_safe(const IRFunc* func, IRVerifyReportFn report, void* ctx)
{
    int ok = 1;
    if (!func || !report) return 0;

    for (const IRBlock* b = func->blocks; b; b = b->next)
    {
        for (const IRInst* inst = b->first; inst; inst = inst->next)
        {
            if (!inst) continue;

            if (inst->op == IR_OP_BR)
            {
                IRValue* t = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
                if (t && t->kind == IR_VAL_BLOCK)
                {
                    if (!t->data.block)
                    {
                        report(ctx, func, b, inst, "تعليمة `قفز`: هدف كتلة فارغ (NULL).");
                        ok = 0;
                    }
                    else if (!block_is_in_func(t->data.block, func))
                    {
                        report(ctx, func, b, inst, "تعليمة `قفز`: الهدف يشير إلى كتلة خارج الدالة (غير مسموح).");
                        ok = 0;
                    }
                }
                continue;
            }

            if (inst->op == IR_OP_BR_COND)
            {
                IRValue* t1 = (inst->operand_count >= 2) ? inst->operands[1] : NULL;
                IRValue* t2 = (inst->operand_count >= 3) ? inst->operands[2] : NULL;

                if (t1 && t1->kind == IR_VAL_BLOCK)
                {
                    if (!t1->data.block)
                    {
                        report(ctx, func, b, inst, "تعليمة `قفز_شرط`: هدف_صواب كتلة فارغ (NULL).");
                        ok = 0;
                    }
                    else if (!block_is_in_func(t1->data.block, func))
                    {
                        report(ctx, func, b, inst, "تعليمة `قفز_شرط`: هدف_صواب يشير إلى كتلة خارج الدالة (غير مسموح).");
                        ok = 0;
                    }
                }

                if (t2 && t2->kind == IR_VAL_BLOCK)
                {
                    if (!t2->data.block)
                    {
                        report(ctx, func, b, inst, "تعليمة `قفز_شرط`: هدف_خطأ كتلة فارغ (NULL).");
                        ok = 0;
                    }
                    else if (!block_is_in_func(t2->data.block, func))
                    {
                        report(ctx, func, b, inst, "تعليمة `قفز_شرط`: هدف_خطأ يشير إلى كتلة خارج الدالة (غير مسموح).");
                        ok = 0;
                    }
                }
                continue;
            }

            if (inst->op == IR_OP_PHI)
            {
                for (const IRPhiEntry* e = inst->phi_entries; e; e = e->next)
                {
                    if (!e->block)
                    {
                        report(ctx, func, b, inst, "تعليمة `فاي`: كتلة سابقة فارغة (NULL).");
                        ok = 0;
                        continue;
                    }
                    if (!block_is_in_func(e->block, func))
                    {
                        report(ctx, func, b, inst, "تعليمة `فاي`: كتلة سابقة خارج الدالة (غير مسموح).");
                        ok = 0;
                    }
                }
                continue;
            }
        }
    }

    return ok;
}
