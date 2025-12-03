# Create the library target first
add_library(baa_codegen)

# Add LLVM-specific files only if LLVM is available and found
if(USE_LLVM)
    # Check if we have manual configuration or find_package success
    if (LLVM_FOUND OR EXISTS "C:/Program Files/LLVM/include/llvm-c/Core.h")
        message(STATUS "Enabling LLVM Backend (Manual/Auto)")
        
        target_sources(baa_codegen PRIVATE
            codegen.c
            llvm_codegen.c
        )
        target_compile_definitions(baa_codegen PRIVATE
            LLVM_AVAILABLE=1
        )

        if (LLVM_FOUND)
             target_include_directories(baa_codegen PRIVATE ${LLVM_INCLUDE_DIRS})
             target_link_libraries(baa_codegen
                PRIVATE
                    LLVM::Core
                    LLVM::Support
                    LLVM::Analysis
                    LLVM::Target
                    LLVM::ExecutionEngine
                    LLVM::NativeTarget
            )
        else()
            # Manual Fallback for Windows Installation
            message(STATUS "Using Manual LLVM Configuration")
            target_include_directories(baa_codegen PRIVATE "C:/Program Files/LLVM/include")
            link_directories("C:/Program Files/LLVM/lib")
            target_link_libraries(baa_codegen PRIVATE "LLVM-C")
        endif()
    else()
        message(WARNING "LLVM requested but not found. Falling back to stub.")
        target_sources(baa_codegen PRIVATE
            codegen.c
            llvm_stub.c
        )
        target_compile_definitions(baa_codegen PRIVATE
            LLVM_AVAILABLE=0
        )
    endif()
else()
    # Add stub implementation when LLVM is not available
    target_sources(baa_codegen PRIVATE
        codegen.c
        llvm_stub.c
    )
    target_compile_definitions(baa_codegen PRIVATE
    LLVM_AVAILABLE=0
    )
endif()

target_include_directories(baa_codegen
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include  # For baa/codegen/codegen.h and baa/codegen/llvm_codegen.h
)

# baa_codegen depends on baa_utils and baa_types
# (assuming AST dependencies in llvm_codegen.c are handled/removed for now)
target_link_libraries(baa_codegen PRIVATE baa_utils baa_types INTERFACE BaaCommonSettings)
