#include "baa.h"

// Helper to generate code for expressions
// Result will always end up in %rax
void gen_expr(Node* node, FILE* file) {
    if (node->type == NODE_INT) {
        fprintf(file, "    mov $%d, %%rax\n", node->data.integer.value);
    }
    else if (node->type == NODE_BIN_OP) {
        // 1. Calculate Right side, push to stack
        gen_expr(node->data.bin_op.right, file);
        fprintf(file, "    push %%rax\n");

        // 2. Calculate Left side, stays in RAX
        gen_expr(node->data.bin_op.left, file);
        
        // 3. Pop Right side into RBX
        fprintf(file, "    pop %%rbx\n");

        // 4. Do the math (RAX = RAX op RBX)
        if (node->data.bin_op.op == OP_ADD) {
            fprintf(file, "    add %%rbx, %%rax\n");
        } else {
            fprintf(file, "    sub %%rbx, %%rax\n");
        }
    }
}

void codegen(Node* node, FILE* file) {
    if (node->type == NODE_PROGRAM) {
        // .globl indicates 'main' is visible to the linker
        fprintf(file, ".globl main\n");
        fprintf(file, "main:\n");
        
        // Generate body
        codegen(node->data.program.statement, file);
    } 
    else if (node->type == NODE_RETURN) {
        // Evaluate the expression (result in RAX)
        gen_expr(node->data.return_stmt.expression, file);
        // Return (RAX is already set)
        fprintf(file, "    ret\n");
    }
}