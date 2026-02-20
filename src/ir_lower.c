/**
 * @file ir_lower.c
 * @brief خفض AST إلى IR (التعبيرات + الجمل + تدفق التحكم).
 * @version 0.3.0.5
 *
 * Implements lowering from validated AST into Baa IR using IRBuilder.
 *
 * v0.3.0.3: Expression lowering
 * v0.3.0.4: Statement lowering (var decl/assign/return/print/read)
 * v0.3.0.5: Control-flow lowering (if/while/for/switch/break/continue)
 */

#include "ir_lower.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

// ============================================================================
// Locals table helpers
// ============================================================================

static IRLowerBinding* find_local(IRLowerCtx* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    // البحث من الأحدث للأقدم لدعم النطاقات المتداخلة.
    for (int i = ctx->local_count - 1; i >= 0; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            return &ctx->locals[i];
        }
    }
    return NULL;
}

static void ir_lower_report_error(IRLowerCtx* ctx, const Node* node, const char* fmt, ...)
{
    if (ctx) ctx->had_error = 1;

    const char* file = (node && node->filename) ? node->filename : "<غير_معروف>";
    int line = (node && node->line > 0) ? node->line : 1;
    int col = (node && node->col > 0) ? node->col : 1;

    fprintf(stderr, "خطأ (تحويل IR): %s:%d:%d: ", file, line, col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

static int ir_lower_current_scope_start(IRLowerCtx* ctx) {
    if (!ctx) return 0;
    if (ctx->scope_depth <= 0) return 0;
    return ctx->scope_stack[ctx->scope_depth - 1];
}

static void ir_lower_scope_push(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->scope_depth >= (int)(sizeof(ctx->scope_stack) / sizeof(ctx->scope_stack[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد مكدس النطاقات.");
        return;
    }
    ctx->scope_stack[ctx->scope_depth++] = ctx->local_count;
}

static void ir_lower_scope_pop(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->scope_depth <= 0) {
        ir_lower_report_error(ctx, NULL, "نقص في مكدس النطاقات.");
        return;
    }
    int start = ctx->scope_stack[--ctx->scope_depth];
    if (start < 0) start = 0;
    if (start > ctx->local_count) start = ctx->local_count;
    ctx->local_count = start;
}

void ir_lower_ctx_init(IRLowerCtx* ctx, IRBuilder* builder) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->builder = builder;
    ctx->had_error = 0;

    // v0.3.0.5 control-flow lowering state
    ctx->label_counter = 0;
    ctx->cf_depth = 0;
    ctx->scope_depth = 0;
}

void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type) {
    if (!ctx || !name) return;

    // ابحث فقط داخل النطاق الحالي: إن وجد، حدّث الربط.
    int start = ir_lower_current_scope_start(ctx);
    for (int i = ctx->local_count - 1; i >= start; i--) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].ptr_reg = ptr_reg;
            ctx->locals[i].value_type = value_type;
            return;
        }
    }

    if (ctx->local_count >= (int)(sizeof(ctx->locals) / sizeof(ctx->locals[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد جدول المتغيرات المحلية أثناء ربط '%s'.", name);
        return;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].ptr_reg = ptr_reg;
    ctx->locals[ctx->local_count].value_type = value_type;
    ctx->local_count++;
}

// ============================================================================
// Expression/Statement lowering helpers
// ============================================================================

static void ir_lower_set_loc(IRBuilder* builder, const Node* node) {
    if (!builder) return;

    if (!node || !node->filename || node->line <= 0) {
        ir_builder_clear_loc(builder);
        return;
    }

    ir_builder_set_loc(builder, node->filename, node->line, node->col);
}

static void ir_lower_tag_last_inst(IRBuilder* builder, IROp expected_op, int expected_dest,
                                   const char* dbg_name) {
    if (!builder || !builder->insert_block) return;
    if (!dbg_name) return;

    IRInst* last = builder->insert_block->last;
    if (!last) return;
    if (last->op != expected_op) return;
    if (expected_dest >= 0 && last->dest != expected_dest) return;

    ir_inst_set_dbg_name(last, dbg_name);
}

static IRType* get_i8_ptr_type(IRModule* module) {
    if (!module) return ir_type_ptr(IR_TYPE_I8_T);
    ir_module_set_current(module);
    return ir_type_ptr(IR_TYPE_I8_T);
}

static IRType* ir_type_from_datatype(IRModule* module, DataType t) {
    switch (t) {
        case TYPE_BOOL:   return IR_TYPE_I1_T;
        case TYPE_STRING: return get_i8_ptr_type(module);
        case TYPE_CHAR:   return IR_TYPE_I32_T;
        case TYPE_ENUM:   return IR_TYPE_I64_T;
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
    }
}

static IRValue* ensure_i64(IRLowerCtx* ctx, IRValue* v)
{
    if (!ctx || !ctx->builder || !v) return ir_value_const_int(0, IR_TYPE_I64_T);
    if (v->type && v->type->kind == IR_TYPE_I64) return v;
    int r = ir_builder_emit_cast(ctx->builder, v, IR_TYPE_I64_T);
    return ir_value_reg(r, IR_TYPE_I64_T);
}

static IRValue* cast_to(IRLowerCtx* ctx, IRValue* v, IRType* to)
{
    if (!ctx || !ctx->builder || !v || !to) return v;
    if (v->type && ir_types_equal(v->type, to)) return v;
    int r = ir_builder_emit_cast(ctx->builder, v, to);
    return ir_value_reg(r, to);
}

// ============================================================================
// Control-flow helpers (v0.3.0.5)
// ============================================================================

static int next_label_id(IRLowerCtx* ctx) {
    if (!ctx) return 0;
    return ctx->label_counter++;
}

static IRBlock* cf_create_block(IRLowerCtx* ctx, const char* base_label) {
    if (!ctx || !ctx->builder || !base_label) return NULL;

    char label[128];
    snprintf(label, sizeof(label), "%s_%d", base_label, next_label_id(ctx));
    return ir_builder_create_block(ctx->builder, label);
}

static void cf_push(IRLowerCtx* ctx, IRBlock* break_target, IRBlock* continue_target) {
    if (!ctx) return;
    if (ctx->cf_depth >= (int)(sizeof(ctx->break_targets) / sizeof(ctx->break_targets[0]))) {
        ir_lower_report_error(ctx, NULL, "تجاوز حد مكدس التحكم (break/continue).");
        return;
    }
    ctx->break_targets[ctx->cf_depth] = break_target;
    ctx->continue_targets[ctx->cf_depth] = continue_target;
    ctx->cf_depth++;
}

static void cf_pop(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->cf_depth <= 0) {
        ir_lower_report_error(ctx, NULL, "نقص في مكدس التحكم (break/continue).");
        return;
    }
    ctx->cf_depth--;
}

static IRBlock* cf_current_break(IRLowerCtx* ctx) {
    if (!ctx || ctx->cf_depth <= 0) return NULL;
    return ctx->break_targets[ctx->cf_depth - 1];
}

static IRBlock* cf_current_continue(IRLowerCtx* ctx) {
    if (!ctx || ctx->cf_depth <= 0) return NULL;
    return ctx->continue_targets[ctx->cf_depth - 1];
}

static IRValue* lower_to_bool(IRLowerCtx* ctx, IRValue* value) {
    if (!ctx || !ctx->builder || !value) return ir_value_const_int(0, IR_TYPE_I1_T);

    // If already i1, keep it.
    if (value->type && value->type->kind == IR_TYPE_I1) {
        return value;
    }

    // Compare value != 0 to produce i1.
    IRType* vt = value->type ? value->type : IR_TYPE_I64_T;
    IRValue* zero = ir_value_const_int(0, vt);

    int reg = ir_builder_emit_cmp_ne(ctx->builder, value, zero);
    return ir_value_reg(reg, IR_TYPE_I1_T);
}

static IROp ir_binop_to_irop(OpType op) {
    switch (op) {
        case OP_ADD: return IR_OP_ADD;
        case OP_SUB: return IR_OP_SUB;
        case OP_MUL: return IR_OP_MUL;
        case OP_DIV: return IR_OP_DIV;
        case OP_MOD: return IR_OP_MOD;
        default:     return IR_OP_NOP;
    }
}

static IRCmpPred ir_cmp_to_pred(OpType op) {
    switch (op) {
        case OP_EQ:  return IR_CMP_EQ;
        case OP_NEQ: return IR_CMP_NE;
        case OP_GT:  return IR_CMP_GT;
        case OP_LT:  return IR_CMP_LT;
        case OP_GTE: return IR_CMP_GE;
        case OP_LTE: return IR_CMP_LE;
        default:     return IR_CMP_EQ;
    }
}

static IRValue* lower_call_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) return ir_builder_const_i64(0);

    // Count args
    int arg_count = 0;
    for (Node* a = expr->data.call.args; a; a = a->next) arg_count++;

    IRValue** args = NULL;
    if (arg_count > 0) {
        args = (IRValue**)malloc(sizeof(IRValue*) * (size_t)arg_count);
        if (!args) {
            ir_lower_report_error(ctx, expr, "فشل تخصيص معاملات النداء.");
            return ir_builder_const_i64(0);
        }
    }

    int i = 0;
    for (Node* a = expr->data.call.args; a; a = a->next) {
        args[i++] = lower_expr(ctx, a);
    }

    // For now (matching current semantic assumptions), calls return i64.
    int dest = ir_builder_emit_call(ctx->builder, expr->data.call.name, IR_TYPE_I64_T, args, arg_count);

    if (args) free(args);

    if (dest < 0) {
        // Void call used as expression: return 0 (placeholder).
        return ir_builder_const_i64(0);
    }

    return ir_value_reg(dest, IR_TYPE_I64_T);
}

// ============================================================================
// Expression lowering (v0.3.0.3)
// ============================================================================

IRValue* lower_expr(IRLowerCtx* ctx, Node* expr) {
    if (!ctx || !ctx->builder || !expr) {
        return ir_builder_const_i64(0);
    }

    // تعيين موقع المصدر لهذه العقدة قبل توليد أي تعليمات.
    ir_lower_set_loc(ctx->builder, expr);

    switch (expr->type) {
        // -----------------------------------------------------------------
        // v0.3.0.3 required nodes
        // -----------------------------------------------------------------
        case NODE_INT:
            return ir_builder_const_i64((int64_t)expr->data.integer.value);

        case NODE_VAR_REF: {
            const char* name = expr->data.var_ref.name;

            // 1) Local variable (stack slot)
            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                if (b->value_type && b->value_type->kind == IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "لا يمكن استخدام المصفوفة '%s' بدون فهرس.", name ? name : "???");
                    return ir_builder_const_i64(0);
                }
                IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

                IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
                IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);
                int loaded = ir_builder_emit_load(ctx->builder, value_type, ptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
                return ir_value_reg(loaded, value_type);
            }

            // 2) Global variable (module scope)
            IRGlobal* g = NULL;
            if (ctx->builder && ctx->builder->module && name) {
                g = ir_module_find_global(ctx->builder->module, name);
            }
            if (g) {
                // Globals are addressable; load from @name
                IRValue* gptr = ir_value_global(name, g->type);
                int loaded = ir_builder_emit_load(ctx->builder, g->type, gptr);
                ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
                return ir_value_reg(loaded, g->type);
            }

            ir_lower_report_error(ctx, expr, "متغير غير معروف '%s'.", name ? name : "???");
            return ir_builder_const_i64(0);
        }

        case NODE_ARRAY_ACCESS: {
            const char* name = expr->data.array_op.name;
            IRType* arr_t = NULL;
            IRValue* base_ptr = NULL;

            IRLowerBinding* b = find_local(ctx, name);
            if (b) {
                if (!b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
                    (void)lower_expr(ctx, expr->data.array_op.index);
                    return ir_builder_const_i64(0);
                }
                arr_t = b->value_type;
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(arr_t));

                IRType* elem_t0 = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
                IRType* elem_ptr_t0 = ir_type_ptr(elem_t0);

                // base: ptr(array) -> cast -> ptr(elem)
                int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t0);
                base_ptr = ir_value_reg(base_reg, elem_ptr_t0);
            } else {
                // مصفوفة عامة: نأخذ عنوانها عبر @name ونُعاملها كمؤشر إلى المصفوفة.
                IRGlobal* g = NULL;
                if (ctx->builder && ctx->builder->module && name) {
                    g = ir_module_find_global(ctx->builder->module, name);
                }
                if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
                    ir_lower_report_error(ctx, expr, "مصفوفة غير معرّفة '%s'.", name ? name : "???");
                    (void)lower_expr(ctx, expr->data.array_op.index);
                    return ir_builder_const_i64(0);
                }
                arr_t = g->type;

                // مهم: لا نستخدم cast على global لأن MOV سيقرأ القيمة من الذاكرة.
                // نُعامل @name مباشرةً كمؤشر إلى العنصر الأول (مثل C: array decays to pointer).
                IRType* elem_t0 = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
                base_ptr = ir_value_global(name, elem_t0);
            }

            IRType* elem_t = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
            IRType* elem_ptr_t = ir_type_ptr(elem_t);

            // تأكد من أن base_ptr نوعه ptr(elem)
            if (!base_ptr) {
                ir_lower_report_error(ctx, expr, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
                (void)lower_expr(ctx, expr->data.array_op.index);
                return ir_builder_const_i64(0);
            }

            IRValue* idx = lower_expr(ctx, expr->data.array_op.index);
            if (idx && idx->type && idx->type->kind != IR_TYPE_I64) {
                int idx64 = ir_builder_emit_cast(ctx->builder, idx, IR_TYPE_I64_T);
                idx = ir_value_reg(idx64, IR_TYPE_I64_T);
            }

            int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
            IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

            int loaded = ir_builder_emit_load(ctx->builder, elem_t, elem_ptr);
            ir_lower_tag_last_inst(ctx->builder, IR_OP_LOAD, loaded, name);
            return ir_value_reg(loaded, elem_t);
        }

        case NODE_BIN_OP: {
            OpType op = expr->data.bin_op.op;

            // Arithmetic
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                IRValue* lhs = ensure_i64(ctx, lower_expr(ctx, expr->data.bin_op.left));
                IRValue* rhs = ensure_i64(ctx, lower_expr(ctx, expr->data.bin_op.right));

                IRType* type = IR_TYPE_I64_T;
                int dest = -1;

                switch (ir_binop_to_irop(op)) {
                    case IR_OP_ADD: dest = ir_builder_emit_add(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_SUB: dest = ir_builder_emit_sub(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MUL: dest = ir_builder_emit_mul(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_DIV: dest = ir_builder_emit_div(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MOD: dest = ir_builder_emit_mod(ctx->builder, type, lhs, rhs); break;
                    default:
                        ir_lower_report_error(ctx, expr, "عملية حسابية غير مدعومة.");
                        return ir_builder_const_i64(0);
                }

                return ir_value_reg(dest, type);
            }

            // Comparison
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                IRValue* lhs0 = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs0 = lower_expr(ctx, expr->data.bin_op.right);
                IRValue* lhs = lhs0;
                IRValue* rhs = rhs0;

                // وحّد النوع عند المقارنة لتجنب i32 vs i64.
                if (!lhs || !rhs || !lhs->type || !rhs->type || !ir_types_equal(lhs->type, rhs->type)) {
                    lhs = ensure_i64(ctx, lhs0);
                    rhs = ensure_i64(ctx, rhs0);
                }

                IRCmpPred pred = ir_cmp_to_pred(op);
                int dest = ir_builder_emit_cmp(ctx->builder, pred, lhs, rhs);
                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            // Logical AND/OR (non-short-circuit form for now; proper short-circuit will be handled
            // during control-flow lowering by introducing blocks).
            if (op == OP_AND || op == OP_OR) {
                IRValue* lhs = lower_to_bool(ctx, lower_expr(ctx, expr->data.bin_op.left));
                IRValue* rhs = lower_to_bool(ctx, lower_expr(ctx, expr->data.bin_op.right));

                int dest = -1;
                if (op == OP_AND) dest = ir_builder_emit_and(ctx->builder, IR_TYPE_I1_T, lhs, rhs);
                else dest = ir_builder_emit_or(ctx->builder, IR_TYPE_I1_T, lhs, rhs);

                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            ir_lower_report_error(ctx, expr, "عملية ثنائية غير مدعومة.");
            return ir_builder_const_i64(0);
        }

        case NODE_UNARY_OP: {
            UnaryOpType op = expr->data.unary_op.op;

            if (op == UOP_NEG) {
                IRValue* operand = ensure_i64(ctx, lower_expr(ctx, expr->data.unary_op.operand));
                int dest = ir_builder_emit_neg(ctx->builder, IR_TYPE_I64_T, operand);
                return ir_value_reg(dest, IR_TYPE_I64_T);
            }

            if (op == UOP_NOT) {
                // Logical NOT: ensure operand is i1 then emit نفي.
                IRValue* operand = lower_to_bool(ctx, lower_expr(ctx, expr->data.unary_op.operand));
                int dest = ir_builder_emit_not(ctx->builder, IR_TYPE_I1_T, operand);
                return ir_value_reg(dest, IR_TYPE_I1_T);
            }

            // ++ / -- in expression form will be addressed later (statement lowering / lvalues).
            ir_lower_report_error(ctx, expr, "عملية أحادية غير مدعومة (%d).", (int)op);
            return ir_builder_const_i64(0);
        }

        case NODE_CALL_EXPR:
            return lower_call_expr(ctx, expr);

        // -----------------------------------------------------------------
        // Extra nodes (not in v0.3.0.3 checklist, but useful for robustness)
        // -----------------------------------------------------------------
        case NODE_BOOL:
            return ir_value_const_int(expr->data.bool_lit.value ? 1 : 0, IR_TYPE_I1_T);

        case NODE_CHAR:
            return ir_value_const_int((int64_t)expr->data.char_lit.value, IR_TYPE_I32_T);

        case NODE_STRING:
            return ir_builder_const_string(ctx->builder, expr->data.string_lit.value);

        case NODE_MEMBER_ACCESS: {
            // ملاحظة: التحليل الدلالي يملأ معلومات الوصول (root/offset/type).
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value, IR_TYPE_I64_T);
            }

            if (!expr->data.member_access.is_struct_member || !expr->data.member_access.root_var) {
                ir_lower_report_error(ctx, expr, "وصول عضو غير صالح في مسار IR.");
                return ir_builder_const_i64(0);
            }

            if (expr->data.member_access.member_type == TYPE_STRUCT ||
                expr->data.member_access.member_type == TYPE_UNION) {
                ir_lower_report_error(ctx, expr, "قراءة عضو هيكل من نوع هيكل كقيمة غير مدعومة.");
                return ir_builder_const_i64(0);
            }

            IRModule* m = ctx->builder->module;
            if (m) ir_module_set_current(m);

            IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
            IRValue* base_ptr = NULL;

            if (expr->data.member_access.root_is_global) {
                base_ptr = ir_value_global(expr->data.member_access.root_var, IR_TYPE_I8_T);
            } else {
                IRLowerBinding* b = find_local(ctx, expr->data.member_access.root_var);
                if (!b || !b->value_type) {
                    ir_lower_report_error(ctx, expr, "هيكل محلي غير معروف '%s'.",
                                          expr->data.member_access.root_var);
                    return ir_builder_const_i64(0);
                }
                IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
                int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
                base_ptr = ir_value_reg(c, ptr_i8_t);
            }

            IRValue* idx = ir_value_const_int((int64_t)expr->data.member_access.member_offset, IR_TYPE_I64_T);
            int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
            IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

            IRType* field_val_t = NULL;
            switch (expr->data.member_access.member_type) {
                case TYPE_BOOL: field_val_t = IR_TYPE_I1_T; break;
                case TYPE_CHAR: field_val_t = IR_TYPE_I32_T; break;
                case TYPE_STRING: field_val_t = get_i8_ptr_type(m); break;
                case TYPE_ENUM:
                case TYPE_INT:
                default: field_val_t = IR_TYPE_I64_T; break;
            }

            IRType* field_ptr_t = ir_type_ptr(field_val_t);
            int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, field_ptr_t);
            IRValue* field_ptr = ir_value_reg(fp, field_ptr_t);
            int r = ir_builder_emit_load(ctx->builder, field_val_t, field_ptr);
            return ir_value_reg(r, field_val_t);
        }

        default:
            ir_lower_report_error(ctx, expr, "عقدة تعبير غير مدعومة (%d).", (int)expr->type);
            return ir_builder_const_i64(0);
    }
}

// ============================================================================
// Statement lowering (v0.3.0.4)
// ============================================================================

static void lower_array_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // التصريحات العامة تُخفض على مستوى الوحدة داخل ir_lower_program().
    if (stmt->data.array_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global array decl '%s' ignored in statement lowering\n",
                stmt->data.array_decl.name ? stmt->data.array_decl.name : "???");
        return;
    }
    if (stmt->data.array_decl.size <= 0) {
        ir_lower_report_error(ctx, stmt, "حجم المصفوفة غير صالح.");
        return;
    }

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* elem_t = IR_TYPE_I64_T;
    IRType* arr_t = ir_type_array(elem_t, stmt->data.array_decl.size);

    int ptr_reg = ir_builder_emit_alloca(ctx->builder, arr_t);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.array_decl.name);

    ir_lower_bind_local(ctx, stmt->data.array_decl.name, ptr_reg, arr_t);

    // تهيئة المصفوفة كما في C:
    // - إذا وُجدت قائمة تهيئة (حتى `{}`)، نُهيّئ العناصر المحددة ثم نملأ الباقي بأصفار.
    if (!stmt->data.array_decl.has_init) {
        return;
    }

    IRType* arr_ptr_t = ir_type_ptr(arr_t);
    IRType* elem_ptr_t = ir_type_ptr(elem_t);
    IRValue* arr_ptr = ir_value_reg(ptr_reg, arr_ptr_t);

    // base: cast ptr(array) -> ptr(elem)
    int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, elem_ptr_t);

    int i = 0;
    Node* v = stmt->data.array_decl.init_values;
    while (v && i < stmt->data.array_decl.size) {
        // اربط موقع التوليد بموقع عنصر التهيئة.
        ir_lower_set_loc(ctx->builder, v);

        IRValue* idx = ir_value_const_int((int64_t)i, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

        IRValue* val = lower_expr(ctx, v);
        if (val && val->type && !ir_types_equal(val->type, elem_t)) {
            int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
            val = ir_value_reg(cv, elem_t);
        }

        ir_builder_emit_store(ctx->builder, val, elem_ptr);
        i++;
        v = v->next;
    }

    // تعبئة الباقي بأصفار
    for (; i < stmt->data.array_decl.size; i++) {
        ir_lower_set_loc(ctx->builder, stmt);
        IRValue* idx = ir_value_const_int((int64_t)i, IR_TYPE_I64_T);
        int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
        IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);
        IRValue* z = ir_value_const_int(0, elem_t);
        ir_builder_emit_store(ctx->builder, z, elem_ptr);
    }
}

static void lower_var_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // Ignore global declarations here (globals are lowered at module scope later).
    if (stmt->data.var_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global var decl '%s' ignored in statement lowering\n",
                stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
        return;
    }

    // نوع مركب محلي (هيكل/اتحاد): نخزّنه ككتلة بايتات (array<i8, N>) على المكدس.
    if (stmt->data.var_decl.type == TYPE_STRUCT || stmt->data.var_decl.type == TYPE_UNION) {
        int n = stmt->data.var_decl.struct_size;
        if (n <= 0) {
            ir_lower_report_error(ctx, stmt, "حجم الهيكل غير صالح أثناء خفض IR.");
            return;
        }

        IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
        int ptr_reg = ir_builder_emit_alloca(ctx->builder, bytes_t);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);
        ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, bytes_t);
        return;
    }

    IRType* value_type = ir_type_from_datatype(ctx && ctx->builder ? ctx->builder->module : NULL,
                                               stmt->data.var_decl.type);

    // %ptr = حجز <value_type>
    int ptr_reg = ir_builder_emit_alloca(ctx->builder, value_type);

    // ربط اسم المتغير بتعليمة الحجز للتتبع.
    ir_lower_tag_last_inst(ctx->builder, IR_OP_ALLOCA, ptr_reg, stmt->data.var_decl.name);

    // Bind name -> ptr reg so NODE_VAR_REF can emit حمل.
    ir_lower_bind_local(ctx, stmt->data.var_decl.name, ptr_reg, value_type);

    // Initializer is required for locals in the current language rules.
    IRValue* init = NULL;
    if (stmt->data.var_decl.expression) {
        init = lower_expr(ctx, stmt->data.var_decl.expression);
    } else {
        // Fallback (should not happen for locals; parser enforces initializer).
        init = ir_value_const_int(0, value_type);
    }

    if (init && init->type && value_type && !ir_types_equal(init->type, value_type)) {
        init = cast_to(ctx, init, value_type);
    }

    // خزن <init>, %ptr
    IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
    IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);

    // نعيد الموقع إلى تصريح المتغير كي تُنسب عملية الخزن لسطر التصريح.
    ir_lower_set_loc(ctx->builder, stmt);
    ir_builder_emit_store(ctx->builder, init, ptr);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, stmt->data.var_decl.name);
}

static void lower_array_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.array_op.name;
    IRType* arr_t = NULL;
    IRValue* arr_ptr = NULL;
    bool is_global_arr = false;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        if (!b->value_type || b->value_type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, stmt, "'%s' ليس مصفوفة في مسار IR.", name ? name : "???");
            (void)lower_expr(ctx, stmt->data.array_op.index);
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }
        arr_t = b->value_type;
        arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(arr_t));
    } else {
        // مصفوفة عامة
        IRGlobal* g = NULL;
        if (ctx->builder && ctx->builder->module && name) {
            g = ir_module_find_global(ctx->builder->module, name);
        }
        if (!g || !g->type || g->type->kind != IR_TYPE_ARRAY) {
            ir_lower_report_error(ctx, stmt, "تعيين إلى مصفوفة غير معرّفة '%s'.", name ? name : "???");
            (void)lower_expr(ctx, stmt->data.array_op.index);
            (void)lower_expr(ctx, stmt->data.array_op.value);
            return;
        }
        arr_t = g->type;
        // مهم: لا نستخدم cast على global لأن MOV سيقرأ القيمة من الذاكرة.
        // سنستخدم @name مباشرةً كقاعدة ptr(elem) لاحقاً.
        is_global_arr = true;
    }

    IRType* elem_t = arr_t->data.array.element ? arr_t->data.array.element : IR_TYPE_I64_T;
    IRType* elem_ptr_t = ir_type_ptr(elem_t);

    IRValue* base_ptr = NULL;
    if (is_global_arr) {
        base_ptr = ir_value_global(name, elem_t);
    } else {
        int base_reg = ir_builder_emit_cast(ctx->builder, arr_ptr, elem_ptr_t);
        base_ptr = ir_value_reg(base_reg, elem_ptr_t);
    }

    if (!base_ptr) {
        ir_lower_report_error(ctx, stmt, "فشل بناء عنوان عنصر المصفوفة '%s'.", name ? name : "???");
        (void)lower_expr(ctx, stmt->data.array_op.index);
        (void)lower_expr(ctx, stmt->data.array_op.value);
        return;
    }

    IRValue* idx = lower_expr(ctx, stmt->data.array_op.index);
    if (idx && idx->type && idx->type->kind != IR_TYPE_I64) {
        int idx64 = ir_builder_emit_cast(ctx->builder, idx, IR_TYPE_I64_T);
        idx = ir_value_reg(idx64, IR_TYPE_I64_T);
    }

    int ep = ir_builder_emit_ptr_offset(ctx->builder, elem_ptr_t, base_ptr, idx);
    IRValue* elem_ptr = ir_value_reg(ep, elem_ptr_t);

    IRValue* val = lower_expr(ctx, stmt->data.array_op.value);
    if (val && val->type && !ir_types_equal(val->type, elem_t)) {
        int cv = ir_builder_emit_cast(ctx->builder, val, elem_t);
        val = ir_value_reg(cv, elem_t);
    }

    ir_builder_emit_store(ctx->builder, val, elem_ptr);
}

static void lower_member_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    Node* target = stmt->data.member_assign.target;
    Node* value = stmt->data.member_assign.value;
    if (!target || target->type != NODE_MEMBER_ACCESS) {
        ir_lower_report_error(ctx, stmt, "هدف إسناد العضو غير صالح.");
        (void)lower_expr(ctx, value);
        return;
    }

    if (!target->data.member_access.is_struct_member || !target->data.member_access.root_var) {
        ir_lower_report_error(ctx, stmt, "إسناد عضو يتطلب عضواً هيكلياً محلولاً دلالياً.");
        (void)lower_expr(ctx, value);
        return;
    }

    if (target->data.member_access.member_type == TYPE_STRUCT ||
        target->data.member_access.member_type == TYPE_UNION) {
        ir_lower_report_error(ctx, stmt, "إسناد عضو من نوع هيكل غير مدعوم حالياً.");
        (void)lower_expr(ctx, value);
        return;
    }

    IRModule* m = ctx->builder->module;
    if (m) ir_module_set_current(m);

    IRType* ptr_i8_t = ir_type_ptr(IR_TYPE_I8_T);
    IRValue* base_ptr = NULL;

    if (target->data.member_access.root_is_global) {
        base_ptr = ir_value_global(target->data.member_access.root_var, IR_TYPE_I8_T);
    } else {
        IRLowerBinding* b = find_local(ctx, target->data.member_access.root_var);
        if (!b || !b->value_type) {
            ir_lower_report_error(ctx, stmt, "هيكل محلي غير معروف '%s'.", target->data.member_access.root_var);
            (void)lower_expr(ctx, value);
            return;
        }
        IRValue* arr_ptr = ir_value_reg(b->ptr_reg, ir_type_ptr(b->value_type));
        int c = ir_builder_emit_cast(ctx->builder, arr_ptr, ptr_i8_t);
        base_ptr = ir_value_reg(c, ptr_i8_t);
    }

    IRValue* idx = ir_value_const_int((int64_t)target->data.member_access.member_offset, IR_TYPE_I64_T);
    int ep = ir_builder_emit_ptr_offset(ctx->builder, ptr_i8_t, base_ptr, idx);
    IRValue* byte_ptr = ir_value_reg(ep, ptr_i8_t);

    IRType* field_val_t = NULL;
    switch (target->data.member_access.member_type) {
        case TYPE_BOOL: field_val_t = IR_TYPE_I1_T; break;
        case TYPE_CHAR: field_val_t = IR_TYPE_I32_T; break;
        case TYPE_STRING: field_val_t = get_i8_ptr_type(m); break;
        case TYPE_ENUM:
        case TYPE_INT:
        default: field_val_t = IR_TYPE_I64_T; break;
    }
    IRType* field_ptr_t = ir_type_ptr(field_val_t);
    int fp = ir_builder_emit_cast(ctx->builder, byte_ptr, field_ptr_t);
    IRValue* field_ptr = ir_value_reg(fp, field_ptr_t);

    IRValue* rhs = lower_expr(ctx, value);
    if (rhs && rhs->type && !ir_types_equal(rhs->type, field_val_t)) {
        int cv = ir_builder_emit_cast(ctx->builder, rhs, field_val_t);
        rhs = ir_value_reg(cv, field_val_t);
    }

    ir_builder_emit_store(ctx->builder, rhs, field_ptr);
}

static void lower_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.assign_stmt.name;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && value_type && !ir_types_equal(rhs->type, value_type)) {
            rhs = cast_to(ctx, rhs, value_type);
        }

        // خزن <rhs>, %ptr
        (void)value_type; // reserved for future casts/type checks
        IRType* ptr_type = ir_type_ptr(value_type ? value_type : IR_TYPE_I64_T);
        IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, ptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, name);
        return;
    }

    // Assignment to a global (module scope)
    IRGlobal* g = NULL;
    if (ctx->builder && ctx->builder->module && name) {
        g = ir_module_find_global(ctx->builder->module, name);
    }
    if (g) {
        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        if (rhs && rhs->type && g->type && !ir_types_equal(rhs->type, g->type)) {
            rhs = cast_to(ctx, rhs, g->type);
        }

        IRValue* gptr = ir_value_global(name, g->type);
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, gptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, name);
        return;
    }

    ir_lower_report_error(ctx, stmt, "تعيين إلى متغير غير معروف '%s'.", name ? name : "???");
}

static void lower_return(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = NULL;
    if (stmt->data.return_stmt.expression) {
        value = lower_expr(ctx, stmt->data.return_stmt.expression);
    }
    ir_builder_emit_ret(ctx->builder, value);
}

static void lower_print_char_utf8(IRLowerCtx* ctx, Node* stmt, IRValue* cp32)
{
    if (!ctx || !ctx->builder || !cp32) return;

    IRBuilder* b = ctx->builder;
    IRModule* m = b->module;
    if (m) ir_module_set_current(m);

    IRType* buf_arr_t = ir_type_array(IR_TYPE_I8_T, 5);
    int buf_ptr_reg = ir_builder_emit_alloca(b, buf_arr_t);
    IRValue* buf_ptr = ir_value_reg(buf_ptr_reg, ir_type_ptr(buf_arr_t));

    IRType* i8_ptr_t = ir_type_ptr(IR_TYPE_I8_T);
    int base_reg = ir_builder_emit_cast(b, buf_ptr, i8_ptr_t);
    IRValue* base_ptr = ir_value_reg(base_reg, i8_ptr_t);

    IRValue* cp = ensure_i64(ctx, cp32);

    IRBlock* bad_bb = cf_create_block(ctx, "اطبع_حرف_غير_صالح");
    IRBlock* check1_bb = cf_create_block(ctx, "اطبع_حرف_فحص1");
    IRBlock* one_bb = cf_create_block(ctx, "اطبع_حرف_1");
    IRBlock* check2_bb = cf_create_block(ctx, "اطبع_حرف_فحص2");
    IRBlock* two_bb = cf_create_block(ctx, "اطبع_حرف_2");
    IRBlock* check3_bb = cf_create_block(ctx, "اطبع_حرف_فحص3");
    IRBlock* three_bb = cf_create_block(ctx, "اطبع_حرف_3");
    IRBlock* four_bb = cf_create_block(ctx, "اطبع_حرف_4");
    IRBlock* do_bb = cf_create_block(ctx, "اطبع_حرف_اطبع");
    IRBlock* cont_bb = cf_create_block(ctx, "اطبع_حرف_تابع");

    // bad = (cp > 0x10FFFF) || (0xD800 <= cp <= 0xDFFF)
    IRValue* max_cp = ir_value_const_int(0x10FFFF, IR_TYPE_I64_T);
    IRValue* sur_lo = ir_value_const_int(0xD800, IR_TYPE_I64_T);
    IRValue* sur_hi = ir_value_const_int(0xDFFF, IR_TYPE_I64_T);

    int gt_max_r = ir_builder_emit_cmp_gt(b, cp, max_cp);
    IRValue* gt_max = ir_value_reg(gt_max_r, IR_TYPE_I1_T);

    int ge_sur_r = ir_builder_emit_cmp_ge(b, cp, sur_lo);
    IRValue* ge_sur = ir_value_reg(ge_sur_r, IR_TYPE_I1_T);
    int le_sur_r = ir_builder_emit_cmp_le(b, cp, sur_hi);
    IRValue* le_sur = ir_value_reg(le_sur_r, IR_TYPE_I1_T);
    int is_sur_r = ir_builder_emit_and(b, IR_TYPE_I1_T, ge_sur, le_sur);
    IRValue* is_sur = ir_value_reg(is_sur_r, IR_TYPE_I1_T);

    int bad_r = ir_builder_emit_or(b, IR_TYPE_I1_T, gt_max, is_sur);
    IRValue* bad = ir_value_reg(bad_r, IR_TYPE_I1_T);

    if (!ir_builder_is_block_terminated(b)) {
        ir_builder_emit_br_cond(b, bad, bad_bb, check1_bb);
    }

    // bad → U+FFFD (EF BF BD)
    ir_builder_set_insert_point(b, bad_bb);
    if (stmt) ir_lower_set_loc(b, stmt);
    {
        IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
        IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
        IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
        IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);
        IRValue* z = ir_value_const_int(0, IR_TYPE_I8_T);

        int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
        int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
        int p2r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2);
        int p3r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3);

        ir_builder_emit_store(b, ir_value_const_int(0xEF, IR_TYPE_I8_T), ir_value_reg(p0r, i8_ptr_t));
        ir_builder_emit_store(b, ir_value_const_int(0xBF, IR_TYPE_I8_T), ir_value_reg(p1r, i8_ptr_t));
        ir_builder_emit_store(b, ir_value_const_int(0xBD, IR_TYPE_I8_T), ir_value_reg(p2r, i8_ptr_t));
        ir_builder_emit_store(b, z, ir_value_reg(p3r, i8_ptr_t));
    }
    if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br(b, do_bb);

    // check1
    ir_builder_set_insert_point(b, check1_bb);
    {
        int le1_r = ir_builder_emit_cmp_le(b, cp, ir_value_const_int(0x7F, IR_TYPE_I64_T));
        IRValue* le1 = ir_value_reg(le1_r, IR_TYPE_I1_T);
        if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br_cond(b, le1, one_bb, check2_bb);
    }

    // one byte
    ir_builder_set_insert_point(b, one_bb);
    {
        IRValue* b0 = cast_to(ctx, cp, IR_TYPE_I8_T);
        IRValue* z = ir_value_const_int(0, IR_TYPE_I8_T);
        IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
        IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
        int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
        int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
        ir_builder_emit_store(b, b0, ir_value_reg(p0r, i8_ptr_t));
        ir_builder_emit_store(b, z, ir_value_reg(p1r, i8_ptr_t));
    }
    if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br(b, do_bb);

    // check2
    ir_builder_set_insert_point(b, check2_bb);
    {
        int le2_r = ir_builder_emit_cmp_le(b, cp, ir_value_const_int(0x7FF, IR_TYPE_I64_T));
        IRValue* le2 = ir_value_reg(le2_r, IR_TYPE_I1_T);
        if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br_cond(b, le2, two_bb, check3_bb);
    }

    // two bytes
    ir_builder_set_insert_point(b, two_bb);
    {
        IRValue* sixty4 = ir_value_const_int(64, IR_TYPE_I64_T);
        int q_r = ir_builder_emit_div(b, IR_TYPE_I64_T, cp, sixty4);
        int r_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, cp, sixty4);
        IRValue* q = ir_value_reg(q_r, IR_TYPE_I64_T);
        IRValue* r = ir_value_reg(r_r, IR_TYPE_I64_T);

        int b0r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0xC0, IR_TYPE_I64_T), q);
        int b1r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r);
        IRValue* b0 = cast_to(ctx, ir_value_reg(b0r, IR_TYPE_I64_T), IR_TYPE_I8_T);
        IRValue* b1 = cast_to(ctx, ir_value_reg(b1r, IR_TYPE_I64_T), IR_TYPE_I8_T);

        IRValue* z = ir_value_const_int(0, IR_TYPE_I8_T);
        IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
        IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
        IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
        int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
        int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
        int p2r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2);
        ir_builder_emit_store(b, b0, ir_value_reg(p0r, i8_ptr_t));
        ir_builder_emit_store(b, b1, ir_value_reg(p1r, i8_ptr_t));
        ir_builder_emit_store(b, z, ir_value_reg(p2r, i8_ptr_t));
    }
    if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br(b, do_bb);

    // check3
    ir_builder_set_insert_point(b, check3_bb);
    {
        int le3_r = ir_builder_emit_cmp_le(b, cp, ir_value_const_int(0xFFFF, IR_TYPE_I64_T));
        IRValue* le3 = ir_value_reg(le3_r, IR_TYPE_I1_T);
        if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br_cond(b, le3, three_bb, four_bb);
    }

    // three bytes
    ir_builder_set_insert_point(b, three_bb);
    {
        IRValue* d4096 = ir_value_const_int(4096, IR_TYPE_I64_T);
        IRValue* d64 = ir_value_const_int(64, IR_TYPE_I64_T);

        int q1_r = ir_builder_emit_div(b, IR_TYPE_I64_T, cp, d4096);
        IRValue* q1 = ir_value_reg(q1_r, IR_TYPE_I64_T);

        int q2_r = ir_builder_emit_div(b, IR_TYPE_I64_T, cp, d64);
        IRValue* q2 = ir_value_reg(q2_r, IR_TYPE_I64_T);
        int q2m_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, q2, ir_value_const_int(64, IR_TYPE_I64_T));
        IRValue* q2m = ir_value_reg(q2m_r, IR_TYPE_I64_T);

        int r_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, cp, d64);
        IRValue* r = ir_value_reg(r_r, IR_TYPE_I64_T);

        int b0r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0xE0, IR_TYPE_I64_T), q1);
        int b1r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q2m);
        int b2r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r);

        IRValue* b0 = cast_to(ctx, ir_value_reg(b0r, IR_TYPE_I64_T), IR_TYPE_I8_T);
        IRValue* b1 = cast_to(ctx, ir_value_reg(b1r, IR_TYPE_I64_T), IR_TYPE_I8_T);
        IRValue* b2 = cast_to(ctx, ir_value_reg(b2r, IR_TYPE_I64_T), IR_TYPE_I8_T);

        IRValue* z = ir_value_const_int(0, IR_TYPE_I8_T);
        IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
        IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
        IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
        IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);
        int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
        int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
        int p2r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2);
        int p3r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3);
        ir_builder_emit_store(b, b0, ir_value_reg(p0r, i8_ptr_t));
        ir_builder_emit_store(b, b1, ir_value_reg(p1r, i8_ptr_t));
        ir_builder_emit_store(b, b2, ir_value_reg(p2r, i8_ptr_t));
        ir_builder_emit_store(b, z, ir_value_reg(p3r, i8_ptr_t));
    }
    if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br(b, do_bb);

    // four bytes
    ir_builder_set_insert_point(b, four_bb);
    {
        IRValue* d262144 = ir_value_const_int(262144, IR_TYPE_I64_T);
        IRValue* d4096 = ir_value_const_int(4096, IR_TYPE_I64_T);
        IRValue* d64 = ir_value_const_int(64, IR_TYPE_I64_T);

        int q1_r = ir_builder_emit_div(b, IR_TYPE_I64_T, cp, d262144);
        IRValue* q1 = ir_value_reg(q1_r, IR_TYPE_I64_T);

        int q2_r = ir_builder_emit_div(b, IR_TYPE_I64_T, cp, d4096);
        IRValue* q2 = ir_value_reg(q2_r, IR_TYPE_I64_T);
        int q2m_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, q2, ir_value_const_int(64, IR_TYPE_I64_T));
        IRValue* q2m = ir_value_reg(q2m_r, IR_TYPE_I64_T);

        int q3_r = ir_builder_emit_div(b, IR_TYPE_I64_T, cp, d64);
        IRValue* q3 = ir_value_reg(q3_r, IR_TYPE_I64_T);
        int q3m_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, q3, ir_value_const_int(64, IR_TYPE_I64_T));
        IRValue* q3m = ir_value_reg(q3m_r, IR_TYPE_I64_T);

        int r_r = ir_builder_emit_mod(b, IR_TYPE_I64_T, cp, d64);
        IRValue* r = ir_value_reg(r_r, IR_TYPE_I64_T);

        int b0r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0xF0, IR_TYPE_I64_T), q1);
        int b1r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q2m);
        int b2r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), q3m);
        int b3r = ir_builder_emit_add(b, IR_TYPE_I64_T, ir_value_const_int(0x80, IR_TYPE_I64_T), r);

        IRValue* b0 = cast_to(ctx, ir_value_reg(b0r, IR_TYPE_I64_T), IR_TYPE_I8_T);
        IRValue* b1 = cast_to(ctx, ir_value_reg(b1r, IR_TYPE_I64_T), IR_TYPE_I8_T);
        IRValue* b2 = cast_to(ctx, ir_value_reg(b2r, IR_TYPE_I64_T), IR_TYPE_I8_T);
        IRValue* b3 = cast_to(ctx, ir_value_reg(b3r, IR_TYPE_I64_T), IR_TYPE_I8_T);

        IRValue* z = ir_value_const_int(0, IR_TYPE_I8_T);
        IRValue* idx0 = ir_value_const_int(0, IR_TYPE_I64_T);
        IRValue* idx1 = ir_value_const_int(1, IR_TYPE_I64_T);
        IRValue* idx2 = ir_value_const_int(2, IR_TYPE_I64_T);
        IRValue* idx3 = ir_value_const_int(3, IR_TYPE_I64_T);
        IRValue* idx4 = ir_value_const_int(4, IR_TYPE_I64_T);
        int p0r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx0);
        int p1r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx1);
        int p2r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx2);
        int p3r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx3);
        int p4r = ir_builder_emit_ptr_offset(b, i8_ptr_t, base_ptr, idx4);
        ir_builder_emit_store(b, b0, ir_value_reg(p0r, i8_ptr_t));
        ir_builder_emit_store(b, b1, ir_value_reg(p1r, i8_ptr_t));
        ir_builder_emit_store(b, b2, ir_value_reg(p2r, i8_ptr_t));
        ir_builder_emit_store(b, b3, ir_value_reg(p3r, i8_ptr_t));
        ir_builder_emit_store(b, z, ir_value_reg(p4r, i8_ptr_t));
    }
    if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br(b, do_bb);

    // do print
    ir_builder_set_insert_point(b, do_bb);
    if (stmt) ir_lower_set_loc(b, stmt);
    {
        IRValue* fmt_val = ir_builder_const_string(b, "%s\n");
        IRValue* args[2] = { fmt_val, base_ptr };
        ir_builder_emit_call_void(b, "اطبع", args, 2);
    }
    if (!ir_builder_is_block_terminated(b)) ir_builder_emit_br(b, cont_bb);

    // continue
    ir_builder_set_insert_point(b, cont_bb);
}

static void lower_print(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = lower_expr(ctx, stmt->data.print_stmt.expression);

    // طباعة حرف UTF-8: نُحوّل نقطة-الكود إلى بايتات UTF-8 ثم نطبعها كنص.
    if (value && value->type && value->type->kind == IR_TYPE_I32) {
        lower_print_char_utf8(ctx, stmt, value);
        return;
    }

    // اختيار صيغة الطباعة بناءً على نوع القيمة
    // ملاحظة: حالياً نستخدم %d للأعداد والمنطقي (توافقاً مع المسار القديم)، و %s للنصوص.
    const char* fmt = "%d\n";
    if (value && value->type && value->type->kind == IR_TYPE_PTR &&
        value->type->data.pointee == IR_TYPE_I8_T) {
        fmt = "%s\n";
    }

    // printf varargs: للأمان، وسّع الأعداد الأصغر إلى i64.
    if (value && value->type && value->type->kind != IR_TYPE_PTR && value->type->kind != IR_TYPE_I64) {
        value = ensure_i64(ctx, value);
    }

    IRValue* fmt_val = ir_builder_const_string(ctx->builder, fmt);

    IRValue* args[2] = { fmt_val, value };

    // Builtin: اطبع(value) → printf(fmt, value)
    ir_builder_emit_call_void(ctx->builder, "اطبع", args, 2);
}

static void lower_read(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRLowerBinding* b = find_local(ctx, stmt->data.read_stmt.var_name);
    if (!b) {
        ir_lower_report_error(ctx, stmt, "قراءة إلى متغير غير معروف '%s'.",
                              stmt->data.read_stmt.var_name ? stmt->data.read_stmt.var_name : "???");
        return;
    }

    // scanf("%d", &var)
    IRValue* fmt_val = ir_builder_const_string(ctx->builder, "%d");
    IRType* ptr_type = ir_type_ptr((b->value_type) ? b->value_type : IR_TYPE_I64_T);
    IRValue* ptr = ir_value_reg(b->ptr_reg, ptr_type);

    IRValue* args[2] = { fmt_val, ptr };

    // Builtin: اقرأ(ptr) → scanf(fmt, ptr)
    ir_builder_emit_call_void(ctx->builder, "اقرأ", args, 2);
}

static IRValue* lower_condition_i1(IRLowerCtx* ctx, Node* cond_expr) {
    // Ensure the condition is i1 for br_cond.
    return lower_to_bool(ctx, lower_expr(ctx, cond_expr));
}

static void lower_if_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRBlock* then_bb = cf_create_block(ctx, "فرع_صواب");
    IRBlock* merge_bb = cf_create_block(ctx, "دمج");
    IRBlock* else_bb = NULL;

    if (stmt->data.if_stmt.else_branch) {
        else_bb = cf_create_block(ctx, "فرع_خطأ");
    }

    IRValue* cond = lower_condition_i1(ctx, stmt->data.if_stmt.condition);

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        if (else_bb) {
            ir_builder_emit_br_cond(ctx->builder, cond, then_bb, else_bb);
        } else {
            ir_builder_emit_br_cond(ctx->builder, cond, then_bb, merge_bb);
        }
    }

    // then
    ir_builder_set_insert_point(ctx->builder, then_bb);
    ir_lower_scope_push(ctx);
    lower_stmt(ctx, stmt->data.if_stmt.then_branch);
    ir_lower_scope_pop(ctx);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, merge_bb);
    }

    // else
    if (else_bb) {
        ir_builder_set_insert_point(ctx->builder, else_bb);
        ir_lower_scope_push(ctx);
        lower_stmt(ctx, stmt->data.if_stmt.else_branch);
        ir_lower_scope_pop(ctx);
        if (!ir_builder_is_block_terminated(ctx->builder)) {
            ir_builder_emit_br(ctx->builder, merge_bb);
        }
    }

    // merge
    ir_builder_set_insert_point(ctx->builder, merge_bb);
}

static void lower_while_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRBlock* header_bb = cf_create_block(ctx, "حلقة_تحقق");
    IRBlock* body_bb = cf_create_block(ctx, "حلقة_جسم");
    IRBlock* exit_bb = cf_create_block(ctx, "حلقة_نهاية");

    // Jump to header from current block
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    // header: evaluate condition
    ir_builder_set_insert_point(ctx->builder, header_bb);
    IRValue* cond = lower_condition_i1(ctx, stmt->data.while_stmt.condition);
    ir_builder_emit_br_cond(ctx->builder, cond, body_bb, exit_bb);

    // body
    cf_push(ctx, exit_bb, header_bb); // break -> exit, continue -> header
    ir_builder_set_insert_point(ctx->builder, body_bb);
    ir_lower_scope_push(ctx);
    lower_stmt(ctx, stmt->data.while_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }
    ir_lower_scope_pop(ctx);
    cf_pop(ctx);

    // continue after loop
    ir_builder_set_insert_point(ctx->builder, exit_bb);
}

static void lower_for_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // نطاق حلقة for: يمنع تسرب متغيرات init خارج الحلقة.
    ir_lower_scope_push(ctx);

    // Init runs in the current block
    if (stmt->data.for_stmt.init) {
        Node* init = stmt->data.for_stmt.init;
        if (init->type == NODE_VAR_DECL || init->type == NODE_ASSIGN || init->type == NODE_BLOCK) {
            lower_stmt(ctx, init);
        } else {
            (void)lower_expr(ctx, init);
        }
    }

    IRBlock* header_bb = cf_create_block(ctx, "لكل_تحقق");
    IRBlock* body_bb = cf_create_block(ctx, "لكل_جسم");
    IRBlock* inc_bb = cf_create_block(ctx, "لكل_زيادة");
    IRBlock* exit_bb = cf_create_block(ctx, "لكل_نهاية");

    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    // header
    ir_builder_set_insert_point(ctx->builder, header_bb);
    if (stmt->data.for_stmt.condition) {
        IRValue* cond = lower_condition_i1(ctx, stmt->data.for_stmt.condition);
        ir_builder_emit_br_cond(ctx->builder, cond, body_bb, exit_bb);
    } else {
        ir_builder_emit_br(ctx->builder, body_bb);
    }

    // body
    cf_push(ctx, exit_bb, inc_bb); // break -> exit, continue -> increment
    ir_builder_set_insert_point(ctx->builder, body_bb);
    lower_stmt(ctx, stmt->data.for_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, inc_bb);
    }
    cf_pop(ctx);

    // increment
    ir_builder_set_insert_point(ctx->builder, inc_bb);
    if (stmt->data.for_stmt.increment) {
        Node* inc = stmt->data.for_stmt.increment;
        if (inc->type == NODE_ASSIGN || inc->type == NODE_VAR_DECL || inc->type == NODE_BLOCK) {
            lower_stmt(ctx, inc);
        } else {
            (void)lower_expr(ctx, inc);
        }
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }

    ir_builder_set_insert_point(ctx->builder, exit_bb);

    ir_lower_scope_pop(ctx);
}

static void lower_break_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_break(ctx);
    if (!target) {
        ir_lower_report_error(ctx, NULL, "استعمال 'break' خارج حلقة/اختر.");
        return;
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, target);
    }
}

static void lower_continue_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_continue(ctx);
    if (!target) {
        ir_lower_report_error(ctx, NULL, "استعمال 'continue' خارج حلقة.");
        return;
    }
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, target);
    }
}

static void lower_switch_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // Gather cases
    int case_count = 0;
    int non_default_count = 0;
    Node* c = stmt->data.switch_stmt.cases;
    while (c) {
        case_count++;
        if (!c->data.case_stmt.is_default) non_default_count++;
        c = c->next;
    }

    IRBlock* end_bb = cf_create_block(ctx, "نهاية_اختر");

    // Switch doesn't define a continue target (inherits from outer loop, if any)
    cf_push(ctx, end_bb, cf_current_continue(ctx));

    IRBlock** case_blocks = NULL;
    Node** case_nodes = NULL;
    IRBlock** non_default_blocks = NULL;
    Node** non_default_nodes = NULL;
    IRBlock* default_bb = NULL;
    Node* default_node = NULL;

    if (case_count > 0) {
        case_blocks = (IRBlock**)malloc(sizeof(IRBlock*) * (size_t)case_count);
        case_nodes = (Node**)malloc(sizeof(Node*) * (size_t)case_count);
    }
    if (non_default_count > 0) {
        non_default_blocks = (IRBlock**)malloc(sizeof(IRBlock*) * (size_t)non_default_count);
        non_default_nodes = (Node**)malloc(sizeof(Node*) * (size_t)non_default_count);
    }

    // Create blocks for all cases in source order (for fallthrough)
    int idx = 0;
    int nd_idx = 0;
    c = stmt->data.switch_stmt.cases;
    while (c) {
        IRBlock* bb = NULL;
        if (c->data.case_stmt.is_default) {
            bb = cf_create_block(ctx, "افتراضي");
            default_bb = bb;
            default_node = c;
        } else {
            bb = cf_create_block(ctx, "حالة");
            if (non_default_blocks && non_default_nodes) {
                non_default_blocks[nd_idx] = bb;
                non_default_nodes[nd_idx] = c;
                nd_idx++;
            }
        }

        if (case_blocks && case_nodes) {
            case_blocks[idx] = bb;
            case_nodes[idx] = c;
            idx++;
        }

        c = c->next;
    }

    // Evaluate switch expression (should be integer per semantic rules)
    IRValue* sw_val = lower_expr(ctx, stmt->data.switch_stmt.expression);
    IRType* sw_type = sw_val && sw_val->type ? sw_val->type : IR_TYPE_I64_T;

    int sw_is_reg = sw_val && sw_val->kind == IR_VAL_REG;
    int sw_reg = sw_is_reg ? sw_val->data.reg_num : -1;
    int64_t sw_const = (!sw_is_reg && sw_val && sw_val->kind == IR_VAL_CONST_INT) ? sw_val->data.const_int : 0;

    // Dispatch comparisons (chain)
    if (non_default_count == 0) {
        // No explicit cases: jump to default or end.
        if (!ir_builder_is_block_terminated(ctx->builder)) {
            ir_builder_emit_br(ctx->builder, default_bb ? default_bb : end_bb);
        }
    } else {
        IRBlock* current_check = ir_builder_get_insert_block(ctx->builder);

        for (int i = 0; i < non_default_count; i++) {
            Node* case_node = non_default_nodes[i];
            IRBlock* true_bb = non_default_blocks[i];
            IRBlock* false_bb = NULL;

            if (i == non_default_count - 1) {
                false_bb = default_bb ? default_bb : end_bb;
            } else {
                false_bb = cf_create_block(ctx, "فحص");
            }

            // Compare sw == case_value
            IRValue* lhs = sw_is_reg ? ir_value_reg(sw_reg, sw_type) : ir_value_const_int(sw_const, sw_type);
            IRValue* rhs = lower_expr(ctx, case_node->data.case_stmt.value);
            int cmp_reg = ir_builder_emit_cmp_eq(ctx->builder, lhs, rhs);
            IRValue* cmp_val = ir_value_reg(cmp_reg, IR_TYPE_I1_T);

            ir_builder_emit_br_cond(ctx->builder, cmp_val, true_bb, false_bb);

            // Continue chain in the false block (only if it's a check block we created)
            if (i < non_default_count - 1) {
                ir_builder_set_insert_point(ctx->builder, false_bb);
                current_check = false_bb;
            } else {
                (void)current_check;
            }
        }
    }

    // Emit case bodies with fallthrough semantics (explicit br to next case block)
    for (int i = 0; i < case_count; i++) {
        IRBlock* bb = case_blocks[i];
        Node* case_node = case_nodes[i];

        ir_builder_set_insert_point(ctx->builder, bb);
        ir_lower_scope_push(ctx);
        lower_stmt_list(ctx, case_node->data.case_stmt.body);
        ir_lower_scope_pop(ctx);

        if (!ir_builder_is_block_terminated(ctx->builder)) {
            IRBlock* next_bb = (i + 1 < case_count) ? case_blocks[i + 1] : end_bb;
            ir_builder_emit_br(ctx->builder, next_bb);
        }
    }

    // Continue after switch
    ir_builder_set_insert_point(ctx->builder, end_bb);

    if (case_blocks) free(case_blocks);
    if (case_nodes) free(case_nodes);
    if (non_default_blocks) free(non_default_blocks);
    if (non_default_nodes) free(non_default_nodes);
    (void)default_node;

    cf_pop(ctx); // pop switch break target
}

void lower_stmt_list(IRLowerCtx* ctx, Node* first_stmt) {
    Node* s = first_stmt;
    while (s) {
        lower_stmt(ctx, s);
        s = s->next;
    }
}

void lower_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    // افتراضي: اربط أي تعليمات صادرة من هذه الجملة بموقعها.
    ir_lower_set_loc(ctx->builder, stmt);

    switch (stmt->type) {
        case NODE_BLOCK:
            ir_lower_scope_push(ctx);
            lower_stmt_list(ctx, stmt->data.block.statements);
            ir_lower_scope_pop(ctx);
            return;

        case NODE_VAR_DECL:
            lower_var_decl(ctx, stmt);
            return;

        case NODE_ARRAY_DECL:
            lower_array_decl(ctx, stmt);
            return;

        case NODE_ASSIGN:
            lower_assign(ctx, stmt);
            return;

        case NODE_MEMBER_ASSIGN:
            lower_member_assign(ctx, stmt);
            return;

        case NODE_ARRAY_ASSIGN:
            lower_array_assign(ctx, stmt);
            return;

        case NODE_RETURN:
            lower_return(ctx, stmt);
            return;

        case NODE_PRINT:
            lower_print(ctx, stmt);
            return;

        case NODE_READ:
            lower_read(ctx, stmt);
            return;

        case NODE_IF:
            lower_if_stmt(ctx, stmt);
            return;

        case NODE_WHILE:
            lower_while_stmt(ctx, stmt);
            return;

        case NODE_FOR:
            lower_for_stmt(ctx, stmt);
            return;

        case NODE_SWITCH:
            lower_switch_stmt(ctx, stmt);
            return;

        case NODE_BREAK:
            lower_break_stmt(ctx);
            return;

        case NODE_CONTINUE:
            lower_continue_stmt(ctx);
            return;

        case NODE_CALL_STMT: {
            // Lower call statement via expression lowering.
            // Construct a temporary NODE_CALL_EXPR-compatible view.
            Node temp;
            memset(&temp, 0, sizeof(temp));
            temp.type = NODE_CALL_EXPR;
            temp.data.call = stmt->data.call;
            temp.filename = stmt->filename;
            temp.line = stmt->line;
            temp.col = stmt->col;
            (void)lower_expr(ctx, &temp);
            return;
        }

        default:
            ir_lower_report_error(ctx, stmt, "عقدة جملة غير مدعومة (%d).", (int)stmt->type);
            return;
    }
}

// ============================================================================
// Program lowering (v0.3.0.7)
// ============================================================================

static uint64_t ir_lower_u64_add_wrap(uint64_t a, uint64_t b) { return a + b; }
static uint64_t ir_lower_u64_sub_wrap(uint64_t a, uint64_t b) { return a - b; }
static uint64_t ir_lower_u64_mul_wrap(uint64_t a, uint64_t b) { return a * b; }

/**
 * @brief تقييم تعبير ثابت للأعداد الصحيحة لاستخدامه في تهيئة العوام.
 *
 * الهدف هنا هو دعم تهيئة العوام بقيم قابلة للإصدار في .data (مثل C):
 * - يسمح بعمليات حسابية بسيطة وتعبيرات مقارنة/منطقية تنتج 0/1.
 * - يطبق التفاف 2's complement لتفادي UB في C عند تجاوز المدى.
 */
static int ir_lower_eval_const_i64(Node* expr, int64_t* out_value) {
    if (!out_value) return 0;
    *out_value = 0;
    if (!expr) return 1;

    switch (expr->type) {
        case NODE_INT:
            *out_value = (int64_t)expr->data.integer.value;
            return 1;
        case NODE_BOOL:
            *out_value = expr->data.bool_lit.value ? 1 : 0;
            return 1;
        case NODE_CHAR:
            *out_value = (int64_t)expr->data.char_lit.value;
            return 1;

        case NODE_UNARY_OP:
        {
            int64_t v = 0;
            if (!ir_lower_eval_const_i64(expr->data.unary_op.operand, &v)) return 0;

            if (expr->data.unary_op.op == UOP_NEG) {
                uint64_t uv = (uint64_t)v;
                *out_value = (int64_t)ir_lower_u64_sub_wrap(0u, uv);
                return 1;
            }
            if (expr->data.unary_op.op == UOP_NOT) {
                *out_value = (v == 0) ? 1 : 0;
                return 1;
            }
            return 0;
        }

        case NODE_BIN_OP:
        {
            int64_t a = 0;
            int64_t b = 0;
            if (!ir_lower_eval_const_i64(expr->data.bin_op.left, &a)) return 0;
            if (!ir_lower_eval_const_i64(expr->data.bin_op.right, &b)) return 0;

            uint64_t ua = (uint64_t)a;
            uint64_t ub = (uint64_t)b;

            switch (expr->data.bin_op.op) {
                case OP_ADD: *out_value = (int64_t)ir_lower_u64_add_wrap(ua, ub); return 1;
                case OP_SUB: *out_value = (int64_t)ir_lower_u64_sub_wrap(ua, ub); return 1;
                case OP_MUL: *out_value = (int64_t)ir_lower_u64_mul_wrap(ua, ub); return 1;

                case OP_DIV:
                    if (b == 0) return 0;
                    if (a == INT64_MIN && b == -1) { *out_value = INT64_MIN; return 1; }
                    *out_value = a / b;
                    return 1;
                case OP_MOD:
                    if (b == 0) return 0;
                    if (a == INT64_MIN && b == -1) { *out_value = 0; return 1; }
                    *out_value = a % b;
                    return 1;

                case OP_EQ:  *out_value = (a == b) ? 1 : 0; return 1;
                case OP_NEQ: *out_value = (a != b) ? 1 : 0; return 1;
                case OP_LT:  *out_value = (a <  b) ? 1 : 0; return 1;
                case OP_GT:  *out_value = (a >  b) ? 1 : 0; return 1;
                case OP_LTE: *out_value = (a <= b) ? 1 : 0; return 1;
                case OP_GTE: *out_value = (a >= b) ? 1 : 0; return 1;

                case OP_AND: *out_value = ((a != 0) && (b != 0)) ? 1 : 0; return 1;
                case OP_OR:  *out_value = ((a != 0) || (b != 0)) ? 1 : 0; return 1;
                default:
                    return 0;
            }
        }

        default:
            return 0;
    }
}

static IRValue* ir_lower_global_init_value(IRBuilder* builder, Node* expr, IRType* expected_type) {
    if (!builder) return NULL;

    if (!expr) {
        return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }

    switch (expr->type) {
        case NODE_INT:
            return ir_value_const_int((int64_t)expr->data.integer.value,
                                      expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_BOOL:
            return ir_value_const_int(expr->data.bool_lit.value ? 1 : 0,
                                      expected_type ? expected_type : IR_TYPE_I1_T);

        case NODE_CHAR:
            return ir_value_const_int((int64_t)expr->data.char_lit.value,
                                      expected_type ? expected_type : IR_TYPE_I32_T);

        case NODE_STRING:
            // Adds to module string table and returns pointer value.
            return ir_builder_const_string(builder, expr->data.string_lit.value);

        case NODE_MEMBER_ACCESS:
            if (expr->data.member_access.is_enum_value) {
                return ir_value_const_int(expr->data.member_access.enum_value,
                                          expected_type ? expected_type : IR_TYPE_I64_T);
            }
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_UNARY_OP:
        case NODE_BIN_OP:
        {
            int64_t v = 0;
            if (ir_lower_eval_const_i64(expr, &v)) {
                return ir_value_const_int(v, expected_type ? expected_type : IR_TYPE_I64_T);
            }
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
        }

        default:
            // Non-constant global initializers are not supported yet; fall back to zero.
            return ir_value_const_int(0, expected_type ? expected_type : IR_TYPE_I64_T);
    }
}

IRModule* ir_lower_program(Node* program, const char* module_name) {
    if (!program || program->type != NODE_PROGRAM) return NULL;

    IRModule* module = ir_module_new(module_name ? module_name : "module");
    if (!module) return NULL;

    IRBuilder* builder = ir_builder_new(module);
    if (!builder) {
        ir_module_free(module);
        return NULL;
    }

    // Walk top-level declarations: globals + functions
    for (Node* decl = program->data.program.declarations; decl; decl = decl->next) {
        // Global arrays
        if (decl->type == NODE_ARRAY_DECL && decl->data.array_decl.is_global) {
            if (!decl->data.array_decl.name) continue;

            IRType* elem_t = IR_TYPE_I64_T;
            IRType* arr_t = ir_type_array(elem_t, decl->data.array_decl.size);

            IRGlobal* g = ir_builder_create_global(builder, decl->data.array_decl.name, arr_t,
                                                   decl->data.array_decl.is_const ? 1 : 0);
            if (!g) continue;

            g->has_init_list = decl->data.array_decl.has_init ? true : false;

            if (decl->data.array_decl.has_init) {
                int n = decl->data.array_decl.init_count;
                if (n < 0) n = 0;
                if (decl->data.array_decl.size > 0 && n > decl->data.array_decl.size) {
                    n = decl->data.array_decl.size;
                }

                // نخزن فقط العناصر المعطاة؛ الباقي يُعتبر صفراً كما في C.
                IRValue** elems = NULL;
                if (n > 0) {
                    elems = (IRValue**)ir_arena_calloc(&module->arena, (size_t)n, sizeof(IRValue*), _Alignof(IRValue*));
                    if (!elems) {
                        ir_builder_free(builder);
                        ir_module_free(module);
                        return NULL;
                    }
                }

                int i = 0;
                for (Node* v = decl->data.array_decl.init_values; v && i < n; v = v->next, i++) {
                    elems[i] = ir_lower_global_init_value(builder, v, elem_t);
                }

                g->init_elems = elems;
                g->init_elem_count = n;
            }

            continue;
        }

        // Globals
        if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
            if (decl->data.var_decl.type == TYPE_STRUCT || decl->data.var_decl.type == TYPE_UNION) {
                int n = decl->data.var_decl.struct_size;
                if (n <= 0) {
                    fprintf(stderr, "IR Lower Error: global struct '%s' has invalid size\n",
                            decl->data.var_decl.name ? decl->data.var_decl.name : "???");
                    continue;
                }
                IRType* bytes_t = ir_type_array(IR_TYPE_I8_T, n);
                (void)ir_builder_create_global(builder, decl->data.var_decl.name, bytes_t,
                                               decl->data.var_decl.is_const ? 1 : 0);
                continue;
            }

            IRType* gtype = ir_type_from_datatype(module, decl->data.var_decl.type);
            IRValue* init = ir_lower_global_init_value(builder, decl->data.var_decl.expression, gtype);

            (void)ir_builder_create_global_init(builder, decl->data.var_decl.name, gtype, init,
                                                decl->data.var_decl.is_const ? 1 : 0);
            continue;
        }

        // Functions
        if (decl->type == NODE_FUNC_DEF) {
            IRType* ret_type = ir_type_from_datatype(module, decl->data.func_def.return_type);
            IRFunc* func = ir_builder_create_func(builder, decl->data.func_def.name, ret_type);
            if (!func) continue;

            func->is_prototype = decl->data.func_def.is_prototype;

            // Prototypes don't have a body or blocks.
            if (func->is_prototype) {
                // Still add parameters for signature printing.
                for (Node* p = decl->data.func_def.params; p; p = p->next) {
                    if (p->type != NODE_VAR_DECL) continue;
                    IRType* ptype = ir_type_from_datatype(module, p->data.var_decl.type);
                    (void)ir_builder_add_param(builder, p->data.var_decl.name, ptype);
                }
                continue;
            }

            // Entry block
            IRBlock* entry = ir_builder_create_block(builder, "بداية");
            ir_builder_set_insert_point(builder, entry);

            IRLowerCtx ctx;
            ir_lower_ctx_init(&ctx, builder);

            // نطاق الدالة: المعاملات + جسم الدالة
            ir_lower_scope_push(&ctx);

            // Parameters: add params, then spill into allocas so existing lowering can treat them like locals.
            for (Node* p = decl->data.func_def.params; p; p = p->next) {
                if (p->type != NODE_VAR_DECL) continue;

                IRType* ptype = ir_type_from_datatype(module, p->data.var_decl.type);
                const char* pname = p->data.var_decl.name ? p->data.var_decl.name : NULL;

                int preg = ir_builder_add_param(builder, pname, ptype);

                if (pname) {
                    // اربط تعليمات تهيئة معامل الدالة بموقعه في المصدر.
                    ir_lower_set_loc(builder, p);

                    int ptr_reg = ir_builder_emit_alloca(builder, ptype);
                    ir_lower_tag_last_inst(builder, IR_OP_ALLOCA, ptr_reg, pname);
                    ir_lower_bind_local(&ctx, pname, ptr_reg, ptype);

                    IRValue* val = ir_value_reg(preg, ptype);   // %معامل<n>
                    IRType* ptr_type = ir_type_ptr(ptype ? ptype : IR_TYPE_I64_T);
                    IRValue* ptr = ir_value_reg(ptr_reg, ptr_type);
                    ir_lower_set_loc(builder, p);
                    ir_builder_emit_store(builder, val, ptr);
                    ir_lower_tag_last_inst(builder, IR_OP_STORE, -1, pname);
                }
            }

            // Lower function body (NODE_BLOCK)
            if (decl->data.func_def.body) {
                lower_stmt(&ctx, decl->data.func_def.body);
            }

            // خروج نطاق الدالة
            ir_lower_scope_pop(&ctx);

            if (ctx.had_error) {
                ir_builder_free(builder);
                ir_module_free(module);
                return NULL;
            }
        }
    }

    ir_builder_free(builder);
    return module;
}
