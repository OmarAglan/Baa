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
        
        // --- FUNCTION ENTRY ---
        // Stack alignment (16-bytes) + Shadow Space (32-bytes)
        // main entry stack is off by 8. sub 40 aligns (8+40=48, 48%16=0) and gives 32b space.
        fprintf(file, "    sub $40, %%rsp\n");

        // Loop through statements
        Node* current = node->data.program.statements;
        while (current != NULL) {
            codegen(current, file);
            current = current->next;
        }

        // --- FUNCTION EXIT (Implicit return 0) ---
        fprintf(file, "    mov $0, %%rax\n");
        fprintf(file, "    add $40, %%rsp\n");
        fprintf(file, "    ret\n");
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
        // Restore stack before returning
        fprintf(file, "    add $40, %%rsp\n");
        fprintf(file, "    ret\n");
    }
}