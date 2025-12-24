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
        node->next = NULL;
        eat(l, TOKEN_INT);
        return node;
    }
    // New: Handle Variable Reference (Usage)
    if (current_token.type == TOKEN_IDENTIFIER) {
        Node* node = malloc(sizeof(Node));
        node->type = NODE_VAR_REF;
        node->data.var_ref.name = strdup(current_token.value);
        node->next = NULL;
        eat(l, TOKEN_IDENTIFIER);
        return node;
    }
    printf("Parser Error: Expected integer or identifier\n");
    exit(1);
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
        new_node->next = NULL;
        // The new node becomes the left side for the next iteration
        // (Handles 1 + 2 + 3)
        left = new_node;
    }
    return left;
}

Node* parse_statement(Lexer* l) {
    Node* stmt = malloc(sizeof(Node));
    stmt->next = NULL;

    if (current_token.type == TOKEN_RETURN) {
        eat(l, TOKEN_RETURN);
        stmt->type = NODE_RETURN;
        stmt->data.return_stmt.expression = parse_expression(l);
        eat(l, TOKEN_DOT);
        return stmt;
    } 
    else if (current_token.type == TOKEN_PRINT) {
        eat(l, TOKEN_PRINT);
        stmt->type = NODE_PRINT;
        stmt->data.print_stmt.expression = parse_expression(l);
        eat(l, TOKEN_DOT);
        return stmt;
    }
    // New: Variable Declaration (صحيح x = 5.)
    else if (current_token.type == TOKEN_KEYWORD_INT) {
        eat(l, TOKEN_KEYWORD_INT); // Eat 'صحيح'
        
        if (current_token.type != TOKEN_IDENTIFIER) {
            printf("Parser Error: Expected variable name after 'صحيح'\n");
            exit(1);
        }
        char* name = strdup(current_token.value);
        eat(l, TOKEN_IDENTIFIER);

        eat(l, TOKEN_ASSIGN); // Eat '='

        Node* expr = parse_expression(l);

        stmt->type = NODE_VAR_DECL;
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.expression = expr;

        eat(l, TOKEN_DOT);
        return stmt;
    }
    
    printf("Parser Error: Unknown statement\n");
    exit(1);
}

Node* parse(Lexer* l) {
    current_token = lexer_next_token(l);
    Node* head = NULL;
    Node* tail = NULL;

    // Parse until EOF
    while (current_token.type != TOKEN_EOF) {
        Node* stmt = parse_statement(l);
        if (head == NULL) {
            head = stmt;
            tail = stmt;
        } else {
            tail->next = stmt;
            tail = stmt;
        }
    }

    Node* program = malloc(sizeof(Node));
    program->type = NODE_PROGRAM;
    program->data.program.statements = head;
    return program;
}