/**
 * @file codegen.c
 * @brief Generates x86_64 Assembly (Windows ABI) from AST.
 */

#include "baa.h"
#include <string.h>

// --- Symbol Table System ---

typedef enum { SCOPE_GLOBAL, SCOPE_LOCAL } ScopeType;

typedef struct {
    char name[32];
    ScopeType scope;
    int offset; // Locals: Offset from RBP (e.g., -8). Globals: 0 (Use label)
} Symbol;

Symbol global_symbols[100];
int global_count = 0;

Symbol local_symbols[100];
int local_count = 0;
int current_stack_offset = 0; // Tracks RSP relative to RBP

int label_counter = 0; // For If/While

void add_global(const char* name) {
    strcpy(global_symbols[global_count].name, name);
    global_symbols[global_count].scope = SCOPE_GLOBAL;
    global_symbols[global_count].offset = 0;
    global_count++;
}

void enter_function_scope() {
    local_count = 0;
    current_stack_offset = 0; // Reset for new stack frame
}

void add_local(const char* name) {
    current_stack_offset -= 8; // 8 bytes (64-bit)
    strcpy(local_symbols[local_count].name, name);
    local_symbols[local_count].scope = SCOPE_LOCAL;
    local_symbols[local_count].offset = current_stack_offset;
    local_count++;
}

Symbol* lookup_symbol(const char* name) {
    // 1. Check Locals (Shadowing)
    for (int i = 0; i < local_count; i++) {
        if (strcmp(local_symbols[i].name, name) == 0) return &local_symbols[i];
    }
    // 2. Check Globals
    for (int i = 0; i < global_count; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) return &global_symbols[i];
    }
    printf("Codegen Error: Undefined symbol '%s'\n", name);
    exit(1);
}

// --- Generator ---

// Forward declaration
void gen_expr(Node* node, FILE* file);

void gen_expr(Node* node, FILE* file) {
    if (node->type == NODE_INT) {
        fprintf(file, "    mov $%d, %%rax\n", node->data.integer.value);
    }
    else if (node->type == NODE_VAR_REF) {
        Symbol* sym = lookup_symbol(node->data.var_ref.name);
        if (sym->scope == SCOPE_LOCAL) {
            fprintf(file, "    mov %d(%%rbp), %%rax\n", sym->offset);
        } else {
            fprintf(file, "    mov %s(%%rip), %%rax\n", sym->name);
        }
    }
    else if (node->type == NODE_CALL_EXPR) {
        // Function Call Expression (x = foo())
        // Windows ABI: First 4 args in RCX, RDX, R8, R9. Rest on stack.
        // Simplified v0.0.8: Only supporting up to 4 args for now.
        
        const char* regs[] = {"%rcx", "%rdx", "%r8", "%r9"};
        int arg_idx = 0;
        Node* arg = node->data.call.args;
        
        while (arg != NULL) {
            gen_expr(arg, file); // Calc arg -> RAX
            fprintf(file, "    push %%rax\n"); // Save temporarily
            arg = arg->next;
            arg_idx++;
        }

        // Pop into registers (Reverse order)
        for (int i = arg_idx - 1; i >= 0; i--) {
            if (i < 4) {
                fprintf(file, "    pop %s\n", regs[i]);
            } else {
                // Stack args not supported yet in this simplified version
                printf("Error: Too many arguments (Max 4 supported in v0.0.8)\n");
                exit(1);
            }
        }

        // Shadow Space allocation (Required by Win ABI)
        // Stack must be 16-byte aligned here. 
        // We allocated a huge frame in prologue, so we assume alignment roughly.
        fprintf(file, "    sub $32, %%rsp\n"); 
        fprintf(file, "    call %s\n", node->data.call.name);
        fprintf(file, "    add $32, %%rsp\n"); // Cleanup shadow
    }
    else if (node->type == NODE_BIN_OP) {
        gen_expr(node->data.bin_op.right, file);
        fprintf(file, "    push %%rax\n");
        gen_expr(node->data.bin_op.left, file);
        fprintf(file, "    pop %%rbx\n");
        
        if (node->data.bin_op.op == OP_ADD) fprintf(file, "    add %%rbx, %%rax\n");
        else if (node->data.bin_op.op == OP_SUB) fprintf(file, "    sub %%rbx, %%rax\n");
        else if (node->data.bin_op.op == OP_EQ) {
            fprintf(file, "    cmp %%rbx, %%rax\n");
            fprintf(file, "    sete %%al\n");
            fprintf(file, "    movzbq %%al, %%rax\n");
        }
        else if (node->data.bin_op.op == OP_NEQ) {
            fprintf(file, "    cmp %%rbx, %%rax\n");
            fprintf(file, "    setne %%al\n");
            fprintf(file, "    movzbq %%al, %%rax\n");
        }
    }
}

void codegen(Node* node, FILE* file) {
    if (node == NULL) return;

    if (node->type == NODE_PROGRAM) {
        // 1. Data Section (Globals)
        fprintf(file, ".section .rdata,\"dr\"\n");
        fprintf(file, "fmt_int: .asciz \"%%d\\n\"\n");
        
        fprintf(file, ".data\n");
        Node* decl = node->data.program.declarations;
        while (decl != NULL) {
            if (decl->type == NODE_VAR_DECL && decl->data.var_decl.is_global) {
                // BUG FIX: Check for initializer value
                int init_value = 0;
                if (decl->data.var_decl.expression != NULL && 
                    decl->data.var_decl.expression->type == NODE_INT) {
                    init_value = decl->data.var_decl.expression->data.integer.value;
                }
                
                fprintf(file, "%s: .quad %d\n", decl->data.var_decl.name, init_value);
                add_global(decl->data.var_decl.name);
            }
            decl = decl->next;
        }

        // 2. Text Section (Functions)
        fprintf(file, ".text\n");
        
        decl = node->data.program.declarations;
        while (decl != NULL) {
            if (decl->type == NODE_FUNC_DEF) {
                codegen(decl, file);
            }
            decl = decl->next;
        }
    }
    else if (node->type == NODE_FUNC_DEF) {
        enter_function_scope();
        
        // Handle Main Entry Point mapping
        if (strcmp(node->data.func_def.name, "الرئيسية") == 0) {
            fprintf(file, ".globl main\nmain:\n");
        } else {
            fprintf(file, "%s:\n", node->data.func_def.name);
        }

        // PROLOGUE
        fprintf(file, "    push %%rbp\n");
        fprintf(file, "    mov %%rsp, %%rbp\n");
        // Allocate generous stack frame (256 bytes + alignment)
        // 264 + 8 (rbp) = 272. 272 % 16 == 0. Aligned!
        fprintf(file, "    sub $264, %%rsp\n"); 

        // SPILL PARAMETERS TO STACK (Treat as locals)
        Node* param = node->data.func_def.params;
        const char* regs[] = {"%rcx", "%rdx", "%r8", "%r9"};
        int p_idx = 0;
        
        while (param != NULL) {
            add_local(param->data.var_decl.name);
            int offset = lookup_symbol(param->data.var_decl.name)->offset;
            if (p_idx < 4) {
                fprintf(file, "    mov %s, %d(%%rbp)\n", regs[p_idx], offset);
            }
            param = param->next;
            p_idx++;
        }

        // BODY
        codegen(node->data.func_def.body, file);

        // DEFAULT RETURN (Void/0)
        fprintf(file, "    mov $0, %%rax\n");
        fprintf(file, "    leave\n    ret\n");
    }
    else if (node->type == NODE_BLOCK) {
        Node* stmt = node->data.block.statements;
        while (stmt != NULL) { codegen(stmt, file); stmt = stmt->next; }
    }
    else if (node->type == NODE_VAR_DECL) {
        // Local variable declaration
        gen_expr(node->data.var_decl.expression, file);
        add_local(node->data.var_decl.name);
        int offset = lookup_symbol(node->data.var_decl.name)->offset;
        fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
    }
    else if (node->type == NODE_ASSIGN) {
        gen_expr(node->data.assign_stmt.expression, file);
        Symbol* sym = lookup_symbol(node->data.assign_stmt.name);
        if (sym->scope == SCOPE_LOCAL) {
            fprintf(file, "    mov %%rax, %d(%%rbp)\n", sym->offset);
        } else {
            fprintf(file, "    mov %%rax, %s(%%rip)\n", sym->name);
        }
    }
    else if (node->type == NODE_CALL_STMT) {
        // Statement wrapper for call expression logic
        // We need to construct a temp expression node or refactor gen_expr to be callable
        // Hack: Create a temporary Node to pass to gen_expr
        Node temp;
        temp.type = NODE_CALL_EXPR;
        temp.data.call = node->data.call;
        gen_expr(&temp, file);
    }
    else if (node->type == NODE_RETURN) {
        gen_expr(node->data.return_stmt.expression, file);
        fprintf(file, "    leave\n    ret\n");
    }
    else if (node->type == NODE_PRINT) {
        gen_expr(node->data.print_stmt.expression, file);
        fprintf(file, "    mov %%rax, %%rdx\n");
        fprintf(file, "    lea fmt_int(%%rip), %%rcx\n");
        fprintf(file, "    sub $32, %%rsp\n"); // Shadow space for printf
        fprintf(file, "    call printf\n");
        fprintf(file, "    add $32, %%rsp\n");
    }
    else if (node->type == NODE_IF) {
        int label_end = label_counter++;
        gen_expr(node->data.if_stmt.condition, file);
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);
        codegen(node->data.if_stmt.then_branch, file);
        fprintf(file, ".Lend_%d:\n", label_end);
    }
    else if (node->type == NODE_WHILE) {
        int label_start = label_counter++;
        int label_end = label_counter++;
        fprintf(file, ".Lstart_%d:\n", label_start);
        gen_expr(node->data.while_stmt.condition, file);
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);
        codegen(node->data.while_stmt.body, file);
        fprintf(file, "    jmp .Lstart_%d\n", label_start);
        fprintf(file, ".Lend_%d:\n", label_end);
    }
}