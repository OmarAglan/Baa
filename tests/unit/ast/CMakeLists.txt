# AST unit tests

# Test for AST Type Representation Nodes
add_executable(test_ast_types test_ast_types.c)
target_link_libraries(test_ast_types
    PRIVATE
        test_framework
        baa_ast
        baa_utils
)
target_include_directories(test_ast_types
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/tests/framework
)

# Add the test to CTest
add_test(NAME test_ast_types COMMAND test_ast_types)
set_tests_properties(test_ast_types PROPERTIES LABELS "unit;ast")
