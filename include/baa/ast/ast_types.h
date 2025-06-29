#ifndef BAA_AST_TYPES_H
#define BAA_AST_TYPES_H

#include <stddef.h>  // For size_t
#include <stdint.h>  // For uint32_t
#include <stdbool.h> // For bool (though Baa might have its own bool via utils.h)

// Forward declare BaaType from the main type system.
// This avoids a direct include dependency cycle if ast_types.h were included by types.h,
// but means we can only use BaaType* here.
typedef struct BaaType BaaType;

// --- Core Source Location Structures ---

/**
 * @brief Defines a specific point in the source code.
 * Used for error reporting and AST node spanning.
 */
typedef struct BaaSourceLocation
{
    const char *filename; /**< Pointer to the canonical filename string (owned elsewhere, e.g., by compiler context). */
    size_t line;          /**< 1-based line number. */
    size_t column;        /**< 1-based column number of the start of the token/construct. */
} BaaSourceLocation;

/**
 * @brief Defines a span of source code, from a start to an end location.
 * Every AST node will have a span.
 */
typedef struct BaaAstSourceSpan
{
    BaaSourceLocation start; /**< The starting location of the AST node's corresponding source text. */
    BaaSourceLocation end;   /**< The ending location of the AST node's corresponding source text. */
} BaaAstSourceSpan;

// --- Unified AST Node Kind Enumeration ---

/**
 * @brief Enumerates all possible kinds of Abstract Syntax Tree nodes in Baa.
 * This will be expanded significantly as more language constructs are supported.
 */
typedef enum BaaNodeKind
{
    // Utility/Placeholder Kinds
    BAA_NODE_KIND_UNKNOWN = 0, /**< Represents an uninitialized or error/placeholder node. data: NULL */

    // Program Structure Kinds
    BAA_NODE_KIND_PROGRAM, /**< Represents the root of the AST, a collection of top-level declarations. data: BaaProgramData* */
    // BAA_NODE_KIND_FUNCTION_DEF,   // To be added: Function definition. data: BaaFunctionDefData*
    // BAA_NODE_KIND_PARAMETER,      // To be added: A function parameter. data: BaaParameterData*

    // Statement Kinds
    // BAA_NODE_KIND_EXPR_STMT,      // To be added: An expression statement. data: BaaExprStmtData*
    // BAA_NODE_KIND_BLOCK_STMT,     // To be added: A block of statements { ... }. data: BaaBlockStmtData*
    // BAA_NODE_KIND_VAR_DECL_STMT,  // To be added: A variable declaration statement. data: BaaVarDeclData*

    // Expression Kinds
    BAA_NODE_KIND_LITERAL_EXPR, /**< Represents a literal value (integer, string, etc.). data: BaaLiteralExprData* */
    // BAA_NODE_KIND_IDENTIFIER_EXPR,// To be added: An identifier used as an expression. data: BaaIdentifierExprData*

    // Type Representation Kinds (for type syntax parsed from code)
    // BAA_NODE_KIND_TYPE,           // To be added: Represents a type specification. data: BaaTypeAstData*

    // Add more kinds here as they are designed and implemented...

} BaaNodeKind;

// --- AST Node Modifiers ---

/**
 * @brief Type for storing AST node modifiers as a bitmask.
 * Modifiers like const (ثابت), static (مستقر), etc.
 */
typedef uint32_t BaaAstNodeModifiers;

// Modifier Flags (examples, to be expanded)
#define BAA_MOD_NONE (0U)        /**< No modifiers. */
#define BAA_MOD_CONST (1U << 0)  /**< 'ثابت' (const) modifier. */
#define BAA_MOD_STATIC (1U << 1) /**< 'مستقر' (static) modifier. */
// Add BAA_MOD_INLINE, BAA_MOD_RESTRICT etc. as needed

// --- Literal Expression Specific Structures ---

/**
 * @brief Enumerates the kinds of literals supported.
 */
typedef enum BaaLiteralKind
{
    BAA_LITERAL_KIND_BOOL,
    BAA_LITERAL_KIND_INT,
    BAA_LITERAL_KIND_FLOAT,
    BAA_LITERAL_KIND_CHAR,
    BAA_LITERAL_KIND_STRING,
    BAA_LITERAL_KIND_NULL, // Representing a 'null' or 'فارغ' literal
} BaaLiteralKind;

/**
 * @brief Data structure for a literal expression node (BAA_NODE_KIND_LITERAL_EXPR).
 */
typedef struct BaaLiteralExprData
{
    BaaLiteralKind literal_kind; /**< The specific kind of literal (int, string, etc.). */
    union
    {
        bool bool_value;       /**< Value if literal_kind is BAA_LITERAL_KIND_BOOL. */
        long long int_value;   /**< Value if literal_kind is BAA_LITERAL_KIND_INT. */
        double float_value;    /**< Value if literal_kind is BAA_LITERAL_KIND_FLOAT. */
        wchar_t char_value;    /**< Value if literal_kind is BAA_LITERAL_KIND_CHAR. */
        wchar_t *string_value; /**< Duplicated string if literal_kind is BAA_LITERAL_KIND_STRING. Owner must free. */
    } value;                   /**< The actual value of the literal. */

    /**
     * @brief Pointer to the canonical BaaType (from types.c) representing the type of this literal.
     * This type is determined by the parser based on the literal's form and any suffixes.
     * E.g., "123" is int, "123.0" is float, "123ط" is long int.
     * The AST node does not own this BaaType*; it's a reference to a global/shared type object.
     */
    BaaType *determined_type;

    // Optional: For debugging or very precise error messages if needed later.
    // wchar_t* original_lexeme;
} BaaLiteralExprData;

// --- Base AST Node Structure ---
// Forward declaration, as BaaNode might be used by specific data structs
// that could be defined before BaaNode itself if we had circular dependencies.
// However, with void* data, this is less of an issue.
typedef struct BaaNode BaaNode;

/**
 * @brief The fundamental building block of the Baa Abstract Syntax Tree.
 * Every element in the AST is a BaaNode.
 */
struct BaaNode
{
    BaaNodeKind kind;      /**< The specific kind of this AST node, determining the structure of 'data'. */
    BaaAstSourceSpan span; /**< The source code span (start and end location) this node represents. */
    void *data;            /**< Pointer to a kind-specific data structure containing details for this node.
                                This must be cast to the appropriate type based on 'kind'. Can be NULL for
                                simple nodes that don't require extra data (e.g., a break statement). */
    // Future considerations:
    // BaaType* resolved_type;    /**< Populated by semantic analyzer: pointer to canonical BaaType. */
    // BaaNode* parent_node;      /**< Optional: pointer to the parent node in the AST. */
    // size_t unique_id;          /**< Optional: for debugging or analysis passes. */
};

#endif // BAA_AST_TYPES_H
