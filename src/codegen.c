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
int label_counter = 0; // New: Unique label generator

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
        
        if (node->data.bin_op.op == OP_ADD) {
            fprintf(file, "    add %%rbx, %%rax\n");
        } else if (node->data.bin_op.op == OP_SUB) {
            fprintf(file, "    sub %%rbx, %%rax\n");
        } else if (node->data.bin_op.op == OP_EQ) {
            fprintf(file, "    cmp %%rbx, %%rax\n");
            fprintf(file, "    sete %%al\n"); // Set AL to 1 if equal
            fprintf(file, "    movzbq %%al, %%rax\n"); // Zero-extend AL to RAX
        } else if (node->data.bin_op.op == OP_NEQ) {
            fprintf(file, "    cmp %%rbx, %%rax\n");
            fprintf(file, "    setne %%al\n"); 
            fprintf(file, "    movzbq %%al, %%rax\n");
        }
    }
}

void codegen(Node* node, FILE* file) {
    // NULL Check
    if (node == NULL) return;
    // Program
    if (node->type == NODE_PROGRAM) {
        fprintf(file, ".section .rdata,\"dr\"\n");
        fprintf(file, "fmt_int: .asciz \"%%d\\n\"\n");
        fprintf(file, ".text\n.globl main\nmain:\n");
        fprintf(file, "    push %%rbp\n");
        fprintf(file, "    mov %%rsp, %%rbp\n");
        fprintf(file, "    sub $160, %%rsp\n"); 

        Node* current = node->data.program.statements;
        while (current != NULL) { codegen(current, file); current = current->next; }

        fprintf(file, "    mov $0, %%rax\n");
        fprintf(file, "    leave\n    ret\n");
    } 
    // Block
    else if (node->type == NODE_BLOCK) {
        Node* current = node->data.block.statements;
        while (current != NULL) { codegen(current, file); current = current->next; }
    }
    // If Statement
    else if (node->type == NODE_IF) {
        int label_end = label_counter++;
        
        // 1. Calculate condition
        gen_expr(node->data.if_stmt.condition, file);
        
        // 2. Check if false (0). If so, jump to end.
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);
        
        // 3. Generate 'Then' block
        codegen(node->data.if_stmt.then_branch, file);
        
        // 4. Label for end
        fprintf(file, ".Lend_%d:\n", label_end);
    }
    // While Statement
    else if (node->type == NODE_WHILE) {
        int label_start = label_counter++;
        int label_end = label_counter++;

        // 1. Label Start
        fprintf(file, ".Lstart_%d:\n", label_start);

        // 2. Calculate Condition
        gen_expr(node->data.while_stmt.condition, file);

        // 3. Check if False (0). Jump to End.
        fprintf(file, "    cmp $0, %%rax\n");
        fprintf(file, "    je .Lend_%d\n", label_end);

        // 4. Generate Body
        codegen(node->data.while_stmt.body, file);

        // 5. Jump back to Start
        fprintf(file, "    jmp .Lstart_%d\n", label_start);

        // 6. Label End
        fprintf(file, ".Lend_%d:\n", label_end);
    }
    // Variable Declaration
    else if (node->type == NODE_VAR_DECL) {
        // Calculate value
        gen_expr(node->data.var_decl.expression, file);
        // Add to symbol table
        add_symbol(node->data.var_decl.name);
        // Store RAX into stack
        int offset = get_symbol_offset(node->data.var_decl.name);
        fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
    }
    // Assignment Statement
    else if (node->type == NODE_ASSIGN) {
        gen_expr(node->data.assign_stmt.expression, file);
        // Find existing offset
        int offset = get_symbol_offset(node->data.assign_stmt.name); 
        fprintf(file, "    mov %%rax, %d(%%rbp)\n", offset);
    }
    // Print Statement
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
        fprintf(file, "    leave\n    ret\n");
    }
}