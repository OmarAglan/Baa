/**
 * @file ir_cse.c
 * @brief IR common subexpression elimination pass (حذف_المكرر).
 * @version 0.3.1.5
 *
 * هدف التمريرة:
 * - اكتشاف التعبيرات المتكررة (نفس العملية + نفس المعاملات)
 * - استبدال الاستعمالات اللاحقة بالنتيجة الأولى
 * - حذف التعليمات المكررة
 *
 * ملاحظات تصميمية:
 * - نستخدم جدول تجزئة (hash table) لتتبع التعبيرات
 * - التمريرة محلية لكل دالة (السجلات محلية)
 * - العمليات الصالحة: الحسابية، المقارنات، المنطقية
 * - غير صالحة: الذاكرة، التحكم، الاستدعاء (لها تأثيرات جانبية)
 */

#include "ir_cse.h"

#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// IRPass integration
// -----------------------------------------------------------------------------

IRPass IR_PASS_CSE = {
    .name = "حذف_المكرر",
    .run = ir_cse_run
};

// -----------------------------------------------------------------------------
// Hash table constants
// -----------------------------------------------------------------------------

#define CSE_HASH_SIZE 256

// -----------------------------------------------------------------------------
// Expression entry for hash table
// -----------------------------------------------------------------------------

typedef struct CSEEntry {
    IROp op;                    // Opcode
    IRCmpPred cmp_pred;         // For comparison ops
    int operand_count;
    int64_t operand_vals[4];    // Operand signatures (reg num or const value)
    int operand_kinds[4];       // IR_VAL_REG or IR_VAL_CONST_INT etc.
    int result_reg;             // Register holding the result
    struct CSEEntry* next;      // Chain for collisions
} CSEEntry;

// -----------------------------------------------------------------------------
// Helpers: determine if operation is eligible for CSE
// -----------------------------------------------------------------------------

static int ir_op_is_cse_eligible(IROp op) {
    switch (op) {
        // Arithmetic operations (pure)
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_MOD:
        case IR_OP_NEG:
        // Comparison operations (pure)
        case IR_OP_CMP:
        // Logical operations (pure)
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_NOT:
            return 1;
        
        // Not eligible: memory ops, control flow, calls, phi, copy
        default:
            return 0;
    }
}

// -----------------------------------------------------------------------------
// Helpers: compute operand signature
// -----------------------------------------------------------------------------

static int64_t ir_value_signature(IRValue* v, int* kind_out) {
    if (!v) {
        *kind_out = IR_VAL_NONE;
        return 0;
    }
    
    *kind_out = (int)v->kind;
    
    switch (v->kind) {
        case IR_VAL_CONST_INT:
            return v->data.const_int;
        case IR_VAL_REG:
            return (int64_t)v->data.reg_num;
        case IR_VAL_GLOBAL:
            // Use pointer as signature (same string = same pointer typically)
            return (int64_t)(uintptr_t)v->data.global_name;
        default:
            return 0;
    }
}

// -----------------------------------------------------------------------------
// Helpers: compute hash for expression
// -----------------------------------------------------------------------------

static unsigned int cse_hash(IROp op, IRCmpPred pred, int64_t* vals, int* kinds, int count) {
    unsigned int h = (unsigned int)op * 31;
    h ^= (unsigned int)pred * 17;
    
    for (int i = 0; i < count; i++) {
        h = h * 37 + (unsigned int)(vals[i] & 0xFFFFFFFF);
        h = h * 41 + (unsigned int)kinds[i];
    }
    
    return h % CSE_HASH_SIZE;
}

// -----------------------------------------------------------------------------
// Helpers: check if two entries match
// -----------------------------------------------------------------------------

static int cse_entries_match(CSEEntry* a, CSEEntry* b) {
    if (a->op != b->op) return 0;
    if (a->op == IR_OP_CMP && a->cmp_pred != b->cmp_pred) return 0;
    if (a->operand_count != b->operand_count) return 0;
    
    for (int i = 0; i < a->operand_count; i++) {
        if (a->operand_kinds[i] != b->operand_kinds[i]) return 0;
        if (a->operand_vals[i] != b->operand_vals[i]) return 0;
    }
    
    return 1;
}

// -----------------------------------------------------------------------------
// Helpers: find or insert expression in hash table
// -----------------------------------------------------------------------------

static int cse_lookup_or_insert(CSEEntry** table, CSEEntry* query, int* found_reg) {
    unsigned int h = cse_hash(query->op, query->cmp_pred, 
                              query->operand_vals, query->operand_kinds, 
                              query->operand_count);
    
    // Search chain
    for (CSEEntry* e = table[h]; e; e = e->next) {
        if (cse_entries_match(e, query)) {
            *found_reg = e->result_reg;
            return 1; // Found existing
        }
    }
    
    // Insert new entry
    CSEEntry* new_entry = (CSEEntry*)malloc(sizeof(CSEEntry));
    if (!new_entry) return 0;
    
    *new_entry = *query;
    new_entry->next = table[h];
    table[h] = new_entry;
    
    return 0; // Not found, inserted
}

// -----------------------------------------------------------------------------
// Helpers: free hash table
// -----------------------------------------------------------------------------

static void cse_table_free(CSEEntry** table) {
    if (!table) return;
    
    for (int i = 0; i < CSE_HASH_SIZE; i++) {
        CSEEntry* e = table[i];
        while (e) {
            CSEEntry* next = e->next;
            free(e);
            e = next;
        }
    }
    
    free(table);
}

// -----------------------------------------------------------------------------
// Helpers: replace register uses in instruction
// -----------------------------------------------------------------------------

static int cse_replace_reg_uses(IRInst* inst, int old_reg, int new_reg, IRType* type) {
    int replaced = 0;
    
    // Replace in operands
    for (int i = 0; i < inst->operand_count; i++) {
        IRValue* v = inst->operands[i];
        if (v && v->kind == IR_VAL_REG && v->data.reg_num == old_reg) {
            v->data.reg_num = new_reg;
            replaced = 1;
        }
    }
    
    // Replace in call args
    if (inst->op == IR_OP_CALL && inst->call_args) {
        for (int i = 0; i < inst->call_arg_count; i++) {
            IRValue* v = inst->call_args[i];
            if (v && v->kind == IR_VAL_REG && v->data.reg_num == old_reg) {
                v->data.reg_num = new_reg;
                replaced = 1;
            }
        }
    }
    
    // Replace in phi entries
    if (inst->op == IR_OP_PHI) {
        for (IRPhiEntry* e = inst->phi_entries; e; e = e->next) {
            if (e->value && e->value->kind == IR_VAL_REG && 
                e->value->data.reg_num == old_reg) {
                e->value->data.reg_num = new_reg;
                replaced = 1;
            }
        }
    }
    
    return replaced;
}

// -----------------------------------------------------------------------------
// Helpers: remove instruction from block
// -----------------------------------------------------------------------------

static void ir_block_remove_inst(IRBlock* block, IRInst* inst) {
    if (!block || !inst) return;

    if (inst->prev) inst->prev->next = inst->next;
    else block->first = inst->next;

    if (inst->next) inst->next->prev = inst->prev;
    else block->last = inst->prev;

    inst->prev = NULL;
    inst->next = NULL;

    if (block->inst_count > 0) block->inst_count--;
    ir_inst_free(inst);
}

// -----------------------------------------------------------------------------
// Core: per-function CSE
// -----------------------------------------------------------------------------

static int ir_cse_func(IRFunc* func) {
    if (!func || func->is_prototype) return 0;
    if (!func->entry) return 0;
    
    // Allocate hash table
    CSEEntry** table = (CSEEntry**)calloc(CSE_HASH_SIZE, sizeof(CSEEntry*));
    if (!table) return 0;
    
    int changed = 0;
    
    // Collect replacements: old_reg -> new_reg
    int max_reg = func->next_reg;
    int* replacements = (int*)calloc((size_t)max_reg, sizeof(int));
    if (!replacements) {
        cse_table_free(table);
        return 0;
    }
    for (int i = 0; i < max_reg; i++) {
        replacements[i] = -1; // No replacement
    }
    
    // Pass 1: Find duplicate expressions
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            // Skip ineligible operations
            if (!ir_op_is_cse_eligible(inst->op)) continue;
            if (inst->dest < 0) continue; // Must produce a value
            
            // Build query entry
            CSEEntry query = {0};
            query.op = inst->op;
            query.cmp_pred = inst->cmp_pred;
            query.operand_count = inst->operand_count;
            query.result_reg = inst->dest;
            
            for (int i = 0; i < inst->operand_count && i < 4; i++) {
                query.operand_vals[i] = ir_value_signature(inst->operands[i], 
                                                           &query.operand_kinds[i]);
            }
            
            // Lookup or insert
            int existing_reg;
            if (cse_lookup_or_insert(table, &query, &existing_reg)) {
                // Found duplicate! Mark for replacement
                replacements[inst->dest] = existing_reg;
                changed = 1;
            }
        }
    }
    
    // Pass 2: Replace uses of duplicate registers
    for (IRBlock* b = func->blocks; b; b = b->next) {
        for (IRInst* inst = b->first; inst; inst = inst->next) {
            for (int r = 0; r < max_reg; r++) {
                if (replacements[r] >= 0) {
                    cse_replace_reg_uses(inst, r, replacements[r], inst->type);
                }
            }
        }
    }
    
    // Pass 3: Remove duplicate instructions
    for (IRBlock* b = func->blocks; b; b = b->next) {
        IRInst* inst = b->first;
        while (inst) {
            IRInst* next = inst->next;
            
            if (inst->dest >= 0 && inst->dest < max_reg && 
                replacements[inst->dest] >= 0) {
                ir_block_remove_inst(b, inst);
            }
            
            inst = next;
        }
    }
    
    // Cleanup
    free(replacements);
    cse_table_free(table);
    
    return changed;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool ir_cse_run(IRModule* module) {
    if (!module) return false;

    int changed = 0;
    for (IRFunc* f = module->funcs; f; f = f->next) {
        changed |= ir_cse_func(f);
    }

    return changed ? true : false;
}
