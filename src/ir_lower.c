/**
 * @file ir_lower.c
 * @brief AST to IR lowering (IR Lowering) - Expressions + Statements + Control Flow
 * @version 0.3.0.5
 *
 * Implements lowering from validated AST into Baa IR using IRBuilder.
 *
 * v0.3.0.3: Expression lowering
 * v0.3.0.4: Statement lowering (var decl/assign/return/print/read)
 * v0.3.0.5: Control-flow lowering (if/while/for/switch/break/continue)
 */

#include "ir_lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Locals table helpers
// ============================================================================

static IRLowerBinding* find_local(IRLowerCtx* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    for (int i = 0; i < ctx->local_count; i++) {
        if (ctx->locals[i].name && strcmp(ctx->locals[i].name, name) == 0) {
            return &ctx->locals[i];
        }
    }
    return NULL;
}

void ir_lower_ctx_init(IRLowerCtx* ctx, IRBuilder* builder) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->builder = builder;

    // v0.3.0.5 control-flow lowering state
    ctx->label_counter = 0;
    ctx->cf_depth = 0;
}

void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type) {
    if (!ctx || !name) return;

    IRLowerBinding* existing = find_local(ctx, name);
    if (existing) {
        existing->ptr_reg = ptr_reg;
        existing->value_type = value_type;
        return;
    }

    if (ctx->local_count >= (int)(sizeof(ctx->locals) / sizeof(ctx->locals[0]))) {
        fprintf(stderr, "IR Lower Error: locals table overflow while binding '%s'\n", name);
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
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
    }
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
        fprintf(stderr, "IR Lower Error: control-flow stack overflow\n");
        return;
    }
    ctx->break_targets[ctx->cf_depth] = break_target;
    ctx->continue_targets[ctx->cf_depth] = continue_target;
    ctx->cf_depth++;
}

static void cf_pop(IRLowerCtx* ctx) {
    if (!ctx) return;
    if (ctx->cf_depth <= 0) {
        fprintf(stderr, "IR Lower Error: control-flow stack underflow\n");
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
            fprintf(stderr, "IR Lower Error: failed to allocate call args array\n");
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
                IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

                // Use a typeless pointer operand to avoid leaking dynamically allocated pointer types.
                IRValue* ptr = ir_value_reg(b->ptr_reg, NULL);
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

            fprintf(stderr, "IR Lower Error: unresolved variable '%s' (no local/global binding)\n",
                    name ? name : "???");
            return ir_builder_const_i64(0);
        }

        case NODE_BIN_OP: {
            OpType op = expr->data.bin_op.op;

            // Arithmetic
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_MOD) {
                IRValue* lhs = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs = lower_expr(ctx, expr->data.bin_op.right);

                IRType* type = IR_TYPE_I64_T;
                int dest = -1;

                switch (ir_binop_to_irop(op)) {
                    case IR_OP_ADD: dest = ir_builder_emit_add(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_SUB: dest = ir_builder_emit_sub(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MUL: dest = ir_builder_emit_mul(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_DIV: dest = ir_builder_emit_div(ctx->builder, type, lhs, rhs); break;
                    case IR_OP_MOD: dest = ir_builder_emit_mod(ctx->builder, type, lhs, rhs); break;
                    default:
                        fprintf(stderr, "IR Lower Error: unsupported arithmetic op\n");
                        return ir_builder_const_i64(0);
                }

                return ir_value_reg(dest, type);
            }

            // Comparison
            if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
                IRValue* lhs = lower_expr(ctx, expr->data.bin_op.left);
                IRValue* rhs = lower_expr(ctx, expr->data.bin_op.right);

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

            fprintf(stderr, "IR Lower Error: unsupported NODE_BIN_OP kind\n");
            return ir_builder_const_i64(0);
        }

        case NODE_UNARY_OP: {
            UnaryOpType op = expr->data.unary_op.op;

            if (op == UOP_NEG) {
                IRValue* operand = lower_expr(ctx, expr->data.unary_op.operand);
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
            fprintf(stderr, "IR Lower Error: unsupported unary op (%d)\n", (int)op);
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
            // Keep as i64 for now (matches current frontend treatment); backend can truncate later.
            return ir_value_const_int((int64_t)expr->data.char_lit.value, IR_TYPE_I64_T);

        case NODE_STRING:
            return ir_builder_const_string(ctx->builder, expr->data.string_lit.value);

        default:
            fprintf(stderr, "IR Lower Error: unsupported expr node type (%d)\n", (int)expr->type);
            return ir_builder_const_i64(0);
    }
}

// ============================================================================
// Statement lowering (v0.3.0.4)
// ============================================================================

static void lower_var_decl(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    // Ignore global declarations here (globals are lowered at module scope later).
    if (stmt->data.var_decl.is_global) {
        fprintf(stderr, "IR Lower Warning: global var decl '%s' ignored in statement lowering\n",
                stmt->data.var_decl.name ? stmt->data.var_decl.name : "???");
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

    // خزن <init>, %ptr
    IRValue* ptr = ir_value_reg(ptr_reg, NULL); // typeless pointer operand

    // نعيد الموقع إلى تصريح المتغير كي تُنسب عملية الخزن لسطر التصريح.
    ir_lower_set_loc(ctx->builder, stmt);
    ir_builder_emit_store(ctx->builder, init, ptr);
    ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, stmt->data.var_decl.name);
}

static void lower_assign(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    ir_lower_set_loc(ctx->builder, stmt);

    const char* name = stmt->data.assign_stmt.name;

    IRLowerBinding* b = find_local(ctx, name);
    if (b) {
        IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

        IRValue* rhs = lower_expr(ctx, stmt->data.assign_stmt.expression);

        // خزن <rhs>, %ptr
        (void)value_type; // reserved for future casts/type checks
        IRValue* ptr = ir_value_reg(b->ptr_reg, NULL);
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
        IRValue* gptr = ir_value_global(name, g->type);
        ir_lower_set_loc(ctx->builder, stmt);
        ir_builder_emit_store(ctx->builder, rhs, gptr);
        ir_lower_tag_last_inst(ctx->builder, IR_OP_STORE, -1, name);
        return;
    }

    fprintf(stderr, "IR Lower Error: assignment to unknown variable '%s'\n", name ? name : "???");
}

static void lower_return(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = NULL;
    if (stmt->data.return_stmt.expression) {
        value = lower_expr(ctx, stmt->data.return_stmt.expression);
    }
    ir_builder_emit_ret(ctx->builder, value);
}

static void lower_print(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

    IRValue* value = lower_expr(ctx, stmt->data.print_stmt.expression);

    // اختيار صيغة الطباعة بناءً على نوع القيمة
    // ملاحظة: حالياً نستخدم %d للأعداد والمنطقي (توافقاً مع المسار القديم)، و %s للنصوص.
    const char* fmt = "%d\n";
    if (value && value->type && value->type->kind == IR_TYPE_PTR &&
        value->type->data.pointee == IR_TYPE_I8_T) {
        fmt = "%s\n";
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
        fprintf(stderr, "IR Lower Error: read into unknown local '%s'\n",
                stmt->data.read_stmt.var_name ? stmt->data.read_stmt.var_name : "???");
        return;
    }

    // scanf("%d", &var)
    IRValue* fmt_val = ir_builder_const_string(ctx->builder, "%d");
    IRValue* ptr = ir_value_reg(b->ptr_reg, NULL);

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
    lower_stmt(ctx, stmt->data.if_stmt.then_branch);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, merge_bb);
    }

    // else
    if (else_bb) {
        ir_builder_set_insert_point(ctx->builder, else_bb);
        lower_stmt(ctx, stmt->data.if_stmt.else_branch);
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
    lower_stmt(ctx, stmt->data.while_stmt.body);
    if (!ir_builder_is_block_terminated(ctx->builder)) {
        ir_builder_emit_br(ctx->builder, header_bb);
    }
    cf_pop(ctx);

    // continue after loop
    ir_builder_set_insert_point(ctx->builder, exit_bb);
}

static void lower_for_stmt(IRLowerCtx* ctx, Node* stmt) {
    if (!ctx || !ctx->builder || !stmt) return;

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
}

static void lower_break_stmt(IRLowerCtx* ctx) {
    if (!ctx || !ctx->builder) return;
    IRBlock* target = cf_current_break(ctx);
    if (!target) {
        fprintf(stderr, "IR Lower Error: 'break' used outside of loop/switch\n");
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
        fprintf(stderr, "IR Lower Error: 'continue' used outside of loop\n");
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
        lower_stmt_list(ctx, case_node->data.case_stmt.body);

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
            lower_stmt_list(ctx, stmt->data.block.statements);
            return;

        case NODE_VAR_DECL:
            lower_var_decl(ctx, stmt);
            return;

        case NODE_ASSIGN:
            lower_assign(ctx, stmt);
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
            fprintf(stderr, "IR Lower Error: unsupported stmt node type (%d)\n", (int)stmt->type);
            return;
    }
}

// ============================================================================
// Program lowering (v0.3.0.7)
// ============================================================================

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
                                      expected_type ? expected_type : IR_TYPE_I64_T);

        case NODE_STRING:
            // Adds to module string table and returns pointer value.
            return ir_builder_const_string(builder, expr->data.string_lit.value);

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
        // Globals
        if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
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
                    IRValue* ptr = ir_value_reg(ptr_reg, NULL); // typeless pointer
                    ir_lower_set_loc(builder, p);
                    ir_builder_emit_store(builder, val, ptr);
                    ir_lower_tag_last_inst(builder, IR_OP_STORE, -1, pname);
                }
            }

            // Lower function body (NODE_BLOCK)
            if (decl->data.func_def.body) {
                lower_stmt(&ctx, decl->data.func_def.body);
            }
        }
    }

    ir_builder_free(builder);
    return module;
}
