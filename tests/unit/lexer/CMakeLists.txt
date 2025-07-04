# CMakeLists.txt for Baa Lexer Unit Tests

# Common settings for all lexer tests
set(LEXER_TEST_LIBRARIES
    test_framework
    baa_lexer
    baa_utils
)

set(LEXER_TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/framework
)

add_executable(test_lexer test_lexer.c)
target_link_libraries(test_lexer PRIVATE baa_utils baa_lexer)

add_executable(test_number_parser test_number_parser.c)
target_link_libraries(test_number_parser PRIVATE baa_utils baa_lexer test_framework)

add_executable(test_tokens test_tokens.c)
target_link_libraries(test_tokens PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_tokens PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_tokens COMMAND test_tokens)
set_tests_properties(test_tokens PROPERTIES LABELS "unit;lexer;tokens")

add_executable(test_program_files test_program_files.c)
target_link_libraries(test_program_files PRIVATE
    baa_utils
    baa_lexer
    test_framework
)

if(WIN32)
    # On Windows, the math functions are in the standard C library
    # No need to link with a separate math library
else()
    # On Unix systems, link with the math library
    target_link_libraries(test_lexer m)
    target_link_libraries(test_number_parser m)
    target_link_libraries(test_tokens m)
    target_link_libraries(test_program_files m)
endif()

# Update existing tests with proper configuration
target_link_libraries(test_lexer PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_lexer PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_lexer COMMAND test_lexer)
set_tests_properties(test_lexer PROPERTIES LABELS "unit;lexer;basic")

target_link_libraries(test_number_parser PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_number_parser PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_number_parser COMMAND test_number_parser)
set_tests_properties(test_number_parser PROPERTIES LABELS "unit;lexer;numbers")

target_link_libraries(test_program_files PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_program_files PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_program_files COMMAND test_program_files)
set_tests_properties(test_program_files PROPERTIES LABELS "unit;lexer;files")

# Comprehensive lexer tests
add_executable(test_lexer_arabic test_lexer_arabic.c)
target_link_libraries(test_lexer_arabic PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_lexer_arabic PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_lexer_arabic COMMAND test_lexer_arabic)
set_tests_properties(test_lexer_arabic PROPERTIES LABELS "unit;lexer;arabic")

add_executable(test_lexer_strings test_lexer_strings.c)
target_link_libraries(test_lexer_strings PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_lexer_strings PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_lexer_strings COMMAND test_lexer_strings)
set_tests_properties(test_lexer_strings PROPERTIES LABELS "unit;lexer;strings")

add_executable(test_lexer_numbers test_lexer_numbers.c)
target_link_libraries(test_lexer_numbers PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_lexer_numbers PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_lexer_numbers COMMAND test_lexer_numbers)
set_tests_properties(test_lexer_numbers PROPERTIES LABELS "unit;lexer;numbers")

add_executable(test_lexer_operators test_lexer_operators.c)
target_link_libraries(test_lexer_operators PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_lexer_operators PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_lexer_operators COMMAND test_lexer_operators)
set_tests_properties(test_lexer_operators PROPERTIES LABELS "unit;lexer;operators")

add_executable(test_lexer_comments test_lexer_comments.c)
target_link_libraries(test_lexer_comments PRIVATE ${LEXER_TEST_LIBRARIES})
target_include_directories(test_lexer_comments PRIVATE ${LEXER_TEST_INCLUDE_DIRS})
add_test(NAME test_lexer_comments COMMAND test_lexer_comments)
set_tests_properties(test_lexer_comments PROPERTIES LABELS "unit;lexer;comments")
