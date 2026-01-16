/**
 * @file ir_lower.c
 * @brief AST to IR lowering (IR Lowering) - Expressions
 * @version 0.3.0.3
 *
 * Implements expression lowering from validated AST into Baa IR using IRBuilder.
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
// Expression lowering helpers
// ============================================================================

static IRType* ir_type_from_datatype(DataType t) {
    switch (t) {
        case TYPE_BOOL:   return IR_TYPE_I1_T;
        case TYPE_STRING: return ir_type_ptr(IR_TYPE_I8_T);
        case TYPE_INT:
        default:          return IR_TYPE_I64_T;
    }
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

    switch (expr->type) {
        // -----------------------------------------------------------------
        // v0.3.0.3 required nodes
        // -----------------------------------------------------------------
        case NODE_INT:
            return ir_builder_const_i64((int64_t)expr->data.integer.value);

        case NODE_VAR_REF: {
            IRLowerBinding* b = find_local(ctx, expr->data.var_ref.name);
            if (!b) {
                // Not yet lowered variable declaration (v0.3.0.4) => no binding.
                fprintf(stderr, "IR Lower Error: unresolved local '%s' (no binding)\n",
                        expr->data.var_ref.name ? expr->data.var_ref.name : "???");
                return ir_builder_const_i64(0);
            }

            IRType* value_type = b->value_type ? b->value_type : IR_TYPE_I64_T;

            // Use a typeless pointer operand to avoid leaking dynamically allocated pointer types.
            IRValue* ptr = ir_value_reg(b->ptr_reg, NULL);
            int loaded = ir_builder_emit_load(ctx->builder, value_type, ptr);
            return ir_value_reg(loaded, value_type);
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