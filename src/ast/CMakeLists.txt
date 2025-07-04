add_library(baa_ast STATIC
    ast_node.c
    ast_expressions.c
    ast_program.c
    ast_statements.c
    ast_types.c
    # Add other ast_*.c files here as they are created
)

target_include_directories(baa_ast
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include # For baa/ast/ast.h, baa/ast/ast_types.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}   # For any internal AST headers if needed
)

# baa_ast depends on baa_utils for memory functions
target_link_libraries(baa_ast
    PRIVATE
        baa_utils
    INTERFACE
        BaaCommonSettings
)
