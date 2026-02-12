/**
 * @file ir_lower.h
 * @brief AST to IR lowering (IR Lowering) - Expressions + Statements + Control Flow
 * @version 0.3.0.5
 *
 * This module lowers validated AST nodes into Baa's IR using IRBuilder.
 *
 * Current scope:
 * - v0.3.0.3: expression lowering
 * - v0.3.0.4: statement lowering (var decl/assign/return/print/read)
 * - v0.3.0.5: control-flow lowering (if/while/for/switch/break/continue)
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

    // مكدس نطاقات المتغيرات المحلية (Local Scopes)
    int scope_stack[64];
    int scope_depth;

    // Label generator for CFG lowering (unique suffixes for Arabic labels).
    int label_counter;

    // Control-flow context stacks (for break/continue).
    struct IRBlock* break_targets[64];
    struct IRBlock* continue_targets[64];
    int cf_depth;
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

// ============================================================================
// Statement lowering (v0.3.0.4)
// ============================================================================

/**
 * @brief Lower a single statement node.
 */
void lower_stmt(IRLowerCtx* ctx, Node* stmt);

/**
 * @brief Lower a linked-list of statements (as used in NODE_BLOCK).
 */
void lower_stmt_list(IRLowerCtx* ctx, Node* first_stmt);

// ============================================================================
// Program lowering (v0.3.0.7)
// ============================================================================

/**
 * @brief Lower a validated AST program (NODE_PROGRAM) into an IR module.
 *
 * This is the main integration entry point for the driver pipeline in v0.3.0.7.
 *
 * @param program Root AST node (must be NODE_PROGRAM).
 * @param module_name Optional module name (usually filename).
 * @return Newly allocated IRModule (caller owns; free with ir_module_free()).
 */
IRModule* ir_lower_program(Node* program, const char* module_name);

#ifdef __cplusplus
}
#endif

#endif // BAA_IR_LOWER_H
