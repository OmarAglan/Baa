# Add subdirectories for each component
add_subdirectory(utils)
add_subdirectory(types)
add_subdirectory(operators)
add_subdirectory(lexer)
add_subdirectory(preprocessor) # Added missing preprocessor
add_subdirectory(analysis)
add_subdirectory(codegen)
add_subdirectory(ast)
# Parser is removed, so no add_subdirectory(parser)

# --- Compiler Library ---
# Create a library for the compiler logic itself
add_library(baa_compiler_lib STATIC
    compiler.c
)
target_include_directories(baa_compiler_lib PRIVATE # Compiler logic likely needs access to all component headers`
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(baa_compiler_lib PRIVATE # Compiler logic uses these
    baa_utils
    baa_types
    baa_operators
    baa_lexer
    baa_preprocessor
    baa_flow_analysis
    baa_codegen
    BaaCommonSettings
)
