add_executable(test_codegen
    test_codegen.c
)

# Add LLVM test
add_executable(test_llvm_codegen
    llvm_test.c
)

# Add literal system test for LLVM
add_executable(test_llvm_literals
    llvm_codegen_test.c
)

target_include_directories(test_codegen
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(test_llvm_codegen
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(test_llvm_literals
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_codegen
    PRIVATE
        baa_utils
        baa_types
        baa_lexer
        baa_parser
        baa_operators
        baa_ast
        baa_codegen
        test_framework
)

target_link_libraries(test_llvm_codegen
    PRIVATE
        baa_utils
        baa_types
        baa_ast
        baa_codegen
)

target_link_libraries(test_llvm_literals
    PRIVATE
        baa_utils
        baa_types
        baa_ast
        baa_codegen
)

# Only run the tests if LLVM is available
if(LLVM_AVAILABLE)
    add_test(
        NAME test_codegen
        COMMAND test_codegen
    )

    add_test(
        NAME test_llvm_codegen
        COMMAND test_llvm_codegen
    )

    add_test(
        NAME test_llvm_literals
        COMMAND test_llvm_literals
    )
endif()
