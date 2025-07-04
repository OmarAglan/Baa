# CMakeLists.txt for Baa Integration Tests

# Common settings for all integration tests
set(INTEGRATION_TEST_LIBRARIES
    test_framework
    baa_preprocessor
    baa_lexer
    baa_parser
    baa_ast
    baa_utils
    baa_types
)

set(INTEGRATION_TEST_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/framework
)

# Complete Pipeline Integration Tests
add_executable(test_pipeline_integration test_pipeline_integration.c)
target_link_libraries(test_pipeline_integration PRIVATE ${INTEGRATION_TEST_LIBRARIES})
target_include_directories(test_pipeline_integration PRIVATE ${INTEGRATION_TEST_INCLUDE_DIRS})
add_test(NAME test_pipeline_integration COMMAND test_pipeline_integration)
set_tests_properties(test_pipeline_integration PROPERTIES LABELS "integration;pipeline;complete")

# Component Interaction Tests
add_executable(test_component_interactions test_component_interactions.c)
target_link_libraries(test_component_interactions PRIVATE ${INTEGRATION_TEST_LIBRARIES})
target_include_directories(test_component_interactions PRIVATE ${INTEGRATION_TEST_INCLUDE_DIRS})
add_test(NAME test_component_interactions COMMAND test_component_interactions)
set_tests_properties(test_component_interactions PROPERTIES LABELS "integration;components;interactions")

# Real-World Scenario Tests
add_executable(test_real_world_scenarios test_real_world_scenarios.c)
target_link_libraries(test_real_world_scenarios PRIVATE ${INTEGRATION_TEST_LIBRARIES})
target_include_directories(test_real_world_scenarios PRIVATE ${INTEGRATION_TEST_INCLUDE_DIRS})
add_test(NAME test_real_world_scenarios COMMAND test_real_world_scenarios)
set_tests_properties(test_real_world_scenarios PROPERTIES LABELS "integration;scenarios;real-world")
