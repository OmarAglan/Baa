# Baa Language LLVM Code Generation Roadmap (Extended Detail)

**Status: This document outlines the implementation plan for generating LLVM Intermediate Representation (IR) from the Baa AST. All items are planned unless otherwise noted. This phase operates on a semantically analyzed and annotated AST.**

## Phase 0: LLVM Setup and Core Infrastructure

- **LLVM Integration & Build System:**
  - [x] Ensure `CMakeLists.txt` correctly finds and links LLVM libraries (core, analysis, codegen, target-specific backends).
  - [ ] Define necessary LLVM C API headers to include in `llvm_codegen.c` (e.g., `llvm-c/Core.h`, `llvm-c/Analysis.h`, `llvm-c/Target.h`, `llvm-c/TargetMachine.h`, `llvm-c/Transforms/Scalar.h`, `llvm-c/BitWriter.h`).
- **Core Code Generation Structures (`llvm_codegen.c` / `codegen.c`):**
  - [ ] Define `BaaLLVMCodeGenerator` struct:
    - [ ] `LLVMContextRef context;`
    - [ ] `LLVMModuleRef module;`
    - [ ] `LLVMBuilderRef builder;`
    - [ ] `LLVMValueRef current_function;` // The LLVM function currently being generated
    - [ ] `LLVMPassManagerRef function_pass_manager;` // Optional, for per-function optimizations
    - [ ] Mappings/Symbol Tables:
      - [ ] `map<BaaSymbol*, LLVMValueRef> named_values;` // Maps AST variable/function symbols to their LLVMValueRef (e.g., allocas, function pointers)
      - [ ] `map<BaaType*, LLVMTypeRef> type_cache;` // Cache for BaaType to LLVMTypeRef mappings
    - [ ] Current basic block management for control flow:
      - [ ] `LLVMBasicBlockRef current_loop_header_bb;`
      - [ ] `LLVMBasicBlockRef current_loop_after_bb;` (for break)
      - [ ] `LLVMBasicBlockRef current_loop_increment_bb;` (for continue in for-loops)
  - [ ] Implement `BaaLLVMCodeGenerator* baa_llvm_codegen_create(const char* module_id);`
  - [ ] Implement `void baa_llvm_codegen_destroy(BaaLLVMCodeGenerator* codegen);`
  - [ ] Implement `LLVMModuleRef baa_llvm_codegen_generate(BaaLLVMCodeGenerator* codegen, BaaProgram* ast_program);` (main dispatch).
  - [ ] Implement `void baa_llvm_codegen_dump_ir(LLVMModuleRef module);` (to stdout or stderr).
  - [ ] Implement `int baa_llvm_codegen_emit_object_file(LLVMModuleRef module, const char* filename, const char* target_triple);`
  - [ ] Implement `int baa_llvm_codegen_emit_assembly_file(LLVMModuleRef module, const char* filename, const char* target_triple);`
- **Basic Type Mapping (`baa_type_to_llvm_type` utility):**
  - [ ] Function signature: `LLVMTypeRef baa_type_to_llvm_type(BaaLLVMCodeGenerator* codegen, BaaType* baa_type);`
  - [ ] Handle `BAA_TYPE_VOID` -> `LLVMVoidTypeInContext(codegen->context)`.
  - [ ] Handle `BAA_TYPE_INT` (considering modifiers for `LLVMInt1Type`, `LLVMInt8Type`, `LLVMInt16Type`, `LLVMInt32Type`, `LLVMInt64Type`).
  - [ ] Handle `BAA_TYPE_FLOAT` (considering modifiers for `LLVMFloatTypeInContext`, `LLVMDoubleTypeInContext`).
  - [ ] Handle `BAA_TYPE_CHAR` -> `LLVMInt16TypeInContext(codegen->context)` (for UTF-16LE `wchar_t`).
  - [ ] Handle `BAA_TYPE_BOOL` -> `LLVMInt1TypeInContext(codegen->context)`.
  - [ ] Implement caching for `LLVMTypeRef` to avoid re-creating types.

## Phase 1: Basic Expressions and Global Variables

- **Literal Value Generation (functions returning `LLVMValueRef`):**
  - [ ] `codegen_integer_literal(BaaLLVMCodeGenerator* codegen, BaaLiteralData* literal_data)` -> `LLVMConstInt(type, value, sign_extend_flag)`.
  - [ ] `codegen_float_literal(BaaLLVMCodeGenerator* codegen, BaaLiteralData* literal_data)` -> `LLVMConstReal(type, value)`.
  - [ ] `codegen_bool_literal(BaaLLVMCodeGenerator* codegen, BaaLiteralData* literal_data)` -> `LLVMConstInt(LLVMInt1Type(), value, 0)`.
  - [ ] `codegen_char_literal(BaaLLVMCodeGenerator* codegen, BaaLiteralData* literal_data)` -> `LLVMConstInt(LLVMInt16Type(), value, 0)`.
  - [ ] `codegen_string_literal(BaaLLVMCodeGenerator* codegen, BaaLiteralData* literal_data)` -> `LLVMBuildGlobalStringPtr(codegen->builder, literal_data->string_value, "str_literal_name")`.
  - [ ] Handle `null` literal (e.g., `LLVMConstNull` for pointer types, or zero for other types if applicable).
- **Global Variable Declarations:**
  - [ ] `codegen_global_variable(BaaLLVMCodeGenerator* codegen, BaaVarDeclStmt* var_decl_node)`:
    - [ ] Use `LLVMAddGlobal(codegen->module, type, var_decl_node->name)`.
    - [ ] Set linkage (e.g., `LLVMExternalLinkage`).
    - [ ] Set thread-local mode if applicable.
    - [ ] Handle initializers: `LLVMSetInitializer(global_var, const_initializer_value)`. Initializer must be a constant expression.
    - [ ] Handle `ثابت` (const) by setting `LLVMSetGlobalConstant(global_var, true)`.
    - [ ] Store the `LLVMValueRef` in the global scope of `named_values`.
- **Local Variable Declarations (Stack Allocation within Functions):**
  - [ ] `codegen_local_variable_decl(BaaLLVMCodeGenerator* codegen, BaaVarDeclStmt* var_decl_node)`:
    - [ ] Create an `alloca` instruction at the entry block of the current function: `LLVMBuildAlloca(builder, type, name)`.
    - [ ] Store the `LLVMValueRef` (pointer from alloca) in the current function's scope `named_values`.
    - [ ] If an initializer exists, generate code for the initializer expression and then `LLVMBuildStore` its value into the alloca.
- **Variable Access (Load/Store within Expressions/Statements):**
  - [ ] `codegen_variable_load(BaaLLVMCodeGenerator* codegen, BaaVariableExpr* var_expr_node)`:
    - [ ] Look up `var_expr_node->name` in `named_values` to get the `LLVMValueRef` (pointer).
    - [ ] Generate `LLVMBuildLoad(builder, pointer_val, name)`.
  - [ ] `codegen_variable_store(BaaLLVMCodeGenerator* codegen, LLVMValueRef ptr_val, LLVMValueRef value_to_store)`:
    - [ ] Generate `LLVMBuildStore(builder, value_to_store, ptr_val)`.

## Phase 2: Basic Operators and Assignments

- **Unary Operators (`codegen_unary_expr`):**
  - [ ] Arithmetic Negation (`-`): `LLVMBuildNSWNeg` (for signed int), `LLVMBuildFNeg` (for float).
  - [ ] Logical NOT (`!`): Compare operand with zero (`LLVMBuildICmp(builder, LLVMIntEQ, operand, LLVMConstInt(LLVMTypeOf(operand), 0, 0), "nottmp")`).
  - [ ] Bitwise NOT (`~`): `LLVMBuildNot`.
- **Binary Operators (`codegen_binary_expr`):**
  - [ ] Arithmetic:
    - `+`: `LLVMBuildAdd` (or `NSWAdd`/`NUWAdd` for integers if overflow behavior is defined), `LLVMBuildFAdd`.
    - `-`: `LLVMBuildSub` (or `NSWSub`/`NUWSub`), `LLVMBuildFSub`.
    - `*`: `LLVMBuildMul` (or `NSWMul`/`NUWMul`), `LLVMBuildFMul`.
    - `/`: `LLVMBuildSDiv` (signed int), `LLVMBuildUDiv` (unsigned int), `LLVMBuildFDiv` (float).
    - `%`: `LLVMBuildSRem` (signed int), `LLVMBuildURem` (unsigned int), `LLVMBuildFRem` (float).
  - [ ] Comparison: `LLVMBuildICmp(builder, predicate, lhs, rhs, name)` for integers/pointers, `LLVMBuildFCmp(builder, predicate, lhs, rhs, name)` for floats.
    - `==`: `LLVMIntEQ` / `LLVMRealOEQ` (or `UEQ`).
    - `!=`: `LLVMIntNE` / `LLVMRealONE`.
    - `<`: `LLVMIntSLT`/`ULT` / `LLVMRealOLT`.
    - `<=`: `LLVMIntSLE`/`ULE` / `LLVMRealOLE`.
    - `>`: `LLVMIntSGT`/`UGT` / `LLVMRealOGT`.
    - `>=`: `LLVMIntSGE`/`UGE` / `LLVMRealOGE`.
  - [ ] Logical `&&` (AND): Implement short-circuiting.
    - Generate code for LHS.
    - Create `then_bb`, `else_bb`, `merge_bb`.
    - Conditional branch based on LHS result to `then_bb` (if true) or `merge_bb` (if false, result is false).
    - In `then_bb`, generate code for RHS, then branch to `merge_bb`.
    - In `merge_bb`, use `LLVMBuildPhi` to select result.
  - [ ] Logical `||` (OR): Implement short-circuiting similarly.
    - Conditional branch based on LHS result to `merge_bb` (if true, result is true) or `else_bb` (if false).
- **Assignment Expressions (`codegen_assignment_expr`):**
  - [ ] Simple Assignment (`=`):
    - Codegen LHS to get pointer (`lvalue_ptr`).
    - Codegen RHS to get value (`rvalue_val`).
    - `LLVMBuildStore(builder, rvalue_val, lvalue_ptr)`.
    - Assignment expression result is `rvalue_val`.
  - [ ] Compound Assignments (`+=`, `-=`, etc.):
    - Codegen LHS to get `lvalue_ptr`.
    - `LLVMBuildLoad` current value from `lvalue_ptr`.
    - Perform binary operation with RHS value.
    - `LLVMBuildStore` result back to `lvalue_ptr`.
    - Result of expression is the stored value.

## Phase 3: Control Flow Statements

- **Basic Block Management:**
  - [ ] `LLVMAppendBasicBlockInContext(context, function, name)`.
  - [ ] `LLVMPositionBuilderAtEnd(builder, bb)`.
  - [ ] Keep track of current basic block.
- **If Statements (`codegen_if_stmt`):**
  - [ ] Codegen condition expression.
  - [ ] Create `then_bb`, `else_bb` (if exists), `merge_bb`.
  - [ ] `LLVMBuildCondBr(builder, condition_val, then_bb, else_bb_or_merge_bb)`.
  - [ ] Populate `then_bb`: codegen then-block, then `LLVMBuildBr(builder, merge_bb)`.
  - [ ] Populate `else_bb` (if exists): codegen else-block, then `LLVMBuildBr(builder, merge_bb)`.
  - [ ] Position builder at `merge_bb`.
- **While Loops (`codegen_while_stmt`):**
  - [ ] Create `loop_header_bb`, `loop_body_bb`, `after_loop_bb`.
  - [ ] `LLVMBuildBr(builder, loop_header_bb)`.
  - [ ] Populate `loop_header_bb`: codegen condition, `LLVMBuildCondBr(builder, cond_val, loop_body_bb, after_loop_bb)`.
  - [ ] Populate `loop_body_bb`: codegen body, then `LLVMBuildBr(builder, loop_header_bb)`.
  - [ ] Set `current_loop_header_bb` and `current_loop_after_bb` for break/continue.
  - [ ] Position builder at `after_loop_bb`.
- **For Loops (`codegen_for_stmt`):**
  - [ ] Codegen initializer.
  - [ ] Create `loop_cond_bb`, `loop_body_bb`, `loop_inc_bb`, `after_loop_bb`.
  - [ ] `LLVMBuildBr(builder, loop_cond_bb)`.
  - [ ] Populate `loop_cond_bb`: codegen condition, `LLVMBuildCondBr(builder, cond_val, loop_body_bb, after_loop_bb)`.
  - [ ] Populate `loop_body_bb`: codegen body, then `LLVMBuildBr(builder, loop_inc_bb)`.
  - [ ] Populate `loop_inc_bb`: codegen increment, then `LLVMBuildBr(builder, loop_cond_bb)`.
  - [ ] Set `current_loop_increment_bb` (for continue) and `current_loop_after_bb` (for break).
  - [ ] Position builder at `after_loop_bb`.
- **Return Statements (`codegen_return_stmt`):**
  - [ ] If value exists, codegen expression for value.
  - [ ] `LLVMBuildRet(builder, value)` or `LLVMBuildRetVoid(builder)`.
- **Break (`codegen_break_stmt`) and Continue (`codegen_continue_stmt`):**
  - [ ] `LLVMBuildBr(builder, codegen->current_loop_after_bb)` for break.
  - [ ] `LLVMBuildBr(builder, codegen->current_loop_header_bb_or_inc_bb)` for continue.
  - [ ] Ensure these are within a valid loop context (semantic analysis should check this).

## Phase 4: Functions

- **Function Declarations & Definitions (`codegen_function_decl_or_def`):**
  - [ ] Create `LLVMTypeRef` for function signature: `LLVMFunctionType(return_type, param_types_array, param_count, is_variadic_flag)`.
  - [ ] `LLVMAddFunction(codegen->module, function_name_str, function_type_ref)`.
  - [ ] Set linkage: `LLVMSetLinkage` (e.g., `LLVMExternalLinkage` for normal functions, `LLVMInternalLinkage` for static/private).
  - [ ] Set calling convention: `LLVMSetFunctionCallConv`.
  - [ ] For definitions (functions with bodies):
    - [ ] `LLVMAppendBasicBlockInContext(codegen->context, llvm_function, "entry")`.
    - [ ] `LLVMPositionBuilderAtEnd(codegen->builder, entry_bb)`.
    - [ ] Store `llvm_function` as `codegen->current_function`.
    - [ ] Allocate space for parameters using `alloca` and store their initial values from `LLVMGetParam`. Add to `named_values` for the function's scope.
    - [ ] Codegen statements in the function body.
    - [ ] Ensure a terminator instruction for all basic blocks (LLVM verification pass helps).
    - [ ] Run `LLVMVerifyFunction(llvm_function, LLVMPrintMessageAction)` for validation.
- **Function Calls (`codegen_call_expr`):**
  - [ ] Codegen callee expression to get `LLVMValueRef` for the function.
  - [ ] Codegen argument expressions to get an array of `LLVMValueRef` for arguments.
  - [ ] `LLVMBuildCall(builder, func_val, args_array, num_args, "calltmp")`.
- **`مضمن` (inline) Hint:**
  - [ ] `LLVMAddFunctionAttr(llvm_function, LLVMInlineAttribute)` or `LLVMAddFunctionAttr(llvm_function, LLVMAlwaysInlineAttribute)`.

## Phase 5: Advanced Types and Features

- **Array Types:**
  - [ ] `baa_type_to_llvm_type` handles `BAA_TYPE_ARRAY` -> `LLVMArrayType(element_llvm_type, count)`.
  - [ ] Global/Local array allocation: `LLVMAddGlobal` or `LLVMBuildAlloca` with array type.
  - [ ] Array Indexing: `LLVMBuildGEP(builder, array_ptr, indices_array, num_indices, "idxptr")` followed by `LLVMBuildLoad` or `LLVMBuildStore`. Indices for GEP usually include a 0 for the initial pointer dereference, then the actual index.
  - [ ] Array Literals: Create as `LLVMConstArray` for global initializers, or generate stores for local initialized arrays.
- **Pointer Types:**
  - [ ] `baa_type_to_llvm_type` handles `BAA_TYPE_POINTER` -> `LLVMPointerType(pointee_llvm_type, address_space)`.
  - [ ] Address-of (`&`): Usually implicit if an lvalue is needed as a pointer (e.g., for GEP). If explicit, it might involve ensuring the operand is an lvalue and returning its alloca/global pointer.
  - [ ] Dereference (`*`): `LLVMBuildLoad` from a pointer value. Storing to a dereferenced pointer is `LLVMBuildStore` to the pointer value.
  - [ ] `مقيد` (restrict): `LLVMAddAttributeAtIndex(llvm_function, param_index, LLVMNoAliasAttribute)`.
- **Struct/Union/Enum Types:**
  - [ ] Structs: `LLVMStructCreateNamed(context, name)` then `LLVMStructSetBody(struct_type, element_types_array, num_elements, is_packed)`.
  - [ ] Unions: Often modeled as a struct containing a byte array of the largest member's size, or by using bitcasts with a struct of the largest member.
  - [ ] Enums: Typically map to an underlying LLVM integer type.
  - [ ] Member Access (`::` or `->`): `LLVMBuildStructGEP(builder, struct_ptr, member_index, name)` or `LLVMBuildGEP` for general aggregates, followed by load/store.
- **Explicit Type Casts (`codegen_cast_expr`):**
  - [ ] Integer to Integer: `LLVMBuildTrunc` (to smaller), `LLVMBuildSExt` (signed extend), `LLVMBuildZExt` (zero extend).
  - [ ] Float to Float: `LLVMBuildFPTrunc` (to smaller), `LLVMBuildFPExt` (to larger).
  - [ ] Int to Float: `LLVMBuildUIToFP` (unsigned int to float), `LLVMBuildSIToFP` (signed int to float).
  - [ ] Float to Int: `LLVMBuildFPToUI` (float to unsigned int), `LLVMBuildFPToSI` (float to signed int).
  - [ ] Pointer to Int / Int to Pointer: `LLVMBuildPtrToInt`, `LLVMBuildIntToPtr`.
  - [ ] Pointer to Pointer (Bitcast): `LLVMBuildBitCast`.

## Phase 6: LLVM Optimization and Output

- **Running Optimization Passes:**
  - [ ] Create `LLVMPassManagerBuilderRef`, set opt level (0-3) and size level (0-2).
  - [ ] Create `LLVMModulePassManagerRef` and `LLVMFunctionPassManagerRef`.
  - [ ] `LLVMPassManagerBuilderPopulateFunctionPassManager(pm_builder, fpm)`.
  - [ ] `LLVMPassManagerBuilderPopulateModulePassManager(pm_builder, mpm)`.
  - [ ] Run FPM on each function: `LLVMRunFunctionPassManager(fpm, llvm_function)`.
  - [ ] Run MPM on the module: `LLVMRunPassManager(mpm, codegen->module)`.
  - [ ] Dispose pass managers and builder.
- **Generating Output Files:**
  - [ ] `LLVMPrintModuleToFile(module, filename, &error_message)` for `.ll` (human-readable IR).
  - [ ] `LLVMWriteBitcodeToFile(module, filename)` for `.bc` (bitcode).
  - [ ] For object/assembly:
    - [ ] `LLVMTargetRef target = LLVMGetTargetFromTriple(target_triple_str, &error);`
    - [ ] `LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, target_triple_str, cpu_str, features_str, level, LLVMRelocDefault, LLVMCodeModelDefault);`
    - [ ] `LLVMTargetMachineEmitToFile(machine, module, filename_char_ptr, LLVMObjectFile, &error_message)` for object file.
    - [ ] `LLVMTargetMachineEmitToFile(machine, module, filename_char_ptr, LLVMAssemblyFile, &error_message)` for assembly.
- **Target Machine Configuration:**
  - [ ] Initialize all targets: `LLVMInitializeAllTargetInfos()`, `LLVMInitializeAllTargets()`, `LLVMInitializeAllTargetMCs()`, `LLVMInitializeAllAsmParsers()`, `LLVMInitializeAllAsmPrinters()`.
  - [ ] Get target triple: `LLVMGetDefaultTargetTriple()` or user-specified.
  - [ ] Specify CPU and features if needed.
- **Linking:**
  - [ ] Decide on strategy: either instruct user to link with `clang` (passing the object file) or use `lld` (LLVM's linker) programmatically or via command line if feasible.

## Future Considerations / Advanced Topics

- **Debug Information Generation (DWARF/CodeView):**
  - [ ] Create `LLVMDIBuilderRef`.
  - [ ] Create `DICompileUnit`, `DIFile`, `DISubprogram` (for functions), `DILocalVariable`, `DIExpression`, `DIType` nodes.
  - [ ] Attach `DILocation` metadata to LLVM instructions.
- **Foreign Function Interface (FFI):**
  - [ ] Generating calls to external C functions: Declare them in LLVM module with correct signature.
  - [ ] Allowing Baa functions to be called from C: Ensure consistent calling conventions and name mangling (or `extern "C"` equivalent).
- **Just-In-Time (JIT) Compilation:**
  - [ ] Use `LLVMOrcJITDylibAddGenerator`, `LLVMOrcCreateNewThreadSafeContext`, `LLVMOrcExecutionSessionRef`.
  - [ ] Compile module and execute functions in memory.
- **Advanced Baa Language Features:**
  - [ ] Coroutines/Generators: May require specific LLVM intrinsics or state machine transformations.
  - [ ] Lambdas/Closures: Involves capturing environment, often implemented as structs with function pointers.
- **Sanitizers:**
  - [ ] Add LLVM passes for AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), etc., possibly via compiler flags.

## Testing Strategy

- [ ] Unit tests for `baa_type_to_llvm_type` and individual `codegen_*` functions for AST nodes.
- [ ] End-to-end tests: Compile Baa source files to LLVM IR. Use `FileCheck` to verify specific IR patterns.
- [ ] Compile to object code, link with a C main (if needed for testing specific functions), run, and check output/return code.
- [ ] Compare generated assembly/object code behavior with `clang` output for equivalent C constructs where feasible.
- [ ] Test on multiple target architectures if configured in CMake (e.g., x86-64, ARM64).
- [ ] Test different optimization levels.
