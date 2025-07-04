# src/parser/CMakeLists.txt
add_library(baa_parser STATIC
    parser.c
    # Add other parser_*.c files here later (e.g., expression_parser.c)
)

target_include_directories(baa_parser
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include # For baa/parser/parser.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}   # For parser_internal.h
)

# Parser depends on utils (for malloc), lexer (for BaaLexer, tokens), and ast (for AST nodes)
target_link_libraries(baa_parser
    PRIVATE
        baa_utils
        baa_lexer
        baa_ast
    INTERFACE
        BaaCommonSettings
)
