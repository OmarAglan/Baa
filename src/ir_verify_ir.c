/**
 * @file ir_verify_ir.c
 * @brief تنفيذ متحقق سلامة/تشكيل الـ IR (Well-Formedness Verifier) (v0.3.2.6.5).
 *
 * هذا المتحقق يركز على قواعد "سلامة الـ IR" العامة (غير SSA-specific):
 * - صحة قائمة التعليمات داخل الكتلة (prev/next/first/last/inst_count/parent)
 * - صحة المنهيات (terminators) وأنها آخر تعليمة في الكتلة
 * - عدد المعاملات لكل تعليمة
 * - اتساق الأنواع بين نتيجة التعليمة ومعاملاتها
 * - تموضع `فاي` في بداية الكتل وصحة مدخلاته (بعد إعادة بناء preds)
 * - صحة `نداء` عند إمكانية التحقق من تواقيع الدوال داخل نفس الوحدة
 *
 * ملاحظة:
 * - المتحقق قد يستدعي ir_func_rebuild_preds() لإعادة بناء succ/pred من المنهيات
 *   (هذا يحدّث كاشات CFG لكنه لا يغيّر المعنى الدلالي للبرنامج).
 */

#include "ir_verify_ir.h"

#include "ir_analysis.h"
#include "ir_verify_common.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// تشخيص (Diagnostics)
// ============================================================================

#define IR_VERIFY_MAX_ERRORS 30

typedef struct {
    FILE* out;
    int error_count;
    int error_cap;
} IRVerifyDiag;

static const char* ir_safe_str(const char* s) {
    return s ? s : "<غير_معروف>";
}

static void ir_print_loc(FILE* out, const IRInst* inst) {
    if (!out || !inst) return;
    if (!inst->src_file || inst->src_line <= 0) return;

    if (inst->src_col > 0) {
        fprintf(out, " (الموقع: %s:%d:%d)", inst->src_file, inst->src_line, inst->src_col);
    } else {
        fprintf(out, " (الموقع: %s:%d)", inst->src_file, inst->src_line);
    }
}

static void ir_report(IRVerifyDiag* diag,
                      const IRModule* module,
                      const IRFunc* func,
                      const IRBlock* block,
                      const IRInst* inst,
                      const char* fmt,
                      ...) {
    if (!diag) return;
    if (!diag->out) diag->out = stderr;
    if (diag->error_count >= diag->error_cap) return;

    fprintf(diag->out, "خطأ IR: ");
    if (module && module->name) {
        fprintf(diag->out, "[%s] ", module->name);
    }

    fprintf(diag->out, "@%s | %s",
            ir_safe_str(func ? func->name : NULL),
            ir_safe_str(block ? block->label : NULL));

    if (inst) {
        fprintf(diag->out, " | %s", ir_op_to_arabic(inst->op));
    }

    fputs(": ", diag->out);

    va_list args;
    va_start(args, fmt);
    vfprintf(diag->out, fmt, args);
    va_end(args);

    if (inst) {
        ir_print_loc(diag->out, inst);
    }

    fputc('\n', diag->out);

    diag->error_count++;
    if (diag->error_count == diag->error_cap) {
        fprintf(diag->out,
                "ملاحظة: تم الوصول إلى حد الأخطاء (%d). سيتم إخفاء الباقي.\n",
                diag->error_cap);
    }
}

// ============================================================================
// مساعدات أنواع/قيم (Type/Value helpers)
// ============================================================================

static int ir_type_is_int(IRType* t) {
    if (!t) return 0;
    switch (t->kind) {
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

static int ir_type_is_ptr(IRType* t) {
    return t && t->kind == IR_TYPE_PTR;
}

static IRType* ir_ptr_pointee(IRType* t) {
    if (!ir_type_is_ptr(t)) return NULL;
    return t->data.pointee;
}

static int ir_value_has_type(const IRValue* v) {
    if (!v) return 0;

    // IR_VAL_BLOCK لا يحمل نوعاً.
    if (v->kind == IR_VAL_BLOCK) return 1;

    return v->type != NULL;
}

static int ir_value_is_block(IRValue* v) {
    return v && v->kind == IR_VAL_BLOCK && v->data.block != NULL;
}

static int ir_block_is_in_func(const IRBlock* b, const IRFunc* func) {
    return b && func && b->parent == func;
}

static int ir_value_is_block_in_func(IRValue* v, const IRFunc* func) {
    if (!ir_value_is_block(v)) return 0;
    return ir_block_is_in_func(v->data.block, func);
}

static int ir_value_is_ptr_value(IRValue* v) {
    if (!v) return 0;
    if (!v->type) return 0;
    return v->type->kind == IR_TYPE_PTR;
}

static int ir_value_reg_in_range(IRValue* v, const IRFunc* func) {
    if (!v || !func) return 0;
    if (v->kind != IR_VAL_REG) return 1; // ليست سجلاً.
    int r = v->data.reg_num;
    return r >= 0 && r < func->next_reg;
}

static int ir_inst_dest_in_range(const IRInst* inst, const IRFunc* func) {
    if (!inst || !func) return 0;
    if (inst->dest < 0) return 1;
    return inst->dest >= 0 && inst->dest < func->next_reg;
}

static int ir_is_terminator_op(IROp op) {
    return op == IR_OP_BR || op == IR_OP_BR_COND || op == IR_OP_RET;
}

typedef struct
{
    IRVerifyDiag* diag;
    const IRModule* module;
    const IRFunc* func;
} IRVerifyCommonCtx;

static void ir_verify_common_report(void* ctx,
                                    const IRFunc* func,
                                    const IRBlock* block,
                                    const IRInst* inst,
                                    const char* msg)
{
    (void)func;
    IRVerifyCommonCtx* c = (IRVerifyCommonCtx*)ctx;
    if (!c || !c->diag) return;
    ir_report(c->diag, c->module, c->func, block, inst, "%s", msg ? msg : "");
}

// ============================================================================
// تحقق قائمة التعليمات داخل الكتلة (Instruction list verification)
// ============================================================================

static void ir_verify_block_list(IRVerifyDiag* diag,
                                 const IRModule* module,
                                 const IRFunc* func,
                                 const IRBlock* block) {
    if (!diag || !func || !block) return;

    if (block->parent != func) {
        ir_report(diag, module, func, block, NULL,
                  "حقل parent للكتلة لا يشير إلى الدالة الحاوية.");
    }

    int count = 0;
    const IRInst* prev = NULL;
    const IRInst* last_seen = NULL;

    for (const IRInst* inst = block->first; inst; inst = inst->next) {
        count++;
        last_seen = inst;

        if (inst->parent != block) {
            ir_report(diag, module, func, block, inst,
                      "حقل parent للتعليمة لا يشير إلى الكتلة الحاوية.");
        }

        if (inst->prev != prev) {
            ir_report(diag, module, func, block, inst,
                      "سلسلة prev/next غير متماسكة داخل الكتلة.");
        }

        prev = inst;
    }

    if (block->first == NULL && block->last != NULL) {
        ir_report(diag, module, func, block, NULL,
                  "block->first فارغ لكن block->last غير فارغ.");
    }

    if (block->first != NULL && block->last == NULL) {
        ir_report(diag, module, func, block, NULL,
                  "block->first غير فارغ لكن block->last فارغ.");
    }

    if (block->last != last_seen) {
        ir_report(diag, module, func, block, NULL,
                  "block->last لا يطابق آخر تعليمة في سلسلة next.");
    }

    if (block->inst_count != count) {
        ir_report(diag, module, func, block, NULL,
                  "inst_count غير مطابق لعدد التعليمات الفعلي (inst_count=%d, actual=%d).",
                  block->inst_count, count);
    }
}

// ============================================================================
// تحقق Phi (فاي)
// ============================================================================

static int ir_block_pred_index_local(const IRBlock* block, const IRBlock* pred) {
    if (!block || !pred) return -1;
    for (int i = 0; i < block->pred_count; i++) {
        if (block->preds[i] == pred) return i;
    }
    return -1;
}

static void ir_verify_phi(IRVerifyDiag* diag,
                          const IRModule* module,
                          const IRFunc* func,
                          const IRBlock* block,
                          const IRInst* phi) {
    if (!diag || !func || !block || !phi) return;
    if (phi->op != IR_OP_PHI) return;

    if (phi->dest < 0) {
        ir_report(diag, module, func, block, phi, "تعليمة `فاي` بدون سجل وجهة.");
    }

    if (!phi->type || phi->type->kind == IR_TYPE_VOID) {
        ir_report(diag, module, func, block, phi, "تعليمة `فاي` بدون نوع صالح.");
    }

    // phi_entries يجب أن يطابق عدد السوابق (بعد rebuild_preds).
    int pred_count = block->pred_count;
    if (pred_count == 0) {
        // قد تكون entry بلا سوابق؛ وجود phi هنا غالباً خطأ.
        if (phi->phi_entries) {
            ir_report(diag, module, func, block, phi, "تعليمة `فاي` داخل كتلة بلا سوابق.");
        }
        return;
    }

    unsigned char* seen = (unsigned char*)calloc((size_t)pred_count, sizeof(unsigned char));
    if (!seen) {
        ir_report(diag, module, func, block, phi, "فشل تخصيص ذاكرة للتحقق من `فاي`.");
        return;
    }

    int entry_count = 0;
    for (IRPhiEntry* e = phi->phi_entries; e; e = e->next) {
        entry_count++;

        if (!e->block || !e->value) {
            ir_report(diag, module, func, block, phi, "مدخل `فاي` غير صالح (كتلة/قيمة فارغة).");
            continue;
        }

        if (e->block->parent != func) {
            ir_report(diag, module, func, block, phi,
                      "مدخل `فاي` يشير إلى كتلة خارج الدالة (غير مسموح).");
            continue;
        }

        int idx = ir_block_pred_index_local(block, e->block);
        if (idx < 0) {
            ir_report(diag, module, func, block, phi,
                      "مدخل `فاي` يشير إلى كتلة ليست سابقاً: %s",
                      ir_safe_str(e->block->label));
            continue;
        }

        if (seen[idx]) {
            ir_report(diag, module, func, block, phi,
                      "مدخل `فاي` مكرر لنفس السابق: %s",
                      ir_safe_str(e->block->label));
            continue;
        }
        seen[idx] = 1;

        // اتساق الأنواع: value.type يجب أن يطابق phi.type (بشكل محافظ).
        if (!e->value->type || !phi->type || !ir_types_equal(e->value->type, phi->type)) {
            ir_report(diag, module, func, block, phi,
                      "نوع قيمة `فاي` لا يطابق نوع `فاي` (value=%s, phi=%s).",
                      ir_type_to_arabic(e->value->type),
                      ir_type_to_arabic(phi->type));
        }

        if (e->value->kind == IR_VAL_REG && !ir_value_reg_in_range(e->value, func)) {
            ir_report(diag, module, func, block, phi,
                      "مدخل `فاي` يستخدم سجلاً خارج نطاق الدالة.");
        }
    }

    if (entry_count != pred_count) {
        // حتى لو احتوت على أكثر من pred_count بسبب تكرارات/أخطاء، نبلغ.
        ir_report(diag, module, func, block, phi,
                  "عدد مداخل `فاي` لا يطابق عدد السوابق (entries=%d, preds=%d).",
                  entry_count, pred_count);
    }

    for (int i = 0; i < pred_count; i++) {
        if (!seen[i]) {
            IRBlock* p = block->preds ? block->preds[i] : NULL;
            ir_report(diag, module, func, block, phi,
                      "مدخل `فاي` مفقود للسابق: %s",
                      ir_safe_str(p ? p->label : NULL));
        }
    }

    free(seen);
}

// ============================================================================
// تحقق تعليمة واحدة (Instruction verification)
// ============================================================================

static void ir_verify_inst_operands_present(IRVerifyDiag* diag,
                                            const IRModule* module,
                                            const IRFunc* func,
                                            const IRBlock* block,
                                            const IRInst* inst,
                                            int required) {
    if (!diag || !inst) return;

    if (inst->operand_count != required) {
        ir_report(diag, module, func, block, inst,
                  "عدد المعاملات غير صحيح (expected=%d, got=%d).",
                  required, inst->operand_count);
        return;
    }

    for (int i = 0; i < required; i++) {
        if (!inst->operands[i]) {
            ir_report(diag, module, func, block, inst,
                      "معامل فارغ (NULL) عند الفهرس %d.", i);
        } else if (!ir_value_has_type(inst->operands[i])) {
            ir_report(diag, module, func, block, inst,
                      "معامل بدون نوع صالح عند الفهرس %d.", i);
        } else if (!ir_value_reg_in_range(inst->operands[i], func)) {
            ir_report(diag, module, func, block, inst,
                      "معامل سجل خارج نطاق الدالة عند الفهرس %d.", i);
        }
    }
}

static void ir_verify_inst_dest_rules(IRVerifyDiag* diag,
                                      const IRModule* module,
                                      const IRFunc* func,
                                      const IRBlock* block,
                                      const IRInst* inst,
                                      int must_have_dest) {
    if (!diag || !inst) return;

    if (must_have_dest) {
        if (inst->dest < 0) {
            ir_report(diag, module, func, block, inst, "التعليمة يجب أن تملك سجل وجهة (dest).");
        }
    } else {
        if (inst->dest >= 0) {
            ir_report(diag, module, func, block, inst, "التعليمة لا يجب أن تملك سجل وجهة (dest).");
        }
    }

    if (!ir_inst_dest_in_range(inst, func)) {
        ir_report(diag, module, func, block, inst, "سجل الوجهة خارج نطاق الدالة.");
    }
}

static void ir_verify_value_type_eq(IRVerifyDiag* diag,
                                    const IRModule* module,
                                    const IRFunc* func,
                                    const IRBlock* block,
                                    const IRInst* inst,
                                    IRType* expected,
                                    IRValue* v,
                                    const char* what) {
    if (!diag || !inst) return;
    if (!expected || !v || !v->type) return;

    if (!ir_types_equal(expected, v->type)) {
        ir_report(diag, module, func, block, inst,
                  "عدم اتساق النوع (%s): expected=%s, got=%s.",
                  what,
                  ir_type_to_arabic(expected),
                  ir_type_to_arabic(v->type));
    }
}


#include "ir_verify_inst.c"
#include "ir_verify_globals_local.c"
