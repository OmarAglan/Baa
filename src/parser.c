#include "baa.h"

Token current_token;

void eat(Lexer* l, TokenType type) {
    if (current_token.type == type) {
        current_token = lexer_next_token(l);
    } else {
        printf("Parser Error: Unexpected token %d, expected %d\n", current_token.type, type);
        exit(1);
    }
}

// Forward declaration
Node* parse_expression(Lexer* l);

Node* parse_primary(Lexer* l) {
    if (current_token.type == TOKEN_INT) {
        Node* node = malloc(sizeof(Node));
        node->type = NODE_INT;
        node->data.integer.value = atoi(current_token.value);
        eat(l, TOKEN_INT);
        return node;
    }
    printf("Parser Error: Expected integer\n");
    exit(1);
    return NULL;
}

Node* parse_expression(Lexer* l) {
    // Parse left side (Example: 5)
    Node* left = parse_primary(l);

    // Look for operators (+ or -)
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        OpType op = (current_token.type == TOKEN_PLUS) ? OP_ADD : OP_SUB;
        eat(l, current_token.type);

        // Parse right side (Example: 2)
        Node* right = parse_primary(l);

        // Create a new parent node
        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;

        // The new node becomes the left side for the next iteration
        // (Handles 1 + 2 + 3)
        left = new_node;
    }
    return left;
}

Node* parse(Lexer* l) {
    current_token = lexer_next_token(l);

    if (current_token.type == TOKEN_RETURN) {
        eat(l, TOKEN_RETURN);

        // Parse Expression instead of just Integer
        Node* expr = parse_expression(l);

        eat(l, TOKEN_DOT);

        Node* ret_node = malloc(sizeof(Node));
        ret_node->type = NODE_RETURN;
        ret_node->data.return_stmt.expression = expr;

        Node* program = malloc(sizeof(Node));
        program->type = NODE_PROGRAM;
        program->data.program.statement = ret_node;

        return program;
    }

    printf("Parser Error: Program must start with return\n");
    exit(1);
    return NULL;
}