# Test Framework Library

add_library(test_framework STATIC
    test_framework.c
    test_framework.h
)

target_include_directories(test_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}    # For test_framework.h
        ${PROJECT_SOURCE_DIR}/include  # For Baa headers
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src      # For internal headers if needed
)

# Link with all Baa libraries that the test framework uses
target_link_libraries(test_framework
    PUBLIC
        baa_ast
        baa_lexer
        baa_parser
        baa_preprocessor
        baa_utils
    INTERFACE
        BaaCommonSettings
)

# Ensure the test framework can access all necessary headers
target_compile_definitions(test_framework
    PUBLIC
        UNICODE
        _UNICODE
)
