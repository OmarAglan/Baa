#include "baa/codegen/llvm_codegen.h"
#include "baa/utils/utils.h"
#include "baa/utils/errors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// LLVM C API includes
#ifdef LLVM_AVAILABLE
// Use the correct include paths for LLVM headers
#ifdef _WIN32
#include "llvm-c/Core.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "llvm-c/ExecutionEngine.h"
#include "llvm-c/BitWriter.h"
#else
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/BitWriter.h>
#endif

// Helper function to convert wchar_t* to char*
static char *wchar_to_char(const wchar_t *wstr)
{
    if (!wstr)
        return NULL;

    size_t len = wcslen(wstr);
    char *str = (char *)malloc(len + 1);
    if (!str)
        return NULL;

    for (size_t i = 0; i < len; i++)
    {
        str[i] = (char)wstr[i]; // Simple conversion, assumes ASCII
    }
    str[len] = '\0';

    return str;
}

// Helper function to convert char* to wchar_t*
static wchar_t *char_to_wchar(const char *str)
{
    if (!str)
        return NULL;

    size_t len = strlen(str);
    wchar_t *wstr = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!wstr)
        return NULL;

    for (size_t i = 0; i < len; i++)
    {
        wstr[i] = (wchar_t)str[i]; // Simple conversion, assumes ASCII
    }
    wstr[len] = L'\0';

    return wstr;
}

// Helper function to set error message
static void set_llvm_error(BaaLLVMContext *context, const wchar_t *message)
{
    context->had_error = true;
    context->error_message = message;
}

// Helper function to convert BaaType to LLVMTypeRef
static LLVMTypeRef baa_type_to_llvm_type(BaaLLVMContext *context, BaaType *type)
{
    if (!type)
    {
        return LLVMVoidTypeInContext(context->llvm_context);
    }

    switch (type->kind)
    {
    case BAA_TYPE_VOID:
        return LLVMVoidTypeInContext(context->llvm_context);
    case BAA_TYPE_INT:
        return LLVMInt32TypeInContext(context->llvm_context);
    case BAA_TYPE_FLOAT:
        return LLVMFloatTypeInContext(context->llvm_context);
    case BAA_TYPE_CHAR:
        return LLVMInt8TypeInContext(context->llvm_context);
    case BAA_TYPE_BOOL:
        return LLVMInt1TypeInContext(context->llvm_context);
    default:
        // Unsupported type
        set_llvm_error(context, L"Unsupported type");
        return NULL;
    }
}

// Initialize LLVM context
bool baa_init_llvm_context(BaaLLVMContext *context, const wchar_t *module_name)
{
    if (!context || !module_name)
    {
        return false;
    }

    // Initialize LLVM targets required for code generation & JIT
    // LLVMInitializeCore(LLVMGetGlobalPassRegistry()); // Remove or comment out
    LLVMInitializeNativeTarget();     // Initialize the native target (for JIT and object emission)
    LLVMInitializeNativeAsmPrinter(); // Initialize the native assembly printer
    LLVMInitializeNativeAsmParser();  // Initialize the native assembly parser (less critical here)
    // You might need others like LLVMInitializeAllTargetInfos(), LLVMInitializeAllTargets(), etc.
    // depending on how you determine the target later. Let's add them for robustness.
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();

    // Create LLVM context
    context->llvm_context = LLVMContextCreate();
    if (!context->llvm_context)
    {
        set_llvm_error(context, L"Failed to create LLVM context");
        return false;
    }

    // Convert module name to char*
    char *c_module_name = wchar_to_char(module_name);
    if (!c_module_name)
    {
        set_llvm_error(context, L"Failed to convert module name");
        LLVMContextDispose(context->llvm_context);
        context->llvm_context = NULL;
        return false;
    }

    // Create LLVM module
    context->llvm_module = LLVMModuleCreateWithNameInContext(c_module_name, context->llvm_context);
    free(c_module_name);

    if (!context->llvm_module)
    {
        set_llvm_error(context, L"Failed to create LLVM module");
        LLVMContextDispose(context->llvm_context);
        context->llvm_context = NULL;
        return false;
    }

    // Create LLVM IR builder
    context->llvm_builder = LLVMCreateBuilderInContext(context->llvm_context);
    if (!context->llvm_builder)
    {
        set_llvm_error(context, L"Failed to create LLVM IR builder");
        LLVMDisposeModule(context->llvm_module);
        LLVMContextDispose(context->llvm_context);
        context->llvm_module = NULL;
        context->llvm_context = NULL;
        return false;
    }

    // Initialize other fields
    context->current_function = NULL;
    context->current_block = NULL;
    context->named_values = NULL;
    context->had_error = false;
    context->error_message = NULL;

    return true;
}

// Clean up LLVM context
void baa_cleanup_llvm_context(BaaLLVMContext *context)
{
    if (!context)
    {
        return;
    }

    // Dispose of LLVM objects in reverse order of creation
    if (context->llvm_builder)
    {
        LLVMDisposeBuilder(context->llvm_builder);
        context->llvm_builder = NULL;
    }

    if (context->llvm_module)
    {
        LLVMDisposeModule(context->llvm_module);
        context->llvm_module = NULL;
    }

    if (context->llvm_context)
    {
        LLVMContextDispose(context->llvm_context);
        context->llvm_context = NULL;
    }

    // Reset other fields
    context->current_function = NULL;
    context->current_block = NULL;
    context->named_values = NULL;
    context->had_error = false;
    context->error_message = NULL;
}

// Generate LLVM IR for a program
bool baa_generate_llvm_ir(BaaLLVMContext *context, BaaNode *program_node)
{
    if (!context || !program_node || program_node->kind != BAA_NODE_KIND_PROGRAM)
    {
        set_llvm_error(context, L"Invalid program node");
        return false;
    }

    BaaProgramData *program_data = (BaaProgramData *)program_node->data;
    if (!program_data)
    {
        set_llvm_error(context, L"Invalid program data");
        return false;
    }

    // Generate code for each top-level declaration
    for (size_t i = 0; i < program_data->count; i++)
    {
        BaaNode *decl = program_data->top_level_declarations[i];
        if (decl->kind == BAA_NODE_KIND_FUNCTION_DEF)
        {
            if (!baa_generate_llvm_function(context, decl))
            {
                return false;
            }
        }
        // Handle global variables later
    }

    // Verify the module
    char *error = NULL;
    LLVMVerifyModule(context->llvm_module, LLVMPrintMessageAction, &error);
    if (error)
    {
        // Convert error message to wchar_t and set it
        wchar_t *werror = char_to_wchar(error);
        if (werror)
        {
            set_llvm_error(context, werror);
            free(werror);
        }
        else
        {
            set_llvm_error(context, L"Module verification failed");
        }

        LLVMDisposeMessage(error);
        return false;
    }

    return true;
}

// Generate LLVM IR for a function
bool baa_generate_llvm_function(BaaLLVMContext *context, BaaNode *function_node)
{
    if (!context || !function_node || function_node->kind != BAA_NODE_KIND_FUNCTION_DEF)
    {
        set_llvm_error(context, L"Invalid function node");
        return false;
    }

    BaaFunctionDefData *func_data = (BaaFunctionDefData *)function_node->data;
    if (!func_data)
    {
        set_llvm_error(context, L"Invalid function data");
        return false;
    }

    // Convert function name to char*
    char *c_function_name = wchar_to_char(func_data->name);
    if (!c_function_name)
    {
        set_llvm_error(context, L"Failed to convert function name");
        return false;
    }

    // TODO: Extract return type from func_data->return_type_node
    // For now assuming INT for simplicity or we need a helper to convert AST Type to BaaType
    // LLVMTypeRef return_type = baa_type_to_llvm_type(context, function->return_type);
    LLVMTypeRef return_type = LLVMInt32TypeInContext(context->llvm_context); // Temporary Fix

    // Create function type
    LLVMTypeRef *param_types = NULL;
    size_t param_count = 0;

    if (func_data->parameter_count > 0 && func_data->parameters)
    {
        param_types = (LLVMTypeRef *)malloc(sizeof(LLVMTypeRef) * func_data->parameter_count);
        // ... parameter processing placeholder ...
        // Needs logic to extract type from AST nodes
        for (size_t i = 0; i < func_data->parameter_count; i++)
        {
            param_types[i] = LLVMInt32TypeInContext(context->llvm_context); // Temporary Fix
        }
        param_count = func_data->parameter_count;
    }

    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, param_count, 0);

    if (param_types)
        free(param_types);

    // Create function
    LLVMValueRef llvm_function = LLVMAddFunction(context->llvm_module, c_function_name, function_type);
    free(c_function_name);

    if (!llvm_function)
    {
        set_llvm_error(context, L"Failed to create function");
        return false;
    }

    // Set function attributes
    LLVMSetFunctionCallConv(llvm_function, LLVMCCallConv);

    // Create entry basic block
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(
        context->llvm_context, llvm_function, "entry");

    LLVMPositionBuilderAtEnd(context->llvm_builder, entry_block);

    context->current_function = llvm_function;
    context->current_block = entry_block;

    // Generate code for function body
    if (func_data->body)
    {
        // Body is a Block Statement Node
        if (!baa_generate_llvm_statement(context, func_data->body))
        {
            return false;
        }
    }

    // Add return instruction if needed (default void/int)
    if (LLVMGetBasicBlockTerminator(entry_block) == NULL)
    {
        LLVMBuildRet(context->llvm_builder, LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, 0));
    }

    return true;
}

// Generate LLVM IR for a statement
bool baa_generate_llvm_statement(BaaLLVMContext *context, BaaStmt *stmt)
{
    if (!context || !stmt)
    {
        set_llvm_error(context, L"Invalid statement");
        return false;
    }

    switch (stmt->kind)
    {
    case BAA_STMT_IF:
    {
        if (!stmt->data)
        {
            set_llvm_error(context, L"Invalid if statement data");
            return false;
        }
        BaaIfStmt *if_stmt = (BaaIfStmt *)stmt->data;
        return baa_generate_llvm_if_statement(context, if_stmt);
    }

    case BAA_STMT_WHILE:
    {
        if (!stmt->data)
        {
            set_llvm_error(context, L"Invalid while statement data");
            return false;
        }
        BaaWhileStmt *while_stmt = (BaaWhileStmt *)stmt->data;
        return baa_generate_llvm_while_statement(context, while_stmt);
    }

    case BAA_STMT_RETURN:
    {
        if (!stmt->data)
        {
            set_llvm_error(context, L"Invalid return statement data");
            return false;
        }
        BaaReturnStmt *return_stmt = (BaaReturnStmt *)stmt->data;
        return baa_generate_llvm_return_statement(context, return_stmt);
    }

    case BAA_STMT_EXPR:
    {
        if (!stmt->data)
        {
            set_llvm_error(context, L"Invalid expression statement data");
            return false;
        }
        BaaExprStmt *expr_stmt = (BaaExprStmt *)stmt->data;

        // Generate expression and discard result
        LLVMValueRef result = baa_generate_llvm_expression(context, expr_stmt->expr); // Use expr
        return result != NULL;
    }

    case BAA_STMT_BLOCK:
    {
        if (!stmt->data)
        {
            set_llvm_error(context, L"Invalid block statement data");
            return false;
        }
        BaaBlock *block = (BaaBlock *)stmt->data;

        // Generate code for each statement in the block
        for (size_t i = 0; i < block->count; i++)
        {
            if (!baa_generate_llvm_statement(context, block->statements[i]))
            {
                return false;
            }
        }
        return true;
    }

    case BAA_STMT_VAR_DECL:
    {
        if (!stmt->data)
        {
            set_llvm_error(context, L"Invalid variable declaration data");
            return false;
        }
        BaaVarDeclStmt *var_decl = (BaaVarDeclStmt *)stmt->data;

        // Get variable name and type
        const wchar_t *var_name = var_decl->name;
        BaaType *var_type = var_decl->type;

        if (!var_name)
        {
            set_llvm_error(context, L"Invalid variable name");
            return false;
        }

        // Convert to LLVM type
        LLVMTypeRef llvm_type = baa_type_to_llvm_type(context, var_type);
        if (!llvm_type)
        {
            return false;
        }

        // Convert variable name to char*
        char *c_var_name = wchar_to_char(var_name);
        if (!c_var_name)
        {
            set_llvm_error(context, L"Failed to convert variable name");
            return false;
        }

        // Create an alloca instruction in the entry block for the variable
        LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(context->current_function);
        LLVMBasicBlockRef current_block_before_alloca = LLVMGetInsertBlock(context->llvm_builder);

        // Position builder at start of entry block
        LLVMPositionBuilderAtEnd(context->llvm_builder, entry_block);

        // Create alloca instruction
        LLVMValueRef alloca = LLVMBuildAlloca(context->llvm_builder, llvm_type, c_var_name);

        // Restore builder position
        LLVMPositionBuilderAtEnd(context->llvm_builder, current_block_before_alloca);

        // Initialize the variable if there's an initializer
        if (var_decl->initializer)
        {
            LLVMValueRef init_value = baa_generate_llvm_expression(context, var_decl->initializer);
            if (!init_value)
            {
                free(c_var_name);
                return false;
            }

            // Store the initial value in the variable
            LLVMBuildStore(context->llvm_builder, init_value, alloca);
        }

        free(c_var_name);
        return true;
    }

    default:
        set_llvm_error(context, L"Unsupported statement type");
        return false;
    }
}

// Generate LLVM IR for an expression
LLVMValueRef baa_generate_llvm_expression(BaaLLVMContext *context, BaaExpr *expr)
{
    if (!context || !expr)
    {
        set_llvm_error(context, L"Invalid expression");
        return NULL;
    }

    switch (expr->kind)
    {
    case BAA_EXPR_LITERAL:
    {
        // Handle literals using the new BaaLiteralData structure
        if (!expr->data)
        {
            set_llvm_error(context, L"Invalid literal expression data");
            return NULL;
        }

        BaaLiteralData *literal_data = (BaaLiteralData *)expr->data;

        switch (literal_data->kind)
        {
        case BAA_LITERAL_BOOL:
        {
            bool value = literal_data->bool_value;
            return LLVMConstInt(LLVMInt1TypeInContext(context->llvm_context), value ? 1 : 0, false);
        }

        case BAA_LITERAL_INT:
        {
            int value = literal_data->int_value;
            return LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), (unsigned long long)value, true);
        }

        case BAA_LITERAL_FLOAT:
        {
            float value = literal_data->float_value;
            return LLVMConstReal(LLVMFloatTypeInContext(context->llvm_context), value);
        }

        case BAA_LITERAL_CHAR:
        {
            wchar_t value = literal_data->char_value;
            return LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), (unsigned long long)value, false);
        }

        case BAA_LITERAL_STRING:
        {
            if (!literal_data->string_value)
            {
                set_llvm_error(context, L"Invalid string literal");
                return NULL;
            }

            // Convert wide string to UTF-8
            char *str = wchar_to_char(literal_data->string_value);
            if (!str)
            {
                set_llvm_error(context, L"Failed to convert string literal");
                return NULL;
            }

            // Create a constant global string
            LLVMValueRef global_str = LLVMAddGlobal(
                context->llvm_module,
                LLVMArrayType(LLVMInt8TypeInContext(context->llvm_context), strlen(str) + 1),
                "str");

            LLVMSetInitializer(global_str, LLVMConstString(str, strlen(str), true));
            LLVMSetLinkage(global_str, LLVMPrivateLinkage);
            LLVMSetGlobalConstant(global_str, true);
            LLVMSetUnnamedAddr(global_str, true);

            // Clean up the converted string
            free(str);

            // Create a pointer to the string
            LLVMValueRef indices[2] = {
                LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, false),
                LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, false)};

            return LLVMConstGEP2(
                LLVMTypeOf(global_str),
                global_str,
                indices,
                2);
        }

        case BAA_LITERAL_NULL:
        {
            // For null literals, create a null pointer
            return LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(context->llvm_context), 0));
        }

        default:
        {
            set_llvm_error(context, L"Unsupported literal type");
            return NULL;
        }
        }
    }

    case BAA_EXPR_VARIABLE:
    {
        if (!expr->data)
        {
            set_llvm_error(context, L"Invalid variable expression data");
            return NULL;
        }

        BaaVariableExpr *var_expr = (BaaVariableExpr *)expr->data;
        const wchar_t *name = var_expr->name;

        if (!name)
        {
            set_llvm_error(context, L"Invalid variable name");
            return NULL;
        }

        // Variable lookup will need to be implemented with a proper variable symbol table
        // This is a placeholder that will fail at runtime
        wchar_t error[256];
        swprintf(error, 256, L"Variable '%ls' not found", name);
        set_llvm_error(context, error);
        return NULL;
    }

    case BAA_EXPR_BINARY:
    {
        if (!expr->data)
        {
            set_llvm_error(context, L"Invalid binary expression data");
            return NULL;
        }

        BaaBinaryExpr *binary_expr = (BaaBinaryExpr *)expr->data;

        // Generate code for left and right expressions
        LLVMValueRef left = baa_generate_llvm_expression(context, binary_expr->left);
        if (!left)
            return NULL;

        LLVMValueRef right = baa_generate_llvm_expression(context, binary_expr->right);
        if (!right)
            return NULL;

        // Generate the binary operation based on op kind
        switch (binary_expr->op)
        {
        case BAA_OP_ADD:
            return LLVMBuildAdd(context->llvm_builder, left, right, "addtmp");

        case BAA_OP_SUB:
            return LLVMBuildSub(context->llvm_builder, left, right, "subtmp");

        case BAA_OP_MUL:
            return LLVMBuildMul(context->llvm_builder, left, right, "multmp");

        case BAA_OP_DIV:
            // Check for signed vs unsigned division
            // For now, we'll assume signed integers
            return LLVMBuildSDiv(context->llvm_builder, left, right, "divtmp");

        case BAA_OP_EQ:
            return LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, left, right, "eqtmp");

        case BAA_OP_NE:
            return LLVMBuildICmp(context->llvm_builder, LLVMIntNE, left, right, "netmp");

        case BAA_OP_LT:
            return LLVMBuildICmp(context->llvm_builder, LLVMIntSLT, left, right, "lttmp");

        case BAA_OP_LE:
            return LLVMBuildICmp(context->llvm_builder, LLVMIntSLE, left, right, "letmp");

        case BAA_OP_GT:
            return LLVMBuildICmp(context->llvm_builder, LLVMIntSGT, left, right, "gttmp");

        case BAA_OP_GE:
            return LLVMBuildICmp(context->llvm_builder, LLVMIntSGE, left, right, "getmp");

        default:
            set_llvm_error(context, L"Unsupported binary operation");
            return NULL;
        }
    }

    case BAA_EXPR_CALL:
    {
        if (!expr->data)
        {
            set_llvm_error(context, L"Invalid call expression data");
            return NULL;
        }

        BaaCallExpr *call_expr = (BaaCallExpr *)expr->data;

        if (!call_expr->callee || !call_expr->callee->data)
        {
            set_llvm_error(context, L"Invalid function callee");
            return NULL;
        }

        // For simplicity, we assume the callee is a variable expression
        if (call_expr->callee->kind != BAA_EXPR_VARIABLE)
        {
            set_llvm_error(context, L"Only direct function calls are supported");
            return NULL;
        }

        BaaVariableExpr *var_expr = (BaaVariableExpr *)call_expr->callee->data;
        const wchar_t *func_name = var_expr->name;

        // Convert function name to char*
        char *c_func_name = wchar_to_char(func_name);
        if (!c_func_name)
        {
            set_llvm_error(context, L"Failed to convert function name");
            return NULL;
        }

        // Get the function from the module
        LLVMValueRef function = LLVMGetNamedFunction(context->llvm_module, c_func_name);
        free(c_func_name);

        if (!function)
        {
            wchar_t error[256];
            swprintf(error, 256, L"Unknown function: '%ls'", func_name);
            set_llvm_error(context, error);
            return NULL;
        }

        // Check that argument count matches parameter count
        size_t expected_args = LLVMCountParams(function);
        if (call_expr->argument_count != expected_args)
        {
            wchar_t error[256];
            swprintf(error, 256, L"Incorrect number of arguments for '%ls': expected %zu, got %zu",
                     func_name, expected_args, call_expr->argument_count);
            set_llvm_error(context, error);
            return NULL;
        }

        // Generate code for each argument
        LLVMValueRef *args = NULL;
        if (call_expr->argument_count > 0)
        {
            args = (LLVMValueRef *)malloc(sizeof(LLVMValueRef) * call_expr->argument_count);
            if (!args)
            {
                set_llvm_error(context, L"Memory allocation failure");
                return NULL;
            }

            for (size_t i = 0; i < call_expr->argument_count; i++)
            {
                args[i] = baa_generate_llvm_expression(context, call_expr->arguments[i]);
                if (!args[i])
                {
                    free(args);
                    return NULL;
                }
            }
        }

        // Create the function call
        LLVMValueRef call = LLVMBuildCall2(
            context->llvm_builder,
            LLVMGetElementType(LLVMTypeOf(function)),
            function,
            args,
            call_expr->argument_count,
            "calltmp");

        // Clean up
        if (args)
            free(args);

        return call;
    }

    default:
        set_llvm_error(context, L"Unsupported expression type");
        return NULL;
    }
}

// Generate LLVM IR for if statement
bool baa_generate_llvm_if_statement(BaaLLVMContext *context, BaaIfStmt *if_stmt)
{
    if (!context || !if_stmt)
    {
        set_llvm_error(context, L"Invalid if statement");
        return false;
    }

    // Generate condition expression
    LLVMValueRef condition = baa_generate_llvm_expression(context, if_stmt->condition);
    if (!condition)
    {
        return false;
    }

    // Convert condition to boolean if needed
    if (LLVMTypeOf(condition) != LLVMInt1TypeInContext(context->llvm_context))
    {
        condition = LLVMBuildICmp(context->llvm_builder, LLVMIntNE,
                                  condition,
                                  LLVMConstInt(LLVMTypeOf(condition), 0, 0),
                                  "ifcond");
    }

    // Create basic blocks for then, else, and merge
    LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(
        context->llvm_context, context->current_function, "then");
    LLVMBasicBlockRef else_block = LLVMAppendBasicBlockInContext(
        context->llvm_context, context->current_function, "else");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(
        context->llvm_context, context->current_function, "ifcont");

    // Create conditional branch instruction
    LLVMBuildCondBr(context->llvm_builder, condition, then_block, else_block);

    // Generate code for then block
    LLVMPositionBuilderAtEnd(context->llvm_builder, then_block);

    // Process then body
    if (if_stmt->if_body)
    {
        // Iterate through statements in the block
        for (size_t i = 0; i < if_stmt->if_body->count; ++i)
        {
            if (!baa_generate_llvm_statement(context, if_stmt->if_body->statements[i]))
            {
                return false;
            }
        }
    }

    // Add branch to merge block if no terminator
    if (LLVMGetBasicBlockTerminator(then_block) == NULL)
    {
        LLVMBuildBr(context->llvm_builder, merge_block);
    }

    // Generate code for else block
    LLVMPositionBuilderAtEnd(context->llvm_builder, else_block);
    if (if_stmt->else_body)
    {
        for (size_t i = 0; i < if_stmt->else_body->count; ++i)
        {
            if (!baa_generate_llvm_statement(context, if_stmt->else_body->statements[i]))
            {
                return false;
            }
        }

        // Add branch to merge block if no terminator
        if (LLVMGetBasicBlockTerminator(else_block) == NULL)
        {
            LLVMBuildBr(context->llvm_builder, merge_block);
        }

        // Position builder at start of merge block
        LLVMPositionBuilderAtEnd(context->llvm_builder, merge_block);

        return true;
    }

    // Generate LLVM IR for while statement
    bool baa_generate_llvm_while_statement(BaaLLVMContext * context, BaaWhileStmt * while_stmt)
    {
        if (!context || !while_stmt)
        {
            set_llvm_error(context, L"Invalid while statement");
            return false;
        }

        // Create basic blocks for condition, loop body, and after
        LLVMBasicBlockRef cond_block = LLVMAppendBasicBlockInContext(
            context->llvm_context, context->current_function, "while.cond");
        LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(
            context->llvm_context, context->current_function, "while.body");
        LLVMBasicBlockRef after_block = LLVMAppendBasicBlockInContext(
            context->llvm_context, context->current_function, "while.end");

        // Branch to condition block
        LLVMBuildBr(context->llvm_builder, cond_block);

        // Generate condition
        LLVMPositionBuilderAtEnd(context->llvm_builder, cond_block);
        LLVMValueRef condition = baa_generate_llvm_expression(context, while_stmt->condition);
        if (!condition)
        {
            return false;
        }

        // Convert condition to boolean if needed
        if (LLVMTypeOf(condition) != LLVMInt1TypeInContext(context->llvm_context))
        {
            condition = LLVMBuildICmp(context->llvm_builder, LLVMIntNE,
                                      condition,
                                      LLVMConstInt(LLVMTypeOf(condition), 0, 0),
                                      "whilecond");
        }

        // Create conditional branch
        LLVMBuildCondBr(context->llvm_builder, condition, body_block, after_block);

        // Generate loop body
        LLVMPositionBuilderAtEnd(context->llvm_builder, body_block);

        // Process body statement
        if (while_stmt->body)
        {
            for (size_t i = 0; i < while_stmt->body->count; ++i)
            {
                if (!baa_generate_llvm_statement(context, while_stmt->body->statements[i]))
                {
                    return false;
                }
            }
        }

        // Add branch back to condition block if no terminator
        if (LLVMGetBasicBlockTerminator(body_block) == NULL)
        {
            LLVMBuildBr(context->llvm_builder, cond_block);
        }

        // Position builder at start of after block
        LLVMPositionBuilderAtEnd(context->llvm_builder, after_block);

        return true;
    }

    // Generate LLVM IR for return statement
    bool baa_generate_llvm_return_statement(BaaLLVMContext * context, BaaReturnStmt * return_stmt)
    {
        if (!context || !return_stmt)
        {
            set_llvm_error(context, L"Invalid return statement");
            return false;
        }

        if (return_stmt->value)
        {
            // Generate expression for return value
            LLVMValueRef return_value = baa_generate_llvm_expression(context, return_stmt->value);
            if (!return_value)
            {
                return false;
            }

            // Build return instruction with value
            LLVMBuildRet(context->llvm_builder, return_value);
        }
        else
        {
            // Build void return instruction
            LLVMBuildRetVoid(context->llvm_builder);
        }

        return true;
    }

    // Write LLVM IR to file
    bool baa_write_llvm_ir_to_file(BaaLLVMContext * context, const wchar_t *filename)
    {
        if (!context || !filename)
        {
            set_llvm_error(context, L"Invalid filename");
            return false;
        }

        // Convert filename to char*
        char *c_filename = wchar_to_char(filename);
        if (!c_filename)
        {
            set_llvm_error(context, L"Failed to convert filename");
            return false;
        }

        // Write LLVM IR to file
        if (LLVMPrintModuleToFile(context->llvm_module, c_filename, NULL) != 0)
        {
            free(c_filename);
            set_llvm_error(context, L"Failed to write LLVM IR to file");
            return false;
        }

        free(c_filename);
        return true;
    }

    // Compile LLVM IR to object file
    bool baa_compile_llvm_ir_to_object(BaaLLVMContext * context, const wchar_t *filename)
    {
        if (!context || !filename)
        {
            set_llvm_error(context, L"Invalid filename for object file");
            return false;
        }

        // Convert filename to char*
        char *c_filename = wchar_to_char(filename);
        if (!c_filename)
        {
            set_llvm_error(context, L"Failed to convert filename");
            return false;
        }

        // Get the target triple for the current platform
        char *target_triple = LLVMGetDefaultTargetTriple();
        LLVMSetTarget(context->llvm_module, target_triple);

        // Initialize all targets
        LLVMInitializeAllTargetInfos();
        LLVMInitializeAllTargets();
        LLVMInitializeAllTargetMCs();
        LLVMInitializeAllAsmParsers();
        LLVMInitializeAllAsmPrinters();

        // Get the target
        char *error = NULL;
        LLVMTargetRef target;
        if (LLVMGetTargetFromTriple(target_triple, &target, &error) != 0)
        {
            free(c_filename);
            LLVMDisposeMessage(target_triple);

            if (error)
            {
                wchar_t *werror = (wchar_t *)malloc((strlen(error) + 1) * sizeof(wchar_t));
                if (werror)
                {
                    for (size_t i = 0; i < strlen(error); i++)
                    {
                        werror[i] = (wchar_t)error[i];
                    }
                    werror[strlen(error)] = L'\0';
                    set_llvm_error(context, werror);
                }
                else
                {
                    set_llvm_error(context, L"Failed to get target from triple");
                }
                LLVMDisposeMessage(error);
            }
            else
            {
                set_llvm_error(context, L"Failed to get target from triple");
            }

            return false;
        }

        // Create a target machine
        char *cpu = LLVMGetHostCPUName();
        char *features = LLVMGetHostCPUFeatures();

        context->llvm_target_machine = LLVMCreateTargetMachine(
            target,
            target_triple,
            cpu,
            features,
            LLVMCodeGenLevelDefault,
            LLVMRelocDefault,
            LLVMCodeModelDefault);

        LLVMDisposeMessage(target_triple);
        LLVMDisposeMessage(cpu);
        LLVMDisposeMessage(features);

        if (!context->llvm_target_machine)
        {
            free(c_filename);
            set_llvm_error(context, L"Failed to create target machine");
            return false;
        }

        // Set data layout
        char *data_layout = LLVMCopyStringRepOfTargetData(
            LLVMCreateTargetDataLayout(context->llvm_target_machine));
        LLVMSetDataLayout(context->llvm_module, data_layout);
        LLVMDisposeMessage(data_layout);

        // Create output file
        if (LLVMTargetMachineEmitToFile(
                context->llvm_target_machine,
                context->llvm_module,
                c_filename,
                LLVMObjectFile,
                &error) != 0)
        {

            free(c_filename);

            if (error)
            {
                wchar_t *werror = (wchar_t *)malloc((strlen(error) + 1) * sizeof(wchar_t));
                if (werror)
                {
                    for (size_t i = 0; i < strlen(error); i++)
                    {
                        werror[i] = (wchar_t)error[i];
                    }
                    werror[strlen(error)] = L'\0';
                    set_llvm_error(context, werror);
                }
                else
                {
                    set_llvm_error(context, L"Failed to emit object file");
                }
                LLVMDisposeMessage(error);
            }
            else
            {
                set_llvm_error(context, L"Failed to emit object file");
            }

            return false;
        }

        free(c_filename);

        // Clean up
        LLVMDisposeTargetMachine(context->llvm_target_machine);
        context->llvm_target_machine = NULL;

        return true;
    }

    // Error handling
    const wchar_t *baa_get_llvm_error(BaaLLVMContext * context)
    {
        if (!context)
        {
            return L"Invalid context";
        }

        return context->error_message;
    }

    void baa_clear_llvm_error(BaaLLVMContext * context)
    {
        if (!context)
        {
            return;
        }

        context->had_error = false;
        context->error_message = NULL;
    }
}

#endif
