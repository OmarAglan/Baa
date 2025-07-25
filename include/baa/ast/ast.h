#ifndef BAA_AST_H
#define BAA_AST_H

#include "baa/ast/ast_types.h" // Include the core type definitions
#include "baa/types/types.h"   // For BaaType
#include <stdbool.h>           // For bool

// --- Core AST Node Lifecycle Functions ---

/**
 * @brief Creates a new generic BaaNode.
 *
 * Allocates memory for a BaaNode and initializes its kind and source span.
 * The 'data' field of the newly created node will be initialized to NULL.
 * Specific node creation functions (e.g., baa_ast_new_literal_expr_node)
 * will call this and then allocate and assign the specific data structure.
 *
 * @param kind The BaaNodeKind for the new node.
 * @param span The BaaAstSourceSpan indicating the node's location in the source code.
 * @return A pointer to the newly allocated BaaNode, or NULL on allocation failure.
 *         The caller is responsible for populating node->data if necessary.
 */
BaaNode *baa_ast_new_node(BaaNodeKind kind, BaaAstSourceSpan span);

/**
 * @brief Frees a BaaNode and its associated data recursively.
 *
 * This function is the primary way to deallocate AST nodes. It will:
 * 1. Check the node's kind.
 * 2. Call a kind-specific helper function to free the contents of node->data
 *    (which includes freeing any duplicated strings and recursively freeing child BaaNodes).
 * 3. Free the node->data pointer itself (if not NULL).
 * 4. Free the BaaNode structure.
 *
 * It is safe to call this function with a NULL node pointer.
 *
 * @param node The BaaNode to be freed.
 */
void baa_ast_free_node(BaaNode *node);

// We will add more specific node creation function prototypes here later,
// e.g., BaaNode* baa_ast_new_literal_int_node(...);
// For now, just the generic ones.
// --- Specific AST Node Creation Functions ---

// == Literal Expressions ==

/**
 * @brief Creates a new AST node representing an integer literal.
 * The node's kind will be BAA_NODE_KIND_LITERAL_EXPR.
 * Its data will point to a BaaLiteralExprData struct.
 *
 * @param span The source span of the literal.
 * @param value The long long value of the integer literal.
 * @param type A pointer to the canonical BaaType (e.g., baa_type_int, baa_type_long) for this literal.
 *             The AST node does not take ownership of this type pointer.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_literal_int_node(BaaAstSourceSpan span, long long value, BaaType *type);

/**
 * @brief Creates a new AST node representing a string literal.
 * The node's kind will be BAA_NODE_KIND_LITERAL_EXPR.
 * Its data will point to a BaaLiteralExprData struct.
 * The provided string value will be duplicated.
 *
 * @param span The source span of the literal.
 * @param value The wide character string value of the literal. This function will duplicate it.
 * @param type A pointer to the canonical BaaType (e.g., baa_type_string or char_array_type) for this literal.
 *             The AST node does not take ownership of this type pointer.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_literal_string_node(BaaAstSourceSpan span, const wchar_t *value, BaaType *type);

// Add prototypes for other literal types (float, bool, char, null) as needed:
// BaaNode* baa_ast_new_literal_float_node(BaaSourceSpan span, double value, BaaType* type);
// BaaNode* baa_ast_new_literal_bool_node(BaaSourceSpan span, bool value, BaaType* type);
// BaaNode* baa_ast_new_literal_char_node(BaaSourceSpan span, wchar_t value, BaaType* type);
// BaaNode* baa_ast_new_literal_null_node(BaaSourceSpan span, BaaType* type);

// == Identifier Expressions ==

/**
 * @brief Creates a new AST node representing an identifier expression.
 * The node's kind will be BAA_NODE_KIND_IDENTIFIER_EXPR.
 * Its data will point to a BaaIdentifierExprData struct.
 * The provided identifier name will be duplicated.
 *
 * @param span The source span of the identifier.
 * @param name The identifier name. This function will duplicate it.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_identifier_expr_node(BaaAstSourceSpan span, const wchar_t *name);

// == Binary Expressions ==

/**
 * @brief Creates a new AST node representing a binary expression.
 * The node's kind will be BAA_NODE_KIND_BINARY_EXPR.
 * Its data will point to a BaaBinaryExprData struct.
 *
 * @param span The source span of the binary expression.
 * @param left_operand The left operand expression. Must not be NULL.
 * @param right_operand The right operand expression. Must not be NULL.
 * @param operator_kind The binary operator kind.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_binary_expr_node(BaaAstSourceSpan span, BaaNode *left_operand, BaaNode *right_operand, BaaBinaryOperatorKind operator_kind);

// == Unary Expressions ==

/**
 * @brief Creates a new AST node representing a unary expression.
 * The node's kind will be BAA_NODE_KIND_UNARY_EXPR.
 * Its data will point to a BaaUnaryExprData struct.
 *
 * @param span The source span of the unary expression.
 * @param operand The operand expression. Must not be NULL.
 * @param operator_kind The unary operator kind.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_unary_expr_node(BaaAstSourceSpan span, BaaNode *operand, BaaUnaryOperatorKind operator_kind);

// == Call Expressions ==

/**
 * @brief Creates a new AST node representing a function call expression.
 * The node's kind will be BAA_NODE_KIND_CALL_EXPR.
 * Its data will point to a BaaCallExprData struct with an empty arguments array.
 *
 * @param span The source span of the call expression.
 * @param callee_expr The callee expression (typically an identifier). Must not be NULL.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_call_expr_node(BaaAstSourceSpan span, BaaNode *callee_expr);

/**
 * @brief Adds an argument to a function call expression node.
 * Handles dynamic array resizing as needed.
 *
 * @param call_expr_node A BaaNode* of kind BAA_NODE_KIND_CALL_EXPR. Must not be NULL.
 * @param argument_node A BaaNode* expression to add as an argument. Must not be NULL.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_call_argument(BaaNode *call_expr_node, BaaNode *argument_node);

// We will also need a specific free function for BaaLiteralExprData's contents.
// This will be declared internally (e.g., in ast_expressions.h if we create it)
// and called by baa_ast_free_node's dispatch.
// Example (internal declaration, not for this public header yet):
// void baa_ast_free_literal_expr_data(BaaLiteralExprData* data);

// == Program Nodes ==

/**
 * @brief Creates a new AST node representing a program (root of the AST).
 * The node's kind will be BAA_NODE_KIND_PROGRAM.
 * Its data will point to a BaaProgramData struct with an empty declarations array.
 *
 * @param span The source span of the program.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_program_node(BaaAstSourceSpan span);

/**
 * @brief Adds a top-level declaration to a program node.
 * Handles dynamic array resizing as needed.
 *
 * @param program_node A BaaNode* of kind BAA_NODE_KIND_PROGRAM.
 * @param declaration_node A BaaNode* representing a top-level declaration.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_declaration_to_program(BaaNode *program_node, BaaNode *declaration_node);

// == Parameter Nodes ==

/**
 * @brief Creates a new AST node representing a function parameter.
 * The node's kind will be BAA_NODE_KIND_PARAMETER.
 * Its data will point to a BaaParameterData struct.
 *
 * @param span The source span of the parameter.
 * @param name The parameter name. This function will duplicate it.
 * @param type_node A BaaNode* of kind BAA_NODE_KIND_TYPE representing the parameter type. Must not be NULL.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_parameter_node(BaaAstSourceSpan span, const wchar_t *name, BaaNode *type_node);

// == Function Definition Nodes ==

/**
 * @brief Creates a new AST node representing a function definition.
 * The node's kind will be BAA_NODE_KIND_FUNCTION_DEF.
 * Its data will point to a BaaFunctionDefData struct with an empty parameters array.
 *
 * @param span The source span of the function definition.
 * @param name The function name. This function will duplicate it.
 * @param modifiers Function modifiers (e.g., static, inline).
 * @param return_type_node A BaaNode* of kind BAA_NODE_KIND_TYPE representing the return type. Must not be NULL.
 * @param body A BaaNode* of kind BAA_NODE_KIND_BLOCK_STMT representing the function body. Must not be NULL.
 * @param is_variadic Whether the function accepts variable arguments.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_function_def_node(BaaAstSourceSpan span, const wchar_t *name,
                                       BaaAstNodeModifiers modifiers, BaaNode *return_type_node,
                                       BaaNode *body, bool is_variadic);

/**
 * @brief Adds a parameter to a function definition node.
 * Handles dynamic array resizing as needed.
 *
 * @param function_def_node A BaaNode* of kind BAA_NODE_KIND_FUNCTION_DEF. Must not be NULL.
 * @param parameter_node A BaaNode* of kind BAA_NODE_KIND_PARAMETER to add. Must not be NULL.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_function_parameter(BaaNode *function_def_node, BaaNode *parameter_node);

// == Statement Nodes ==

/**
 * @brief Creates a new AST node representing an expression statement.
 * The node's kind will be BAA_NODE_KIND_EXPR_STMT.
 * Its data will point to a BaaExprStmtData struct.
 *
 * @param span The source span of the statement.
 * @param expression_node A BaaNode* representing the expression. Must not be NULL.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_expr_stmt_node(BaaAstSourceSpan span, BaaNode *expression_node);

/**
 * @brief Creates a new AST node representing a block statement.
 * The node's kind will be BAA_NODE_KIND_BLOCK_STMT.
 * Its data will point to a BaaBlockStmtData struct with an empty statements array.
 *
 * @param span The source span of the block.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_block_stmt_node(BaaAstSourceSpan span);

/**
 * @brief Adds a statement to a block statement node.
 * Handles dynamic array resizing as needed.
 *
 * @param block_node A BaaNode* of kind BAA_NODE_KIND_BLOCK_STMT.
 * @param statement_node A BaaNode* representing a statement.
 * @return true on success, false on failure (e.g., memory allocation failure).
 */
bool baa_ast_add_stmt_to_block(BaaNode *block_node, BaaNode *statement_node);

// == Type Representation Nodes ==

/**
 * @brief Creates a new AST node representing a primitive type specification.
 * The node's kind will be BAA_NODE_KIND_TYPE.
 * Its data will point to a BaaTypeAstData struct with BAA_TYPE_AST_KIND_PRIMITIVE.
 *
 * @param span The source span of the type specification.
 * @param type_name The name of the primitive type (e.g., L"عدد_صحيح"). This function will duplicate it.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_primitive_type_node(BaaAstSourceSpan span, const wchar_t *type_name);

/**
 * @brief Creates a new AST node representing an array type specification.
 * The node's kind will be BAA_NODE_KIND_TYPE.
 * Its data will point to a BaaTypeAstData struct with BAA_TYPE_AST_KIND_ARRAY.
 *
 * @param span The source span of the array type specification.
 * @param element_type_node A BaaNode* of kind BAA_NODE_KIND_TYPE representing the element type. Must not be NULL.
 * @param size_expr A BaaNode* expression for the array size, or NULL for dynamic arrays.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_array_type_node(BaaAstSourceSpan span, BaaNode *element_type_node, BaaNode *size_expr);

// == Variable Declaration Statements ==

/**
 * @brief Creates a new AST node representing a variable declaration statement.
 * The node's kind will be BAA_NODE_KIND_VAR_DECL_STMT.
 * Its data will point to a BaaVarDeclData struct.
 *
 * @param span The source span of the variable declaration.
 * @param name The variable name. This function will duplicate it.
 * @param modifiers Modifiers like const (ثابت), static (مستقر), etc.
 * @param type_node A BaaNode* of kind BAA_NODE_KIND_TYPE representing the variable type. Must not be NULL.
 * @param initializer_expr A BaaNode* expression for initialization, or NULL if no initializer.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_var_decl_node(BaaAstSourceSpan span, const wchar_t *name,
                                   BaaAstNodeModifiers modifiers, BaaNode *type_node,
                                   BaaNode *initializer_expr);

// == Control Flow Statements ==

/**
 * @brief Creates a new AST node representing an if statement.
 * The node's kind will be BAA_NODE_KIND_IF_STMT.
 * Its data will point to a BaaIfStmtData struct.
 *
 * @param span The source span of the if statement.
 * @param condition_expr A BaaNode* expression for the condition. Must not be NULL.
 * @param then_stmt A BaaNode* statement for the 'then' branch. Must not be NULL.
 * @param else_stmt A BaaNode* statement for the 'else' branch, or NULL if no else clause.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_if_stmt_node(BaaAstSourceSpan span, BaaNode *condition_expr, BaaNode *then_stmt, BaaNode *else_stmt);

/**
 * @brief Creates a new AST node representing a while statement.
 * The node's kind will be BAA_NODE_KIND_WHILE_STMT.
 * Its data will point to a BaaWhileStmtData struct.
 *
 * @param span The source span of the while statement.
 * @param condition_expr A BaaNode* expression for the condition. Must not be NULL.
 * @param body_stmt A BaaNode* statement for the body. Must not be NULL.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_while_stmt_node(BaaAstSourceSpan span, BaaNode *condition_expr, BaaNode *body_stmt);

/**
 * @brief Creates a new AST node representing a for statement.
 * The node's kind will be BAA_NODE_KIND_FOR_STMT.
 * Its data will point to a BaaForStmtData struct.
 *
 * @param span The source span of the for statement.
 * @param initializer_stmt A BaaNode* statement for initialization, or NULL.
 * @param condition_expr A BaaNode* expression for the condition, or NULL.
 * @param increment_expr A BaaNode* expression for the increment, or NULL.
 * @param body_stmt A BaaNode* statement for the body. Must not be NULL.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_for_stmt_node(BaaAstSourceSpan span, BaaNode *initializer_stmt, BaaNode *condition_expr, BaaNode *increment_expr, BaaNode *body_stmt);

/**
 * @brief Creates a new AST node representing a return statement.
 * The node's kind will be BAA_NODE_KIND_RETURN_STMT.
 * Its data will point to a BaaReturnStmtData struct.
 *
 * @param span The source span of the return statement.
 * @param value_expr A BaaNode* expression for the return value, or NULL for void returns.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_return_stmt_node(BaaAstSourceSpan span, BaaNode *value_expr);

/**
 * @brief Creates a new AST node representing a break statement.
 * The node's kind will be BAA_NODE_KIND_BREAK_STMT.
 * Its data will be NULL (break statements don't need additional data).
 *
 * @param span The source span of the break statement.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_break_stmt_node(BaaAstSourceSpan span);

/**
 * @brief Creates a new AST node representing a continue statement.
 * The node's kind will be BAA_NODE_KIND_CONTINUE_STMT.
 * Its data will be NULL (continue statements don't need additional data).
 *
 * @param span The source span of the continue statement.
 * @return A pointer to the new BaaNode, or NULL on failure.
 */
BaaNode *baa_ast_new_continue_stmt_node(BaaAstSourceSpan span);

#endif // BAA_AST_H
