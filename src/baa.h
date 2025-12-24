#ifndef BAA_H
#define BAA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// --- LEXER ---

typedef enum {
    TOKEN_EOF,
    TOKEN_INT,      // 0-9 or ٠-٩
    TOKEN_RETURN,   // إرجع
    TOKEN_PRINT,    // اطبع
    TOKEN_DOT,      // .
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char* value;    // For numbers
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
    NODE_RETURN,
    NODE_PRINT,     // New Node
    NODE_INT,       // Leaf node: 5
    NODE_BIN_OP     // Tree node: 5 + 2
} NodeType;

typedef enum { OP_ADD, OP_SUB } OpType;

struct Node; // Forward declaration

typedef struct Node {
    NodeType type;
    struct Node* next; // For Linked List of statements
    union {
        struct {
            struct Node* statements;
        } program;
        
        struct {
            struct Node* expression; // Return now holds an expression, not just int
        } return_stmt;

        struct {
            struct Node* expression;
        } print_stmt;

        struct {
            int value;
        } integer;

        struct {
            struct Node* left;
            struct Node* right;
            OpType op;
        } bin_op;
    } data;
} Node;

Node* parse(Lexer* lexer);

// --- CODEGEN ---

void codegen(Node* node, FILE* file);

#endif // BAA_H