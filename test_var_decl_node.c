#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"
#include <stdio.h>
#include <wchar.h>

int main() {
    printf("Testing Variable Declaration Node Implementation...\n");
    
    // Create a source span for testing
    BaaAstSourceSpan span = {
        .start = {.filename = "test.baa", .line = 1, .column = 1},
        .end = {.filename = "test.baa", .line = 1, .column = 20}
    };
    
    // Create a primitive type node for "عدد_صحيح" (integer)
    BaaNode *type_node = baa_ast_new_primitive_type_node(span, L"عدد_صحيح");
    if (!type_node) {
        printf("ERROR: Failed to create type node\n");
        return 1;
    }
    printf("✓ Created primitive type node for 'عدد_صحيح'\n");
    
    // Create an integer literal for initialization (value = 42)
    BaaNode *initializer = baa_ast_new_literal_int_node(span, 42, NULL);
    if (!initializer) {
        printf("ERROR: Failed to create initializer node\n");
        baa_ast_free_node(type_node);
        return 1;
    }
    printf("✓ Created integer literal initializer (42)\n");
    
    // Create a variable declaration: "ثابت عدد_صحيح س = 42"
    BaaNode *var_decl = baa_ast_new_var_decl_node(
        span, 
        L"س",                    // variable name (Arabic letter 'س')
        BAA_MOD_CONST,          // const modifier
        type_node,              // type: عدد_صحيح
        initializer             // initializer: 42
    );
    
    if (!var_decl) {
        printf("ERROR: Failed to create variable declaration node\n");
        baa_ast_free_node(type_node);
        baa_ast_free_node(initializer);
        return 1;
    }
    printf("✓ Created variable declaration node\n");
    
    // Verify the node structure
    if (var_decl->kind != BAA_NODE_KIND_VAR_DECL_STMT) {
        printf("ERROR: Wrong node kind\n");
        baa_ast_free_node(var_decl);
        return 1;
    }
    printf("✓ Node kind is correct (BAA_NODE_KIND_VAR_DECL_STMT)\n");
    
    BaaVarDeclData *data = (BaaVarDeclData *)var_decl->data;
    if (!data) {
        printf("ERROR: Node data is NULL\n");
        baa_ast_free_node(var_decl);
        return 1;
    }
    printf("✓ Node data is not NULL\n");
    
    if (wcscmp(data->name, L"س") != 0) {
        printf("ERROR: Variable name mismatch\n");
        baa_ast_free_node(var_decl);
        return 1;
    }
    printf("✓ Variable name is correct ('س')\n");
    
    if (data->modifiers != BAA_MOD_CONST) {
        printf("ERROR: Modifiers mismatch\n");
        baa_ast_free_node(var_decl);
        return 1;
    }
    printf("✓ Modifiers are correct (BAA_MOD_CONST)\n");
    
    if (!data->type_node || data->type_node->kind != BAA_NODE_KIND_TYPE) {
        printf("ERROR: Type node is invalid\n");
        baa_ast_free_node(var_decl);
        return 1;
    }
    printf("✓ Type node is valid\n");
    
    if (!data->initializer_expr || data->initializer_expr->kind != BAA_NODE_KIND_LITERAL_EXPR) {
        printf("ERROR: Initializer expression is invalid\n");
        baa_ast_free_node(var_decl);
        return 1;
    }
    printf("✓ Initializer expression is valid\n");
    
    // Test cleanup
    baa_ast_free_node(var_decl);
    printf("✓ Successfully freed variable declaration node\n");
    
    printf("\n🎉 All tests passed! Variable Declaration Node implementation is working correctly.\n");
    return 0;
}
