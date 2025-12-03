#ifndef BAA_LLVM_CODEGEN_H
#define BAA_LLVM_CODEGEN_H

// Forward-declare LLVM types to avoid including the full header
#if LLVM_AVAILABLE       // Check the VALUE, not just if defined
#include "llvm-c/Core.h" // Include actual LLVM headers if available
#else
// Define dummy struct and pointer for non-LLVM builds
// This helps maintain type safety even without LLVM.
struct BaaDummyLLVMValue
{
    int _dummy;
}; // Define the struct with a dummy member
typedef struct BaaDummyLLVMValue *LLVMValueRef;
#endif

#include "baa/codegen/codegen.h"
#include "baa/ast/ast.h"
#include <stdbool.h>
#include <wchar.h>

// LLVM context and module state
typedef struct
{
    void *llvm_context;        // LLVMContextRef
    void *llvm_module;         // LLVMModuleRef
    void *llvm_builder;        // LLVMBuilderRef
    void *llvm_target;         // LLVMTargetRef
    void *llvm_target_machine; // LLVMTargetMachineRef
    void *current_function;    // LLVMValueRef
    void *current_block;       // LLVMBasicBlockRef
    void *named_values;        // Hash table for variable values
    bool had_error;
    const wchar_t *error_message;
} BaaLLVMContext;

// Initialize LLVM context
bool baa_init_llvm_context(BaaLLVMContext *context, const wchar_t *module_name);

// Clean up LLVM context
void baa_cleanup_llvm_context(BaaLLVMContext *context);

// Generate LLVM IR for a program
bool baa_generate_llvm_ir(BaaLLVMContext *context, BaaNode *program_node);

// Generate LLVM IR for a function
bool baa_generate_llvm_function(BaaLLVMContext *context, BaaNode *function_node);

// Generate LLVM IR for a statement
bool baa_generate_llvm_statement(BaaLLVMContext *context, BaaNode *stmt_node);

// Generate LLVM IR for an expression
LLVMValueRef baa_generate_llvm_expression(BaaLLVMContext *context, BaaNode *expr_node);

// Generate LLVM IR for control flow statements
bool baa_generate_llvm_if_statement(BaaLLVMContext *context, BaaIfStmtData *if_stmt_data);
bool baa_generate_llvm_while_statement(BaaLLVMContext *context, BaaWhileStmtData *while_stmt_data);
bool baa_generate_llvm_return_statement(BaaLLVMContext *context, BaaReturnStmtData *return_stmt_data);

// Write LLVM IR to file
bool baa_write_llvm_ir_to_file(BaaLLVMContext *context, const wchar_t *filename);

// Compile LLVM IR to object file
bool baa_compile_llvm_ir_to_object(BaaLLVMContext *context, const wchar_t *filename);

// Error handling
const wchar_t *baa_get_llvm_error(BaaLLVMContext *context);
void baa_clear_llvm_error(BaaLLVMContext *context);

#endif /* BAA_LLVM_CODEGEN_H */
