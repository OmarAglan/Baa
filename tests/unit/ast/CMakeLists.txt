# AST unit tests

# Common settings for all AST tests
set(AST_TEST_LIBRARIES
    test_framework
    baa_ast
    baa_utils
    baa_types
)

set(AST_TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/framework
)

# Test for AST Type Representation Nodes
add_executable(test_ast_types test_ast_types.c)
target_link_libraries(test_ast_types PRIVATE ${AST_TEST_LIBRARIES})
target_include_directories(test_ast_types PRIVATE ${AST_TEST_INCLUDE_DIRS})
add_test(NAME test_ast_types COMMAND test_ast_types)
set_tests_properties(test_ast_types PROPERTIES LABELS "unit;ast;types")

# Test for AST Program Nodes
add_executable(test_ast_program test_ast_program.c)
target_link_libraries(test_ast_program PRIVATE ${AST_TEST_LIBRARIES})
target_include_directories(test_ast_program PRIVATE ${AST_TEST_INCLUDE_DIRS})
add_test(NAME test_ast_program COMMAND test_ast_program)
set_tests_properties(test_ast_program PROPERTIES LABELS "unit;ast;program")

# Test for AST Literal Expression Nodes
add_executable(test_ast_literals test_ast_literals.c)
target_link_libraries(test_ast_literals PRIVATE ${AST_TEST_LIBRARIES})
target_include_directories(test_ast_literals PRIVATE ${AST_TEST_INCLUDE_DIRS})
add_test(NAME test_ast_literals COMMAND test_ast_literals)
set_tests_properties(test_ast_literals PROPERTIES LABELS "unit;ast;literals")

# Test for AST Identifier Expression Nodes
add_executable(test_ast_identifiers test_ast_identifiers.c)
target_link_libraries(test_ast_identifiers PRIVATE ${AST_TEST_LIBRARIES})
target_include_directories(test_ast_identifiers PRIVATE ${AST_TEST_INCLUDE_DIRS})
add_test(NAME test_ast_identifiers COMMAND test_ast_identifiers)
set_tests_properties(test_ast_identifiers PROPERTIES LABELS "unit;ast;identifiers")

# Test for AST Binary Expression Nodes
add_executable(test_ast_binary_expressions test_ast_binary_expressions.c)
target_link_libraries(test_ast_binary_expressions PRIVATE ${AST_TEST_LIBRARIES})
target_include_directories(test_ast_binary_expressions PRIVATE ${AST_TEST_INCLUDE_DIRS})
add_test(NAME test_ast_binary_expressions COMMAND test_ast_binary_expressions)
set_tests_properties(test_ast_binary_expressions PROPERTIES LABELS "unit;ast;binary_expressions")
