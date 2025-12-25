/**
 * @file baa.h
 * @brief Main header file defining Data Structures for the Baa Compiler.
 * @version 0.0.9
 */

#ifndef BAA_H
#define BAA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// LEXER DEFINITIONS
// ============================================================================

/**
 * @enum TokenType
 * @brief Categorizes the atomic units of the source code.
 */
typedef enum {
    TOKEN_EOF,
    TOKEN_INT,          // Literals: 123
    TOKEN_IDENTIFIER,   // Names: x, my_func
    
    // Keywords
    TOKEN_KEYWORD_INT,  // صحيح
    TOKEN_RETURN,       // إرجع
    TOKEN_PRINT,        // اطبع
    TOKEN_IF,           // إذا
    TOKEN_WHILE,        // طالما
    
    // Symbols
    TOKEN_ASSIGN,       // =
    TOKEN_DOT,          // .
    TOKEN_COMMA,        // ,
    
    // Math
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // * (New)
    TOKEN_SLASH,        // / (New)
    TOKEN_PERCENT,      // % (New)
    
    // Logic / Relational
    TOKEN_EQ,           // ==
    TOKEN_NEQ,          // !=
    TOKEN_LT,           // < (New)
    TOKEN_GT,           // > (New)
    TOKEN_LTE,          // <= (New)
    TOKEN_GTE,          // >= (New)
    
    // Grouping
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    
    TOKEN_INVALID
} TokenType;

/**
 * @struct Token
 * @brief Represents a single token extracted from the source.
 */
typedef struct {
    TokenType type;
    char* value;    // ASCII representation of payload (Name or Number)
    int line;       // Source line number for debugging
} Token;

/**
 * @struct Lexer
 * @brief Maintains the state of the text scanning process.
 */
typedef struct {
    const char* src;
    size_t pos;
    size_t len;
    int line;
} Lexer;

void lexer_init(Lexer* lexer, const char* src);
Token lexer_next_token(Lexer* lexer);

// ============================================================================
// PARSER & AST DEFINITIONS
// ============================================================================

/**
 * @enum NodeType
 * @brief Discriminator for the Polymorphic AST Node.
 */
typedef enum {
    // Top Level
    NODE_PROGRAM,       // Root: Contains list of Declarations
    NODE_FUNC_DEF,      // Function Definition
    NODE_VAR_DECL,      // Variable Declaration (Global or Local)
    
    // Statements
    NODE_BLOCK,         // Scope { ... }
    NODE_RETURN,        // Return statement
    NODE_PRINT,         // Print statement
    NODE_IF,            // Conditional
    NODE_WHILE,         // Loop
    NODE_ASSIGN,        // Re-assignment: x = 5
    NODE_CALL_STMT,     // Function Call as a statement: foo();
    
    // Expressions
    NODE_BIN_OP,        // Math/Logic
    NODE_INT,           // Literal: 5
    NODE_VAR_REF,       // Usage: x
    NODE_CALL_EXPR      // Function Call as expression: x = foo();
} NodeType;

/**
 * @enum OpType
 * @brief Supported binary operations.
 */
typedef enum { 
    OP_ADD,     // +
    OP_SUB,     // -
    OP_MUL,     // *
    OP_DIV,     // /
    OP_MOD,     // %
    OP_EQ,      // ==
    OP_NEQ,     // !=
    OP_LT,      // <
    OP_GT,      // >
    OP_LTE,     // <=
    OP_GTE      // >=
} OpType;

struct Node; // Forward declaration

/**
 * @struct Node
 * @brief The fundamental building block of the Abstract Syntax Tree.
 *        Uses a Tagged Union to store data specific to the Node Type.
 */
typedef struct Node {
    NodeType type;
    struct Node* next; // Linked List pointer (next statement/declaration)

    union {
        // Root: List of functions and globals
        struct { struct Node* declarations; } program;
        
        // Scope: List of statements
        struct { struct Node* statements; } block;
        
        // Function Definition
        struct { 
            char* name; 
            struct Node* params; // Linked list of VarDecls (reused for params)
            struct Node* body;   // NODE_BLOCK
        } func_def;

        // Variable Declaration (Used for Globals, Locals, and Params)
        struct { 
            char* name; 
            struct Node* expression; // Initial value (optional for params)
            bool is_global;          // Flag for memory storage location
        } var_decl;

        // Function Call
        struct {
            char* name;
            struct Node* args;       // Linked list of expressions
        } call;

        // Statements
        struct { struct Node* condition; struct Node* then_branch; } if_stmt;
        struct { struct Node* condition; struct Node* body; } while_stmt;
        struct { struct Node* expression; } return_stmt;
        struct { struct Node* expression; } print_stmt;
        struct { char* name; struct Node* expression; } assign_stmt;

        // Expressions
        struct { int value; } integer;
        struct { char* name; } var_ref;
        struct { struct Node* left; struct Node* right; OpType op; } bin_op;
    } data;
} Node;

// Parser needs a Lookahead buffer now
typedef struct {
    Lexer* lexer;
    Token current;
    Token next;     // Lookahead token
} Parser;

Node* parse(Lexer* lexer);

// ============================================================================
// CODEGEN DEFINITIONS
// ============================================================================

void codegen(Node* node, FILE* file);

#endif // BAA_H