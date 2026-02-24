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

static void ir_verify_inst(IRVerifyDiag* diag,
                           const IRModule* module,
                           const IRFunc* func,
                           const IRBlock* block,
                           const IRInst* inst,
                           int seen_non_phi,
                           const IRModule* verify_module_for_calls) {
    if (!diag || !func || !block || !inst) return;

    // قاعدة: بعد رؤية تعليمة غير فاي، لا يُسمح بوجود فاي.
    if (inst->op == IR_OP_PHI && seen_non_phi) {
        ir_report(diag, module, func, block, inst, "تعليمة `فاي` ليست في بداية الكتلة.");
    }

    // تحقق عام للنوع/الوجهة.
    if (inst->op != IR_OP_STORE && inst->op != IR_OP_BR && inst->op != IR_OP_BR_COND && inst->op != IR_OP_RET) {
        // معظم التعليمات المنتجة للقيم يجب أن تملك type غير void.
        if (!inst->type) {
            ir_report(diag, module, func, block, inst, "تعليمة بدون نوع (type=NULL).");
        }
    }

    switch (inst->op) {
        // --------------------------------------------------------------------
        // تعليمات ثنائية (2 operands + dest)
        // --------------------------------------------------------------------
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_XOR:
        case IR_OP_SHL:
        case IR_OP_SHR: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة ثنائية بدون نوع نتيجة صالح.");
                break;
            }

            bool is_float = (inst->type && inst->type->kind == IR_TYPE_F64);
            bool is_int = ir_type_is_int(inst->type);

            // العمليات البتية/الإزاحة/باقي القسمة: أعداد صحيحة فقط.
            if (inst->op == IR_OP_AND || inst->op == IR_OP_OR || inst->op == IR_OP_XOR ||
                inst->op == IR_OP_SHL || inst->op == IR_OP_SHR || inst->op == IR_OP_MOD) {
                if (!is_int) {
                    ir_report(diag, module, func, block, inst, "تعليمة ثنائية تتوقع نوعاً صحيحاً.");
                    break;
                }
            } else {
                // ADD/SUB/MUL/DIV: صحيح أو عشري.
                if (!(is_int || is_float)) {
                    ir_report(diag, module, func, block, inst, "تعليمة ثنائية تتوقع نوعاً عددياً (صحيح/عشري).");
                    break;
                }
            }

            if (inst->operands[0]) ir_verify_value_type_eq(diag, module, func, block, inst, inst->type, inst->operands[0], "lhs");
            if (inst->operands[1]) ir_verify_value_type_eq(diag, module, func, block, inst, inst->type, inst->operands[1], "rhs");
            break;
        }

        // --------------------------------------------------------------------
        // تعليمات أحادية (1 operand + dest)
        // --------------------------------------------------------------------
        case IR_OP_NEG:
        case IR_OP_NOT:
        case IR_OP_COPY: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة أحادية بدون نوع نتيجة صالح.");
                break;
            }

            if (inst->operands[0]) {
                ir_verify_value_type_eq(diag, module, func, block, inst, inst->type, inst->operands[0], "operand");
            }
            break;
        }

        // --------------------------------------------------------------------
        // alloca: dest + type = ptr(pointee), operand_count=0
        // --------------------------------------------------------------------
        case IR_OP_ALLOCA: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);

            if (inst->operand_count != 0) {
                ir_report(diag, module, func, block, inst, "تعليمة `حجز` يجب ألا تملك معاملات.");
            }

            if (!inst->type || inst->type->kind != IR_TYPE_PTR || !inst->type->data.pointee) {
                ir_report(diag, module, func, block, inst, "تعليمة `حجز` يجب أن تنتج نوع مؤشر صالح.");
            }

            if (inst->type && inst->type->kind == IR_TYPE_PTR &&
                inst->type->data.pointee && inst->type->data.pointee->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `حجز` لا يجب أن تحجز نوع فراغ.");
            }
            break;
        }

        // --------------------------------------------------------------------
        // load: dest + 1 operand (ptr), type == pointee(ptr)
        // --------------------------------------------------------------------
        case IR_OP_LOAD: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            IRValue* ptr = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            if (!ptr || !ir_value_is_ptr_value(ptr)) {
                ir_report(diag, module, func, block, inst, "تعليمة `حمل` تتطلب معاملاً من نوع مؤشر.");
                break;
            }

            IRType* pointee = ir_ptr_pointee(ptr->type);
            if (!pointee) {
                ir_report(diag, module, func, block, inst, "تعليمة `حمل`: نوع المؤشر بدون pointee.");
                break;
            }

            if (!inst->type) {
                ir_report(diag, module, func, block, inst, "تعليمة `حمل` بدون نوع نتيجة.");
                break;
            }

            if (!ir_types_equal(inst->type, pointee)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `حمل`: نوع النتيجة لا يطابق pointee للمؤشر (load=%s, ptr=%s).",
                          ir_type_to_arabic(inst->type),
                          ir_type_to_arabic(pointee));
            }
            break;
        }

        // --------------------------------------------------------------------
        // store: no dest + 2 operands (value, ptr), ptr pointee == value.type
        // --------------------------------------------------------------------
        case IR_OP_STORE: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind != IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن` يجب أن يكون نوعها فراغ.");
            }

            IRValue* val = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            IRValue* ptr = (inst->operand_count >= 2) ? inst->operands[1] : NULL;

            if (!val || !val->type) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن`: قيمة بدون نوع.");
                break;
            }

            if (!ptr || !ir_value_is_ptr_value(ptr)) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن` تتطلب مؤشر كمعامل ثانٍ.");
                break;
            }

            IRType* pointee = ir_ptr_pointee(ptr->type);
            if (!pointee) {
                ir_report(diag, module, func, block, inst, "تعليمة `خزن`: نوع المؤشر بدون pointee.");
                break;
            }

            if (!ir_types_equal(pointee, val->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `خزن`: نوع pointee لا يطابق نوع القيمة (val=%s, ptr=%s).",
                          ir_type_to_arabic(val->type),
                          ir_type_to_arabic(pointee));
            }
            break;
        }

        // --------------------------------------------------------------------
        // ptr.offset: dest + 2 operands (base_ptr, index_int), result ptr
        // --------------------------------------------------------------------
        case IR_OP_PTR_OFFSET: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind != IR_TYPE_PTR || !inst->type->data.pointee) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر` يجب أن تنتج نوع مؤشر صالح.");
                break;
            }

            IRValue* base = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            IRValue* idx  = (inst->operand_count >= 2) ? inst->operands[1] : NULL;

            if (!base || !ir_value_is_ptr_value(base)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: المعامل الأول يجب أن يكون مؤشراً.");
                break;
            }

            if (base->type && inst->type && !ir_types_equal(base->type, inst->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: نوع base لا يطابق نوع النتيجة (base=%s, res=%s).",
                          ir_type_to_arabic(base->type),
                          ir_type_to_arabic(inst->type));
            }

            if (!idx || !idx->type || !ir_type_is_int(idx->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: المعامل الثاني (index) يجب أن يكون عدداً صحيحاً.");
            }

            if (base && !ir_value_reg_in_range(base, func)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: base سجل خارج نطاق الدالة.");
            }
            if (idx && !ir_value_reg_in_range(idx, func)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `إزاحة_مؤشر`: index سجل خارج نطاق الدالة.");
            }

            break;
        }

        // --------------------------------------------------------------------
        // cmp: dest + 2 operands, result i1, operands same int type
        // --------------------------------------------------------------------
        case IR_OP_CMP: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 2);

            if (!inst->type || inst->type->kind != IR_TYPE_I1) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن` يجب أن تنتج ص١.");
            }

            IRValue* a = (inst->operand_count >= 1) ? inst->operands[0] : NULL;
            IRValue* b = (inst->operand_count >= 2) ? inst->operands[1] : NULL;

            bool a_int = (a && a->type) ? ir_type_is_int(a->type) : 0;
            bool b_int = (b && b->type) ? ir_type_is_int(b->type) : 0;
            bool a_f64 = (a && a->type && a->type->kind == IR_TYPE_F64);
            bool b_f64 = (b && b->type && b->type->kind == IR_TYPE_F64);

            if (!a || !a->type || !(a_int || a_f64)) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن`: المعامل الأول ليس نوعاً عددياً.");
                break;
            }
            if (!b || !b->type || !(b_int || b_f64)) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن`: المعامل الثاني ليس نوعاً عددياً.");
                break;
            }
            if (!ir_types_equal(a->type, b->type)) {
                ir_report(diag, module, func, block, inst, "تعليمة `قارن`: نوعا المعاملين غير متطابقين.");
            }

            // قيود المحمول على العشري: لا نسمح بمقارنات unsigned.
            if (a_f64 || b_f64) {
                if (inst->cmp_pred == IR_CMP_UGT || inst->cmp_pred == IR_CMP_ULT ||
                    inst->cmp_pred == IR_CMP_UGE || inst->cmp_pred == IR_CMP_ULE)
                {
                    ir_report(diag, module, func, block, inst, "تعليمة `قارن`: محمول unsigned غير صالح على 'عشري'.");
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // br: no dest + 1 operand block
        // --------------------------------------------------------------------
        case IR_OP_BR: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            if (!ir_value_is_block(inst->operands[0])) {
                ir_report(diag, module, func, block, inst, "تعليمة `قفز` يجب أن تشير إلى كتلة.");
            } else if (!ir_value_is_block_in_func(inst->operands[0], func)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `قفز`: الهدف يشير إلى كتلة خارج الدالة (غير مسموح).");
            }
            break;
        }

        // --------------------------------------------------------------------
        // br_cond: no dest + 3 operands: cond(i1), true(block), false(block)
        // --------------------------------------------------------------------
        case IR_OP_BR_COND: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 3);

            IRValue* cond = inst->operands[0];
            if (!cond || !cond->type || cond->type->kind != IR_TYPE_I1) {
                ir_report(diag, module, func, block, inst, "تعليمة `قفز_شرط`: الشرط يجب أن يكون ص١.");
            }

            if (!ir_value_is_block(inst->operands[1]) || !ir_value_is_block(inst->operands[2])) {
                ir_report(diag, module, func, block, inst, "تعليمة `قفز_شرط`: أهداف القفز يجب أن تكون كتل.");
            } else {
                if (!ir_value_is_block_in_func(inst->operands[1], func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `قفز_شرط`: هدف_صواب يشير إلى كتلة خارج الدالة (غير مسموح).");
                }
                if (!ir_value_is_block_in_func(inst->operands[2], func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `قفز_شرط`: هدف_خطأ يشير إلى كتلة خارج الدالة (غير مسموح).");
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // ret: no dest, 0 أو 1 معامل، النوع يجب أن يطابق نوع الدالة
        // --------------------------------------------------------------------
        case IR_OP_RET: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 0);

            if (inst->operand_count != 0 && inst->operand_count != 1) {
                ir_report(diag, module, func, block, inst, "تعليمة `رجوع`: عدد معاملات غير صالح.");
                break;
            }

            if (inst->operand_count == 0) {
                if (!func->ret_type || func->ret_type->kind != IR_TYPE_VOID) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `رجوع` بدون قيمة لكن نوع الدالة ليس فراغاً.");
                }
                break;
            }

            IRValue* v = inst->operands[0];
            if (!v || !v->type) {
                ir_report(diag, module, func, block, inst, "تعليمة `رجوع`: قيمة بدون نوع.");
                break;
            }

            if (!func->ret_type) {
                ir_report(diag, module, func, block, inst, "تعليمة `رجوع`: func->ret_type فارغ.");
                break;
            }

            if (!ir_types_equal(func->ret_type, v->type)) {
                ir_report(diag, module, func, block, inst,
                          "تعليمة `رجوع`: نوع القيمة لا يطابق نوع الدالة (val=%s, func=%s).",
                          ir_type_to_arabic(v->type),
                          ir_type_to_arabic(func->ret_type));
            }
            break;
        }

        // --------------------------------------------------------------------
        // call: dest اختياري، call_target غير فارغ، call_args حسب call_arg_count
        // --------------------------------------------------------------------
        case IR_OP_CALL: {
            if (!inst->call_target || !inst->call_target[0]) {
                ir_report(diag, module, func, block, inst, "تعليمة `نداء`: اسم الهدف فارغ.");
            }

            if (inst->call_arg_count < 0) {
                ir_report(diag, module, func, block, inst, "تعليمة `نداء`: call_arg_count سالب.");
            }

            if (inst->call_arg_count > 0 && !inst->call_args) {
                ir_report(diag, module, func, block, inst, "تعليمة `نداء`: call_args فارغ رغم وجود معاملات.");
            }

            for (int i = 0; i < inst->call_arg_count; i++) {
                IRValue* a = inst->call_args ? inst->call_args[i] : NULL;
                if (!a) {
                    ir_report(diag, module, func, block, inst, "تعليمة `نداء`: معامل فارغ عند الفهرس %d.", i);
                    continue;
                }
                if (!a->type) {
                    ir_report(diag, module, func, block, inst, "تعليمة `نداء`: معامل بدون نوع عند الفهرس %d.", i);
                }
                if (!ir_value_reg_in_range(a, func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: معامل سجل خارج نطاق الدالة عند الفهرس %d.", i);
                }
            }

            // قواعد dest/ret_type.
            if (inst->dest >= 0) {
                if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: dest موجود لكن نوع الإرجاع فراغ/غير صالح.");
                }
                if (!ir_inst_dest_in_range(inst, func)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: سجل الوجهة خارج نطاق الدالة.");
                }
            } else {
                if (inst->type && inst->type->kind != IR_TYPE_VOID) {
                    // ليس خطأ قاتل، لكن يساعد على كشف تناقضات البناء.
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `نداء`: لا يوجد dest لكن نوع التعليمة ليس فراغاً.");
                }
            }

            // تحقق توقيع الدالة إن أمكن (فقط على مستوى الوحدة).
            if (verify_module_for_calls && inst->call_target) {
                IRFunc* callee = ir_module_find_func((IRModule*)verify_module_for_calls, inst->call_target);
                if (callee) {
                    if (callee->param_count != inst->call_arg_count) {
                        ir_report(diag, module, func, block, inst,
                                  "تعليمة `نداء`: عدد المعاملات لا يطابق توقيع الدالة (%d vs %d).",
                                  inst->call_arg_count, callee->param_count);
                    } else {
                        for (int i = 0; i < callee->param_count; i++) {
                            IRType* pt = callee->params[i].type;
                            IRValue* av = inst->call_args ? inst->call_args[i] : NULL;
                            if (pt && av && av->type && !ir_types_equal(pt, av->type)) {
                                ir_report(diag, module, func, block, inst,
                                          "تعليمة `نداء`: نوع المعامل %d لا يطابق (arg=%s, param=%s).",
                                          i,
                                          ir_type_to_arabic(av->type),
                                          ir_type_to_arabic(pt));
                            }
                        }
                    }

                    // نوع الإرجاع.
                    if (inst->dest >= 0) {
                        if (callee->ret_type && inst->type && !ir_types_equal(callee->ret_type, inst->type)) {
                            ir_report(diag, module, func, block, inst,
                                      "تعليمة `نداء`: نوع الإرجاع لا يطابق توقيع الدالة (call=%s, callee=%s).",
                                      ir_type_to_arabic(inst->type),
                                      ir_type_to_arabic(callee->ret_type));
                        }
                    }
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // phi
        // --------------------------------------------------------------------
        case IR_OP_PHI:
            ir_verify_phi(diag, module, func, block, inst);
            break;

        // --------------------------------------------------------------------
        // cast: dest + 1 operand، النوعان غير void
        // --------------------------------------------------------------------
        case IR_OP_CAST: {
            ir_verify_inst_dest_rules(diag, module, func, block, inst, 1);
            ir_verify_inst_operands_present(diag, module, func, block, inst, 1);

            if (!inst->type || inst->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `تحويل`: نوع الهدف غير صالح.");
            }

            IRValue* v = inst->operands[0];
            if (v && v->type && v->type->kind == IR_TYPE_VOID) {
                ir_report(diag, module, func, block, inst, "تعليمة `تحويل`: نوع المصدر فراغ غير صالح.");
            }

            // قواعد تحويل محافظة: نسمح بتحويل عدد <-> عدد أو مؤشر <-> مؤشر أو ص١ -> عدد.
            if (inst->type && v && v->type) {
                int from_int = ir_type_is_int(v->type);
                int to_int = ir_type_is_int(inst->type);
                int from_ptr = ir_type_is_ptr(v->type);
                int to_ptr = ir_type_is_ptr(inst->type);

                int from_f64 = (v->type->kind == IR_TYPE_F64);
                int to_f64 = (inst->type->kind == IR_TYPE_F64);

                // سماح: int <-> f64 كتحويل عددي.
                int ok_num_f64 = (from_int && to_f64) || (from_f64 && to_int) || (from_f64 && to_f64);

                if (!((from_int && to_int) || (from_ptr && to_ptr) || (v->type->kind == IR_TYPE_I1 && to_int) || ok_num_f64)) {
                    ir_report(diag, module, func, block, inst,
                              "تعليمة `تحويل`: تحويل غير مدعوم/غير واضح (from=%s, to=%s).",
                              ir_type_to_arabic(v->type),
                              ir_type_to_arabic(inst->type));
                }
            }
            break;
        }

        // --------------------------------------------------------------------
        // nop: بدون معاملات ولا وجهة
        // --------------------------------------------------------------------
        case IR_OP_NOP:
            if (inst->operand_count != 0) {
                ir_report(diag, module, func, block, inst, "تعليمة NOP يجب ألا تملك معاملات.");
            }
            if (inst->dest >= 0) {
                ir_report(diag, module, func, block, inst, "تعليمة NOP لا يجب أن تملك وجهة.");
            }
            break;

        default:
            ir_report(diag, module, func, block, inst, "Opcode غير معروف/غير مدعوم في المتحقق.");
            break;
    }
}

// ============================================================================
// تحقق دالة (Function verification)
// ============================================================================

static bool ir_verify_func_internal(IRModule* module_for_diag,
                                    IRModule* module_for_calls,
                                    IRFunc* func,
                                    FILE* out) {
    IRVerifyDiag diag = {0};
    diag.out = out ? out : stderr;
    diag.error_cap = IR_VERIFY_MAX_ERRORS;

    if (!func) return false;
    if (func->is_prototype) return true;

    if (!func->entry) {
        ir_report(&diag, module_for_diag, func, NULL, NULL,
                  "الدالة تحتوي جسماً بدون كتلة دخول (entry).");
        return false;
    }

    // تحقق مبكر: لا تسمح بمراجع كتل خارج هذه الدالة (حماية قبل rebuild_preds).
    IRVerifyCommonCtx cctx;
    cctx.diag = &diag;
    cctx.module = module_for_diag;
    cctx.func = func;
    if (!ir_verify_func_block_refs_safe(func, ir_verify_common_report, &cctx))
        return false;

    // إعادة بناء succ/pred لضمان أن pred_count دقيق لفحص `فاي` و CFG.
    ir_func_rebuild_preds(func);

    // تحقق سريع للـ CFG (منهيات/أهداف القفز).
    if (!ir_func_validate_cfg(func)) {
        ir_report(&diag, module_for_diag, func, func->entry, func->entry ? func->entry->last : NULL,
                  "CFG غير صالح (منهيات/أهداف قفز).");
        return false;
    }

    // تحقق المعاملات: توافر params/أنواعها.
    if (func->param_count < 0) {
        ir_report(&diag, module_for_diag, func, func->entry, NULL, "param_count سالب.");
    }
    if (func->param_count > 0 && !func->params) {
        ir_report(&diag, module_for_diag, func, func->entry, NULL, "params فارغ رغم وجود معاملات.");
    }
    for (int i = 0; i < func->param_count; i++) {
        if (!func->params[i].type) {
            ir_report(&diag, module_for_diag, func, func->entry, NULL,
                      "نوع معامل الدالة فارغ عند الفهرس %d.", i);
        }
        if (func->params[i].reg < 0 || func->params[i].reg >= func->next_reg) {
            ir_report(&diag, module_for_diag, func, func->entry, NULL,
                      "سجل معامل الدالة خارج نطاق next_reg عند الفهرس %d.", i);
        }
    }

    if (func->next_reg < 0) {
        ir_report(&diag, module_for_diag, func, func->entry, NULL, "next_reg سالب.");
    }

    // تحقق الكتل.
    for (IRBlock* b = func->blocks; b; b = b->next) {
        if (!b) continue;

        ir_verify_block_list(&diag, module_for_diag, func, b);

        int seen_non_phi = 0;
        int seen_terminator = 0;

        for (IRInst* inst = b->first; inst; inst = inst->next) {
            if (!inst) continue;

            if (seen_terminator) {
                ir_report(&diag, module_for_diag, func, b, inst,
                          "تعليمة بعد المنهي داخل نفس الكتلة (غير مسموح).");
            }

            if (inst->op != IR_OP_PHI) {
                seen_non_phi = 1;
            }

            ir_verify_inst(&diag, module_for_diag, func, b, inst, seen_non_phi, module_for_calls);

            if (ir_is_terminator_op(inst->op)) {
                seen_terminator = 1;
                // يجب أن يكون المنهي هو آخر تعليمة.
                if (inst != b->last) {
                    ir_report(&diag, module_for_diag, func, b, inst,
                              "المنهي ليس آخر تعليمة في الكتلة.");
                }
            }

            // تحقق نطاق الوجهة أيضاً (مفيد للـ SSA/التمريرات).
            if (!ir_inst_dest_in_range(inst, func)) {
                ir_report(&diag, module_for_diag, func, b, inst, "وجهة خارج نطاق next_reg.");
            }
        }
    }

    return diag.error_count == 0;
}

// ============================================================================
// تحقق Global (على مستوى الوحدة)
// ============================================================================

static void ir_verify_globals(IRVerifyDiag* diag, IRModule* module) {
    if (!diag || !module) return;

    for (IRGlobal* g = module->globals; g; g = g->next) {
        if (!g->name || !g->name[0]) {
            ir_report(diag, module, NULL, NULL, NULL, "متغير عام بدون اسم.");
        }
        if (!g->type) {
            ir_report(diag, module, NULL, NULL, NULL, "متغير عام @%s بدون نوع.",
                      ir_safe_str(g->name));
            continue;
        }
        if (g->type->kind == IR_TYPE_VOID) {
            ir_report(diag, module, NULL, NULL, NULL, "متغير عام @%s من نوع فراغ (غير مسموح).",
                      ir_safe_str(g->name));
        }

        // مصفوفة عامة: تحقق من قائمة التهيئة
        if (g->type->kind == IR_TYPE_ARRAY) {
            IRType* elem_t = g->type->data.array.element;
            int count = g->type->data.array.count;

            if (g->init) {
                ir_report(diag, module, NULL, NULL, NULL,
                          "متغير عام @%s من نوع مصفوفة لا يجب أن يملك init مفرداً.",
                          ir_safe_str(g->name));
            }

            if (count < 0) {
                ir_report(diag, module, NULL, NULL, NULL,
                          "متغير عام @%s لديه حجم مصفوفة غير صالح.",
                          ir_safe_str(g->name));
            }

            if (g->init_elem_count < 0 || (count >= 0 && g->init_elem_count > count)) {
                ir_report(diag, module, NULL, NULL, NULL,
                          "تهيئة مصفوفة عامة @%s بعدد عناصر غير صالح (init=%d, size=%d).",
                          ir_safe_str(g->name), g->init_elem_count, count);
            }

            for (int i = 0; i < g->init_elem_count; i++) {
                IRValue* v = g->init_elems ? g->init_elems[i] : NULL;
                if (!v) {
                    ir_report(diag, module, NULL, NULL, NULL,
                              "تهيئة مصفوفة عامة @%s تحتوي على عنصر NULL عند %d.",
                              ir_safe_str(g->name), i);
                    continue;
                }
                if (elem_t && v->type && !ir_types_equal(elem_t, v->type)) {
                    ir_report(diag, module, NULL, NULL, NULL,
                              "تهيئة مصفوفة عامة @%s بعنصر نوعه لا يطابق (elem=%s, init=%s).",
                              ir_safe_str(g->name),
                              ir_type_to_arabic(elem_t),
                              ir_type_to_arabic(v->type));
                }

                // قيود: إن كان العنصر عدداً صحيحاً، يجب أن يكون ثابتاً.
                if (elem_t && (elem_t->kind == IR_TYPE_I1 || elem_t->kind == IR_TYPE_I8 ||
                              elem_t->kind == IR_TYPE_I16 || elem_t->kind == IR_TYPE_I32 ||
                              elem_t->kind == IR_TYPE_I64 ||
                              elem_t->kind == IR_TYPE_U8 || elem_t->kind == IR_TYPE_U16 ||
                              elem_t->kind == IR_TYPE_U32 || elem_t->kind == IR_TYPE_U64))
                {
                    if (v->kind != IR_VAL_CONST_INT) {
                        ir_report(diag, module, NULL, NULL, NULL,
                                  "تهيئة مصفوفة عامة @%s يجب أن تكون ثوابت عددية.",
                                  ir_safe_str(g->name));
                    }
                }
            }

            continue;
        }

        if (g->init && g->init->type && !ir_types_equal(g->type, g->init->type)) {
            // ملاحظة: في IR الحالي، global->type هو "نوع القيمة" وليس نوع المؤشر.
            ir_report(diag, module, NULL, NULL, NULL,
                      "تهيئة متغير عام @%s بنوع لا يطابق (init=%s, global=%s).",
                      ir_safe_str(g->name),
                      ir_type_to_arabic(g->init->type),
                      ir_type_to_arabic(g->type));
        }
    }
}

// ============================================================================
// الواجهات العامة
// ============================================================================

bool ir_func_verify_ir(IRFunc* func, FILE* out) {
    // لا يوجد سياق وحدة هنا → لا نتحقق من تواقيع النداءات عبر الوحدة.
    return ir_verify_func_internal(NULL, NULL, func, out);
}

bool ir_module_verify_ir(IRModule* module, FILE* out) {
    IRVerifyDiag diag = {0};
    diag.out = out ? out : stderr;
    diag.error_cap = IR_VERIFY_MAX_ERRORS;

    if (!module) return false;

    // تحقق globals أولاً.
    ir_verify_globals(&diag, module);

    int ok = (diag.error_count == 0) ? 1 : 0;

    // تحقق الدوال، مع تمكين فحص تواقيع `نداء` عندما تكون الدالة الهدف داخل الوحدة.
    for (IRFunc* f = module->funcs; f; f = f->next) {
        if (!ir_verify_func_internal(module, module, f, diag.out)) {
            ok = 0;
        }
    }

    return ok ? true : false;
}
