/**
 * @file parser.c
 * @brief Parses tokens into an AST using Recursive Descent with Lookahead.
 */

#include "baa.h"

// The Parser Object handles the state and lookahead
Parser parser;

// --- Helper Functions ---

void advance() {
    parser.current = parser.next;
    parser.next = lexer_next_token(parser.lexer);
}

void init_parser(Lexer* l) {
    parser.lexer = l;
    parser.current = lexer_next_token(l); // Load 1st token
    parser.next = lexer_next_token(l);    // Load 2nd token (Lookahead)
}

void eat(TokenType type) {
    if (parser.current.type == type) {
        advance();
    } else {
        printf("Parser Error: Unexpected token %d (Expected %d) at line %d\n", 
               parser.current.type, type, parser.current.line);
        exit(1);
    }
}

// --- Forward Declarations ---
Node* parse_expression();
Node* parse_statement();
Node* parse_block();

// --- Expression Parsing ---

Node* parse_primary() {
    if (parser.current.type == TOKEN_INT) {
        Node* node = malloc(sizeof(Node));
        node->type = NODE_INT;
        node->data.integer.value = atoi(parser.current.value);
        node->next = NULL;
        eat(TOKEN_INT);
        return node;
    }
    
    if (parser.current.type == TOKEN_IDENTIFIER) {
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // Check if Function Call: name(...)
        if (parser.current.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            Node* head_arg = NULL;
            Node* tail_arg = NULL;

            // Parse Arguments (comma separated)
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    Node* arg = parse_expression();
                    arg->next = NULL;
                    if (head_arg == NULL) { head_arg = arg; tail_arg = arg; }
                    else { tail_arg->next = arg; tail_arg = arg; }

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            Node* node = malloc(sizeof(Node));
            node->type = NODE_CALL_EXPR;
            node->data.call.name = name;
            node->data.call.args = head_arg;
            node->next = NULL;
            return node;
        }

        // Otherwise, Variable Reference
        Node* node = malloc(sizeof(Node));
        node->type = NODE_VAR_REF;
        node->data.var_ref.name = name;
        node->next = NULL;
        return node;
    }

    if (parser.current.type == TOKEN_LPAREN) {
        eat(TOKEN_LPAREN);
        Node* expr = parse_expression();
        eat(TOKEN_RPAREN);
        return expr;
    }

    printf("Parser Error: Expected expression at line %d\n", parser.current.line);
    exit(1);
}

Node* parse_expression() {
    Node* left = parse_primary();
    
    while (parser.current.type == TOKEN_PLUS || parser.current.type == TOKEN_MINUS ||
           parser.current.type == TOKEN_EQ || parser.current.type == TOKEN_NEQ) {
        
        OpType op;
        if (parser.current.type == TOKEN_PLUS) op = OP_ADD;
        else if (parser.current.type == TOKEN_MINUS) op = OP_SUB;
        else if (parser.current.type == TOKEN_EQ) op = OP_EQ;
        else op = OP_NEQ;

        eat(parser.current.type);
        Node* right = parse_primary();
        
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

// --- Statement Parsing ---

Node* parse_block() {
    eat(TOKEN_LBRACE);
    Node* head = NULL;
    Node* tail = NULL;

    while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
        Node* stmt = parse_statement();
        if (head == NULL) { head = stmt; tail = stmt; } 
        else { tail->next = stmt; tail = stmt; }
    }
    eat(TOKEN_RBRACE);

    Node* block = malloc(sizeof(Node));
    block->type = NODE_BLOCK;
    block->data.block.statements = head;
    block->next = NULL;
    return block;
}

Node* parse_statement() {
    Node* stmt = malloc(sizeof(Node));
    stmt->next = NULL;

    if (parser.current.type == TOKEN_LBRACE) {
        free(stmt); return parse_block();
    }
    else if (parser.current.type == TOKEN_RETURN) {
        eat(TOKEN_RETURN);
        stmt->type = NODE_RETURN;
        stmt->data.return_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    } 
    else if (parser.current.type == TOKEN_PRINT) {
        eat(TOKEN_PRINT);
        stmt->type = NODE_PRINT;
        stmt->data.print_stmt.expression = parse_expression();
        eat(TOKEN_DOT);
        return stmt;
    }
    else if (parser.current.type == TOKEN_IF) {
        eat(TOKEN_IF);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* then = parse_statement();
        stmt->type = NODE_IF;
        stmt->data.if_stmt.condition = cond;
        stmt->data.if_stmt.then_branch = then;
        return stmt;
    }
    else if (parser.current.type == TOKEN_WHILE) {
        eat(TOKEN_WHILE);
        eat(TOKEN_LPAREN);
        Node* cond = parse_expression();
        eat(TOKEN_RPAREN);
        Node* body = parse_statement();
        stmt->type = NODE_WHILE;
        stmt->data.while_stmt.condition = cond;
        stmt->data.while_stmt.body = body;
        return stmt;
    }
    else if (parser.current.type == TOKEN_KEYWORD_INT) {
        // Local Variable Declaration
        eat(TOKEN_KEYWORD_INT);
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);
        eat(TOKEN_ASSIGN);
        Node* expr = parse_expression();
        eat(TOKEN_DOT);
        
        stmt->type = NODE_VAR_DECL;
        stmt->data.var_decl.name = name;
        stmt->data.var_decl.expression = expr;
        stmt->data.var_decl.is_global = false; // Inside function/block
        return stmt;
    }
    else if (parser.current.type == TOKEN_IDENTIFIER) {
        // Lookahead needed: Assignment (x = 1) vs Call (x())
        if (parser.next.type == TOKEN_ASSIGN) {
            char* name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);
            eat(TOKEN_ASSIGN);
            Node* expr = parse_expression();
            eat(TOKEN_DOT);
            stmt->type = NODE_ASSIGN;
            stmt->data.assign_stmt.name = name;
            stmt->data.assign_stmt.expression = expr;
            return stmt;
        } 
        else if (parser.next.type == TOKEN_LPAREN) {
            // Function Call Statement
            Node* expr = parse_expression(); // This will return a NODE_CALL_EXPR
            eat(TOKEN_DOT);
            // Convert Expr Node to Stmt Node
            stmt->type = NODE_CALL_STMT;
            stmt->data.call.name = expr->data.call.name;
            stmt->data.call.args = expr->data.call.args;
            free(expr); // Free the wrapper, keep data
            return stmt;
        }
    }
    
    printf("Parser Error: Unknown statement at line %d\n", parser.current.line);
    exit(1);
}

// --- Declaration Parsing (Globals & Functions) ---

Node* parse_declaration() {
    // Both start with "صحيح Identifier"
    if (parser.current.type == TOKEN_KEYWORD_INT) {
        eat(TOKEN_KEYWORD_INT);
        
        if (parser.current.type != TOKEN_IDENTIFIER) {
            printf("Parser Error: Expected Identifier\n"); exit(1);
        }
        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        // DISAMBIGUATION: Check Lookahead (parser.current is now the token AFTER ID)
        
        // 1. Function Definition: صحيح main ( ...
        if (parser.current.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            Node* head_param = NULL;
            Node* tail_param = NULL;

            // Parse Parameters
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    eat(TOKEN_KEYWORD_INT); // Only int supported
                    char* pname = strdup(parser.current.value);
                    eat(TOKEN_IDENTIFIER);

                    Node* param = malloc(sizeof(Node));
                    param->type = NODE_VAR_DECL; // Reuse VarDecl for params
                    param->data.var_decl.name = pname;
                    param->data.var_decl.expression = NULL;
                    param->data.var_decl.is_global = false;
                    param->next = NULL;

                    if (head_param == NULL) { head_param = param; tail_param = param; }
                    else { tail_param->next = param; tail_param = param; }

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            Node* body = parse_block();

            Node* func = malloc(sizeof(Node));
            func->type = NODE_FUNC_DEF;
            func->data.func_def.name = name;
            func->data.func_def.params = head_param;
            func->data.func_def.body = body;
            func->next = NULL;
            return func;
        }
        // 2. Global Variable: صحيح x = 5.
        else {
            Node* expr = NULL;
            if (parser.current.type == TOKEN_ASSIGN) {
                eat(TOKEN_ASSIGN);
                expr = parse_expression();
            }
            eat(TOKEN_DOT);

            Node* var = malloc(sizeof(Node));
            var->type = NODE_VAR_DECL;
            var->data.var_decl.name = name;
            var->data.var_decl.expression = expr;
            var->data.var_decl.is_global = true;
            var->next = NULL;
            return var;
        }
    }
    printf("Parser Error: Unexpected token at Top Level\n");
    exit(1);
}

Node* parse(Lexer* l) {
    init_parser(l);
    Node* head = NULL;
    Node* tail = NULL;

    while (parser.current.type != TOKEN_EOF) {
        Node* decl = parse_declaration();
        if (head == NULL) { head = decl; tail = decl; }
        else { tail->next = decl; tail = decl; }
    }

    Node* program = malloc(sizeof(Node));
    program->type = NODE_PROGRAM;
    program->data.program.declarations = head;
    return program;
}