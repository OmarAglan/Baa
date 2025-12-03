#include "baa/codegen/llvm_codegen.h"
#include "baa/utils/utils.h"
#include "baa/utils/errors.h"
#include "baa/ast/ast_types.h"
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

    // Initialize LLVM targets
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
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
        // Global variables can be added here in future
    }

    // Verify the module
    char *error = NULL;
    LLVMVerifyModule(context->llvm_module, LLVMPrintMessageAction, &error);
    if (error)
    {
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

    // Assuming Int32 return type for simplicity until Type Resolution is fully integrated
    LLVMTypeRef return_type = LLVMInt32TypeInContext(context->llvm_context);

    // Create function type
    LLVMTypeRef *param_types = NULL;
    size_t param_count = 0;

    if (func_data->parameter_count > 0 && func_data->parameters)
    {
        param_types = (LLVMTypeRef *)malloc(sizeof(LLVMTypeRef) * func_data->parameter_count);
        if (!param_types)
        {
            free(c_function_name);
            set_llvm_error(context, L"Memory allocation failure");
            return false;
        }

        for (size_t i = 0; i < func_data->parameter_count; i++)
        {
            param_types[i] = LLVMInt32TypeInContext(context->llvm_context);
        }
        param_count = func_data->parameter_count;
    }

    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, param_count, 0);

    if (param_types)
    {
        free(param_types);
    }

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

    // Handle Parameters (Name them and create allocas)
    if (func_data->parameter_count > 0 && func_data->parameters)
    {
        for (size_t i = 0; i < func_data->parameter_count; i++)
        {
            LLVMValueRef param = LLVMGetParam(llvm_function, i);
            BaaParameterData* param_data = (BaaParameterData*)func_data->parameters[i]->data;
            
            char *param_name = NULL;
            if (param_data && param_data->name) {
                param_name = wchar_to_char(param_data->name);
                LLVMSetValueName2(param, param_name, strlen(param_name));
            } else {
                param_name = strdup("arg");
            }

            LLVMValueRef alloca = LLVMBuildAlloca(context->llvm_builder, LLVMTypeOf(param), param_name);
            LLVMBuildStore(context->llvm_builder, param, alloca);
            
            if (param_name) free(param_name);
        }
    }

    // Generate code for function body
    if (func_data->body)
    {
        if (!baa_generate_llvm_statement(context, func_data->body))
        {
            return false;
        }
    }

    // Add return instruction if needed
    if (LLVMGetBasicBlockTerminator(entry_block) == NULL)
    {
        LLVMBuildRet(context->llvm_builder, LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, 0));
    }

    return true;
}

// Generate LLVM IR for a statement
bool baa_generate_llvm_statement(BaaLLVMContext *context, BaaNode *stmt_node)
{
    if (!context || !stmt_node)
    {
        set_llvm_error(context, L"Invalid statement node");
        return false;
    }

    switch (stmt_node->kind)
    {
    case BAA_NODE_KIND_IF_STMT:
        return baa_generate_llvm_if_statement(context, (BaaIfStmtData*)stmt_node->data);

    case BAA_NODE_KIND_WHILE_STMT:
        return baa_generate_llvm_while_statement(context, (BaaWhileStmtData*)stmt_node->data);

    case BAA_NODE_KIND_RETURN_STMT:
        return baa_generate_llvm_return_statement(context, (BaaReturnStmtData*)stmt_node->data);

    case BAA_NODE_KIND_EXPR_STMT:
    {
        BaaExprStmtData *data = (BaaExprStmtData*)stmt_node->data;
        if (!data) return false;
        LLVMValueRef result = baa_generate_llvm_expression(context, data->expression);
        return result != NULL;
    }

    case BAA_NODE_KIND_BLOCK_STMT:
    {
        BaaBlockStmtData *data = (BaaBlockStmtData*)stmt_node->data;
        if (!data) return false;
        for (size_t i = 0; i < data->count; i++)
        {
            if (!baa_generate_llvm_statement(context, data->statements[i]))
            {
                return false;
            }
        }
        return true;
    }

    case BAA_NODE_KIND_VAR_DECL_STMT:
    {
        BaaVarDeclData *var_decl = (BaaVarDeclData*)stmt_node->data;
        if (!var_decl) return false;

        // Simplify type handling for now - assume int32
        LLVMTypeRef llvm_type = LLVMInt32TypeInContext(context->llvm_context);
        
        char *c_var_name = wchar_to_char(var_decl->name);
        if (!c_var_name) return false;

        LLVMBasicBlockRef entry_block = LLVMGetEntryBasicBlock(context->current_function);
        LLVMBasicBlockRef current_block = LLVMGetInsertBlock(context->llvm_builder);
        
        LLVMPositionBuilderAtEnd(context->llvm_builder, entry_block);
        LLVMValueRef alloca = LLVMBuildAlloca(context->llvm_builder, llvm_type, c_var_name);
        LLVMPositionBuilderAtEnd(context->llvm_builder, current_block);

        if (var_decl->initializer_expr)
        {
            LLVMValueRef init_val = baa_generate_llvm_expression(context, var_decl->initializer_expr);
            if (init_val)
                LLVMBuildStore(context->llvm_builder, init_val, alloca);
        }

        free(c_var_name);
        return true;
    }

    default:
        // Ignore other statements for now
        return true;
    }
}

// Generate LLVM IR for an expression
LLVMValueRef baa_generate_llvm_expression(BaaLLVMContext *context, BaaNode *expr_node)
{
    if (!context || !expr_node)
        return NULL;

    switch (expr_node->kind)
    {
    case BAA_NODE_KIND_LITERAL_EXPR:
    {
        BaaLiteralExprData *lit = (BaaLiteralExprData*)expr_node->data;
        if (!lit) return NULL;

        switch (lit->literal_kind)
        {
        case BAA_LITERAL_KIND_INT:
            return LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), (unsigned long long)lit->value.int_value, true);
        case BAA_LITERAL_KIND_BOOL:
            return LLVMConstInt(LLVMInt1TypeInContext(context->llvm_context), lit->value.bool_value ? 1 : 0, false);
        case BAA_LITERAL_KIND_FLOAT:
            return LLVMConstReal(LLVMFloatTypeInContext(context->llvm_context), lit->value.float_value);
        default:
            return LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, false);
        }
    }

    case BAA_NODE_KIND_IDENTIFIER_EXPR:
    {
        // Placeholder for identifier lookup
        return LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, false);
    }

    case BAA_NODE_KIND_BINARY_EXPR:
    {
        BaaBinaryExprData *bin = (BaaBinaryExprData*)expr_node->data;
        LLVMValueRef left = baa_generate_llvm_expression(context, bin->left_operand);
        LLVMValueRef right = baa_generate_llvm_expression(context, bin->right_operand);
        if (!left || !right) return NULL;

        switch (bin->operator_kind)
        {
        case BAA_BINARY_OP_ADD: return LLVMBuildAdd(context->llvm_builder, left, right, "addtmp");
        case BAA_BINARY_OP_SUBTRACT: return LLVMBuildSub(context->llvm_builder, left, right, "subtmp");
        case BAA_BINARY_OP_MULTIPLY: return LLVMBuildMul(context->llvm_builder, left, right, "multmp");
        case BAA_BINARY_OP_DIVIDE: return LLVMBuildSDiv(context->llvm_builder, left, right, "divtmp");
        default: return NULL;
        }
    }
    
    case BAA_NODE_KIND_CALL_EXPR:
    {
        BaaCallExprData *call = (BaaCallExprData*)expr_node->data;
        if (!call) return NULL;
        return LLVMConstInt(LLVMInt32TypeInContext(context->llvm_context), 0, false);
    }

    default:
        return NULL;
    }
}

bool baa_generate_llvm_if_statement(BaaLLVMContext *context, BaaIfStmtData *if_stmt)
{
    if (!context || !if_stmt) return false;

    LLVMValueRef cond = baa_generate_llvm_expression(context, if_stmt->condition_expr);
    if (!cond) return false;

    cond = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, false), "ifcond");

    LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(context->llvm_context, context->current_function, "then");
    LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(context->llvm_context, context->current_function, "else");
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(context->llvm_context, context->current_function, "ifcont");

    LLVMBuildCondBr(context->llvm_builder, cond, then_bb, else_bb);

    LLVMPositionBuilderAtEnd(context->llvm_builder, then_bb);
    if (if_stmt->then_stmt) baa_generate_llvm_statement(context, if_stmt->then_stmt);
    if (!LLVMGetBasicBlockTerminator(then_bb)) LLVMBuildBr(context->llvm_builder, merge_bb);

    LLVMPositionBuilderAtEnd(context->llvm_builder, else_bb);
    if (if_stmt->else_stmt) baa_generate_llvm_statement(context, if_stmt->else_stmt);
    if (!LLVMGetBasicBlockTerminator(else_bb)) LLVMBuildBr(context->llvm_builder, merge_bb);

    LLVMPositionBuilderAtEnd(context->llvm_builder, merge_bb);
    return true;
}

bool baa_generate_llvm_while_statement(BaaLLVMContext *context, BaaWhileStmtData *while_stmt)
{
    if (!context || !while_stmt) return false;

    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(context->llvm_context, context->current_function, "while.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(context->llvm_context, context->current_function, "while.body");
    LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(context->llvm_context, context->current_function, "while.end");

    LLVMBuildBr(context->llvm_builder, cond_bb);

    LLVMPositionBuilderAtEnd(context->llvm_builder, cond_bb);
    LLVMValueRef cond = baa_generate_llvm_expression(context, while_stmt->condition_expr);
    if (!cond) return false;
    cond = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, cond, LLVMConstInt(LLVMTypeOf(cond), 0, false), "loopcond");
    LLVMBuildCondBr(context->llvm_builder, cond, body_bb, end_bb);

    LLVMPositionBuilderAtEnd(context->llvm_builder, body_bb);
    if (while_stmt->body_stmt) baa_generate_llvm_statement(context, while_stmt->body_stmt);
    if (!LLVMGetBasicBlockTerminator(body_bb)) LLVMBuildBr(context->llvm_builder, cond_bb);

    LLVMPositionBuilderAtEnd(context->llvm_builder, end_bb);
    return true;
}

bool baa_generate_llvm_return_statement(BaaLLVMContext *context, BaaReturnStmtData *return_stmt)
{
    if (!context || !return_stmt) return false;

    if (return_stmt->value_expr)
    {
        LLVMValueRef ret_val = baa_generate_llvm_expression(context, return_stmt->value_expr);
        if (ret_val) LLVMBuildRet(context->llvm_builder, ret_val);
        else return false;
    }
    else
    {
        LLVMBuildRetVoid(context->llvm_builder);
    }
    return true;
}

bool baa_write_llvm_ir_to_file(BaaLLVMContext *context, const wchar_t *filename)
{
    if (!context || !filename) return false;
    char *c_filename = wchar_to_char(filename);
    if (!c_filename) return false;

    char *error = NULL;
    if (LLVMPrintModuleToFile(context->llvm_module, c_filename, &error) != 0)
    {
        free(c_filename);
        if (error) LLVMDisposeMessage(error);
        return false;
    }
    free(c_filename);
    return true;
}

bool baa_compile_llvm_ir_to_object(BaaLLVMContext *context, const wchar_t *filename)
{
    if (!context || !filename) return false;
    char *c_filename = wchar_to_char(filename);
    if (!c_filename) return false;

    // Default triple
    char *triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(context->llvm_module, triple);

    char *error = NULL;
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error)) {
        free(c_filename);
        LLVMDisposeMessage(triple);
        if (error) LLVMDisposeMessage(error);
        return false;
    }

    char *cpu = LLVMGetHostCPUName();
    char *features = LLVMGetHostCPUFeatures();
    
    context->llvm_target_machine = LLVMCreateTargetMachine(target, triple, cpu, features, LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    
    LLVMDisposeMessage(triple);
    LLVMDisposeMessage(cpu);
    LLVMDisposeMessage(features);

    if (LLVMTargetMachineEmitToFile(context->llvm_target_machine, context->llvm_module, c_filename, LLVMObjectFile, &error)) {
        free(c_filename);
        if (error) LLVMDisposeMessage(error);
        return false;
    }

    free(c_filename);
    return true;
}

const wchar_t *baa_get_llvm_error(BaaLLVMContext *context)
{
    return context ? context->error_message : L"Invalid Context";
}

void baa_clear_llvm_error(BaaLLVMContext *context)
{
    if (context)
    {
        context->had_error = false;
        context->error_message = NULL;
    }
}

#endif
