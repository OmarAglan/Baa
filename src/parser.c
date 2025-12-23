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

Node* parse(Lexer* l) {
    current_token = lexer_next_token(l);

    // We expect "إرجع"
    if (current_token.type == TOKEN_RETURN) {
        eat(l, TOKEN_RETURN);

        // We expect an Integer
        if (current_token.type != TOKEN_INT) {
            printf("Parser Error: Expected integer after return\n");
            exit(1);
        }
        int value = atoi(current_token.value);
        eat(l, TOKEN_INT);

        // We expect "."
        eat(l, TOKEN_DOT);

        // Build AST
        Node* ret_node = malloc(sizeof(Node));
        ret_node->type = NODE_RETURN;
        ret_node->data.return_stmt.value = value;

        Node* program = malloc(sizeof(Node));
        program->type = NODE_PROGRAM;
        program->data.program.statement = ret_node;

        return program;
    }

    printf("Parser Error: Program must start with return\n");
    exit(1);
    return NULL;
}