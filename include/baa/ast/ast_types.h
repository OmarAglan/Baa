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
    const wchar_t *filename; /**< Pointer to the canonical filename string (owned elsewhere, e.g., by compiler context). */
    size_t line;             /**< 1-based line number. */
    size_t column;           /**< 1-based column number of the start of the token/construct. */
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
    BAA_NODE_KIND_PROGRAM,      /**< Represents the root of the AST, a collection of top-level declarations. data: BaaProgramData* */
    BAA_NODE_KIND_PARAMETER,    /**< A function parameter. data: BaaParameterData* */
    BAA_NODE_KIND_FUNCTION_DEF, /**< A function definition. data: BaaFunctionDefData* */

    // Statement Kinds
    BAA_NODE_KIND_EXPR_STMT,     /**< An expression statement. data: BaaExprStmtData* */
    BAA_NODE_KIND_BLOCK_STMT,    /**< A block of statements { ... }. data: BaaBlockStmtData* */
    BAA_NODE_KIND_VAR_DECL_STMT, /**< A variable declaration statement. data: BaaVarDeclData* */
    BAA_NODE_KIND_IF_STMT,       /**< An if statement (إذا). data: BaaIfStmtData* */
    BAA_NODE_KIND_WHILE_STMT,    /**< A while loop statement (طالما). data: BaaWhileStmtData* */
    BAA_NODE_KIND_FOR_STMT,      /**< A for loop statement (لكل). data: BaaForStmtData* */
    BAA_NODE_KIND_RETURN_STMT,   /**< A return statement (إرجع). data: BaaReturnStmtData* */
    BAA_NODE_KIND_BREAK_STMT,    /**< A break statement (توقف). data: NULL */
    BAA_NODE_KIND_CONTINUE_STMT, /**< A continue statement (استمر). data: NULL */

    // Expression Kinds
    BAA_NODE_KIND_LITERAL_EXPR,    /**< Represents a literal value (integer, string, etc.). data: BaaLiteralExprData* */
    BAA_NODE_KIND_IDENTIFIER_EXPR, /**< An identifier used as an expression. data: BaaIdentifierExprData* */
    BAA_NODE_KIND_BINARY_EXPR,     /**< A binary expression (e.g., a + b). data: BaaBinaryExprData* */
    BAA_NODE_KIND_UNARY_EXPR,      /**< A unary expression (e.g., -a, !b). data: BaaUnaryExprData* */
    BAA_NODE_KIND_CALL_EXPR,       /**< A function call expression (e.g., func(a, b)). data: BaaCallExprData* */

    // Type Representation Kinds (for type syntax parsed from code)
    BAA_NODE_KIND_TYPE, /**< Represents a type specification. data: BaaTypeAstData* */

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

// --- Specific AST Node Data Structures ---

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

// == Identifier Expression Data ==

/**
 * @brief Data structure for an identifier expression node (BAA_NODE_KIND_IDENTIFIER_EXPR).
 */
typedef struct BaaIdentifierExprData
{
    wchar_t *name; /**< Duplicated identifier name. Owner must free. */
    // Future: BaaSymbol* resolved_symbol; /**< Link to symbol table entry after resolution. */
} BaaIdentifierExprData;

// == Binary Expression Data ==

/**
 * @brief Enumerates the different kinds of binary operators.
 */
typedef enum BaaBinaryOperatorKind
{
    // Arithmetic operators
    BAA_BINARY_OP_ADD,      /**< Addition (+) */
    BAA_BINARY_OP_SUBTRACT, /**< Subtraction (-) */
    BAA_BINARY_OP_MULTIPLY, /**< Multiplication (*) */
    BAA_BINARY_OP_DIVIDE,   /**< Division (/) */
    BAA_BINARY_OP_MODULO,   /**< Modulo (%) */

    // Comparison operators
    BAA_BINARY_OP_EQUAL,         /**< Equality (==) */
    BAA_BINARY_OP_NOT_EQUAL,     /**< Inequality (!=) */
    BAA_BINARY_OP_LESS_THAN,     /**< Less than (<) */
    BAA_BINARY_OP_LESS_EQUAL,    /**< Less than or equal (<=) */
    BAA_BINARY_OP_GREATER_THAN,  /**< Greater than (>) */
    BAA_BINARY_OP_GREATER_EQUAL, /**< Greater than or equal (>=) */

    // Logical operators
    BAA_BINARY_OP_LOGICAL_AND, /**< Logical AND (&&) */
    BAA_BINARY_OP_LOGICAL_OR,  /**< Logical OR (||) */

    // Bitwise operators (for future expansion)
    // BAA_BINARY_OP_BITWISE_AND,
    // BAA_BINARY_OP_BITWISE_OR,
    // BAA_BINARY_OP_BITWISE_XOR,
    // BAA_BINARY_OP_LEFT_SHIFT,
    // BAA_BINARY_OP_RIGHT_SHIFT,

    // Assignment operators (for future expansion)
    // BAA_BINARY_OP_ASSIGN,
    // BAA_BINARY_OP_ADD_ASSIGN,
    // etc.
} BaaBinaryOperatorKind;

// == Unary Expression Data ==

/**
 * @brief Enumerates the different kinds of unary operators.
 */
typedef enum BaaUnaryOperatorKind
{
    // Arithmetic unary operators
    BAA_UNARY_OP_PLUS,  /**< Unary plus (+a) */
    BAA_UNARY_OP_MINUS, /**< Unary minus (-a) */

    // Logical unary operators
    BAA_UNARY_OP_LOGICAL_NOT, /**< Logical NOT (!a) */

    // Bitwise unary operators (for future expansion)
    // BAA_UNARY_OP_BITWISE_NOT, /**< Bitwise NOT (~a) */

    // Pre/post increment/decrement (for future expansion)
    // BAA_UNARY_OP_PRE_INCREMENT,
    // BAA_UNARY_OP_POST_INCREMENT,
    // BAA_UNARY_OP_PRE_DECREMENT,
    // BAA_UNARY_OP_POST_DECREMENT,
} BaaUnaryOperatorKind;

// BaaBinaryExprData and BaaUnaryExprData will be defined after BaaNode to avoid circular dependency

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

// --- Additional AST Node Data Structures (defined after BaaNode) ---

// == Program Structure Data ==

/**
 * @brief Data structure for BAA_NODE_KIND_PROGRAM nodes.
 * Represents the root of the AST, containing all top-level declarations.
 */
typedef struct BaaProgramData
{
    BaaNode **top_level_declarations; /**< Dynamic array of BaaNode* (function defs, global var decls). */
    size_t count;                     /**< Number of declarations currently stored. */
    size_t capacity;                  /**< Current capacity of the declarations array. */
} BaaProgramData;

/**
 * @brief Data structure for BAA_NODE_KIND_PARAMETER nodes.
 * Represents a function parameter with its name and type.
 */
typedef struct BaaParameterData
{
    wchar_t *name;         /**< Parameter name (duplicated string). */
    BaaNode *type_node;    /**< Type specification (BaaNode* of kind BAA_NODE_KIND_TYPE). */
    // Future: BaaType* resolved_type; /**< Resolved type after semantic analysis. */
} BaaParameterData;

/**
 * @brief Data structure for BAA_NODE_KIND_FUNCTION_DEF nodes.
 * Represents a function definition with its name, return type, parameters, and body.
 */
typedef struct BaaFunctionDefData
{
    wchar_t *name;                      /**< Function name (duplicated string). */
    BaaAstNodeModifiers modifiers;      /**< Function modifiers (e.g., static, inline). */
    BaaNode *return_type_node;          /**< Return type specification (BaaNode* of kind BAA_NODE_KIND_TYPE). */
    BaaNode **parameters;               /**< Dynamic array of BaaNode* (each of kind BAA_NODE_KIND_PARAMETER). */
    size_t parameter_count;             /**< Number of parameters currently stored. */
    size_t parameter_capacity;          /**< Current capacity of the parameters array. */
    BaaNode *body;                      /**< Function body (BaaNode* of kind BAA_NODE_KIND_BLOCK_STMT). */
    bool is_variadic;                   /**< For C-style varargs (...). */
    // Future: BaaSymbol* symbol_entry; /**< Link to symbol table after resolution. */
} BaaFunctionDefData;

// == Statement Data ==

/**
 * @brief Data structure for an expression statement node (BAA_NODE_KIND_EXPR_STMT).
 * Represents a statement that consists of a single expression followed by a terminator.
 */
typedef struct BaaExprStmtData
{
    BaaNode *expression; /**< The expression (BaaNode* with an expression kind). */
} BaaExprStmtData;

/**
 * @brief Data structure for a binary expression node (BAA_NODE_KIND_BINARY_EXPR).
 */
typedef struct BaaBinaryExprData
{
    BaaNode *left_operand;               /**< Left operand expression. */
    BaaNode *right_operand;              /**< Right operand expression. */
    BaaBinaryOperatorKind operator_kind; /**< The binary operator. */
    // Future: BaaType* result_type; /**< Type of the result after semantic analysis. */
} BaaBinaryExprData;

/**
 * @brief Data structure for a unary expression node (BAA_NODE_KIND_UNARY_EXPR).
 */
typedef struct BaaUnaryExprData
{
    BaaNode *operand;                   /**< The operand expression. */
    BaaUnaryOperatorKind operator_kind; /**< The unary operator. */
    // Future: BaaType* result_type; /**< Type of the result after semantic analysis. */
} BaaUnaryExprData;

/**
 * @brief Data structure for a function call expression node (BAA_NODE_KIND_CALL_EXPR).
 * Represents a function call with its callee and arguments.
 */
typedef struct BaaCallExprData
{
    BaaNode *callee_expr;               /**< The callee expression (typically an identifier or member access). */
    BaaNode **arguments;                /**< Dynamic array of BaaNode* (expression kinds) for arguments. */
    size_t argument_count;              /**< Number of arguments currently stored. */
    size_t argument_capacity;           /**< Current capacity of the arguments array. */
    // Future: BaaType* result_type; /**< Type of the result after semantic analysis. */
} BaaCallExprData;

/**
 * @brief Data structure for a block statement node (BAA_NODE_KIND_BLOCK_STMT).
 * Represents a compound statement containing multiple sub-statements.
 */
typedef struct BaaBlockStmtData
{
    BaaNode **statements; /**< Dynamic array of BaaNode* (statement kinds). */
    size_t count;         /**< Number of statements currently stored. */
    size_t capacity;      /**< Current capacity of the statements array. */
    // Future: BaaScope* scope; /**< Link to its scope in symbol table. */
} BaaBlockStmtData;

/**
 * @brief Data structure for a variable declaration statement node (BAA_NODE_KIND_VAR_DECL_STMT).
 * Represents a variable declaration with optional type annotation and initializer.
 */
typedef struct BaaVarDeclData
{
    wchar_t *name;                 /**< Duplicated variable name. Owner must free. */
    BaaAstNodeModifiers modifiers; /**< Modifiers like const (ثابت), static (مستقر), etc. */
    BaaNode *type_node;            /**< BaaNode* of kind BAA_NODE_KIND_TYPE (the declared type syntax). */
    BaaNode *initializer_expr;     /**< Optional initializer expression (BaaNode* with an expression kind). Can be NULL. */
    // Future: BaaSymbol* symbol_entry; /**< Link to symbol table after resolution. */
    // BaaType* resolved_canonical_type; /**< Pointer to canonical BaaType after semantic analysis. */
} BaaVarDeclData;

/**
 * @brief Data structure for an if statement node (BAA_NODE_KIND_IF_STMT).
 * Represents an if statement with optional else branch.
 */
typedef struct BaaIfStmtData
{
    BaaNode *condition_expr; /**< Condition expression (BaaNode* with an expression kind). */
    BaaNode *then_stmt;      /**< Statement or block for the 'then' branch (BaaNode* with a statement kind). */
    BaaNode *else_stmt;      /**< Optional statement or block for the 'else' branch (BaaNode* with a statement kind). Can be NULL. */
} BaaIfStmtData;

/**
 * @brief Data structure for a while statement node (BAA_NODE_KIND_WHILE_STMT).
 * Represents a while loop statement.
 */
typedef struct BaaWhileStmtData
{
    BaaNode *condition_expr; /**< Condition expression (BaaNode* with an expression kind). */
    BaaNode *body_stmt;      /**< Body statement or block (BaaNode* with a statement kind). */
} BaaWhileStmtData;

/**
 * @brief Data structure for a for statement node (BAA_NODE_KIND_FOR_STMT).
 * Represents a C-style for loop statement.
 */
typedef struct BaaForStmtData
{
    BaaNode *initializer_stmt; /**< Initializer statement (variable declaration or expression statement). Can be NULL. */
    BaaNode *condition_expr;   /**< Condition expression (BaaNode* with an expression kind). Can be NULL. */
    BaaNode *increment_expr;   /**< Increment expression (BaaNode* with an expression kind). Can be NULL. */
    BaaNode *body_stmt;        /**< Body statement or block (BaaNode* with a statement kind). */
} BaaForStmtData;

/**
 * @brief Data structure for a return statement node (BAA_NODE_KIND_RETURN_STMT).
 * Represents a return statement with optional return value.
 */
typedef struct BaaReturnStmtData
{
    BaaNode *value_expr; /**< Optional return value expression (BaaNode* with an expression kind). Can be NULL for void returns. */
} BaaReturnStmtData;

// == Type Representation Data ==

/**
 * @brief Enumerates the different kinds of type specifications that can be parsed from source code.
 * This represents the syntactic form of types as they appear in the source, before semantic analysis.
 */
typedef enum BaaTypeAstKind
{
    BAA_TYPE_AST_KIND_PRIMITIVE,    /**< Primitive type (e.g., "عدد_صحيح", "حرف"). */
    BAA_TYPE_AST_KIND_ARRAY,        /**< Array type (e.g., "عدد_صحيح[10]"). */
    BAA_TYPE_AST_KIND_POINTER,      /**< Pointer type (future implementation). */
    BAA_TYPE_AST_KIND_USER_DEFINED, /**< User-defined type (future implementation for struct/enum names). */
} BaaTypeAstKind;

/**
 * @brief Data structure for a type representation node (BAA_NODE_KIND_TYPE).
 * This structure represents the type as parsed from the source code.
 * Semantic analysis will resolve this to a canonical BaaType* from the types.c system.
 */
typedef struct BaaTypeAstData
{
    BaaTypeAstKind type_ast_kind; /**< The kind of type specification. */
    union
    {
        struct
        {                  // For BAA_TYPE_AST_KIND_PRIMITIVE
            wchar_t *name; /**< Duplicated type name (e.g., L"عدد_صحيح"). */
        } primitive;
        struct
        {                               // For BAA_TYPE_AST_KIND_ARRAY
            BaaNode *element_type_node; /**< BaaNode* of kind BAA_NODE_KIND_TYPE for the element type. */
            BaaNode *size_expr;         /**< Optional: BaaNode* expression for array size (NULL for dynamic arrays). */
        } array;
        // Future: struct for pointer, struct/union/enum names
    } specifier;
    // Future: BaaType* resolved_canonical_type; /**< Populated by semantic analyzer. */
} BaaTypeAstData;

#endif // BAA_AST_TYPES_H
