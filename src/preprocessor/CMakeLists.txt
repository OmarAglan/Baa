cmake_minimum_required(VERSION 3.20)

# Define the preprocessor library
add_library(baa_preprocessor STATIC
    preprocessor.c
    preprocessor_utils.c
    preprocessor_macros.c
    preprocessor_expansion.c
    preprocessor_conditionals.c
    preprocessor_expr_eval.c
    preprocessor_core.c
    preprocessor_directives.c
    preprocessor_line_processing.c
)

# Ensure the library can find the main include directory
target_include_directories(baa_preprocessor
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include # For baa/preprocessor/preprocessor.h
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # For preprocessor_internal.h
)

target_link_libraries(baa_preprocessor PRIVATE baa_utils INTERFACE BaaCommonSettings)
