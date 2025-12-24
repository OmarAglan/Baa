#include "baa.h"
#include <string.h>
// --- Simple Symbol Table ---
// Maps names to stack offsets (e.g., "x" -> -8)
typedef struct {
    char name[32];
    int offset;
} Symbol;

Symbol symbols[100];
int symbol_count = 0;
int current_stack_offset = 0;

void add_symbol(const char* name) {
    current_stack_offset -= 8; // 8 bytes for 64-bit integer
    strcpy(symbols[symbol_count].name, name);
    symbols[symbol_count].offset = current_stack_offset;
    symbol_count++;
}

int get_symbol_offset(const char* name) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) {
            return symbols[i].offset;
        }
    }
    printf("Codegen Error: Undefined variable %s\n", name);
    exit(1);
}
// Helper to generate code for expressions
// Result will always end up in %rax
void gen_expr(Node* node, FILE* file) {
    if (node->type == NODE_INT) {
        fprintf(file, "    mov $%d, %%rax\n", node->data.integer.value);
    }
    else if (node->type == NODE_VAR_REF) {
        // Load variable from stack to RAX
        int offset = get_symbol_offset(node->data.var_ref.name);
        fprintf(file, "    mov %d(%%rbp), %%rax\n", offset);
    }
    else if (node->type == NODE_BIN_OP) {
        // 1. Calculate Right side, push to stack
        gen_expr(node->data.bin_op.right, file);
        fprintf(file, "    push %%rax\n");

        // 2. Calculate Left side, stays in RAX
        gen_expr(node->data.bin_op.left, file);
        
        // 3. Pop Right side into RBX
        fprintf(file, "    pop %%rbx\n");
        if (node->data.bin_op.op == OP_ADD) fprintf(file, "    add %%rbx, %%rax\n");
        else fprintf(file, "    sub %%rbx, %%rax\n");
    }
}

void codegen(Node* node, FILE* file) {
    if (node->type == NODE_PROGRAM) {
        // Data section for format string
        fprintf(file, ".section .rdata,\"dr\"\n");
        fprintf(file, "fmt_int: .asciz \"%%d\\n\"\n"); // "%d\n"

        fprintf(file, ".text\n");
        fprintf(file, ".globl main\n");
        fprintf(file, "main:\n");
        
        // --- PROLOGUE (Stack Frame) ---
        fprintf(file, "    push %%rbp\n");      // Save old base pointer
        fprintf(file, "    mov %%rsp, %%rbp\n"); // Set new base pointer
        // Reserve space for 16 variables (16 * 8 = 128 bytes) + Shadow Space (32)
        // Must align stack to 16 bytes.
        fprintf(file, "    sub $160, %%rsp\n"); 

        // Loop through statements
        Node* current = node->data.program.statements;
        while (current != NULL) {
            codegen(current, file);
            current = current->next;
        }

        // --- EPILOGUE ---
        fprintf(file, "    mov $0, %%rax\n");
        fprintf(file, "    leave\n"); // Restore RSP and RBP
        fprintf(file, "    ret\n");
    } 
    else if (node->type == NODE_VAR_DECL) {
        // Calculate value
        gen_expr(node->data.var_decl.expression, file);
        // Add to symbol table
        add_symbol(node->data.var_decl.name);
        // Store RAX into stack
        int offset = get_symbol_offset(node->data.var_decl.name);
        fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
    }
    else if (node->type == NODE_PRINT) {
        gen_expr(node->data.print_stmt.expression, file);
        // Windows ABI:
        // Arg1 (RCX) = Format String
        // Arg2 (RDX) = Value (currently in RAX)
        fprintf(file, "    mov %%rax, %%rdx\n");
        fprintf(file, "    lea fmt_int(%%rip), %%rcx\n"); 
        fprintf(file, "    call printf\n"); 
    }
    else if (node->type == NODE_RETURN) {
        gen_expr(node->data.return_stmt.expression, file);
        fprintf(file, "    leave\n");
        fprintf(file, "    ret\n");
    }
}