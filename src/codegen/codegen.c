#include "baa/codegen/codegen.h"
#include "baa/utils/utils.h"
#include "baa/utils/errors.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef LLVM_AVAILABLE
#include "baa/codegen/llvm_codegen.h"

// Global LLVM context
static BaaLLVMContext g_llvm_context;
static bool g_llvm_initialized = false;
#endif

// Initialize code generation
void baa_init_codegen(BaaCodeGen* gen, void* unused_program /* BaaProgram* program */, const BaaCodeGenOptions* options) { // AST type removed
    if (!gen || !options) { // Removed program check
        return;
    }

    // gen->program = program; // Removed as AST is being removed
    gen->options = *options;
    gen->had_error = false;
    gen->error_message = NULL;

#ifdef LLVM_AVAILABLE
    // Initialize LLVM context if not already initialized
    if (!g_llvm_initialized) {
        const wchar_t* module_name = L"baa_module";
        if (!baa_init_llvm_context(&g_llvm_context, module_name)) {
            gen->had_error = true;
            gen->error_message = baa_get_llvm_error(&g_llvm_context);
            return;
        }
        g_llvm_initialized = true;
    }
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
#endif
}

// Generate code
bool baa_generate_code(BaaCodeGen* gen) {
    if (!gen /* || !gen->program */) { // Removed program check
        return false;
    }

#ifdef LLVM_AVAILABLE
    // Generate LLVM IR
    // The call to baa_generate_llvm_ir needs to be updated as its signature changed (BaaProgram* removed)
    // For now, commenting out the direct AST-dependent call.
    // This function will need significant rework without an AST.
    // if (!baa_generate_llvm_ir(&g_llvm_context, gen->program /* This was the AST program */)) {
    if (!baa_generate_llvm_ir(&g_llvm_context, NULL /* Placeholder for removed program */)) {
        gen->had_error = true;
        gen->error_message = baa_get_llvm_error(&g_llvm_context);
        return false;
    }

    // Write LLVM IR to file if output file is specified
    if (gen->options.output_file) {
        if (!baa_write_llvm_ir_to_file(&g_llvm_context, gen->options.output_file)) {
            gen->had_error = true;
            gen->error_message = baa_get_llvm_error(&g_llvm_context);
            return false;
        }
    }

    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Generate code for a function
bool baa_generate_function(BaaCodeGen* gen, void* unused_function /* BaaFunction* function */) { // AST type removed
    if (!gen || !unused_function) {
        return false;
    }

#ifdef LLVM_AVAILABLE
    // This will be implemented in llvm_codegen.c
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Generate code for a statement
bool baa_generate_statement(BaaCodeGen* gen, void* unused_stmt /* BaaStmt* stmt */) { // AST type removed
    if (!gen || !unused_stmt) {
        return false;
    }

#ifdef LLVM_AVAILABLE
    // This will be implemented in llvm_codegen.c
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Generate code for an expression
bool baa_generate_expression(BaaCodeGen* gen, void* unused_expr /* BaaExpr* expr */) { // AST type removed
    if (!gen || !unused_expr) {
        return false;
    }

#ifdef LLVM_AVAILABLE
    // This will be implemented in llvm_codegen.c
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Target-specific code generation
bool baa_generate_x86_64(BaaCodeGen* gen) {
#ifdef LLVM_AVAILABLE
    // Placeholder for x86-64 code generation
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

bool baa_generate_arm64(BaaCodeGen* gen) {
#ifdef LLVM_AVAILABLE
    // Placeholder for ARM64 code generation
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

bool baa_generate_wasm(BaaCodeGen* gen) {
#ifdef LLVM_AVAILABLE
    // Placeholder for WebAssembly code generation
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Optimization functions
bool baa_optimize_code(BaaCodeGen* gen) {
#ifdef LLVM_AVAILABLE
    // Placeholder for code optimization
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

bool baa_optimize_function(BaaCodeGen* gen, void* unused_function /* BaaFunction* function */) { // AST type removed
#ifdef LLVM_AVAILABLE
    // Placeholder for function optimization
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Debug information generation
bool baa_generate_debug_info(BaaCodeGen* gen) {
#ifdef LLVM_AVAILABLE
    // Placeholder for debug information generation
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

bool baa_generate_function_debug_info(BaaCodeGen* gen, void* unused_function /* BaaFunction* function */) { // AST type removed
#ifdef LLVM_AVAILABLE
    // Placeholder for function debug information generation
    return true;
#else
    gen->had_error = true;
    gen->error_message = L"LLVM support not available. Recompile with LLVM.";
    return false;
#endif
}

// Error handling
const wchar_t* baa_get_codegen_error(BaaCodeGen* gen) {
    if (!gen) {
        return L"Invalid code generator";
    }

    return gen->error_message;
}

void baa_clear_codegen_error(BaaCodeGen* gen) {
    if (!gen) {
        return;
    }

    gen->had_error = false;
    gen->error_message = NULL;
}

// Cleanup code generation
void baa_cleanup_codegen() {
#ifdef LLVM_AVAILABLE
    if (g_llvm_initialized) {
        baa_cleanup_llvm_context(&g_llvm_context);
        g_llvm_initialized = false;
    }
#endif
}
