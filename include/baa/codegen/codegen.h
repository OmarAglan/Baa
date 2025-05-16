#ifndef BAA_CODEGEN_H
#define BAA_CODEGEN_H

#include <stdbool.h>
#include <wchar.h>

// Target platform
typedef enum {
    BAA_TARGET_X86_64,  // x86-64 architecture
    BAA_TARGET_ARM64,   // ARM64 architecture
    BAA_TARGET_WASM     // WebAssembly
} BaaTargetPlatform;

// Code generation options
typedef struct {
    BaaTargetPlatform target;  // Target platform
    bool optimize;             // Enable optimizations
    bool debug_info;          // Include debug information
    const wchar_t* output_file; // Output file path
} BaaCodeGenOptions;

// Code generator state
typedef struct {
    // BaaProgram* program;       // AST root (Removed as AST is being removed)
    BaaCodeGenOptions options; // Generation options
    bool had_error;            // Error flag
    const wchar_t* error_message;  // Error message if had_error is true
} BaaCodeGen;

// Code generator initialization and cleanup
void baa_init_codegen(BaaCodeGen* gen, void* unused_program /* BaaProgram* program, */, const BaaCodeGenOptions* options); // AST parts removed
void baa_cleanup_codegen(void);

// Code generation functions (AST-dependent, to be refactored or removed)
// bool baa_generate_code(BaaCodeGen* gen);
// bool baa_generate_function(BaaCodeGen* gen, BaaFunction* function);
// bool baa_generate_statement(BaaCodeGen* gen, BaaStmt* stmt);
// bool baa_generate_expression(BaaCodeGen* gen, BaaExpr* expr);

// Target-specific code generation (May need refactoring if AST-dependent)
bool baa_generate_x86_64(BaaCodeGen* gen);
bool baa_generate_arm64(BaaCodeGen* gen);
bool baa_generate_wasm(BaaCodeGen* gen);

// Optimization functions (AST-dependent, to be refactored or removed)
// bool baa_optimize_code(BaaCodeGen* gen);
// bool baa_optimize_function(BaaCodeGen* gen, BaaFunction* function);

// Debug information generation (AST-dependent, to be refactored or removed)
// bool baa_generate_debug_info(BaaCodeGen* gen);
// bool baa_generate_function_debug_info(BaaCodeGen* gen, BaaFunction* function);

// Error handling
const wchar_t* baa_get_codegen_error(BaaCodeGen* gen);
void baa_clear_codegen_error(BaaCodeGen* gen);

#endif /* BAA_CODEGEN_H */
