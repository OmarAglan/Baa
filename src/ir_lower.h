/**
 * @file ir_lower.h
 * @brief AST to IR lowering (IR Lowering) - Expressions
 * @version 0.3.0.3
 *
 * This module lowers validated AST nodes into Baa's IR using IRBuilder.
 *
 * Current scope (v0.3.0.3): expression lowering:
 * - NODE_INT
 * - NODE_VAR_REF
 * - NODE_BIN_OP (add/sub/mul/div)
 * - NODE_UNARY_OP (neg/not)
 * - NODE_CALL_EXPR
 */

#ifndef BAA_IR_LOWER_H
#define BAA_IR_LOWER_H

#include "baa.h"
#include "ir_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Lowering Context (سياق التحويل)
// ============================================================================

typedef struct IRLowerBinding {
    const char* name;     // Variable name (UTF-8)
    int ptr_reg;          // Register that holds the pointer (result of حجز/alloca)
    IRType* value_type;   // Type of the value stored at ptr_reg (e.g., ص٦٤)
} IRLowerBinding;

typedef struct IRLowerCtx {
    IRBuilder* builder;

    // Simple local variable bindings table.
    // Each binding maps a variable name -> pointer register.
    IRLowerBinding locals[256];
    int local_count;
} IRLowerCtx;

/**
 * @brief Initialize an IR lowering context.
 */
void ir_lower_ctx_init(IRLowerCtx* ctx, IRBuilder* builder);

/**
 * @brief Bind a local variable name to its alloca pointer register.
 *
 * This is expected to be used by statement lowering (v0.3.0.4) when emitting
 * `حجز` for `NODE_VAR_DECL` so that `NODE_VAR_REF` can emit `حمل`.
 */
void ir_lower_bind_local(IRLowerCtx* ctx, const char* name, int ptr_reg, IRType* value_type);

// ============================================================================
// Expression lowering (v0.3.0.3)
// ============================================================================

/**
 * @brief Lower an expression AST node to an IRValue.
 *
 * Ownership note: The returned IRValue is intended to be used as an operand
 * exactly once (passed into an instruction). If you need to reuse it, create a
 * fresh copy via ir_value_reg()/ir_value_const_int(), etc.
 */
IRValue* lower_expr(IRLowerCtx* ctx, Node* expr);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_LOWER_H