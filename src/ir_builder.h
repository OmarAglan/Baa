/**
 * @file ir_builder.h
 * @brief Baa IR Builder - Convenient API for IR Construction
 * @version 0.3.0
 * 
 * This file provides a builder pattern API for constructing IR.
 * The IRBuilder maintains insertion context and provides emit_*
 * functions that automatically append to the current block.
 * 
 * Example usage:
 * ```c
 * IRBuilder* builder = ir_builder_new(module);
 * IRFunc* func = ir_builder_create_func(builder, "الرئيسية", IR_TYPE_I64_T);
 * IRBlock* entry = ir_builder_create_block(builder, "بداية");
 * ir_builder_set_insert_point(builder, entry);
 * 
 * int reg = ir_builder_emit_add(builder, IR_TYPE_I64_T, val1, val2);
 * ir_builder_emit_ret(builder, ir_value_reg(reg, IR_TYPE_I64_T));
 * 
 * ir_builder_free(builder);
 * ```
 */

#ifndef BAA_IR_BUILDER_H
#define BAA_IR_BUILDER_H

#include "ir.h"

// ============================================================================
// IR Builder Context (سياق بناء النواة)
// ============================================================================

/**
 * @struct IRBuilder
 * @brief Builder context for IR construction.
 * 
 * Maintains state for convenient IR construction including:
 * - Current module being built
 * - Current function being built
 * - Current block for instruction insertion
 * - Source location for debugging
 */
typedef struct IRBuilder {
    IRModule* module;           // Target module
    IRFunc* current_func;       // Current function being built
    IRBlock* insert_block;      // Current insertion block
    
    // Source location tracking (for debugging)
    const char* src_file;
    int src_line;
    int src_col;
    
    // Statistics
    int insts_emitted;
    int blocks_created;
} IRBuilder;

// ============================================================================
// Builder Lifecycle
// ============================================================================

/**
 * @brief Create a new IR builder.
 * @param module The target module (can be NULL, set later with ir_builder_set_module).
 * @return New builder instance.
 */
IRBuilder* ir_builder_new(IRModule* module);

/**
 * @brief Free an IR builder.
 * @param builder The builder to free.
 * @note This does NOT free the module or any IR created by the builder.
 */
void ir_builder_free(IRBuilder* builder);

/**
 * @brief Set the target module.
 * @param builder The builder.
 * @param module The target module.
 */
void ir_builder_set_module(IRBuilder* builder, IRModule* module);

// ============================================================================
// Function Creation
// ============================================================================

/**
 * @brief Create a new function and set it as current.
 * @param builder The builder.
 * @param name Function name.
 * @param ret_type Return type.
 * @return The new function.
 */
IRFunc* ir_builder_create_func(IRBuilder* builder, const char* name, IRType* ret_type);

/**
 * @brief Add a parameter to the current function.
 * @param builder The builder.
 * @param name Parameter name.
 * @param type Parameter type.
 * @return Register number assigned to the parameter.
 */
int ir_builder_add_param(IRBuilder* builder, const char* name, IRType* type);

/**
 * @brief Set the current function.
 * @param builder The builder.
 * @param func The function to work on.
 */
void ir_builder_set_func(IRBuilder* builder, IRFunc* func);

/**
 * @brief Get the current function.
 * @param builder The builder.
 * @return Current function (or NULL).
 */
IRFunc* ir_builder_get_func(IRBuilder* builder);

// ============================================================================
// Block Creation & Navigation
// ============================================================================

/**
 * @brief Create a new block in the current function.
 * @param builder The builder.
 * @param label Block label (Arabic name like "بداية").
 * @return The new block.
 */
IRBlock* ir_builder_create_block(IRBuilder* builder, const char* label);

/**
 * @brief Create a new block and set it as insertion point.
 * @param builder The builder.
 * @param label Block label.
 * @return The new block.
 */
IRBlock* ir_builder_create_block_and_set(IRBuilder* builder, const char* label);

/**
 * @brief Set the insertion point (where new instructions will be added).
 * @param builder The builder.
 * @param block The block to insert into.
 */
void ir_builder_set_insert_point(IRBuilder* builder, IRBlock* block);

/**
 * @brief Get the current insertion block.
 * @param builder The builder.
 * @return Current block (or NULL).
 */
IRBlock* ir_builder_get_insert_block(IRBuilder* builder);

/**
 * @brief Check if the current block is terminated.
 * @param builder The builder.
 * @return 1 if terminated, 0 otherwise.
 */
int ir_builder_is_block_terminated(IRBuilder* builder);

// ============================================================================
// Register Allocation
// ============================================================================

/**
 * @brief Allocate a new virtual register.
 * @param builder The builder.
 * @return New register number (%م<n>).
 */
int ir_builder_alloc_reg(IRBuilder* builder);

/**
 * @brief Create a register value for a given register number.
 * @param builder The builder.
 * @param reg Register number.
 * @param type The type.
 * @return IRValue for the register.
 */
IRValue* ir_builder_reg_value(IRBuilder* builder, int reg, IRType* type);

// ============================================================================
// Source Location Tracking
// ============================================================================

/**
 * @brief Set source location for subsequent instructions.
 * @param builder The builder.
 * @param file Source filename.
 * @param line Line number.
 * @param col Column number.
 */
void ir_builder_set_loc(IRBuilder* builder, const char* file, int line, int col);

/**
 * @brief Clear source location (for generated code).
 * @param builder The builder.
 */
void ir_builder_clear_loc(IRBuilder* builder);

// ============================================================================
// Arithmetic Instructions (العمليات الحسابية)
// ============================================================================

/**
 * @brief Emit an addition instruction: %dest = جمع type lhs, rhs
 * @return Destination register number.
 */
int ir_builder_emit_add(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit a subtraction instruction: %dest = طرح type lhs, rhs
 * @return Destination register number.
 */
int ir_builder_emit_sub(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit a multiplication instruction: %dest = ضرب type lhs, rhs
 * @return Destination register number.
 */
int ir_builder_emit_mul(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit a division instruction: %dest = قسم type lhs, rhs
 * @return Destination register number.
 */
int ir_builder_emit_div(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit a modulo instruction: %dest = باقي type lhs, rhs
 * @return Destination register number.
 */
int ir_builder_emit_mod(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit a negation instruction: %dest = سالب type operand
 * @return Destination register number.
 */
int ir_builder_emit_neg(IRBuilder* builder, IRType* type, IRValue* operand);

// ============================================================================
// Memory Instructions (عمليات الذاكرة)
// ============================================================================

/**
 * @brief Emit an alloca instruction: %dest = حجز type
 * @param builder The builder.
 * @param type Type to allocate (result is pointer to this type).
 * @return Destination register number (pointer to allocated space).
 */
int ir_builder_emit_alloca(IRBuilder* builder, IRType* type);

/**
 * @brief Emit a load instruction: %dest = حمل type, ptr
 * @param builder The builder.
 * @param type Type to load.
 * @param ptr Pointer to load from.
 * @return Destination register number.
 */
int ir_builder_emit_load(IRBuilder* builder, IRType* type, IRValue* ptr);

/**
 * @brief Emit a store instruction: خزن value, ptr
 * @param builder The builder.
 * @param value Value to store.
 * @param ptr Pointer to store to.
 * @note Store has no result register.
 */
void ir_builder_emit_store(IRBuilder* builder, IRValue* value, IRValue* ptr);

// ============================================================================
// Comparison Instructions (عمليات المقارنة)
// ============================================================================

/**
 * @brief Emit a comparison instruction: %dest = قارن pred type lhs, rhs
 * @param builder The builder.
 * @param pred Comparison predicate.
 * @param lhs Left operand.
 * @param rhs Right operand.
 * @return Destination register number (i1/boolean result).
 */
int ir_builder_emit_cmp(IRBuilder* builder, IRCmpPred pred, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit equal comparison: %dest = قارن يساوي lhs, rhs
 */
int ir_builder_emit_cmp_eq(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit not-equal comparison: %dest = قارن لا_يساوي lhs, rhs
 */
int ir_builder_emit_cmp_ne(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit greater-than comparison: %dest = قارن أكبر lhs, rhs
 */
int ir_builder_emit_cmp_gt(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit less-than comparison: %dest = قارن أصغر lhs, rhs
 */
int ir_builder_emit_cmp_lt(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit greater-or-equal comparison: %dest = قارن أكبر_أو_يساوي lhs, rhs
 */
int ir_builder_emit_cmp_ge(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit less-or-equal comparison: %dest = قارن أصغر_أو_يساوي lhs, rhs
 */
int ir_builder_emit_cmp_le(IRBuilder* builder, IRValue* lhs, IRValue* rhs);

// ============================================================================
// Logical Instructions (العمليات المنطقية)
// ============================================================================

/**
 * @brief Emit bitwise AND: %dest = و type lhs, rhs
 */
int ir_builder_emit_and(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit bitwise OR: %dest = أو type lhs, rhs
 */
int ir_builder_emit_or(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs);

/**
 * @brief Emit bitwise NOT: %dest = نفي type operand
 */
int ir_builder_emit_not(IRBuilder* builder, IRType* type, IRValue* operand);

// ============================================================================
// Control Flow Instructions (عمليات التحكم)
// ============================================================================

/**
 * @brief Emit unconditional branch: قفز target
 * @param builder The builder.
 * @param target Target block.
 */
void ir_builder_emit_br(IRBuilder* builder, IRBlock* target);

/**
 * @brief Emit conditional branch: قفز_شرط cond, if_true, if_false
 * @param builder The builder.
 * @param cond Condition value (i1/boolean).
 * @param if_true Block to jump to if condition is true.
 * @param if_false Block to jump to if condition is false.
 */
void ir_builder_emit_br_cond(IRBuilder* builder, IRValue* cond, 
                              IRBlock* if_true, IRBlock* if_false);

/**
 * @brief Emit return instruction: رجوع value
 * @param builder The builder.
 * @param value Value to return (NULL for void return).
 */
void ir_builder_emit_ret(IRBuilder* builder, IRValue* value);

/**
 * @brief Emit void return: رجوع
 * @param builder The builder.
 */
void ir_builder_emit_ret_void(IRBuilder* builder);

/**
 * @brief Emit return with integer constant.
 * @param builder The builder.
 * @param value Integer value to return.
 */
void ir_builder_emit_ret_int(IRBuilder* builder, int64_t value);

// ============================================================================
// Function Call Instructions (استدعاء الدوال)
// ============================================================================

/**
 * @brief Emit a function call: %dest = نداء ret_type @target(args...)
 * @param builder The builder.
 * @param target Function name.
 * @param ret_type Return type.
 * @param args Array of argument values.
 * @param arg_count Number of arguments.
 * @return Destination register number (-1 for void calls).
 */
int ir_builder_emit_call(IRBuilder* builder, const char* target, IRType* ret_type,
                         IRValue** args, int arg_count);

/**
 * @brief Emit a void function call: نداء فراغ @target(args...)
 * @param builder The builder.
 * @param target Function name.
 * @param args Array of argument values.
 * @param arg_count Number of arguments.
 */
void ir_builder_emit_call_void(IRBuilder* builder, const char* target,
                                IRValue** args, int arg_count);

// ============================================================================
// SSA Instructions (عمليات SSA)
// ============================================================================

/**
 * @brief Emit a phi node: %dest = فاي type [val1, block1], [val2, block2], ...
 * @param builder The builder.
 * @param type Result type.
 * @return Destination register number.
 * @note Use ir_builder_phi_add_incoming() to add incoming values.
 */
int ir_builder_emit_phi(IRBuilder* builder, IRType* type);

/**
 * @brief Add an incoming value to a phi node.
 * @param builder The builder.
 * @param phi_reg Register number of the phi instruction.
 * @param value Incoming value.
 * @param block Predecessor block for this value.
 */
void ir_builder_phi_add_incoming(IRBuilder* builder, int phi_reg, 
                                  IRValue* value, IRBlock* block);

/**
 * @brief Emit a copy instruction: %dest = نسخ type source
 * @param builder The builder.
 * @param type Type.
 * @param source Source value.
 * @return Destination register number.
 */
int ir_builder_emit_copy(IRBuilder* builder, IRType* type, IRValue* source);

// ============================================================================
// Type Conversion (تحويل الأنواع)
// ============================================================================

/**
 * @brief Emit a type cast: %dest = تحويل from_type to to_type value
 * @param builder The builder.
 * @param value Value to cast.
 * @param to_type Target type.
 * @return Destination register number.
 */
int ir_builder_emit_cast(IRBuilder* builder, IRValue* value, IRType* to_type);

// ============================================================================
// Constant Value Helpers
// ============================================================================

/**
 * @brief Create an integer constant value.
 * @param value Integer value.
 * @return IRValue for the constant.
 */
IRValue* ir_builder_const_int(int64_t value);

/**
 * @brief Create an i64 constant value.
 * @param value Integer value.
 * @return IRValue for the constant.
 */
IRValue* ir_builder_const_i64(int64_t value);

/**
 * @brief Create an i32 constant value.
 * @param value Integer value.
 * @return IRValue for the constant.
 */
IRValue* ir_builder_const_i32(int32_t value);

/**
 * @brief Create an i1 (boolean) constant value.
 * @param value Boolean value (0 or 1).
 * @return IRValue for the constant.
 */
IRValue* ir_builder_const_bool(int value);

/**
 * @brief Create a string constant (adds to module string table).
 * @param builder The builder.
 * @param str String content.
 * @return IRValue for the string pointer.
 */
IRValue* ir_builder_const_string(IRBuilder* builder, const char* str);

// ============================================================================
// Global Variables
// ============================================================================

/**
 * @brief Create a global variable.
 * @param builder The builder.
 * @param name Variable name.
 * @param type Variable type.
 * @param is_const Whether the variable is constant.
 * @return The global variable.
 */
IRGlobal* ir_builder_create_global(IRBuilder* builder, const char* name, 
                                    IRType* type, int is_const);

/**
 * @brief Create a global variable with an initializer.
 * @param builder The builder.
 * @param name Variable name.
 * @param type Variable type.
 * @param init Initial value.
 * @param is_const Whether the variable is constant.
 * @return The global variable.
 */
IRGlobal* ir_builder_create_global_init(IRBuilder* builder, const char* name,
                                         IRType* type, IRValue* init, int is_const);

/**
 * @brief Get a reference to a global variable.
 * @param builder The builder.
 * @param name Global name.
 * @return IRValue reference to the global.
 */
IRValue* ir_builder_get_global(IRBuilder* builder, const char* name);

// ============================================================================
// Control Flow Graph Helpers
// ============================================================================

/**
 * @brief Create an if-then structure.
 * @param builder The builder.
 * @param cond Condition value.
 * @param then_label Label for then block.
 * @param merge_label Label for merge block.
 * @param[out] then_block Output: the then block.
 * @param[out] merge_block Output: the merge block.
 */
void ir_builder_create_if_then(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* merge_label,
                                IRBlock** then_block, IRBlock** merge_block);

/**
 * @brief Create an if-then-else structure.
 * @param builder The builder.
 * @param cond Condition value.
 * @param then_label Label for then block.
 * @param else_label Label for else block.
 * @param merge_label Label for merge block.
 * @param[out] then_block Output: the then block.
 * @param[out] else_block Output: the else block.
 * @param[out] merge_block Output: the merge block.
 */
void ir_builder_create_if_else(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* else_label,
                                const char* merge_label,
                                IRBlock** then_block, IRBlock** else_block,
                                IRBlock** merge_block);

/**
 * @brief Create a while loop structure.
 * @param builder The builder.
 * @param header_label Label for loop header block.
 * @param body_label Label for loop body block.
 * @param exit_label Label for loop exit block.
 * @param[out] header_block Output: the header block (condition check).
 * @param[out] body_block Output: the body block.
 * @param[out] exit_block Output: the exit block.
 */
void ir_builder_create_while(IRBuilder* builder,
                              const char* header_label, const char* body_label,
                              const char* exit_label,
                              IRBlock** header_block, IRBlock** body_block,
                              IRBlock** exit_block);

// ============================================================================
// Debugging & Statistics
// ============================================================================

/**
 * @brief Get number of instructions emitted.
 * @param builder The builder.
 * @return Instruction count.
 */
int ir_builder_get_inst_count(IRBuilder* builder);

/**
 * @brief Get number of blocks created.
 * @param builder The builder.
 * @return Block count.
 */
int ir_builder_get_block_count(IRBuilder* builder);

/**
 * @brief Print builder statistics to stderr.
 * @param builder The builder.
 */
void ir_builder_print_stats(IRBuilder* builder);

#endif // BAA_IR_BUILDER_H