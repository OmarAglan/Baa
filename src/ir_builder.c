/**
 * @file ir_builder.c
 * @brief Baa IR Builder - Implementation
 * @version 0.3.0
 * 
 * Implementation of the builder pattern API for IR construction.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_builder.h"

// ============================================================================
// Builder Lifecycle
// ============================================================================

IRBuilder* ir_builder_new(IRModule* module) {
    IRBuilder* builder = (IRBuilder*)malloc(sizeof(IRBuilder));
    if (!builder) {
        fprintf(stderr, "خطأ: فشل تخصيص باني النواة\n");
        return NULL;
    }
    
    builder->module = module;
    builder->current_func = NULL;
    builder->insert_block = NULL;
    builder->src_file = NULL;
    builder->src_line = 0;
    builder->src_col = 0;
    builder->insts_emitted = 0;
    builder->blocks_created = 0;
    
    return builder;
}

void ir_builder_free(IRBuilder* builder) {
    if (!builder) return;
    // Note: We don't free the module or IR - that's owned elsewhere
    free(builder);
}

void ir_builder_set_module(IRBuilder* builder, IRModule* module) {
    if (!builder) return;
    builder->module = module;
}

// ============================================================================
// Function Creation
// ============================================================================

IRFunc* ir_builder_create_func(IRBuilder* builder, const char* name, IRType* ret_type) {
    if (!builder || !builder->module) return NULL;
    
    IRFunc* func = ir_func_new(name, ret_type);
    if (!func) return NULL;
    
    ir_module_add_func(builder->module, func);
    builder->current_func = func;
    builder->insert_block = NULL;
    
    return func;
}

int ir_builder_add_param(IRBuilder* builder, const char* name, IRType* type) {
    if (!builder || !builder->current_func) return -1;
    
    int param_count_before = builder->current_func->param_count;
    ir_func_add_param(builder->current_func, name, type);
    
    // Return the register assigned to the parameter
    if (builder->current_func->param_count > param_count_before) {
        return builder->current_func->params[param_count_before].reg;
    }
    return -1;
}

void ir_builder_set_func(IRBuilder* builder, IRFunc* func) {
    if (!builder) return;
    builder->current_func = func;
    builder->insert_block = NULL;
}

IRFunc* ir_builder_get_func(IRBuilder* builder) {
    if (!builder) return NULL;
    return builder->current_func;
}

// ============================================================================
// Block Creation & Navigation
// ============================================================================

IRBlock* ir_builder_create_block(IRBuilder* builder, const char* label) {
    if (!builder || !builder->current_func) return NULL;
    
    IRBlock* block = ir_func_new_block(builder->current_func, label);
    if (block) {
        builder->blocks_created++;
    }
    return block;
}

IRBlock* ir_builder_create_block_and_set(IRBuilder* builder, const char* label) {
    IRBlock* block = ir_builder_create_block(builder, label);
    if (block) {
        builder->insert_block = block;
    }
    return block;
}

void ir_builder_set_insert_point(IRBuilder* builder, IRBlock* block) {
    if (!builder) return;
    builder->insert_block = block;
}

IRBlock* ir_builder_get_insert_block(IRBuilder* builder) {
    if (!builder) return NULL;
    return builder->insert_block;
}

int ir_builder_is_block_terminated(IRBuilder* builder) {
    if (!builder || !builder->insert_block) return 0;
    return ir_block_is_terminated(builder->insert_block);
}

// ============================================================================
// Register Allocation
// ============================================================================

int ir_builder_alloc_reg(IRBuilder* builder) {
    if (!builder || !builder->current_func) return -1;
    return ir_func_alloc_reg(builder->current_func);
}

IRValue* ir_builder_reg_value(IRBuilder* builder, int reg, IRType* type) {
    (void)builder;  // Unused, but kept for API consistency
    return ir_value_reg(reg, type);
}

// ============================================================================
// Source Location Tracking
// ============================================================================

void ir_builder_set_loc(IRBuilder* builder, const char* file, int line, int col) {
    if (!builder) return;
    builder->src_file = file;
    builder->src_line = line;
    builder->src_col = col;
}

void ir_builder_clear_loc(IRBuilder* builder) {
    if (!builder) return;
    builder->src_file = NULL;
    builder->src_line = 0;
    builder->src_col = 0;
}

// ============================================================================
// Internal Helper: Emit instruction to current block
// ============================================================================

static void emit_inst(IRBuilder* builder, IRInst* inst) {
    if (!builder || !builder->insert_block || !inst) return;
    
    // Set source location if available
    if (builder->src_file) {
        ir_inst_set_loc(inst, builder->src_file, builder->src_line, builder->src_col);
    }
    
    ir_block_append(builder->insert_block, inst);
    builder->insts_emitted++;
}

// ============================================================================
// Arithmetic Instructions
// ============================================================================

int ir_builder_emit_add(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_ADD, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_sub(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_SUB, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_mul(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_MUL, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_div(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_DIV, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_mod(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_MOD, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_neg(IRBuilder* builder, IRType* type, IRValue* operand) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_unary(IR_OP_NEG, type, dest, operand);
    emit_inst(builder, inst);
    return dest;
}

// ============================================================================
// Memory Instructions
// ============================================================================

int ir_builder_emit_alloca(IRBuilder* builder, IRType* type) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_alloca(type, dest);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_load(IRBuilder* builder, IRType* type, IRValue* ptr) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_load(type, dest, ptr);
    emit_inst(builder, inst);
    return dest;
}

void ir_builder_emit_store(IRBuilder* builder, IRValue* value, IRValue* ptr) {
    if (!builder) return;
    
    IRInst* inst = ir_inst_store(value, ptr);
    emit_inst(builder, inst);
}

// ============================================================================
// Comparison Instructions
// ============================================================================

int ir_builder_emit_cmp(IRBuilder* builder, IRCmpPred pred, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_cmp(pred, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_cmp_eq(IRBuilder* builder, IRValue* lhs, IRValue* rhs) {
    return ir_builder_emit_cmp(builder, IR_CMP_EQ, lhs, rhs);
}

int ir_builder_emit_cmp_ne(IRBuilder* builder, IRValue* lhs, IRValue* rhs) {
    return ir_builder_emit_cmp(builder, IR_CMP_NE, lhs, rhs);
}

int ir_builder_emit_cmp_gt(IRBuilder* builder, IRValue* lhs, IRValue* rhs) {
    return ir_builder_emit_cmp(builder, IR_CMP_GT, lhs, rhs);
}

int ir_builder_emit_cmp_lt(IRBuilder* builder, IRValue* lhs, IRValue* rhs) {
    return ir_builder_emit_cmp(builder, IR_CMP_LT, lhs, rhs);
}

int ir_builder_emit_cmp_ge(IRBuilder* builder, IRValue* lhs, IRValue* rhs) {
    return ir_builder_emit_cmp(builder, IR_CMP_GE, lhs, rhs);
}

int ir_builder_emit_cmp_le(IRBuilder* builder, IRValue* lhs, IRValue* rhs) {
    return ir_builder_emit_cmp(builder, IR_CMP_LE, lhs, rhs);
}

// ============================================================================
// Logical Instructions
// ============================================================================

int ir_builder_emit_and(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_AND, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_or(IRBuilder* builder, IRType* type, IRValue* lhs, IRValue* rhs) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_binary(IR_OP_OR, type, dest, lhs, rhs);
    emit_inst(builder, inst);
    return dest;
}

int ir_builder_emit_not(IRBuilder* builder, IRType* type, IRValue* operand) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_unary(IR_OP_NOT, type, dest, operand);
    emit_inst(builder, inst);
    return dest;
}

// ============================================================================
// Control Flow Instructions
// ============================================================================

void ir_builder_emit_br(IRBuilder* builder, IRBlock* target) {
    if (!builder || !target) return;
    
    IRInst* inst = ir_inst_br(target);
    emit_inst(builder, inst);
    
    // Update CFG
    if (builder->insert_block) {
        ir_block_add_succ(builder->insert_block, target);
    }
}

void ir_builder_emit_br_cond(IRBuilder* builder, IRValue* cond, 
                              IRBlock* if_true, IRBlock* if_false) {
    if (!builder || !if_true || !if_false) return;
    
    IRInst* inst = ir_inst_br_cond(cond, if_true, if_false);
    emit_inst(builder, inst);
    
    // Update CFG
    if (builder->insert_block) {
        ir_block_add_succ(builder->insert_block, if_true);
        ir_block_add_succ(builder->insert_block, if_false);
    }
}

void ir_builder_emit_ret(IRBuilder* builder, IRValue* value) {
    if (!builder) return;
    
    IRInst* inst = ir_inst_ret(value);
    emit_inst(builder, inst);
}

void ir_builder_emit_ret_void(IRBuilder* builder) {
    ir_builder_emit_ret(builder, NULL);
}

void ir_builder_emit_ret_int(IRBuilder* builder, int64_t value) {
    IRValue* val = ir_value_const_int(value, IR_TYPE_I64_T);
    ir_builder_emit_ret(builder, val);
}

// ============================================================================
// Function Call Instructions
// ============================================================================

int ir_builder_emit_call(IRBuilder* builder, const char* target, IRType* ret_type,
                         IRValue** args, int arg_count) {
    if (!builder || !target) return -1;
    
    int dest = -1;
    if (ret_type && ret_type->kind != IR_TYPE_VOID) {
        dest = ir_builder_alloc_reg(builder);
    }
    
    IRInst* inst = ir_inst_call(target, ret_type, dest, args, arg_count);
    emit_inst(builder, inst);
    
    return dest;
}

void ir_builder_emit_call_void(IRBuilder* builder, const char* target,
                                IRValue** args, int arg_count) {
    ir_builder_emit_call(builder, target, IR_TYPE_VOID_T, args, arg_count);
}

// ============================================================================
// SSA Instructions
// ============================================================================

int ir_builder_emit_phi(IRBuilder* builder, IRType* type) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_phi(type, dest);
    emit_inst(builder, inst);
    return dest;
}

void ir_builder_phi_add_incoming(IRBuilder* builder, int phi_reg, 
                                  IRValue* value, IRBlock* block) {
    if (!builder || !builder->insert_block) return;
    
    // Find the phi instruction in the current block
    IRInst* inst = builder->insert_block->first;
    while (inst) {
        if (inst->op == IR_OP_PHI && inst->dest == phi_reg) {
            ir_inst_phi_add(inst, value, block);
            return;
        }
        inst = inst->next;
    }
    
    fprintf(stderr, "خطأ: لم يتم العثور على تعليمة فاي للسجل %d\n", phi_reg);
}

int ir_builder_emit_copy(IRBuilder* builder, IRType* type, IRValue* source) {
    if (!builder || !builder->current_func) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_unary(IR_OP_COPY, type, dest, source);
    emit_inst(builder, inst);
    return dest;
}

// ============================================================================
// Type Conversion
// ============================================================================

int ir_builder_emit_cast(IRBuilder* builder, IRValue* value, IRType* to_type) {
    if (!builder || !builder->current_func || !value) return -1;
    
    int dest = ir_builder_alloc_reg(builder);
    IRInst* inst = ir_inst_new(IR_OP_CAST, to_type, dest);
    ir_inst_add_operand(inst, value);
    emit_inst(builder, inst);
    return dest;
}

// ============================================================================
// Constant Value Helpers
// ============================================================================

IRValue* ir_builder_const_int(int64_t value) {
    return ir_value_const_int(value, IR_TYPE_I64_T);
}

IRValue* ir_builder_const_i64(int64_t value) {
    return ir_value_const_int(value, IR_TYPE_I64_T);
}

IRValue* ir_builder_const_i32(int32_t value) {
    return ir_value_const_int((int64_t)value, IR_TYPE_I32_T);
}

IRValue* ir_builder_const_bool(int value) {
    return ir_value_const_int(value ? 1 : 0, IR_TYPE_I1_T);
}

IRValue* ir_builder_const_string(IRBuilder* builder, const char* str) {
    if (!builder || !builder->module || !str) return NULL;
    
    int id = ir_module_add_string(builder->module, str);
    return ir_value_const_str(str, id);
}

// ============================================================================
// Global Variables
// ============================================================================

IRGlobal* ir_builder_create_global(IRBuilder* builder, const char* name, 
                                    IRType* type, int is_const) {
    if (!builder || !builder->module) return NULL;
    
    IRGlobal* global = ir_global_new(name, type, is_const);
    if (!global) return NULL;
    
    ir_module_add_global(builder->module, global);
    return global;
}

IRGlobal* ir_builder_create_global_init(IRBuilder* builder, const char* name,
                                         IRType* type, IRValue* init, int is_const) {
    IRGlobal* global = ir_builder_create_global(builder, name, type, is_const);
    if (global && init) {
        ir_global_set_init(global, init);
    }
    return global;
}

IRValue* ir_builder_get_global(IRBuilder* builder, const char* name) {
    if (!builder || !builder->module || !name) return NULL;
    
    IRGlobal* global = ir_module_find_global(builder->module, name);
    if (!global) return NULL;
    
    return ir_value_global(name, global->type);
}

// ============================================================================
// Control Flow Graph Helpers
// ============================================================================

void ir_builder_create_if_then(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* merge_label,
                                IRBlock** then_block, IRBlock** merge_block) {
    if (!builder || !builder->current_func) return;
    
    // Create blocks
    IRBlock* then_bb = ir_builder_create_block(builder, then_label);
    IRBlock* merge_bb = ir_builder_create_block(builder, merge_label);
    
    // Emit conditional branch
    ir_builder_emit_br_cond(builder, cond, then_bb, merge_bb);
    
    // Output the blocks
    if (then_block) *then_block = then_bb;
    if (merge_block) *merge_block = merge_bb;
}

void ir_builder_create_if_else(IRBuilder* builder, IRValue* cond,
                                const char* then_label, const char* else_label,
                                const char* merge_label,
                                IRBlock** then_block, IRBlock** else_block,
                                IRBlock** merge_block) {
    if (!builder || !builder->current_func) return;
    
    // Create blocks
    IRBlock* then_bb = ir_builder_create_block(builder, then_label);
    IRBlock* else_bb = ir_builder_create_block(builder, else_label);
    IRBlock* merge_bb = ir_builder_create_block(builder, merge_label);
    
    // Emit conditional branch
    ir_builder_emit_br_cond(builder, cond, then_bb, else_bb);
    
    // Output the blocks
    if (then_block) *then_block = then_bb;
    if (else_block) *else_block = else_bb;
    if (merge_block) *merge_block = merge_bb;
}

void ir_builder_create_while(IRBuilder* builder,
                              const char* header_label, const char* body_label,
                              const char* exit_label,
                              IRBlock** header_block, IRBlock** body_block,
                              IRBlock** exit_block) {
    if (!builder || !builder->current_func) return;
    
    // Create blocks
    IRBlock* header_bb = ir_builder_create_block(builder, header_label);
    IRBlock* body_bb = ir_builder_create_block(builder, body_label);
    IRBlock* exit_bb = ir_builder_create_block(builder, exit_label);
    
    // Jump to header from current block
    ir_builder_emit_br(builder, header_bb);
    
    // Output the blocks
    if (header_block) *header_block = header_bb;
    if (body_block) *body_block = body_bb;
    if (exit_block) *exit_block = exit_bb;
}

// ============================================================================
// Debugging & Statistics
// ============================================================================

int ir_builder_get_inst_count(IRBuilder* builder) {
    if (!builder) return 0;
    return builder->insts_emitted;
}

int ir_builder_get_block_count(IRBuilder* builder) {
    if (!builder) return 0;
    return builder->blocks_created;
}

void ir_builder_print_stats(IRBuilder* builder) {
    if (!builder) return;
    
    fprintf(stderr, "=== إحصائيات باني النواة ===\n");
    fprintf(stderr, "التعليمات المُنتَجة: %d\n", builder->insts_emitted);
    fprintf(stderr, "الكتل المُنشَأة: %d\n", builder->blocks_created);
    
    if (builder->current_func) {
        fprintf(stderr, "الدالة الحالية: %s\n", 
                builder->current_func->name ? builder->current_func->name : "???");
        fprintf(stderr, "السجلات المُخصَّصة: %d\n", builder->current_func->next_reg);
    }
}