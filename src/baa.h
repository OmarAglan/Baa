#ifndef BAA_H
#define BAA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// --- LEXER ---

typedef enum {
    TOKEN_EOF,
    TOKEN_INT,      // 0-9 or ٠-٩
    TOKEN_RETURN,   // إرجع
    TOKEN_PRINT,    // اطبع
    TOKEN_KEYWORD_INT, // رقم
    TOKEN_IDENTIFIER, // Variable names (e.g., س)
    TOKEN_ASSIGN,     // =
    TOKEN_DOT,      // .
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    char* value; // For keywords and identifiers
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
    NODE_PROGRAM, // New: Program
    NODE_RETURN,  // New: Return
    NODE_PRINT,   // New: Print
    NODE_VAR_DECL, // New: Declaration
    NODE_VAR_REF,  // New: Usage
    NODE_INT,     // New: Integer
    NODE_BIN_OP   // New: Binary Operation
} NodeType;

typedef enum { OP_ADD, OP_SUB } OpType;

struct Node; // Forward declaration

typedef struct Node {
    NodeType type;
    struct Node* next; // For linked list of statements
    union {
        struct { struct Node* statements; } program;
        struct { struct Node* expression; } return_stmt;
        struct { struct Node* expression; } print_stmt;
        
        struct { 
            char* name; 
            struct Node* expression; 
        } var_decl; // رقم x = 5

        struct { 
            char* name; 
        } var_ref;  // x + 1

        struct { int value; } integer;
        struct { struct Node* left; struct Node* right; OpType op; } bin_op;
    } data;
} Node;

Node* parse(Lexer* lexer);

// --- CODEGEN ---

void codegen(Node* node, FILE* file);

#endif // BAA_H