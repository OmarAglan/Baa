# Baa Compiler Test Suite

## Overview

The Baa compiler test suite provides comprehensive testing infrastructure for all compiler components. This document describes the test framework architecture, how to run tests, how to add new tests, and testing best practices.

## Test Framework Architecture

### Directory Structure
```
tests/
├── CMakeLists.txt              # Main test configuration
├── framework/                  # Test framework implementation
│   ├── test_framework.h        # Core testing macros and utilities
│   └── test_framework.c        # Test framework implementation
├── unit/                       # Unit tests for individual components
│   ├── CMakeLists.txt
│   ├── ast/                    # AST component tests
│   ├── lexer/                  # Lexer component tests
│   ├── parser/                 # Parser component tests
│   ├── preprocessor/           # Preprocessor component tests
│   └── utils/                  # Utility function tests
├── component/                  # Component integration tests
├── integration/                # End-to-end pipeline tests
├── regression/                 # Regression test cases
├── data/                       # Test data and sample programs
│   ├── valid/                  # Valid Baa programs
│   ├── invalid/                # Invalid programs (for error testing)
│   └── edge_cases/             # Edge case scenarios
└── expected/                   # Expected outputs and AST structures
```

### Test Framework Components

#### Core Testing Macros
```c
// Basic assertions
#define ASSERT(condition) 
#define ASSERT_EQ(expected, actual)
#define ASSERT_NE(expected, actual)
#define ASSERT_NULL(ptr)
#define ASSERT_NOT_NULL(ptr)
#define ASSERT_STR_EQ(expected, actual)

// Memory testing
#define ASSERT_NO_MEMORY_LEAKS()
#define TRACK_MEMORY_ALLOCATION()

// Test organization
#define TEST_CASE(name)
#define TEST_SUITE(name)
#define RUN_TEST(test_function)
```

#### Test Utilities
```c
// AST testing utilities
BaaNode* create_test_ast_node(BaaNodeKind kind, const char* test_data);
void assert_ast_structure(BaaNode* root, const char* expected_structure);
void print_ast_debug(BaaNode* root);

// Parser testing utilities
BaaNode* parse_test_string(const wchar_t* source);
void assert_parse_success(const wchar_t* source);
void assert_parse_error(const wchar_t* source, BaaErrorCode expected_error);

// Lexer testing utilities
BaaToken* tokenize_test_string(const wchar_t* source);
void assert_token_sequence(const wchar_t* source, BaaTokenType* expected_types, size_t count);

// File testing utilities
wchar_t* load_test_file(const char* relative_path);
void compare_with_expected_file(const char* actual_output, const char* expected_file);
```

## Running Tests

### Command Line Interface

#### Run All Tests
```bash
# Build and run all tests
cmake --build build --target all
ctest --test-dir build

# Run with verbose output
ctest --test-dir build --verbose

# Run with parallel execution
ctest --test-dir build --parallel 4
```

#### Run Specific Test Categories
```bash
# Run only unit tests
ctest --test-dir build --label-regex "unit"

# Run only AST tests
ctest --test-dir build --label-regex "ast"

# Run only integration tests
ctest --test-dir build --label-regex "integration"
```

#### Run Individual Tests
```bash
# Run specific test executable
./build/tests/test_ast_types
./build/tests/test_parser_expressions

# Run with memory checking (if available)
valgrind ./build/tests/test_ast_types
```

### IDE Integration

#### Visual Studio Code
```json
// .vscode/tasks.json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Run All Tests",
            "type": "shell",
            "command": "ctest",
            "args": ["--test-dir", "build", "--verbose"],
            "group": "test"
        }
    ]
}
```

#### CLion/Visual Studio
- Tests automatically discovered through CMake integration
- Run individual tests from Test Explorer
- Debug tests with integrated debugger

## Adding New Tests

### Creating Unit Tests

#### 1. Create Test File
```c
// tests/unit/component/test_new_feature.c
#include "test_framework.h"
#include "baa/component/new_feature.h"

TEST_CASE(test_basic_functionality)
{
    // Arrange
    setup_test_data();
    
    // Act
    result = call_function_under_test();
    
    // Assert
    ASSERT_NOT_NULL(result);
    ASSERT_EQ(expected_value, result->value);
    
    // Cleanup
    cleanup_test_data();
}

TEST_CASE(test_error_handling)
{
    // Test error conditions
    ASSERT_NULL(call_function_with_invalid_input());
}

int main(void)
{
    TEST_SUITE("New Feature Tests");
    
    RUN_TEST(test_basic_functionality);
    RUN_TEST(test_error_handling);
    
    return test_suite_result();
}
```

#### 2. Update CMakeLists.txt
```cmake
# tests/unit/component/CMakeLists.txt
add_executable(test_new_feature test_new_feature.c)
target_link_libraries(test_new_feature 
    PRIVATE 
        baa_component
        test_framework
)
add_test(NAME test_new_feature COMMAND test_new_feature)
set_tests_properties(test_new_feature PROPERTIES LABELS "unit;component")
```

### Creating Integration Tests

#### 1. Create Test File
```c
// tests/integration/test_pipeline_feature.c
#include "test_framework.h"
#include "baa/preprocessor/preprocessor.h"
#include "baa/lexer/lexer.h"
#include "baa/parser/parser.h"

TEST_CASE(test_complete_pipeline)
{
    // Load test source
    wchar_t* source = load_test_file("data/valid/basic/simple_program.baa");
    
    // Preprocess
    wchar_t* preprocessed = baa_preprocess(source);
    ASSERT_NOT_NULL(preprocessed);
    
    // Tokenize
    BaaLexer lexer;
    baa_init_lexer(&lexer, preprocessed, "test.baa");
    
    // Parse
    BaaNode* ast = baa_parse_program(&lexer);
    ASSERT_NOT_NULL(ast);
    ASSERT_EQ(BAA_NODE_KIND_PROGRAM, ast->kind);
    
    // Verify AST structure
    assert_ast_structure(ast, "expected/simple_program.ast");
    
    // Cleanup
    baa_ast_free_node(ast);
    baa_free(preprocessed);
    baa_free(source);
}
```

### Creating Test Data

#### 1. Valid Test Programs
```baa
// tests/data/valid/basic/variable_declaration.baa
متغير عدد_صحيح العدد = 42.
متغير نص الاسم = "مرحبا".
```

#### 2. Expected AST Structure
```json
// tests/expected/variable_declaration.ast
{
    "kind": "PROGRAM",
    "declarations": [
        {
            "kind": "VAR_DECL_STMT",
            "type": {"kind": "PRIMITIVE", "name": "عدد_صحيح"},
            "identifier": "العدد",
            "initializer": {"kind": "LITERAL_EXPR", "value": 42}
        },
        {
            "kind": "VAR_DECL_STMT", 
            "type": {"kind": "PRIMITIVE", "name": "نص"},
            "identifier": "الاسم",
            "initializer": {"kind": "LITERAL_EXPR", "value": "مرحبا"}
        }
    ]
}
```

## Testing Best Practices

### Test Organization
1. **One Test Per Function**: Each test should focus on a single aspect
2. **Descriptive Names**: Test names should clearly indicate what is being tested
3. **Arrange-Act-Assert**: Structure tests with clear setup, execution, and verification phases
4. **Independent Tests**: Tests should not depend on each other's state

### Test Data Management
1. **Isolated Test Data**: Each test should use its own test data
2. **Cleanup**: Always clean up allocated resources
3. **Realistic Data**: Use realistic Baa language constructs
4. **Edge Cases**: Include boundary conditions and error cases

### Error Testing
1. **Expected Errors**: Test that invalid input produces expected errors
2. **Error Messages**: Verify error messages are helpful and accurate
3. **Recovery**: Test error recovery mechanisms
4. **Memory Safety**: Ensure no memory leaks even in error conditions

### Performance Testing
1. **Benchmarks**: Include performance benchmarks for critical paths
2. **Regression Detection**: Monitor for performance regressions
3. **Memory Usage**: Track memory usage patterns
4. **Scalability**: Test with various input sizes

## Debugging Tests

### Common Issues
1. **Memory Leaks**: Use memory debugging tools
2. **Assertion Failures**: Check expected vs actual values
3. **Segmentation Faults**: Verify pointer validity
4. **Test Timeouts**: Check for infinite loops

### Debugging Tools
```bash
# Memory debugging
valgrind --leak-check=full ./test_executable

# Address sanitizer (if compiled with -fsanitize=address)
./test_executable

# GDB debugging
gdb ./test_executable
(gdb) run
(gdb) bt  # backtrace on crash
```

### Test Output Analysis
```bash
# Verbose test output
ctest --test-dir build --verbose --output-on-failure

# Test timing information
ctest --test-dir build --verbose --extra-verbose
```

## Continuous Integration

### Automated Testing
- All tests run automatically on every commit
- Tests must pass before code can be merged
- Coverage reports generated automatically
- Performance regression detection

### Quality Gates
- Minimum 90% test coverage for new code
- All tests must pass
- No memory leaks detected
- Performance within acceptable bounds

## Coverage Analysis

### Generating Coverage Reports
```bash
# Configure with coverage
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

# Build and run tests
cmake --build build
ctest --test-dir build

# Generate coverage report
gcov build/src/**/*.gcno
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### Coverage Targets
- **Unit Tests**: 90% line coverage minimum
- **Integration Tests**: 100% of public APIs
- **Critical Paths**: 100% coverage for memory management and error handling

This test suite documentation will be updated as the testing infrastructure evolves and new testing patterns are established.
