# CMakeLists.txt for Baa Preprocessor Unit Tests

# Common settings for all preprocessor tests
set(PREPROCESSOR_TEST_LIBRARIES
    test_framework
    baa_preprocessor
    baa_utils
)

set(PREPROCESSOR_TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/framework
    ${PROJECT_SOURCE_DIR}/src/preprocessor
)

# Original preprocessor conditional tests
add_executable(test_preprocessor test_preprocessor.c)
target_link_libraries(test_preprocessor PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor COMMAND test_preprocessor)
set_tests_properties(test_preprocessor PROPERTIES LABELS "unit;preprocessor;conditionals")

# Enhanced error system test
add_executable(test_enhanced_error_system test_enhanced_error_system.c)
target_link_libraries(test_enhanced_error_system PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_enhanced_error_system PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_enhanced_error_system COMMAND test_enhanced_error_system)
set_tests_properties(test_enhanced_error_system PROPERTIES LABELS "unit;preprocessor;errors")

# Comprehensive macro tests
add_executable(test_preprocessor_macros test_preprocessor_macros.c)
target_link_libraries(test_preprocessor_macros PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_macros PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_macros COMMAND test_preprocessor_macros)
set_tests_properties(test_preprocessor_macros PROPERTIES LABELS "unit;preprocessor;macros")

# Predefined macro tests
add_executable(test_preprocessor_predefined test_preprocessor_predefined.c)
target_link_libraries(test_preprocessor_predefined PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_predefined PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_predefined COMMAND test_preprocessor_predefined)
set_tests_properties(test_preprocessor_predefined PROPERTIES LABELS "unit;preprocessor;predefined")

# Conditional compilation tests
add_executable(test_preprocessor_conditionals test_preprocessor_conditionals.c)
target_link_libraries(test_preprocessor_conditionals PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_conditionals PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_conditionals COMMAND test_preprocessor_conditionals)
set_tests_properties(test_preprocessor_conditionals PROPERTIES LABELS "unit;preprocessor;conditionals")

# Error and warning directive tests
add_executable(test_preprocessor_directives test_preprocessor_directives.c)
target_link_libraries(test_preprocessor_directives PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_directives PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_directives COMMAND test_preprocessor_directives)
set_tests_properties(test_preprocessor_directives PROPERTIES LABELS "unit;preprocessor;directives")

# Comprehensive file-based preprocessor tests
add_executable(test_preprocessor_comprehensive test_preprocessor_comprehensive.c)
target_link_libraries(test_preprocessor_comprehensive PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_comprehensive PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_comprehensive COMMAND test_preprocessor_comprehensive)
set_tests_properties(test_preprocessor_comprehensive PROPERTIES LABELS "unit;preprocessor;comprehensive;integration")

# Advanced macro feature tests
add_executable(test_preprocessor_advanced_macros test_preprocessor_advanced_macros.c)
target_link_libraries(test_preprocessor_advanced_macros PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_advanced_macros PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_advanced_macros COMMAND test_preprocessor_advanced_macros)
set_tests_properties(test_preprocessor_advanced_macros PROPERTIES LABELS "unit;preprocessor;macros;advanced")

# Advanced expression evaluation tests
add_executable(test_preprocessor_expressions test_preprocessor_expressions.c)
target_link_libraries(test_preprocessor_expressions PRIVATE ${PREPROCESSOR_TEST_LIBRARIES})
target_include_directories(test_preprocessor_expressions PRIVATE ${PREPROCESSOR_TEST_INCLUDE_DIRS})
add_test(NAME test_preprocessor_expressions COMMAND test_preprocessor_expressions)
set_tests_properties(test_preprocessor_expressions PROPERTIES LABELS "unit;preprocessor;expressions;advanced")
