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
    TOKEN_KEYWORD_INT, // صحيح
    TOKEN_IF,           // إذا
    TOKEN_WHILE,      // طالما
    TOKEN_IDENTIFIER, // Variable names (e.g., س)
    TOKEN_ASSIGN,     // =
    TOKEN_EQ,           // ==
    TOKEN_NEQ,          // !=
    TOKEN_DOT,      // .
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_INVALID  // Invalid token
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
    NODE_BLOCK,         // New: List of statements
    NODE_RETURN,  // New: Return
    NODE_ASSIGN, // New: Assignment
    NODE_PRINT,   // New: Print
    NODE_VAR_DECL, // New: Declaration
    NODE_IF,            // New: If statement
    NODE_WHILE,         // New: While statement
    NODE_VAR_REF,  // New: Usage
    NODE_INT,     // New: Integer
    NODE_BIN_OP   // New: Binary Operation
} NodeType;

typedef enum { 
    OP_ADD,     // +
    OP_SUB,     // -
    OP_EQ,      // ==
    OP_NEQ      // !=
} OpType;

struct Node; // Forward declaration

typedef struct Node {
    NodeType type;
    struct Node* next; // Link to next node
    union {
        struct { struct Node* statements; } program; // List of statements
        struct { struct Node* statements; } block; // { stmt; stmt; }
        
        // New: If structure
        struct { 
            struct Node* condition;
            struct Node* then_branch;
        } if_stmt; // if (condition) { stmt; stmt; }
        
        // New: While structure (Same structure as If, fundamentally)
        struct {
            struct Node* condition;
            struct Node* body;
        } while_stmt;
        
        struct { struct Node* expression; } return_stmt; // return <expression>;
        struct { struct Node* expression; } print_stmt; // print <expression>;
        struct { char* name; struct Node* expression; } var_decl; // <identifier> = <expression>;
        struct { char* name; } var_ref; // <identifier>
        struct { int value; } integer; // <integer>
        struct { struct Node* left; struct Node* right; OpType op; } bin_op; // <left> <op> <right>
        struct { char* name; struct Node* expression; } assign_stmt; // Existing var
    } data;
} Node;

Node* parse(Lexer* lexer);

// --- CODEGEN ---

void codegen(Node* node, FILE* file);

#endif // BAA_H