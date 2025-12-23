#include "baa.h"

void codegen(Node* node, FILE* file) {
    if (node->type == NODE_PROGRAM) {
        // .globl indicates 'main' is visible to the linker
        fprintf(file, ".globl main\n");
        fprintf(file, "main:\n");
        
        // Generate body
        codegen(node->data.program.statement, file);
    } 
    else if (node->type == NODE_RETURN) {
        // In x86_64, the return value goes into the RAX register.
        // The 'ret' instruction returns control to the OS (C Runtime).
        fprintf(file, "    mov $%d, %%rax\n", node->data.return_stmt.value);
        fprintf(file, "    ret\n");
    }
}