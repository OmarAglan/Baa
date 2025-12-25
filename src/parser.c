#include "baa.h"

Token current_token;

void eat(Lexer* l, TokenType type) {
    if (current_token.type == type) {
        current_token = lexer_next_token(l);
    } else {
        printf("Parser Error: Unexpected token %d, expected %d at line %d\n", current_token.type, type, current_token.line);
        exit(1);
    }
}

// Forward declarations
Node* parse_expression(Lexer* l);
Node* parse_statement(Lexer* l);

Node* parse_primary(Lexer* l) {
    if (current_token.type == TOKEN_INT) {
        Node* node = malloc(sizeof(Node));
        node->type = NODE_INT;
        node->data.integer.value = atoi(current_token.value);
        node->next = NULL;
        eat(l, TOKEN_INT);
        return node;
    }
    // Handle Variable Reference (Usage)
    if (current_token.type == TOKEN_IDENTIFIER) {
        Node* node = malloc(sizeof(Node));
        node->type = NODE_VAR_REF;
        node->data.var_ref.name = strdup(current_token.value);
        node->next = NULL;
        eat(l, TOKEN_IDENTIFIER);
        return node;
    }
    if (current_token.type == TOKEN_LPAREN) {
        eat(l, TOKEN_LPAREN);
        Node* expr = parse_expression(l);
        eat(l, TOKEN_RPAREN);
        return expr;
    }
    printf("Parser Error: Expected expression\n");
    exit(1);
}

Node* parse_expression(Lexer* l) {
    Node* left = parse_primary(l);
    
    // Supports +, -, ==, !=
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS ||
           current_token.type == TOKEN_EQ || current_token.type == TOKEN_NEQ) {
        
        OpType op;
        if (current_token.type == TOKEN_PLUS) op = OP_ADD;
        else if (current_token.type == TOKEN_MINUS) op = OP_SUB;
        else if (current_token.type == TOKEN_EQ) op = OP_EQ;
        else op = OP_NEQ;

        eat(l, current_token.type);
        Node* right = parse_primary(l);
        Node* new_node = malloc(sizeof(Node));
        new_node->type = NODE_BIN_OP;
        new_node->data.bin_op.left = left;
        new_node->data.bin_op.right = right;
        new_node->data.bin_op.op = op;
        new_node->next = NULL;
        left = new_node;
    }
    return left;
}

// Parse { stmt; stmt; }
Node* parse_block(Lexer* l) {
    eat(l, TOKEN_LBRACE);
    Node* head = NULL;
    Node* tail = NULL;

    while (current_token.type != TOKEN_RBRACE && current_token.type != TOKEN_EOF) {
        Node* stmt = parse_statement(l);
        if (head == NULL) { head = stmt; tail = stmt; } 
        else { tail->next = stmt; tail = stmt; }
    }
    eat(l, TOKEN_RBRACE);

    Node* block = malloc(sizeof(Node));
    block->type = NODE_BLOCK;
    block->data.block.statements = head;
    block->next = NULL;
    return block;
}

Node* parse_statement(Lexer* l) {
    Node* stmt = malloc(sizeof(Node));
    stmt->next = NULL;
    
    // Block
    if (current_token.type == TOKEN_LBRACE) {
        free(stmt); // Avoid leak, parse_block allocates new
        return parse_block(l);
    }
    // Return Statement
    else if (current_token.type == TOKEN_RETURN) {
        eat(l, TOKEN_RETURN);
        stmt->type = NODE_RETURN;
        stmt->data.return_stmt.expression = parse_expression(l);
        eat(l, TOKEN_DOT);
        return stmt;
    } 
    // Print Statement
    else if (current_token.type == TOKEN_PRINT) {
        eat(l, TOKEN_PRINT);
        stmt->type = NODE_PRINT;
        stmt->data.print_stmt.expression = parse_expression(l);
        eat(l, TOKEN_DOT);
        return stmt;
    }
    // Variable Declaration
    else if (current_token.type == TOKEN_KEYWORD_INT) {
        eat(l, TOKEN_KEYWORD_INT);
        char* name = strdup(current_token.value);
        eat(l, TOKEN_IDENTIFIER);
        eat(l, TOKEN_ASSIGN);
        Node* expr = parse_expression(l);
        stmt->type = NODE_VAR_DECL;
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.expression = expr;
        eat(l, TOKEN_DOT);
        return stmt;
    }
    // Check for Identifier start (e.g. "x = ...")
    else if (current_token.type == TOKEN_IDENTIFIER) {
        char* name = strdup(current_token.value);
        eat(l, TOKEN_IDENTIFIER);
        eat(l, TOKEN_ASSIGN);
        Node* expr = parse_expression(l);
        
        stmt->type = NODE_ASSIGN;
        stmt->data.assign_stmt.name = name;
        stmt->data.assign_stmt.expression = expr;
    
        eat(l, TOKEN_DOT);
        return stmt;
    }
    // If Statement
    else if (current_token.type == TOKEN_IF) {
        eat(l, TOKEN_IF);
        eat(l, TOKEN_LPAREN);
        Node* condition = parse_expression(l);
        eat(l, TOKEN_RPAREN);
        
        Node* then_branch = parse_statement(l); // Can be a block or single stmt

        stmt->type = NODE_IF;
        stmt->data.if_stmt.condition = condition;
        stmt->data.if_stmt.then_branch = then_branch;
        return stmt;
    }
    // While Statement
    else if (current_token.type == TOKEN_WHILE) {
        eat(l, TOKEN_WHILE);
        eat(l, TOKEN_LPAREN);
        Node* condition = parse_expression(l);
        eat(l, TOKEN_RPAREN);
        
        Node* body = parse_statement(l); // Parses block or single stmt

        stmt->type = NODE_WHILE;
        stmt->data.while_stmt.condition = condition;
        stmt->data.while_stmt.body = body;
        return stmt;
    }
    // Invalid Statement
    printf("Parser Error: Unknown statement type %d\n", current_token.type);
    exit(1);
}

Node* parse(Lexer* l) {
    current_token = lexer_next_token(l);
    Node* head = NULL;
    Node* tail = NULL;

    // Parse until EOF
    while (current_token.type != TOKEN_EOF) {
        Node* stmt = parse_statement(l);
        if (head == NULL) { head = stmt; tail = stmt; } 
        else { tail->next = stmt; tail = stmt; }
    }

    Node* program = malloc(sizeof(Node));
    program->type = NODE_PROGRAM;
    program->data.program.statements = head;
    return program;
}