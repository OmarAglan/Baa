/**
 * @file ir.h
 * @brief Baa Intermediate Representation (IR) Definitions
 * @version 0.3.0
 * 
 * This file defines the data structures for the Baa IR, which uses
 * SSA (Static Single Assignment) form with Arabic naming conventions.
 * 
 * See docs/BAA_IR_SPECIFICATION.md for the full specification.
 */

#ifndef BAA_IR_H
#define BAA_IR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ir_arena.h"

// إعلان مسبق لتجنب دورات تضمين بين ir.h و ir_defuse.h
typedef struct IRDefUse IRDefUse;

// ============================================================================
// IR Opcodes (أكواد العمليات)
// ============================================================================

/**
 * @enum IROp
 * @brief All IR instruction opcodes.
 * 
 * Arabic names in comments correspond to the textual IR format.
 */
typedef enum {
    // --------------------------------------------------------------------
    // Arithmetic Operations (العمليات الحسابية)
    // --------------------------------------------------------------------
    IR_OP_ADD,      // جمع - Addition
    IR_OP_SUB,      // طرح - Subtraction
    IR_OP_MUL,      // ضرب - Multiplication
    IR_OP_DIV,      // قسم - Signed division
    IR_OP_MOD,      // باقي - Modulo (remainder)
    IR_OP_NEG,      // سالب - Negation (unary minus)
    
    // --------------------------------------------------------------------
    // Memory Operations (عمليات الذاكرة)
    // --------------------------------------------------------------------
    IR_OP_ALLOCA,   // حجز - Stack allocation
    IR_OP_LOAD,     // حمل - Load from memory
    IR_OP_STORE,    // خزن - Store to memory
    
    // --------------------------------------------------------------------
    // Comparison Operations (عمليات المقارنة)
    // --------------------------------------------------------------------
    IR_OP_CMP,      // قارن - Compare (with predicate)
    
    // --------------------------------------------------------------------
    // Logical Operations (العمليات المنطقية)
    // --------------------------------------------------------------------
    IR_OP_AND,      // و - Bitwise AND
    IR_OP_OR,       // أو - Bitwise OR
    IR_OP_NOT,      // نفي - Bitwise NOT
    
    // --------------------------------------------------------------------
    // Control Flow Operations (عمليات التحكم)
    // --------------------------------------------------------------------
    IR_OP_BR,       // قفز - Unconditional branch
    IR_OP_BR_COND,  // قفز_شرط - Conditional branch
    IR_OP_RET,      // رجوع - Return from function
    IR_OP_CALL,     // نداء - Function call
    
    // --------------------------------------------------------------------
    // SSA Operations (عمليات SSA)
    // --------------------------------------------------------------------
    IR_OP_PHI,      // فاي - Phi node for SSA
    IR_OP_COPY,     // نسخ - Copy (for SSA construction)
    
    // --------------------------------------------------------------------
    // Type Conversion (تحويل الأنواع)
    // --------------------------------------------------------------------
    IR_OP_CAST,     // تحويل - Type cast/conversion
    
    // --------------------------------------------------------------------
    // Special Operations
    // --------------------------------------------------------------------
    IR_OP_NOP,      // No operation (placeholder)
    
    IR_OP_COUNT     // Total number of opcodes
} IROp;

// ============================================================================
// Comparison Predicates (مقارنات)
// ============================================================================

/**
 * @enum IRCmpPred
 * @brief Comparison predicates for IR_OP_CMP instruction.
 */
typedef enum {
    IR_CMP_EQ,      // يساوي - Equal
    IR_CMP_NE,      // لا_يساوي - Not equal
    IR_CMP_GT,      // أكبر - Greater than (signed)
    IR_CMP_LT,      // أصغر - Less than (signed)
    IR_CMP_GE,      // أكبر_أو_يساوي - Greater or equal (signed)
    IR_CMP_LE,      // أصغر_أو_يساوي - Less or equal (signed)
} IRCmpPred;

// ============================================================================
// IR Types (الأنواع)
// ============================================================================

/**
 * @enum IRTypeKind
 * @brief Kind of IR type.
 */
typedef enum {
    IR_TYPE_VOID,   // فراغ - Void (no value)
    IR_TYPE_I1,     // ص١ - 1-bit integer (boolean)
    IR_TYPE_I8,     // ص٨ - 8-bit integer (byte/char)
    IR_TYPE_I16,    // ص١٦ - 16-bit integer
    IR_TYPE_I32,    // ص٣٢ - 32-bit integer
    IR_TYPE_I64,    // ص٦٤ - 64-bit integer (primary)
    IR_TYPE_PTR,    // مؤشر - Pointer type
    IR_TYPE_ARRAY,  // مصفوفة - Array type
    IR_TYPE_FUNC,   // دالة - Function type
} IRTypeKind;

/**
 * @struct IRType
 * @brief Represents an IR type.
 * 
 * For simple types (i1, i8, i32, i64, void), only `kind` is used.
 * For pointers, `pointee` contains the pointed-to type.
 * For arrays, `element` is the element type and `count` is the size.
 */
typedef struct IRType {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IRTypeKind kind;
    union {
        struct IRType* pointee;     // For IR_TYPE_PTR
        struct {
            struct IRType* element; // For IR_TYPE_ARRAY
            int count;
        } array;
        struct {
            struct IRType* ret;     // For IR_TYPE_FUNC
            struct IRType** params;
            int param_count;
        } func;
    } data;
} IRType;

// ============================================================================
// IR Values and Operands (القيم والمعاملات)
// ============================================================================

/**
 * @enum IRValueKind
 * @brief Discriminator for IRValue union.
 */
typedef enum {
    IR_VAL_NONE,        // No value (void)
    IR_VAL_CONST_INT,   // Constant integer
    IR_VAL_CONST_STR,   // Constant string (pointer to .rdata)
    IR_VAL_REG,         // Virtual register (%م<n>)
    IR_VAL_GLOBAL,      // Global variable (@name)
    IR_VAL_FUNC,        // Function reference (@name)
    IR_VAL_BLOCK,       // Basic block reference
} IRValueKind;

/**
 * @struct IRValue
 * @brief Represents a value/operand in IR instructions.
 */
typedef struct IRValue {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IRValueKind kind;
    IRType* type;
    union {
        int64_t const_int;          // For IR_VAL_CONST_INT
        struct {
            char* data;             // For IR_VAL_CONST_STR
            int id;                 // String table ID
        } const_str;
        int reg_num;                // For IR_VAL_REG (%م<reg_num>)
        char* global_name;          // For IR_VAL_GLOBAL and IR_VAL_FUNC
        struct IRBlock* block;      // For IR_VAL_BLOCK
    } data;
} IRValue;

// ============================================================================
// IR Instructions (التعليمات)
// ============================================================================

/**
 * @struct IRPhiEntry
 * @brief A single entry in a Phi node: [value, predecessor_block]
 */
typedef struct IRPhiEntry {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IRValue* value;
    struct IRBlock* block;
    struct IRPhiEntry* next;
} IRPhiEntry;

/**
 * @struct IRInst
 * @brief Represents a single IR instruction.
 * 
 * Instructions are stored as a doubly-linked list within a basic block.
 */
typedef struct IRInst {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    IROp op;                    // Opcode
    IRType* type;               // Result type (or void for stores/branches)

    // معرّف ثابت للتعليمة داخل الدالة (للتشخيص/الاختبارات)
    int id;
    
    // Destination register (for instructions that produce a value)
    int dest;                   // -1 if no destination
    
    // Operands (meaning depends on opcode)
    IRValue* operands[4];       // Up to 4 operands
    int operand_count;
    
    // For comparison instructions
    IRCmpPred cmp_pred;
    
    // For Phi nodes
    IRPhiEntry* phi_entries;    // Linked list of [value, block] pairs
    
    // For calls
    char* call_target;          // Function name
    IRValue** call_args;        // Argument list
    int call_arg_count;
    
    // Source location (for debugging)
    const char* src_file;
    int src_line;
    int src_col;

    // الأب (الكتلة التي تحتوي التعليمة)
    struct IRBlock* parent;
    
    // Linked list within block
    struct IRInst* prev;
    struct IRInst* next;
} IRInst;

// ============================================================================
// Basic Blocks (الكتل الأساسية)
// ============================================================================

/**
 * @struct IRBlock
 * @brief Represents a basic block in the CFG.
 * 
 * A basic block is a sequence of instructions with:
 * - A unique label
 * - Linear execution (no internal branches)
 * - A single terminating instruction (branch or return)
 */
typedef struct IRBlock {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* label;                // Block label (Arabic name like "بداية")
    int id;                     // Numeric ID for internal use

    // الأب (الدالة التي تحتوي الكتلة)
    struct IRFunc* parent;
    
    // Instructions (doubly-linked list)
    IRInst* first;
    IRInst* last;
    int inst_count;
    
    // Control flow graph edges
    struct IRBlock* succs[2];   // Successors (0-2 for br/br_cond)
    int succ_count;
    
    struct IRBlock** preds;     // Predecessors (dynamic array)
    int pred_count;
    int pred_capacity;
    
    // For dominance analysis
    struct IRBlock* idom;       // Immediate dominator
    struct IRBlock** dom_frontier;
    int dom_frontier_count;
    
    // Linked list of blocks in function
    struct IRBlock* next;
} IRBlock;

// ============================================================================
// IR Functions (الدوال)
// ============================================================================

/**
 * @struct IRParam
 * @brief Represents a function parameter.
 */
typedef struct IRParam {
    char* name;                 // Parameter name
    IRType* type;               // Parameter type
    int reg;                    // Virtual register assigned (%معامل<n>)
} IRParam;

/**
 * @struct IRFunc
 * @brief Represents a function in the IR.
 */
typedef struct IRFunc {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* name;                 // Function name
    IRType* ret_type;           // Return type
    
    // Parameters
    IRParam* params;
    int param_count;
    
    // Basic blocks (linked list)
    IRBlock* entry;             // Entry block (first block)
    IRBlock* blocks;            // All blocks
    int block_count;
    
    // Virtual register counter
    int next_reg;               // Next available %م<n>

    // عدّاد معرفات التعليمات
    int next_inst_id;

    // عدّاد تغييرات IR داخل الدالة (لتبطل التحليلات مثل Def-Use)
    uint32_t ir_epoch;

    // كاش Def-Use (تحليل) — يُخصّص على heap ويُعاد بناؤه عند التغييرات.
    IRDefUse* def_use;
    
    // Block ID counter
    int next_block_id;
    
    // Is this a prototype (declaration without body)?
    bool is_prototype;
    
    // Linked list of functions in module
    struct IRFunc* next;
} IRFunc;

// ============================================================================
// IR Module (الوحدة)
// ============================================================================

/**
 * @struct IRGlobal
 * @brief Represents a global variable.
 */
typedef struct IRGlobal {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* name;                 // Variable name
    IRType* type;               // Variable type
    IRValue* init;              // Initial value (or NULL)
    bool is_const;              // Is this a constant?
    struct IRGlobal* next;
} IRGlobal;

/**
 * @struct IRStringEntry
 * @brief Represents a string literal in the string table.
 */
typedef struct IRStringEntry {
    // ملاحظة: هذه البنية تُخصَّص داخل ساحة IR (Arena) وتُحرَّر دفعة واحدة.
    char* content;              // String content (UTF-8)
    int id;                     // Unique ID (.Lstr_<id>)
    struct IRStringEntry* next;
} IRStringEntry;

/**
 * @struct IRModule
 * @brief Represents a complete IR module (translation unit).
 */
typedef struct IRModule {
    // ملاحظة: هذه البنية تُخصَّص بـ malloc وتحتوي على ساحة IR.
    char* name;                 // Module name (source file)

    // ساحة ذاكرة IR: كل كائنات IR تُخصَّص منها.
    IRArena arena;

    // كاش داخلي لأنواع شائعة لكل وحدة
    IRType* cached_i8_ptr_type;
    
    // Global variables
    IRGlobal* globals;
    int global_count;
    
    // Functions
    IRFunc* funcs;
    int func_count;
    
    // String table
    IRStringEntry* strings;
    int string_count;
} IRModule;

// ============================================================================
// سياق الساحة (Arena) للوحدة الحالية
// ============================================================================

void ir_module_set_current(IRModule* module);
IRModule* ir_module_get_current(void);

// ============================================================================
// Predefined Types (أنواع محددة مسبقاً)
// ============================================================================

extern IRType* IR_TYPE_VOID_T;
extern IRType* IR_TYPE_I1_T;
extern IRType* IR_TYPE_I8_T;
extern IRType* IR_TYPE_I16_T;
extern IRType* IR_TYPE_I32_T;
extern IRType* IR_TYPE_I64_T;

// ============================================================================
// Arabic Opcode Names (for printing)
// ============================================================================

/**
 * @brief Get the Arabic name for an IR opcode.
 * @param op The opcode.
 * @return Arabic string representation.
 */
const char* ir_op_to_arabic(IROp op);

/**
 * @brief Get the Arabic name for a comparison predicate.
 * @param pred The predicate.
 * @return Arabic string representation.
 */
const char* ir_cmp_pred_to_arabic(IRCmpPred pred);

/**
 * @brief Get the Arabic name for a type.
 * @param type The type.
 * @return Arabic string representation.
 */
const char* ir_type_to_arabic(IRType* type);

// ============================================================================
// Arabic Numeral Conversion (for register printing)
// ============================================================================

/**
 * @brief Convert an integer to Arabic numerals (٠١٢٣٤٥٦٧٨٩).
 * @param n The number to convert.
 * @param buf Output buffer (must be large enough).
 * @return Pointer to buf.
 */
char* int_to_arabic_numerals(int n, char* buf);

// ============================================================================
// Type Construction Functions
// ============================================================================

IRType* ir_type_ptr(IRType* pointee);
IRType* ir_type_array(IRType* element, int count);
IRType* ir_type_func(IRType* ret, IRType** params, int param_count);
void ir_type_free(IRType* type);
int ir_types_equal(IRType* a, IRType* b);
int ir_type_bits(IRType* type);

// ============================================================================
// Value Construction Functions
// ============================================================================

IRValue* ir_value_reg(int reg_num, IRType* type);
IRValue* ir_value_const_int(int64_t value, IRType* type);
IRValue* ir_value_const_str(const char* str, int id);
IRValue* ir_value_block(IRBlock* block);
IRValue* ir_value_global(const char* name, IRType* type);
IRValue* ir_value_func_ref(const char* name, IRType* type);
void ir_value_free(IRValue* val);

// ============================================================================
// Instruction Construction Functions
// ============================================================================

IRInst* ir_inst_new(IROp op, IRType* type, int dest);
void ir_inst_add_operand(IRInst* inst, IRValue* operand);
void ir_inst_set_loc(IRInst* inst, const char* file, int line, int col);
IRInst* ir_inst_binary(IROp op, IRType* type, int dest, IRValue* lhs, IRValue* rhs);
IRInst* ir_inst_unary(IROp op, IRType* type, int dest, IRValue* operand);
IRInst* ir_inst_cmp(IRCmpPred pred, int dest, IRValue* lhs, IRValue* rhs);
IRInst* ir_inst_alloca(IRType* type, int dest);
IRInst* ir_inst_load(IRType* type, int dest, IRValue* ptr);
IRInst* ir_inst_store(IRValue* value, IRValue* ptr);
IRInst* ir_inst_br(IRBlock* target);
IRInst* ir_inst_br_cond(IRValue* cond, IRBlock* if_true, IRBlock* if_false);
IRInst* ir_inst_ret(IRValue* value);
IRInst* ir_inst_call(const char* target, IRType* ret_type, int dest,
                     IRValue** args, int arg_count);
IRInst* ir_inst_phi(IRType* type, int dest);
void ir_inst_phi_add(IRInst* phi, IRValue* value, IRBlock* block);
void ir_inst_free(IRInst* inst);

// ============================================================================
// Block Construction Functions
// ============================================================================

IRBlock* ir_block_new(const char* label, int id);
void ir_block_append(IRBlock* block, IRInst* inst);
void ir_block_add_pred(IRBlock* block, IRBlock* pred);
void ir_block_add_succ(IRBlock* block, IRBlock* succ);
int ir_block_is_terminated(IRBlock* block);
void ir_block_free(IRBlock* block);

// ============================================================================
// Function Construction Functions
// ============================================================================

IRFunc* ir_func_new(const char* name, IRType* ret_type);
int ir_func_alloc_reg(IRFunc* func);
int ir_func_alloc_block_id(IRFunc* func);
void ir_func_add_block(IRFunc* func, IRBlock* block);
IRBlock* ir_func_new_block(IRFunc* func, const char* label);
void ir_func_add_param(IRFunc* func, const char* name, IRType* type);
void ir_func_free(IRFunc* func);

// ============================================================================
// Global Variable Construction Functions
// ============================================================================

IRGlobal* ir_global_new(const char* name, IRType* type, int is_const);
void ir_global_set_init(IRGlobal* global, IRValue* init);
void ir_global_free(IRGlobal* global);

// ============================================================================
// Module Construction Functions
// ============================================================================

IRModule* ir_module_new(const char* name);
void ir_module_add_func(IRModule* module, IRFunc* func);
void ir_module_add_global(IRModule* module, IRGlobal* global);
int ir_module_add_string(IRModule* module, const char* str);
IRFunc* ir_module_find_func(IRModule* module, const char* name);
IRGlobal* ir_module_find_global(IRModule* module, const char* name);
const char* ir_module_get_string(IRModule* module, int id);
void ir_module_free(IRModule* module);

// ============================================================================
// Printing Functions (for --dump-ir)
// ============================================================================

void ir_value_print(IRValue* val, FILE* out, int use_arabic);
void ir_inst_print(IRInst* inst, FILE* out, int use_arabic);
void ir_block_print(IRBlock* block, FILE* out, int use_arabic);
void ir_func_print(IRFunc* func, FILE* out, int use_arabic);
void ir_global_print(IRGlobal* global, FILE* out, int use_arabic);
void ir_module_print(IRModule* module, FILE* out, int use_arabic);
void ir_module_dump(IRModule* module, const char* filename, int use_arabic);

// English name conversion (for debugging)
const char* ir_op_to_english(IROp op);
const char* ir_cmp_pred_to_english(IRCmpPred pred);
const char* ir_type_to_english(IRType* type);

#endif // BAA_IR_H
