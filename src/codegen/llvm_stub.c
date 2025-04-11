#include "baa/codegen/llvm_codegen.h"
#include "baa/ast/ast.h"
#include <stdbool.h>
#include <wchar.h>

// Stub implementations for when LLVM is not available

// Initialize LLVM context
bool baa_init_llvm_context(BaaLLVMContext* context, const wchar_t* module_name) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

// Get LLVM error
const wchar_t* baa_get_llvm_error(BaaLLVMContext* context) {
    if (context && context->had_error) {
        return context->error_message;
    }
    return L"LLVM support not available";
}

// Generate LLVM IR for a program
bool baa_generate_llvm_ir(BaaLLVMContext* context, BaaProgram* program) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

// Write LLVM IR to file
bool baa_write_llvm_ir_to_file(BaaLLVMContext* context, const wchar_t* filename) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

// Clean up LLVM context
void baa_cleanup_llvm_context(BaaLLVMContext* context) {
    // No-op in this stub implementation
}

// Additional stubs required by the header
bool baa_generate_llvm_function(BaaLLVMContext* context, BaaFunction* function) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

bool baa_generate_llvm_statement(BaaLLVMContext* context, BaaStmt* stmt) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

void* baa_generate_llvm_expression(BaaLLVMContext* context, BaaExpr* expr) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return NULL;
}

bool baa_generate_llvm_if_statement(BaaLLVMContext* context, BaaIfStmt* if_stmt) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

bool baa_generate_llvm_while_statement(BaaLLVMContext* context, BaaWhileStmt* while_stmt) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

bool baa_generate_llvm_return_statement(BaaLLVMContext* context, BaaReturnStmt* return_stmt) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

bool baa_compile_llvm_ir_to_object(BaaLLVMContext* context, const wchar_t* filename) {
    if (context) {
        context->had_error = true;
        context->error_message = L"LLVM support not available";
    }
    return false;
}

void baa_clear_llvm_error(BaaLLVMContext* context) {
    if (context) {
        context->had_error = false;
        context->error_message = NULL;
    }
}
