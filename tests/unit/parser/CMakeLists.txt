# Parser unit tests

# Common settings for all parser tests
set(PARSER_TEST_LIBRARIES
    test_framework
    baa_parser
    baa_lexer
    baa_preprocessor
    baa_ast
    baa_utils
    baa_types
)

set(PARSER_TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/framework
)

# Test for Parser Core Functions
add_executable(test_parser_core test_parser_core.c)
target_link_libraries(test_parser_core PRIVATE ${PARSER_TEST_LIBRARIES})
target_include_directories(test_parser_core PRIVATE ${PARSER_TEST_INCLUDE_DIRS})
add_test(NAME test_parser_core COMMAND test_parser_core)
set_tests_properties(test_parser_core PROPERTIES LABELS "unit;parser;core")

# Test for Parser Expression Functions
add_executable(test_parser_expressions test_parser_expressions.c)
target_link_libraries(test_parser_expressions PRIVATE ${PARSER_TEST_LIBRARIES})
target_include_directories(test_parser_expressions PRIVATE ${PARSER_TEST_INCLUDE_DIRS})
add_test(NAME test_parser_expressions COMMAND test_parser_expressions)
set_tests_properties(test_parser_expressions PROPERTIES LABELS "unit;parser;expressions")

# Test for Parser Statement Functions
add_executable(test_parser_statements test_parser_statements.c)
target_link_libraries(test_parser_statements PRIVATE ${PARSER_TEST_LIBRARIES})
target_include_directories(test_parser_statements PRIVATE ${PARSER_TEST_INCLUDE_DIRS})
add_test(NAME test_parser_statements COMMAND test_parser_statements)
set_tests_properties(test_parser_statements PROPERTIES LABELS "unit;parser;statements")
