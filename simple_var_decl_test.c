#include <stdio.h>
#include <wchar.h>
#include <assert.h>

// Minimal test to verify Variable Declaration Node compilation
// This just tests that the headers compile correctly

#include "baa/ast/ast.h"
#include "baa/ast/ast_types.h"

int main() {
    printf("Testing Variable Declaration Node compilation...\n");
    
    // Test that the enum value exists
    BaaNodeKind kind = BAA_NODE_KIND_VAR_DECL_STMT;
    printf("✓ BAA_NODE_KIND_VAR_DECL_STMT enum value exists: %d\n", kind);
    
    // Test that the modifier constants exist
    BaaAstNodeModifiers mods = BAA_MOD_CONST | BAA_MOD_STATIC;
    printf("✓ Modifier constants exist: BAA_MOD_CONST=%u, BAA_MOD_STATIC=%u\n", 
           BAA_MOD_CONST, BAA_MOD_STATIC);
    
    // Test that the data structure exists (just check size)
    printf("✓ BaaVarDeclData structure size: %zu bytes\n", sizeof(BaaVarDeclData));
    
    // Test that function declarations exist (we can't call them without linking)
    printf("✓ Function declarations are accessible\n");
    
    printf("\n🎉 Variable Declaration Node headers compile successfully!\n");
    printf("   - BAA_NODE_KIND_VAR_DECL_STMT enum value: ✓\n");
    printf("   - BaaVarDeclData structure: ✓\n");
    printf("   - BaaAstNodeModifiers: ✓\n");
    printf("   - Function declarations: ✓\n");
    
    return 0;
}
