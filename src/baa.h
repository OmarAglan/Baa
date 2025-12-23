#ifndef BAA_H
#define BAA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// --- LEXER ---

typedef enum {
    TOKEN_EOF,
    TOKEN_INT,      // 0-9
    TOKEN_RETURN,   // إرجع
    TOKEN_DOT,      // .
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char* value;    // For identifiers/numbers (UTF-8)
    int line;
} Token;

typedef struct {
    const char* src;
    size_t pos;
    size_t len;
    int line;
} Lexer;

void lexer_init(Lexer* lexer, const char* src);
Token lexer_next_token(Lexer* lexer);

// --- AST (PARSER) ---

typedef enum {
    NODE_PROGRAM,
    NODE_RETURN
} NodeType;

typedef struct Node {
    NodeType type;
    union {
        struct {
            struct Node* statement;
        } program;
        
        struct {
            int value;
        } return_stmt;
    } data;
} Node;

Node* parse(Lexer* lexer);

// --- CODEGEN ---

void codegen(Node* node, FILE* file);

#endif // BAA_H