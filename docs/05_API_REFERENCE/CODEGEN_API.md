# Baa Code Generation API Reference

**Status:** 🔄 In Progress (Stub Implementation)  
**Last Updated:** 2025-12-05  
**Version Compatibility:** v0.1.27.0+

## Overview

The Baa Code Generation module is responsible for transforming the validated AST into executable code or intermediate representation (LLVM IR). Currently, the infrastructure is in place but uses a stub implementation while the LLVM backend is being integrated.

> **Note:** The code generator is functional but produces no output until LLVM libraries are linked. Enable with `-DUSE_LLVM=ON` during CMake configuration.

## Core Data Structures

### BaaTargetPlatform

Supported target architectures:

```c
typedef enum {
    BAA_TARGET_X86_64,  // x86-64 architecture
    BAA_TARGET_ARM64,   // ARM64 architecture
    BAA_TARGET_WASM     // WebAssembly
} BaaTargetPlatform;
```

### BaaCodeGenOptions

Configuration options for code generation:

```c
typedef struct {
    BaaTargetPlatform target;    // Target platform
    bool optimize;               // Enable optimizations
    bool debug_info;             // Include debug information
    const wchar_t *output_file;  // Output file path (.ll)
} BaaCodeGenOptions;
```

### BaaCodeGen

Main code generator state:

```c
typedef struct {
    BaaNode *program;              // AST root
    BaaCodeGenOptions options;     // Generation options
    bool had_error;                // Error flag
    const wchar_t *error_message;  // Error message
} BaaCodeGen;
```

## Core Functions

### Initialization

#### `baa_init_codegen`
```c
void baa_init_codegen(BaaCodeGen *gen, BaaNode *program, const BaaCodeGenOptions *options);
```
Initializes the code generator with an AST and options.

**Parameters:**
- `gen`: Code generator state to initialize
- `program`: Root AST node (BAA_NODE_KIND_PROGRAM)
- `options`: Code generation options

#### `baa_cleanup_codegen`
```c
void baa_cleanup_codegen(void);
```
Cleans up global code generator resources.

### Code Generation

#### `baa_generate_code`
```c
bool baa_generate_code(BaaCodeGen *gen);
```
Main entry point for code generation. Traverses the AST and emits code.

**Parameters:**
- `gen`: Initialized code generator

**Returns:** `true` on success, `false` on error

### Target-Specific Generation

#### `baa_generate_x86_64`
```c
bool baa_generate_x86_64(BaaCodeGen *gen);
```
Generates code for x86-64 architecture.

#### `baa_generate_arm64`
```c
bool baa_generate_arm64(BaaCodeGen *gen);
```
Generates code for ARM64 architecture.

#### `baa_generate_wasm`
```c
bool baa_generate_wasm(BaaCodeGen *gen);
```
Generates WebAssembly output.

### Error Handling

#### `baa_get_codegen_error`
```c
const wchar_t *baa_get_codegen_error(BaaCodeGen *gen);
```
Returns the error message if code generation failed.

#### `baa_clear_codegen_error`
```c
void baa_clear_codegen_error(BaaCodeGen *gen);
```
Clears any error state.

## Usage Example

```c
#include "baa/codegen/codegen.h"
#include "baa/ast/ast.h"

int main() {
    // Assume 'program' is a parsed AST
    BaaNode *program = parse_source_file("hello.baa");
    
    // Configure options
    BaaCodeGenOptions options = {
        .target = BAA_TARGET_X86_64,
        .optimize = true,
        .debug_info = true,
        .output_file = L"hello.ll"
    };
    
    // Initialize and generate
    BaaCodeGen codegen;
    baa_init_codegen(&codegen, program, &options);
    
    if (baa_generate_code(&codegen)) {
        wprintf(L"Code generation successful!\n");
    } else {
        fwprintf(stderr, L"Error: %ls\n", baa_get_codegen_error(&codegen));
    }
    
    // Cleanup
    baa_cleanup_codegen();
    baa_ast_free_node(program);
    
    return 0;
}
```

## Current Implementation Status

| Feature | Status |
|---------|--------|
| Infrastructure | ✅ Complete |
| AST Traversal | ✅ Complete |
| LLVM Integration | 🔄 In Progress |
| x86-64 Output | 📋 Planned |
| ARM64 Output | 📋 Planned |
| WASM Output | 📋 Planned |
| Optimizations | 📋 Planned |
| Debug Info | 📋 Planned |

## Future Enhancements

- **LLVM Backend**: Full LLVM IR generation from AST
- **Optimization Passes**: Dead code elimination, constant folding
- **Debug Information**: DWARF debug info for source-level debugging
- **Multiple Targets**: Cross-compilation support

## See Also

- [Architecture Overview](../02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md)
- [Code Generation Documentation](../02_COMPILER_ARCHITECTURE/CODE_GENERATION.md)
- [LLVM Codegen Roadmap](../04_ROADMAP/LLVM_CODEGEN_ROADMAP.md)
- [AST API](AST_API.md)

---

*This document is part of the [Baa Language Documentation](../index.md)*
