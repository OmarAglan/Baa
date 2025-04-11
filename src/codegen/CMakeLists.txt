add_library(baa_codegen
    codegen.c
)

# Add LLVM-specific files only if LLVM is available
if(LLVM_AVAILABLE)
    target_sources(baa_codegen PRIVATE
        llvm_codegen.c
    )

    target_compile_definitions(baa_codegen PRIVATE
        -DLLVM_AVAILABLE=1
    )

    # Make sure LLVM CMake modules are included
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
    include(AddLLVM)

    # Get required LLVM libraries
    llvm_map_components_to_libnames(llvm_libs
        core
        support
        irreader
        analysis
        target
        mcjit
        native
        executionengine
    )

    # Print the determined LLVM libraries for debugging
    message(STATUS "LLVM Libraries: ${llvm_libs}")

    target_link_libraries(baa_codegen
        PRIVATE
            ${llvm_libs}
    )
else()
    # Add stub implementation when LLVM is not available
    target_sources(baa_codegen PRIVATE
        llvm_stub.c
    )

    target_compile_definitions(baa_codegen PRIVATE
        -DLLVM_AVAILABLE=0
    )
endif()

target_include_directories(baa_codegen
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
        ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(baa_codegen
    PRIVATE
        baa_utils
        baa_types
        baa_ast
)
