/**
 * @file ir.c
 * @brief Baa IR (نواة باء) - Implementation
 * @version 0.3.0.1
 * 
 * Implementation of Baa's Arabic-first Intermediate Representation.
 * Provides helper functions for IR construction, printing, and memory management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "baa.h"  // For error functions

// ============================================================================
// Predefined Type Instances (Singletons)
// ============================================================================

static IRType ir_type_void_instance   = { .kind = IR_TYPE_VOID };
static IRType ir_type_i1_instance     = { .kind = IR_TYPE_I1 };
static IRType ir_type_i8_instance     = { .kind = IR_TYPE_I8 };
static IRType ir_type_i16_instance    = { .kind = IR_TYPE_I16 };
static IRType ir_type_i32_instance    = { .kind = IR_TYPE_I32 };
static IRType ir_type_i64_instance    = { .kind = IR_TYPE_I64 };

// Global type accessors
IRType* IR_TYPE_VOID_T  = &ir_type_void_instance;
IRType* IR_TYPE_I1_T    = &ir_type_i1_instance;
IRType* IR_TYPE_I8_T    = &ir_type_i8_instance;
IRType* IR_TYPE_I16_T   = &ir_type_i16_instance;
IRType* IR_TYPE_I32_T   = &ir_type_i32_instance;
IRType* IR_TYPE_I64_T   = &ir_type_i64_instance;

// ============================================================================
// Arabic Numeral Conversion
// ============================================================================

/**
 * Arabic-Indic numerals (٠١٢٣٤٥٦٧٨٩)
 * Each digit is 2 bytes in UTF-8: 0xD9 0x80-0x89
 */
static const char* arabic_digits[] = {
    "٠", "١", "٢", "٣", "٤", "٥", "٦", "٧", "٨", "٩"
};

/**
 * Convert an integer to Arabic-Indic numerals
 * @param n The number to convert
 * @param buf Output buffer (must be large enough, at least 32 bytes)
 * @return Pointer to buf
 */
char* int_to_arabic_numerals(int n, char* buf) {
    if (n == 0) {
        strcpy(buf, "٠");
        return buf;
    }
    
    int is_negative = 0;
    if (n < 0) {
        is_negative = 1;
        n = -n;
    }
    
    // Build digits in reverse order
    char temp[64];
    int pos = 0;
    
    while (n > 0) {
        int digit = n % 10;
        // Copy the Arabic digit (each is 2 bytes UTF-8)
        temp[pos++] = arabic_digits[digit][0];
        temp[pos++] = arabic_digits[digit][1];
        n /= 10;
    }
    
    // Reverse the digits into buf
    int buf_pos = 0;
    if (is_negative) {
        buf[buf_pos++] = '-';
    }
    
    // Arabic digits are 2 bytes each, reverse in pairs
    for (int i = pos - 2; i >= 0; i -= 2) {
        buf[buf_pos++] = temp[i];
        buf[buf_pos++] = temp[i + 1];
    }
    buf[buf_pos] = '\0';
    
    return buf;
}

// ============================================================================
// Arabic Name Conversions
// ============================================================================

/**
 * Convert IR opcode to Arabic name
 */
const char* ir_op_to_arabic(IROp op) {
    switch (op) {
        // Arithmetic
        case IR_OP_ADD:     return "جمع";      // Add
        case IR_OP_SUB:     return "طرح";      // Subtract
        case IR_OP_MUL:     return "ضرب";      // Multiply
        case IR_OP_DIV:     return "قسم";      // Divide
        case IR_OP_MOD:     return "باقي";     // Modulo
        case IR_OP_NEG:     return "سالب";     // Negate
        
        // Memory
        case IR_OP_ALLOCA:  return "حجز";      // Stack allocation
        case IR_OP_LOAD:    return "حمل";      // Load
        case IR_OP_STORE:   return "خزن";      // Store
        
        // Comparison
        case IR_OP_CMP:     return "قارن";     // Compare
        
        // Logical
        case IR_OP_AND:     return "و";        // Bitwise AND
        case IR_OP_OR:      return "أو";       // Bitwise OR
        case IR_OP_NOT:     return "نفي";      // Bitwise NOT
        
        // Control Flow
        case IR_OP_BR:      return "قفز";      // Branch (unconditional)
        case IR_OP_BR_COND: return "قفز_شرط";  // Branch (conditional)
        case IR_OP_RET:     return "رجوع";     // Return
        case IR_OP_CALL:    return "نداء";     // Function call
        
        // SSA
        case IR_OP_PHI:     return "فاي";      // Phi node
        case IR_OP_COPY:    return "نسخ";      // Copy/Move
        
        // Type Conversion
        case IR_OP_CAST:    return "تحويل";    // Type cast
        
        // Misc
        case IR_OP_NOP:     return "لاعمل";    // No operation
        
        default:            return "مجهول";    // Unknown
    }
}

/**
 * Convert IR opcode to English name (for debugging/compatibility)
 */
const char* ir_op_to_english(IROp op) {
    switch (op) {
        case IR_OP_ADD:     return "add";
        case IR_OP_SUB:     return "sub";
        case IR_OP_MUL:     return "mul";
        case IR_OP_DIV:     return "div";
        case IR_OP_MOD:     return "mod";
        case IR_OP_NEG:     return "neg";
        case IR_OP_ALLOCA:  return "alloca";
        case IR_OP_LOAD:    return "load";
        case IR_OP_STORE:   return "store";
        case IR_OP_CMP:     return "cmp";
        case IR_OP_AND:     return "and";
        case IR_OP_OR:      return "or";
        case IR_OP_NOT:     return "not";
        case IR_OP_BR:      return "br";
        case IR_OP_BR_COND: return "br.cond";
        case IR_OP_RET:     return "ret";
        case IR_OP_CALL:    return "call";
        case IR_OP_PHI:     return "phi";
        case IR_OP_COPY:    return "copy";
        case IR_OP_CAST:    return "cast";
        case IR_OP_NOP:     return "nop";
        default:            return "unknown";
    }
}

/**
 * Convert comparison predicate to Arabic name
 */
const char* ir_cmp_pred_to_arabic(IRCmpPred pred) {
    switch (pred) {
        case IR_CMP_EQ:  return "يساوي";           // Equal
        case IR_CMP_NE:  return "لا_يساوي";        // Not Equal
        case IR_CMP_GT:  return "أكبر";            // Greater Than
        case IR_CMP_LT:  return "أصغر";            // Less Than
        case IR_CMP_GE:  return "أكبر_أو_يساوي";   // Greater or Equal
        case IR_CMP_LE:  return "أصغر_أو_يساوي";   // Less or Equal
        default:         return "مجهول";
    }
}

/**
 * Convert comparison predicate to English name
 */
const char* ir_cmp_pred_to_english(IRCmpPred pred) {
    switch (pred) {
        case IR_CMP_EQ:  return "eq";
        case IR_CMP_NE:  return "ne";
        case IR_CMP_GT:  return "sgt";
        case IR_CMP_LT:  return "slt";
        case IR_CMP_GE:  return "sge";
        case IR_CMP_LE:  return "sle";
        default:         return "unknown";
    }
}

/**
 * Convert IR type to Arabic representation
 */
const char* ir_type_to_arabic(IRType* type) {
    if (!type) return "فراغ";
    
    switch (type->kind) {
        case IR_TYPE_VOID:   return "فراغ";     // Void
        case IR_TYPE_I1:     return "ص١";       // 1-bit (bool)
        case IR_TYPE_I8:     return "ص٨";       // 8-bit
        case IR_TYPE_I16:    return "ص١٦";      // 16-bit
        case IR_TYPE_I32:    return "ص٣٢";      // 32-bit
        case IR_TYPE_I64:    return "ص٦٤";      // 64-bit
        case IR_TYPE_PTR:    return "مؤشر";     // Pointer
        case IR_TYPE_ARRAY:  return "مصفوفة";   // Array
        case IR_TYPE_FUNC:   return "دالة";     // Function
        default:             return "مجهول";    // Unknown
    }
}

/**
 * Convert IR type to English representation
 */
const char* ir_type_to_english(IRType* type) {
    if (!type) return "void";
    
    switch (type->kind) {
        case IR_TYPE_VOID:   return "void";
        case IR_TYPE_I1:     return "i1";
        case IR_TYPE_I8:     return "i8";
        case IR_TYPE_I16:    return "i16";
        case IR_TYPE_I32:    return "i32";
        case IR_TYPE_I64:    return "i64";
        case IR_TYPE_PTR:    return "ptr";
        case IR_TYPE_ARRAY:  return "array";
        case IR_TYPE_FUNC:   return "func";
        default:             return "unknown";
    }
}

// ============================================================================
// Type Construction
// ============================================================================

/**
 * Create a pointer type
 */
IRType* ir_type_ptr(IRType* pointee) {
    IRType* type = (IRType*)malloc(sizeof(IRType));
    if (!type) {
        fprintf(stderr, "خطأ: فشل تخصيص نوع مؤشر\n");
        return NULL;
    }
    type->kind = IR_TYPE_PTR;
    type->data.pointee = pointee;
    return type;
}

/**
 * Create an array type
 */
IRType* ir_type_array(IRType* element, int count) {
    IRType* type = (IRType*)malloc(sizeof(IRType));
    if (!type) {
        fprintf(stderr, "خطأ: فشل تخصيص نوع مصفوفة\n");
        return NULL;
    }
    type->kind = IR_TYPE_ARRAY;
    type->data.array.element = element;
    type->data.array.count = count;
    return type;
}

/**
 * Create a function type
 */
IRType* ir_type_func(IRType* ret, IRType** params, int param_count) {
    IRType* type = (IRType*)malloc(sizeof(IRType));
    if (!type) {
        fprintf(stderr, "خطأ: فشل تخصيص نوع دالة\n");
        return NULL;
    }
    type->kind = IR_TYPE_FUNC;
    type->data.func.ret = ret;
    type->data.func.params = params;
    type->data.func.param_count = param_count;
    return type;
}

/**
 * Free a dynamically allocated type
 */
void ir_type_free(IRType* type) {
    if (!type) return;
    // Don't free singleton types
    if (type == IR_TYPE_VOID_T || type == IR_TYPE_I1_T ||
        type == IR_TYPE_I8_T || type == IR_TYPE_I16_T ||
        type == IR_TYPE_I32_T || type == IR_TYPE_I64_T) {
        return;
    }
    // For function types, free the params array
    if (type->kind == IR_TYPE_FUNC && type->data.func.params) {
        free(type->data.func.params);
    }
    free(type);
}

/**
 * Check if two types are equal
 */
int ir_types_equal(IRType* a, IRType* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    
    switch (a->kind) {
        case IR_TYPE_PTR:
            return ir_types_equal(a->data.pointee, b->data.pointee);
        case IR_TYPE_ARRAY:
            return a->data.array.count == b->data.array.count &&
                   ir_types_equal(a->data.array.element, b->data.array.element);
        case IR_TYPE_FUNC:
            if (a->data.func.param_count != b->data.func.param_count) return 0;
            if (!ir_types_equal(a->data.func.ret, b->data.func.ret)) return 0;
            for (int i = 0; i < a->data.func.param_count; i++) {
                if (!ir_types_equal(a->data.func.params[i], b->data.func.params[i])) {
                    return 0;
                }
            }
            return 1;
        default:
            return 1;  // Simple types match by kind
    }
}

/**
 * Get the size in bits for a type
 */
int ir_type_bits(IRType* type) {
    if (!type) return 0;
    switch (type->kind) {
        case IR_TYPE_VOID:  return 0;
        case IR_TYPE_I1:    return 1;
        case IR_TYPE_I8:    return 8;
        case IR_TYPE_I16:   return 16;
        case IR_TYPE_I32:   return 32;
        case IR_TYPE_I64:   return 64;
        case IR_TYPE_PTR:   return 64;  // 64-bit pointers
        case IR_TYPE_ARRAY:
            return type->data.array.count * ir_type_bits(type->data.array.element);
        case IR_TYPE_FUNC:  return 64;  // Function pointers
        default:            return 0;
    }
}

// ============================================================================
// IR Value Construction
// ============================================================================

/**
 * Create a register value
 */
IRValue* ir_value_reg(int reg_num, IRType* type) {
    IRValue* val = (IRValue*)malloc(sizeof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_REG;
    val->type = type;
    val->data.reg_num = reg_num;
    return val;
}

/**
 * Create an integer constant value
 */
IRValue* ir_value_const_int(int64_t value, IRType* type) {
    IRValue* val = (IRValue*)malloc(sizeof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_CONST_INT;
    val->type = type ? type : IR_TYPE_I64_T;
    val->data.const_int = value;
    return val;
}

/**
 * Create a string constant value
 */
IRValue* ir_value_const_str(const char* str, int id) {
    IRValue* val = (IRValue*)malloc(sizeof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_CONST_STR;
    val->type = ir_type_ptr(IR_TYPE_I8_T);
    val->data.const_str.data = str ? strdup(str) : NULL;
    val->data.const_str.id = id;
    return val;
}

/**
 * Create a block reference value
 */
IRValue* ir_value_block(IRBlock* block) {
    IRValue* val = (IRValue*)malloc(sizeof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_BLOCK;
    val->type = NULL;  // Blocks don't have a type
    val->data.block = block;
    return val;
}

/**
 * Create a global variable reference value
 */
IRValue* ir_value_global(const char* name, IRType* type) {
    IRValue* val = (IRValue*)malloc(sizeof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_GLOBAL;
    val->type = type ? ir_type_ptr(type) : NULL;
    val->data.global_name = name ? strdup(name) : NULL;
    return val;
}

/**
 * Create a function reference value
 */
IRValue* ir_value_func_ref(const char* name, IRType* type) {
    IRValue* val = (IRValue*)malloc(sizeof(IRValue));
    if (!val) return NULL;
    val->kind = IR_VAL_FUNC;
    val->type = type;
    val->data.global_name = name ? strdup(name) : NULL;
    return val;
}

/**
 * Free a value
 */
void ir_value_free(IRValue* val) {
    if (!val) return;
    if (val->kind == IR_VAL_CONST_STR && val->data.const_str.data) {
        free(val->data.const_str.data);
    }
    if ((val->kind == IR_VAL_GLOBAL || val->kind == IR_VAL_FUNC) && 
        val->data.global_name) {
        free(val->data.global_name);
    }
    if (val->type && val->kind == IR_VAL_GLOBAL) {
        ir_type_free(val->type);
    }
    free(val);
}

// ============================================================================
// IR Instruction Construction
// ============================================================================

/**
 * Create a new instruction
 */
IRInst* ir_inst_new(IROp op, IRType* type, int dest) {
    IRInst* inst = (IRInst*)malloc(sizeof(IRInst));
    if (!inst) return NULL;
    
    inst->op = op;
    inst->type = type;
    inst->dest = dest;
    inst->operand_count = 0;
    for (int i = 0; i < 4; i++) {
        inst->operands[i] = NULL;
    }
    inst->cmp_pred = IR_CMP_EQ;  // Default
    inst->phi_entries = NULL;
    inst->call_target = NULL;
    inst->call_args = NULL;
    inst->call_arg_count = 0;
    inst->src_file = NULL;
    inst->src_line = 0;
    inst->src_col = 0;
    inst->prev = NULL;
    inst->next = NULL;
    
    return inst;
}

/**
 * Add an operand to an instruction
 */
void ir_inst_add_operand(IRInst* inst, IRValue* operand) {
    if (!inst || !operand) return;
    if (inst->operand_count >= 4) {
        fprintf(stderr, "خطأ: تجاوز الحد الأقصى للمعاملات (٤)\n");
        return;
    }
    inst->operands[inst->operand_count++] = operand;
}

/**
 * Set source location for an instruction
 */
void ir_inst_set_loc(IRInst* inst, const char* file, int line, int col) {
    if (!inst) return;
    inst->src_file = file;
    inst->src_line = line;
    inst->src_col = col;
}

/**
 * Create a binary operation (add, sub, mul, div, etc.)
 */
IRInst* ir_inst_binary(IROp op, IRType* type, int dest, IRValue* lhs, IRValue* rhs) {
    IRInst* inst = ir_inst_new(op, type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, lhs);
    ir_inst_add_operand(inst, rhs);
    return inst;
}

/**
 * Create a unary operation (neg, not)
 */
IRInst* ir_inst_unary(IROp op, IRType* type, int dest, IRValue* operand) {
    IRInst* inst = ir_inst_new(op, type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, operand);
    return inst;
}

/**
 * Create a comparison instruction
 */
IRInst* ir_inst_cmp(IRCmpPred pred, int dest, IRValue* lhs, IRValue* rhs) {
    IRInst* inst = ir_inst_new(IR_OP_CMP, IR_TYPE_I1_T, dest);
    if (!inst) return NULL;
    inst->cmp_pred = pred;
    ir_inst_add_operand(inst, lhs);
    ir_inst_add_operand(inst, rhs);
    return inst;
}

/**
 * Create an alloca instruction
 */
IRInst* ir_inst_alloca(IRType* type, int dest) {
    IRType* ptr_type = ir_type_ptr(type);
    IRInst* inst = ir_inst_new(IR_OP_ALLOCA, ptr_type, dest);
    return inst;
}

/**
 * Create a load instruction
 */
IRInst* ir_inst_load(IRType* type, int dest, IRValue* ptr) {
    IRInst* inst = ir_inst_new(IR_OP_LOAD, type, dest);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, ptr);
    return inst;
}

/**
 * Create a store instruction
 */
IRInst* ir_inst_store(IRValue* value, IRValue* ptr) {
    IRInst* inst = ir_inst_new(IR_OP_STORE, IR_TYPE_VOID_T, -1);  // No destination
    if (!inst) return NULL;
    ir_inst_add_operand(inst, value);
    ir_inst_add_operand(inst, ptr);
    return inst;
}

/**
 * Create an unconditional branch
 */
IRInst* ir_inst_br(IRBlock* target) {
    IRInst* inst = ir_inst_new(IR_OP_BR, IR_TYPE_VOID_T, -1);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, ir_value_block(target));
    return inst;
}

/**
 * Create a conditional branch
 */
IRInst* ir_inst_br_cond(IRValue* cond, IRBlock* if_true, IRBlock* if_false) {
    IRInst* inst = ir_inst_new(IR_OP_BR_COND, IR_TYPE_VOID_T, -1);
    if (!inst) return NULL;
    ir_inst_add_operand(inst, cond);
    ir_inst_add_operand(inst, ir_value_block(if_true));
    ir_inst_add_operand(inst, ir_value_block(if_false));
    return inst;
}

/**
 * Create a return instruction
 */
IRInst* ir_inst_ret(IRValue* value) {
    IRType* type = value ? value->type : IR_TYPE_VOID_T;
    IRInst* inst = ir_inst_new(IR_OP_RET, type, -1);
    if (!inst) return NULL;
    if (value) {
        ir_inst_add_operand(inst, value);
    }
    return inst;
}

/**
 * Create a call instruction
 */
IRInst* ir_inst_call(const char* target, IRType* ret_type, int dest, 
                     IRValue** args, int arg_count) {
    IRInst* inst = ir_inst_new(IR_OP_CALL, ret_type, dest);
    if (!inst) return NULL;
    
    inst->call_target = target ? strdup(target) : NULL;
    inst->call_arg_count = arg_count;
    
    // Copy arguments
    if (arg_count > 0 && args) {
        inst->call_args = (IRValue**)malloc(arg_count * sizeof(IRValue*));
        if (inst->call_args) {
            for (int i = 0; i < arg_count; i++) {
                inst->call_args[i] = args[i];
            }
        }
    }
    
    return inst;
}

/**
 * Create a phi node
 */
IRInst* ir_inst_phi(IRType* type, int dest) {
    IRInst* inst = ir_inst_new(IR_OP_PHI, type, dest);
    if (!inst) return NULL;
    inst->phi_entries = NULL;  // Entries added later
    return inst;
}

/**
 * Add an incoming value to a phi node
 */
void ir_inst_phi_add(IRInst* phi, IRValue* value, IRBlock* block) {
    if (!phi || phi->op != IR_OP_PHI || !value || !block) return;
    
    IRPhiEntry* entry = (IRPhiEntry*)malloc(sizeof(IRPhiEntry));
    if (!entry) return;
    
    entry->value = value;
    entry->block = block;
    entry->next = phi->phi_entries;
    phi->phi_entries = entry;
}

/**
 * Free an instruction
 */
void ir_inst_free(IRInst* inst) {
    if (!inst) return;
    
    // Free operands
    for (int i = 0; i < inst->operand_count; i++) {
        ir_value_free(inst->operands[i]);
    }
    
    // Free phi entries
    IRPhiEntry* phi = inst->phi_entries;
    while (phi) {
        IRPhiEntry* next = phi->next;
        ir_value_free(phi->value);
        free(phi);
        phi = next;
    }
    
    // Free call data
    if (inst->call_target) {
        free(inst->call_target);
    }
    if (inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            ir_value_free(inst->call_args[i]);
        }
        free(inst->call_args);
    }
    
    free(inst);
}

// ============================================================================
// IR Block Construction
// ============================================================================

/**
 * Create a new basic block
 */
IRBlock* ir_block_new(const char* label, int id) {
    IRBlock* block = (IRBlock*)malloc(sizeof(IRBlock));
    if (!block) return NULL;
    
    block->label = label ? strdup(label) : NULL;
    block->id = id;
    block->first = NULL;
    block->last = NULL;
    block->inst_count = 0;
    block->succs[0] = NULL;
    block->succs[1] = NULL;
    block->succ_count = 0;
    block->preds = NULL;
    block->pred_count = 0;
    block->pred_capacity = 0;
    block->idom = NULL;
    block->dom_frontier = NULL;
    block->dom_frontier_count = 0;
    block->next = NULL;
    
    return block;
}

/**
 * Append an instruction to a block
 */
void ir_block_append(IRBlock* block, IRInst* inst) {
    if (!block || !inst) return;
    
    inst->prev = block->last;
    inst->next = NULL;
    
    if (block->last) {
        block->last->next = inst;
    } else {
        block->first = inst;
    }
    block->last = inst;
    block->inst_count++;
}

/**
 * Add a predecessor to a block
 */
void ir_block_add_pred(IRBlock* block, IRBlock* pred) {
    if (!block || !pred) return;
    
    // Grow array if needed
    if (block->pred_count >= block->pred_capacity) {
        int new_cap = block->pred_capacity == 0 ? 4 : block->pred_capacity * 2;
        IRBlock** new_preds = (IRBlock**)realloc(block->preds, new_cap * sizeof(IRBlock*));
        if (!new_preds) return;
        block->preds = new_preds;
        block->pred_capacity = new_cap;
    }
    
    block->preds[block->pred_count++] = pred;
}

/**
 * Add a successor to a block
 */
void ir_block_add_succ(IRBlock* block, IRBlock* succ) {
    if (!block || !succ || block->succ_count >= 2) return;
    block->succs[block->succ_count++] = succ;
    ir_block_add_pred(succ, block);
}

/**
 * Check if a block is terminated (ends with br, br_cond, or ret)
 */
int ir_block_is_terminated(IRBlock* block) {
    if (!block || !block->last) return 0;
    IROp op = block->last->op;
    return op == IR_OP_BR || op == IR_OP_BR_COND || op == IR_OP_RET;
}

/**
 * Free a basic block and its instructions
 */
void ir_block_free(IRBlock* block) {
    if (!block) return;
    
    // Free all instructions
    IRInst* inst = block->first;
    while (inst) {
        IRInst* next = inst->next;
        ir_inst_free(inst);
        inst = next;
    }
    
    // Free predecessors array
    if (block->preds) {
        free(block->preds);
    }
    
    // Free dominance frontier
    if (block->dom_frontier) {
        free(block->dom_frontier);
    }
    
    // Free label
    if (block->label) {
        free(block->label);
    }
    
    free(block);
}

// ============================================================================
// IR Function Construction
// ============================================================================

/**
 * Create a new function
 */
IRFunc* ir_func_new(const char* name, IRType* ret_type) {
    IRFunc* func = (IRFunc*)malloc(sizeof(IRFunc));
    if (!func) return NULL;
    
    func->name = name ? strdup(name) : NULL;
    func->ret_type = ret_type ? ret_type : IR_TYPE_VOID_T;
    func->params = NULL;
    func->param_count = 0;
    func->entry = NULL;
    func->blocks = NULL;
    func->block_count = 0;
    func->next_reg = 0;
    func->next_block_id = 0;
    func->is_prototype = false;
    func->next = NULL;
    
    return func;
}

/**
 * Allocate a new virtual register in a function
 */
int ir_func_alloc_reg(IRFunc* func) {
    if (!func) return -1;
    return func->next_reg++;
}

/**
 * Allocate a new block ID in a function
 */
int ir_func_alloc_block_id(IRFunc* func) {
    if (!func) return -1;
    return func->next_block_id++;
}

/**
 * Add a basic block to a function
 */
void ir_func_add_block(IRFunc* func, IRBlock* block) {
    if (!func || !block) return;
    
    if (!func->blocks) {
        func->blocks = block;
        func->entry = block;
    } else {
        // Find last block and append
        IRBlock* last = func->blocks;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
    }
    func->block_count++;
}

/**
 * Create and add a new block to a function
 */
IRBlock* ir_func_new_block(IRFunc* func, const char* label) {
    if (!func) return NULL;
    
    int id = ir_func_alloc_block_id(func);
    IRBlock* block = ir_block_new(label, id);
    if (!block) return NULL;
    
    ir_func_add_block(func, block);
    return block;
}

/**
 * Add a parameter to a function
 */
void ir_func_add_param(IRFunc* func, const char* name, IRType* type) {
    if (!func) return;
    
    // Create new param array
    IRParam* new_params = (IRParam*)realloc(func->params, 
                                            (func->param_count + 1) * sizeof(IRParam));
    if (!new_params) return;
    func->params = new_params;
    
    // Add parameter
    func->params[func->param_count].name = name ? strdup(name) : NULL;
    func->params[func->param_count].type = type;
    func->params[func->param_count].reg = ir_func_alloc_reg(func);
    func->param_count++;
}

/**
 * Free a function and all its blocks
 */
void ir_func_free(IRFunc* func) {
    if (!func) return;
    
    // Free all blocks
    IRBlock* block = func->blocks;
    while (block) {
        IRBlock* next = block->next;
        ir_block_free(block);
        block = next;
    }
    
    // Free parameters
    for (int i = 0; i < func->param_count; i++) {
        if (func->params[i].name) {
            free(func->params[i].name);
        }
    }
    if (func->params) {
        free(func->params);
    }
    
    // Free name
    if (func->name) {
        free(func->name);
    }
    
    free(func);
}

// ============================================================================
// IR Global Variable Construction
// ============================================================================

/**
 * Create a new global variable
 */
IRGlobal* ir_global_new(const char* name, IRType* type, int is_const) {
    IRGlobal* global = (IRGlobal*)malloc(sizeof(IRGlobal));
    if (!global) return NULL;
    
    global->name = name ? strdup(name) : NULL;
    global->type = type;
    global->init = NULL;
    global->is_const = is_const ? true : false;
    global->next = NULL;
    
    return global;
}

/**
 * Set initializer for a global
 */
void ir_global_set_init(IRGlobal* global, IRValue* init) {
    if (!global) return;
    global->init = init;
}

/**
 * Free a global variable
 */
void ir_global_free(IRGlobal* global) {
    if (!global) return;
    
    if (global->init) {
        ir_value_free(global->init);
    }
    if (global->name) {
        free(global->name);
    }
    free(global);
}

// ============================================================================
// IR Module Construction
// ============================================================================

/**
 * Create a new module
 */
IRModule* ir_module_new(const char* name) {
    IRModule* module = (IRModule*)malloc(sizeof(IRModule));
    if (!module) return NULL;
    
    module->name = name ? strdup(name) : NULL;
    module->globals = NULL;
    module->global_count = 0;
    module->funcs = NULL;
    module->func_count = 0;
    module->strings = NULL;
    module->string_count = 0;
    
    return module;
}

/**
 * Add a function to a module
 */
void ir_module_add_func(IRModule* module, IRFunc* func) {
    if (!module || !func) return;
    
    if (!module->funcs) {
        module->funcs = func;
    } else {
        IRFunc* last = module->funcs;
        while (last->next) {
            last = last->next;
        }
        last->next = func;
    }
    module->func_count++;
}

/**
 * Add a global variable to a module
 */
void ir_module_add_global(IRModule* module, IRGlobal* global) {
    if (!module || !global) return;
    
    if (!module->globals) {
        module->globals = global;
    } else {
        IRGlobal* last = module->globals;
        while (last->next) {
            last = last->next;
        }
        last->next = global;
    }
    module->global_count++;
}

/**
 * Add a string constant to a module (returns string ID)
 */
int ir_module_add_string(IRModule* module, const char* str) {
    if (!module || !str) return -1;
    
    // Check if string already exists
    IRStringEntry* entry = module->strings;
    while (entry) {
        if (entry->content && strcmp(entry->content, str) == 0) {
            return entry->id;
        }
        entry = entry->next;
    }
    
    // Create new entry
    IRStringEntry* new_entry = (IRStringEntry*)malloc(sizeof(IRStringEntry));
    if (!new_entry) return -1;
    
    new_entry->id = module->string_count++;
    new_entry->content = strdup(str);
    new_entry->next = module->strings;
    module->strings = new_entry;
    
    return new_entry->id;
}

/**
 * Find a function by name
 */
IRFunc* ir_module_find_func(IRModule* module, const char* name) {
    if (!module || !name) return NULL;
    
    IRFunc* func = module->funcs;
    while (func) {
        if (func->name && strcmp(func->name, name) == 0) {
            return func;
        }
        func = func->next;
    }
    return NULL;
}

/**
 * Find a global by name
 */
IRGlobal* ir_module_find_global(IRModule* module, const char* name) {
    if (!module || !name) return NULL;
    
    IRGlobal* global = module->globals;
    while (global) {
        if (global->name && strcmp(global->name, name) == 0) {
            return global;
        }
        global = global->next;
    }
    return NULL;
}

/**
 * Get a string by ID
 */
const char* ir_module_get_string(IRModule* module, int id) {
    if (!module) return NULL;
    
    IRStringEntry* entry = module->strings;
    while (entry) {
        if (entry->id == id) {
            return entry->content;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * Free a module and all its contents
 */
void ir_module_free(IRModule* module) {
    if (!module) return;
    
    // Free functions
    IRFunc* func = module->funcs;
    while (func) {
        IRFunc* next = func->next;
        ir_func_free(func);
        func = next;
    }
    
    // Free globals
    IRGlobal* global = module->globals;
    while (global) {
        IRGlobal* next = global->next;
        ir_global_free(global);
        global = next;
    }
    
    // Free string table
    IRStringEntry* str = module->strings;
    while (str) {
        IRStringEntry* next = str->next;
        if (str->content) free(str->content);
        free(str);
        str = next;
    }
    
    // Free name
    if (module->name) {
        free(module->name);
    }
    
    free(module);
}

// ============================================================================
// IR Printing (for debugging with --dump-ir)
// ============================================================================

/**
 * Print a value
 */
void ir_value_print(IRValue* val, FILE* out, int use_arabic) {
    if (!val) {
        fprintf(out, "null");
        return;
    }
    
    char num_buf[64];
    
    switch (val->kind) {
        case IR_VAL_NONE:
            fprintf(out, "void");
            break;
            
        case IR_VAL_REG:
            fprintf(out, "%%");
            if (use_arabic) {
                fprintf(out, "م%s", int_to_arabic_numerals(val->data.reg_num, num_buf));
            } else {
                fprintf(out, "r%d", val->data.reg_num);
            }
            break;
            
        case IR_VAL_CONST_INT:
            if (use_arabic) {
                fprintf(out, "%s", int_to_arabic_numerals((int)val->data.const_int, num_buf));
            } else {
                fprintf(out, "%lld", (long long)val->data.const_int);
            }
            break;
            
        case IR_VAL_CONST_STR:
            if (use_arabic) {
                fprintf(out, "@نص%s", int_to_arabic_numerals(val->data.const_str.id, num_buf));
            } else {
                fprintf(out, "@str%d", val->data.const_str.id);
            }
            break;
            
        case IR_VAL_BLOCK:
            if (val->data.block && val->data.block->label) {
                fprintf(out, ":%s", val->data.block->label);
            } else if (val->data.block) {
                fprintf(out, ":block%d", val->data.block->id);
            } else {
                fprintf(out, ":???");
            }
            break;
            
        case IR_VAL_FUNC:
        case IR_VAL_GLOBAL:
            if (val->data.global_name) {
                fprintf(out, "@%s", val->data.global_name);
            } else {
                fprintf(out, "@???");
            }
            break;
            
        default:
            fprintf(out, "???");
    }
}

/**
 * Print an instruction
 */
void ir_inst_print(IRInst* inst, FILE* out, int use_arabic) {
    if (!inst) return;
    
    char num_buf[64];
    
    // Print destination if it exists
    if (inst->dest >= 0) {
        fprintf(out, "    %%");
        if (use_arabic) {
            fprintf(out, "م%s", int_to_arabic_numerals(inst->dest, num_buf));
        } else {
            fprintf(out, "r%d", inst->dest);
        }
        fprintf(out, " = ");
    } else {
        fprintf(out, "    ");
    }
    
    // Print opcode
    if (use_arabic) {
        fprintf(out, "%s ", ir_op_to_arabic(inst->op));
    } else {
        fprintf(out, "%s ", ir_op_to_english(inst->op));
    }
    
    // Print type for typed operations
    if (inst->type && inst->type->kind != IR_TYPE_VOID && 
        inst->op != IR_OP_BR && inst->op != IR_OP_BR_COND) {
        if (use_arabic) {
            fprintf(out, "%s ", ir_type_to_arabic(inst->type));
        } else {
            fprintf(out, "%s ", ir_type_to_english(inst->type));
        }
    }
    
    // Print comparison predicate for CMP
    if (inst->op == IR_OP_CMP) {
        if (use_arabic) {
            fprintf(out, "%s ", ir_cmp_pred_to_arabic(inst->cmp_pred));
        } else {
            fprintf(out, "%s ", ir_cmp_pred_to_english(inst->cmp_pred));
        }
    }
    
    // Print operands for most instructions
    if (inst->op != IR_OP_CALL && inst->op != IR_OP_PHI) {
        for (int i = 0; i < inst->operand_count; i++) {
            if (i > 0) fprintf(out, ", ");
            ir_value_print(inst->operands[i], out, use_arabic);
        }
    }
    
    // Special handling for call
    if (inst->op == IR_OP_CALL) {
        fprintf(out, "@%s(", inst->call_target ? inst->call_target : "???");
        for (int i = 0; i < inst->call_arg_count; i++) {
            if (i > 0) fprintf(out, ", ");
            if (inst->call_args && inst->call_args[i]) {
                ir_value_print(inst->call_args[i], out, use_arabic);
            }
        }
        fprintf(out, ")");
    }
    
    // Special handling for phi
    if (inst->op == IR_OP_PHI) {
        IRPhiEntry* entry = inst->phi_entries;
        while (entry) {
            fprintf(out, " [");
            ir_value_print(entry->value, out, use_arabic);
            fprintf(out, ", ");
            if (entry->block && entry->block->label) {
                fprintf(out, ":%s", entry->block->label);
            } else if (entry->block) {
                fprintf(out, ":block%d", entry->block->id);
            }
            fprintf(out, "]");
            entry = entry->next;
        }
    }
    
    fprintf(out, "\n");
}

/**
 * Print a basic block
 */
void ir_block_print(IRBlock* block, FILE* out, int use_arabic) {
    if (!block) return;
    
    // Print label
    if (block->label) {
        fprintf(out, "%s:\n", block->label);
    } else {
        fprintf(out, "block%d:\n", block->id);
    }
    
    // Print instructions
    IRInst* inst = block->first;
    while (inst) {
        ir_inst_print(inst, out, use_arabic);
        inst = inst->next;
    }
}

/**
 * Print a function
 */
void ir_func_print(IRFunc* func, FILE* out, int use_arabic) {
    if (!func) return;
    
    char num_buf[64];
    
    // Print function header
    if (use_arabic) {
        fprintf(out, "دالة %s(", func->name ? func->name : "???");
    } else {
        fprintf(out, "func %s(", func->name ? func->name : "???");
    }
    
    // Print parameters
    for (int i = 0; i < func->param_count; i++) {
        if (i > 0) fprintf(out, ", ");
        if (use_arabic) {
            fprintf(out, "%s %%م%s", 
                    ir_type_to_arabic(func->params[i].type),
                    int_to_arabic_numerals(func->params[i].reg, num_buf));
        } else {
            fprintf(out, "%s %%r%d", 
                    ir_type_to_english(func->params[i].type),
                    func->params[i].reg);
        }
    }
    
    fprintf(out, ") -> ");
    if (use_arabic) {
        fprintf(out, "%s", ir_type_to_arabic(func->ret_type));
    } else {
        fprintf(out, "%s", ir_type_to_english(func->ret_type));
    }
    
    if (func->is_prototype) {
        fprintf(out, ";\n\n");
        return;
    }
    
    fprintf(out, " {\n");
    
    // Print blocks
    IRBlock* block = func->blocks;
    while (block) {
        ir_block_print(block, out, use_arabic);
        block = block->next;
    }
    
    fprintf(out, "}\n\n");
}

/**
 * Print a global variable
 */
void ir_global_print(IRGlobal* global, FILE* out, int use_arabic) {
    if (!global) return;
    
    fprintf(out, "@%s: ", global->name ? global->name : "???");
    
    if (use_arabic) {
        fprintf(out, "%s", ir_type_to_arabic(global->type));
        if (global->is_const) fprintf(out, " ثابت");
    } else {
        fprintf(out, "%s", ir_type_to_english(global->type));
        if (global->is_const) fprintf(out, " const");
    }
    
    if (global->init) {
        fprintf(out, " = ");
        ir_value_print(global->init, out, use_arabic);
    }
    
    fprintf(out, "\n");
}

/**
 * Print entire module
 */
void ir_module_print(IRModule* module, FILE* out, int use_arabic) {
    if (!module) return;
    
    if (use_arabic) {
        fprintf(out, ";; نواة باء - %s\n\n", module->name ? module->name : "وحدة");
    } else {
        fprintf(out, ";; Baa IR - %s\n\n", module->name ? module->name : "module");
    }
    
    // Print string table
    IRStringEntry* str = module->strings;
    if (str) {
        char num_buf[64];
        if (use_arabic) {
            fprintf(out, ";; جدول النصوص\n");
        } else {
            fprintf(out, ";; String Table\n");
        }
        while (str) {
            if (use_arabic) {
                fprintf(out, "@نص%s = \"%s\"\n", 
                        int_to_arabic_numerals(str->id, num_buf), 
                        str->content ? str->content : "");
            } else {
                fprintf(out, "@str%d = \"%s\"\n", str->id, 
                        str->content ? str->content : "");
            }
            str = str->next;
        }
        fprintf(out, "\n");
    }
    
    // Print globals
    IRGlobal* global = module->globals;
    if (global) {
        if (use_arabic) {
            fprintf(out, ";; المتغيرات العامة\n");
        } else {
            fprintf(out, ";; Global Variables\n");
        }
        while (global) {
            ir_global_print(global, out, use_arabic);
            global = global->next;
        }
        fprintf(out, "\n");
    }
    
    // Print functions
    IRFunc* func = module->funcs;
    while (func) {
        ir_func_print(func, out, use_arabic);
        func = func->next;
    }
}

/**
 * Dump module to file (convenience wrapper)
 */
void ir_module_dump(IRModule* module, const char* filename, int use_arabic) {
    FILE* out = fopen(filename, "w");
    if (!out) {
        fprintf(stderr, "خطأ: لا يمكن فتح الملف %s\n", filename);
        return;
    }
    ir_module_print(module, out, use_arabic);
    fclose(out);
}